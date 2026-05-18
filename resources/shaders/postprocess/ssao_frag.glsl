#version 330 core

in vec2 TexCoords;
out float FragColor;

// depth image of the scene (how far away each pixel's surface is)
uniform sampler2D depthTex;
// tiny 4x4 texture of random rotation vectors, tiled across the screen
uniform sampler2D noiseTex;
// 64 pre-generated "tennis ball" directions in a hemisphere
uniform vec3 samples[64];
// camera projection matrix (3d -> screen)
uniform mat4 projection;
// inverse of projection (screen -> 3d), used to reconstruct positions
uniform mat4 invProjection;
// how many times the noise texture tiles across the screen (screenRes / 4)
uniform vec2 noiseScale;
// how far each sample travels from the surface (in world units)
uniform float radius;
// tiny depth offset to stop a surface from occluding itself
uniform float bias;
// size of one pixel in uv space (1/screenWidth, 1/screenHeight)
uniform vec2 texelSize;

// takes a screen uv and returns the 3d position of whatever surface is there
vec3 reconstructViewPos(vec2 uv) {
    float depth = texture(depthTex, uv).r;
    vec4 ndcPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = invProjection * ndcPos;
    return viewPos.xyz / viewPos.w;
}

// depth-aware normal reconstruction:
// dFdx/dFdy goes haywire at silhouette/voxel edges (the two derivative samples land on
// different surfaces and the resulting "normal" points in a random direction, which
// then biases the AO hemisphere into the geometry and produces dark blotches).
//
// instead, look at all four neighbors and pick the pair (left/right, top/bottom) that
// is closer in depth to the center pixel. that pair almost always lies on the same
// surface as the center, giving a robust normal even one pixel from a silhouette.
vec3 reconstructNormal(vec3 centerPos, vec2 uv) {
    vec3 pL = reconstructViewPos(uv - vec2(texelSize.x, 0.0));
    vec3 pR = reconstructViewPos(uv + vec2(texelSize.x, 0.0));
    vec3 pD = reconstructViewPos(uv - vec2(0.0, texelSize.y));
    vec3 pU = reconstructViewPos(uv + vec2(0.0, texelSize.y));

    vec3 dx = (abs(pR.z - centerPos.z) < abs(centerPos.z - pL.z)) ? (pR - centerPos)
                                                                  : (centerPos - pL);
    vec3 dy = (abs(pU.z - centerPos.z) < abs(centerPos.z - pD.z)) ? (pU - centerPos)
                                                                  : (centerPos - pD);

    return normalize(cross(dx, dy));
}

void main() {
    // --- where are we? ---
    vec3 fragPos = reconstructViewPos(TexCoords);

    // --- which way is "up" from this surface? ---
    vec3 normal = reconstructNormal(fragPos, TexCoords);

    // --- randomize the throw direction so we don't get repeating patterns ---
    vec3 randomVec = normalize(texture(noiseTex, TexCoords * noiseScale).xyz);
    // gram-schmidt: make randomVec perpendicular to the normal (so it lies flat on the surface)
    vec3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    // TBN: transforms sample directions to align with this surface + random spin
    mat3 TBN       = mat3(tangent, bitangent, normal);

    // --- throw 64 balls and count how many hit something ---
    float occlusion = 0.0;
    for (int i = 0; i < 64; ++i) {

        // rotate the sample direction to face this surface, scale by radius
        vec3 samplePos = fragPos + TBN * samples[i] * radius;

        // figure out which screen pixel this 3d point corresponds to
        vec4 offset = projection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;           // perspective divide
        offset.xyz = offset.xyz * 0.5 + 0.5; // ndc [-1,1] -> uv [0,1]

        // skip samples that project off-screen — clamped border depth reads as a
        // false occluder and darkens screen edges (especially the gun in the corner).
        if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0)
            continue;

        // look up the actual surface depth at that screen pixel
        float sampleDepth = reconstructViewPos(offset.xy).z;

        // range check: contributions fall off smoothly to zero when the occluder is
        // farther than `radius` from us. the previous form (smoothstep on radius/|dz|)
        // never fully attenuates — even very distant geometry leaks in.
        float rangeCheck = 1.0 - smoothstep(radius * 0.5, radius, abs(fragPos.z - sampleDepth));

        // if the real surface is at the same depth or closer than our sample point,
        // something is blocking it. bias prevents self-intersection.
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }

    // 1.0 = no occlusion, 0.0 = fully occluded
    FragColor = 1.0 - (occlusion / 64.0);
}
