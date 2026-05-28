#version 330 core
#define PI 3.1415926538
#define MIN_FLOAT_VALUE 0.00001
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
    vec4 diffuse;
    float roughness;
    float metallic;
    int diffRandomFactor;
    int roughRandomFactor;
    int metallicRandomFactor;
    int pattern;
};

struct PatternNoise {
    vec3 diffuseNoise;
    float roughnessNoise;
    float metallicNoise;
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

float computeShadowDepthBias(vec3 N, vec3 L, float texelSize)
{
    float ndotl = max(dot(N, L), 0.0001);
    float slope = sqrt(clamp(1.0 - ndotl * ndotl, 0.0, 1.0)) / ndotl;

    // Base bias in light-space depth; slope term reduces acne on grazing faces.
    float bias = 0.0003 + 0.002 * slope;

    // Wider PCF kernels sample across depth discontinuities — scale bias with filter radius.
    bias += texelSize * 2.5;

    // Voxel-sized guard band so block faces don't self-shadow in the penumbra.
    bias += normalBiasVoxels * voxelSizeMeters * 0.004;

    return bias;
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
    float texel = shadowBlurTexels / shadowMapSize;
    float depthBias = computeShadowDepthBias(N, L, texel);
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

PatternNoise getGradientPattern(vec3 localCoord, float random){
    PatternNoise pn;

    float frequency = 0.2;
    float scale = 0.10a;
    float randomScale = 1000;
    float noise = sin(localCoord.x*frequency + random*frequency*randomScale) * sin(localCoord.y*frequency + random*frequency*randomScale) * sin(localCoord.z*frequency + random*frequency*randomScale) * scale;
    pn.diffuseNoise = vec3(noise);
    pn.roughnessNoise = 0.0;
    pn.metallicNoise = 0.0;
    return pn;
}

// GGX Trowbridge-Reitz (Cook-Torrance) model
float NormalDistribution(vec3 normal, vec3 halfWayVector, float roughnessFactor)
{
    float alpha = roughnessFactor * roughnessFactor;
    float alphaSquare = alpha * alpha;
    float nDotH = clamp(dot(normal, halfWayVector), 0.0, 1.0);
    return alphaSquare / (max(PI * pow((nDotH * nDotH * (alphaSquare - 1.0) + 1.0), 2.0), MIN_FLOAT_VALUE));
}

vec3 fresnelShclick(vec3 matDiffuse, float matMetallicFactor, float vDotH)
{
    vec3 baseReflectivity = mix(vec3(0.04, 0.04, 0.04), matDiffuse, matMetallicFactor);
    return baseReflectivity + (1.0 - baseReflectivity) * pow(clamp(1.0 - vDotH, 0.0, 1.0), 5.0);
}

float SchlickBeckmannGS(vec3 normal, vec3 x, float roughnessFactor)
{
    float k = roughnessFactor / 2.0;
    float nDotX = clamp(dot(normal, x), 0.0, 1.0);
    return nDotX / (max((nDotX * (1.0 - k) + k), MIN_FLOAT_VALUE));
}

float GeometryShadowing(vec3 normal, vec3 viewDir, vec3 lightDir, float roughnessFactor)
{
    return SchlickBeckmannGS(normal, viewDir, roughnessFactor) * SchlickBeckmannGS(normal, lightDir, roughnessFactor);
}

vec3 cookTorranceBRDF(vec3 matDiffuse, float matRoughnessFactor, float matMetallicFactor,
                      vec3 normal, vec3 viewDir, vec3 dirToLight, vec3 lightColor,
                      float directVisibility)
{
    vec3 halfway = normalize(viewDir + dirToLight);
    float vDotH = max(dot(viewDir, halfway), 0.0);

    float NDF = NormalDistribution(normal, halfway, matRoughnessFactor);
    vec3 F = fresnelShclick(matDiffuse, matMetallicFactor, vDotH);
    float G = GeometryShadowing(normal, viewDir, dirToLight, matRoughnessFactor);
    float wo = clamp(dot(viewDir, normal), 0.001, 1.0);
    float wi = clamp(dot(dirToLight, normal), 0.001, 1.0);

    vec3 specular = (NDF * F * G) / max(4.0 * wi * wo, MIN_FLOAT_VALUE);
    vec3 ks = F;
    vec3 kd = mix(vec3(1.0) - F, vec3(0.0), matMetallicFactor);
    vec3 diffuse = matDiffuse / PI;
    vec3 ambient = mix(matDiffuse * vec3(0.15), matDiffuse * 0.12, matMetallicFactor);

    vec3 direct = (kd * diffuse + ks * specular) * lightColor * max(0.0, dot(normal, dirToLight));
    return ambient + direct * directVisibility;
}

void main()
{
    vec3 normal = normalLookup[frag_normalID];
    vec3 voxelPos = (floor(worldPos / voxelSizeMeters)) * voxelSizeMeters + 0.5 * voxelSizeMeters;
    ivec3 localCoord = ivec3(((((worldPos - chunkWorldPos) / (voxelSizeMeters)) - normal * 0.5) - 0.001));
    ivec3 textureCoord = ivec3((localCoord / 2));

    uint matID = texelFetch(matIDTex, textureCoord, 0).x;
    Material m = materialArray[matID];
    float random1 = (rand(localCoord)-0.5)/100.0; // range is [-0.005, 0.005]
    // float random2 = (rand(vec3(localCoord.z, localCoord.x, localCoord.y))-0.5)/100.0; // range is [-0.005, 0.005]
    float random2 = (rand(localCoord))/200.0; // range is [0.000, 0.005]


    // Get material properties.
    // int diffRandomFactor = 10;
    // int roughRandomFactor = 50;
    // int metallicRandomFactor = 5;
    
    
    
    
    // modify material Properties.
    PatternNoise patNoise;
    
    switch (m.pattern)
    {
        // case 1: //brick pattern
        // {patNoise = getBrickPattern();}

        case 2: // gradient pattern
        {
            patNoise = getGradientPattern(voxelPos, random1);
            break;
        }

        default: //No noise/pattern
        {
            patNoise.diffuseNoise = vec3(0,0,0);
            patNoise.roughnessNoise = 0.0;
            patNoise.metallicNoise = 0.0;
            break;
        }
    }
    // base + pattern + [-0.005, 0.005]*factor
    vec3 matDiffuse = clamp(m.diffuse.rgb + patNoise.diffuseNoise + random1*m.diffRandomFactor, 0.0, 1.0);
    // vec3 matDiffuse = clamp(m.diffuse.rgb + patNoise.diffuseNoise, 0.0, 1.0);
    // base + pattern - [0.000, 0.005]*factor : so roughRandomFactor makes it more shiny.
    float matRoughnessFactor = clamp(m.roughness + patNoise.roughnessNoise - random2*m.roughRandomFactor, 0.0, 1.0);
    // base + pattern + [0.000, 0.005]*factor : so metallicRandomFactor makes it more metalic.
    float matMetallicFactor = clamp(m.metallic + patNoise.metallicNoise + random2*m.metallicRandomFactor, 0.0, 1.0);

    // Lighting Calculation
    vec3 N = normalize(normal);
    vec3 dirToLight;
    if (dot(lightDir, lightDir) <= 0.0001) {
        dirToLight = normalize(lightPos - voxelPos);
    } else {
        dirToLight = normalize(lightDir);
    }
    vec3 viewDir = normalize(camPos - voxelPos);
    float directVisibility = shadowVisibility(voxelPos, N, dirToLight);
    vec3 rgb = cookTorranceBRDF(matDiffuse, matRoughnessFactor, matMetallicFactor,
                                N, viewDir, dirToLight, lightColor, directVisibility);

    color = vec4(rgb, 1.0);
}
