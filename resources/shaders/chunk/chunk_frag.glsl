#version 330 core
// Textures
uniform usampler3D matIDTex;

// General Chunk Data
uniform vec3 chunkWorldPos;
//uniform float chunkSizeMeters;
uniform float voxelSizeMeters;
uniform mat4 V;

// Lighting
uniform vec3 lightPos; //added
uniform vec3 lightDir;
uniform vec3 camPos; // added
uniform vec3 lightColor; //added

// Cascaded voxel-locked shadows
const int MAX_CASCADES = 4;
uniform sampler2DArray shadowMap;
uniform mat4 lightSpaceMatrices[MAX_CASCADES];
uniform float cascadeEnds[MAX_CASCADES];
uniform int cascadeCount;
uniform int shadowEnabled;
uniform float shadowMapSize;
uniform float shadowStrength;
uniform float minShadowVisibility;
uniform float shadowBlurTexels;
uniform float normalBiasVoxels;

// vertex shader ins.
flat in uint frag_normalID;
in vec3 worldPos;

// Material lookup information
// pads to next 16 bytes
// vec4 16 bytes, float 8 bytes, so each Material is 64 bytes
struct Material {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
};
// import materials.
layout(std140) uniform materials {
    Material materialArray[256];
};
// looks up normal vector from the given index.
vec3 normalLookup[6] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(-1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, -1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(0.0, 0.0, -1.0)
);

out vec4 color;

// create a random number [0-1] from a float.
float rand(float n) {
    return fract(sin(n) * 43758.5453123);
}
// Creates a random number [0-1] from a vec3.
float rand(vec3 p) {
    return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453123);
}

int selectCascade(float depth)
{
    for (int i = 0; i < MAX_CASCADES; ++i) {
        if (i >= cascadeCount) {
            break;
        }
        if (depth <= cascadeEnds[i]) {
            return i;
        }
    }
    return max(cascadeCount - 1, 0);
}

float tentWeight(int offset)
{
    return 3.0 - abs(float(offset));
}

float sampleShadowCascade(int cascade, vec3 receiver, vec3 N, vec3 L, out bool valid)
{
    valid = false;
    if (cascade < 0 || cascade >= cascadeCount) {
        return 1.0;
    }

    vec4 lightSpace = lightSpaceMatrices[cascade] * vec4(receiver, 1.0);
    vec3 proj = lightSpace.xyz / lightSpace.w;
    proj = proj * 0.5 + 0.5;

    if (proj.x < 0.0 || proj.x > 1.0 ||
        proj.y < 0.0 || proj.y > 1.0 ||
        proj.z < 0.0 || proj.z > 1.0) {
        return 1.0;
    }
    valid = true;

    float currentDepth = proj.z;
    float depthBias = max(0.00035 * (1.0 - dot(N, L)), 0.00008);
    float texel = shadowBlurTexels / shadowMapSize;
    float lit = 0.0;
    float weightSum = 0.0;

    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            float weight = tentWeight(x) * tentWeight(y);
            vec2 uv = proj.xy + vec2(float(x), float(y)) * texel;
            float closestDepth = texture(shadowMap, vec3(uv, float(cascade))).r;
            lit += ((currentDepth - depthBias) <= closestDepth ? 1.0 : 0.0) * weight;
            weightSum += weight;
        }
    }

    float rawVisibility = (weightSum > 0.0) ? lit / weightSum : 1.0;
    float softened = mix(1.0, rawVisibility, clamp(shadowStrength, 0.0, 1.0));
    return max(clamp(minShadowVisibility, 0.0, 1.0), softened);
}

float shadowVisibility(vec3 voxelPos, vec3 N, vec3 L)
{
    if (shadowEnabled == 0 || cascadeCount <= 0 || shadowMapSize <= 0.0) {
        return 1.0;
    }

    float voxelViewDepth = -(V * vec4(voxelPos, 1.0)).z;
    int cascade = selectCascade(voxelViewDepth);
    vec3 receiver = voxelPos + N * normalBiasVoxels * voxelSizeMeters;
    bool visibilityValid = false;
    float visibility = sampleShadowCascade(cascade, receiver, N, L, visibilityValid);

    if (!visibilityValid && cascade > 0) {
        visibility = sampleShadowCascade(cascade - 1, receiver, N, L, visibilityValid);
    }
    if (!visibilityValid && cascade < cascadeCount - 1) {
        visibility = sampleShadowCascade(cascade + 1, receiver, N, L, visibilityValid);
    }
    if (!visibilityValid) {
        return 1.0;
    }

    if (cascade < cascadeCount - 1) {
        float cascadeStart = (cascade == 0) ? 0.0 : cascadeEnds[cascade - 1];
        float cascadeEnd = cascadeEnds[cascade];
        float cascadeSpan = max(cascadeEnd - cascadeStart, 0.001);
        float blendWidth = max(2.0, cascadeSpan * 0.12);
        float blend = smoothstep(cascadeEnd - blendWidth, cascadeEnd, voxelViewDepth);
        if (blend > 0.0) {
            bool nextValid = false;
            float nextVisibility = sampleShadowCascade(cascade + 1, receiver, N, L, nextValid);
            if (nextValid) {
                visibility = mix(visibility, nextVisibility, blend);
            }
        }
    }

    return visibility;
}

void main()
{
    // VARIOUS POSITIONS
    //get normal vector
    vec3 normal = normalLookup[frag_normalID];
    // calculates the center of the voxel.
    vec3 voxelPos = (floor(worldPos/voxelSizeMeters))*voxelSizeMeters+0.5*voxelSizeMeters;
    // vec3 voxelPos = worldPos; //alternative for full range of specular.
    //get coordinate of one voxel.
    ivec3 localCoord = ivec3(((((worldPos-chunkWorldPos)/(voxelSizeMeters))-normal*0.5)-0.001));
    //snap voxel coordinate to the 2x2 grid.
    ivec3 textureCoord = ivec3((localCoord/2));
    
    // MATERIAL INFORMATION
    //get material information for 2x2x2 area.
    uint matID = texelFetch(matIDTex, textureCoord, 0).x;
    Material m = materialArray[matID];
    float random = rand(localCoord)-0.5;
    vec3 matSpecular = m.specular.rgb+random/7.5;
    vec3 matDiffuse = m.diffuse.rgb+random/17.5;
    vec3 matAmbient = m.ambient.rgb+random/17.5;
    float shininess = max(m.shininess, 0);

    // LIGHTING EQUATIONS
    // blinn phong lighting
    vec3 N = normalize(normal);
	vec3 L;
    if (dot(lightDir, lightDir) <= 0.0001) {
        L = normalize(lightPos - voxelPos);
    } else {
        L = normalize(lightDir);
    }
    vec3 Vdir = normalize(camPos - voxelPos);
	vec3 H = normalize(L + Vdir);

	float diff = max(dot(N, L), 0.0);
	float spec = 0.0;
	if (diff > 0.0) {
        spec = pow(max(dot(N, H), 0.0), shininess);
    }
        
	vec3 ambient = matAmbient;
    float directVisibility = shadowVisibility(voxelPos, N, L);
	vec3 diffuse = matDiffuse * lightColor * diff * directVisibility;
	vec3 specular = matSpecular * lightColor * spec * directVisibility;
    
    vec3 rgb = ambient + diffuse + specular;
    color = vec4(rgb, 1.0);

    // Testing Color Outputs.
    //color = vec4(vec3(max(dot(N,H),0.0)),1.0);
    //color = vec4((normal+1)/2.0, 0);
    //color = vec4(matDiffuse, 1.0);
    //color = vec4(vec3(spec), 1.0);
    //color = texelFetch(colorTex, localCoord, 0);
}
