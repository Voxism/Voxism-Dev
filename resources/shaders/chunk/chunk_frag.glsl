#version 330 core
#define PI 3.1415926538
#define MIN_FLOAT_VALUE 0.00001
// Textures
uniform usampler3D matIDTex;

// General Chunk Data
uniform vec3 chunkWorldPos;
//uniform float chunkSizeMeters;
uniform float voxelSizeMeters;

// Lighting
uniform vec3 lightPos; //added
uniform vec3 camPos; // added
uniform vec3 lightColor; //added

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
    // vec4 ambient;
    // vec4 diffuse;
    // vec4 specular;
    // float shininess;
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

// GGX TrowBridge Reitx model
// Approximates microfaucets
float NormalDistribution(vec3 normal, vec3 halfWayVector, float roughnessFactor)
{
    // roughnessFactor = max(roughnessFactor, 0.35);
    float alpha = roughnessFactor * roughnessFactor;
    float alphaSquare = alpha * alpha;
    float nDotH = clamp(dot(normal, halfWayVector), 0.0, 1.0);
    return alphaSquare / (max(PI * pow((nDotH * nDotH * (alphaSquare - 1.0) + 1.0), 2.0), MIN_FLOAT_VALUE));
}

// Determines how much light is reflected or refracted based on viewing angle.
// Think of how the surface of water reflects best at certain angles.
vec3 fresnelShclick(vec3 matDiffuse, float matMetallicFactor, float vDotH)
{
    vec3 baseReflectivity = mix(vec3(0.04, 0.04, 0.04), matDiffuse, matMetallicFactor);
    return baseReflectivity + (1.0 - baseReflectivity) * pow(clamp(1.0 - vDotH, 0.0, 1.0), 5.0);
}

// Calculates how much surface area is visible or not obstructed by itself.
// if x is light = calculates self shadowing
// if x is view = calculates surface obstruction.
float SchlickBeckmannGS(vec3 normal, vec3 x, float roughnessFactor)
{
    float k = roughnessFactor / 2.0;
    // float r = roughnessFactor + 1.0;
    // float k = (r * r) / 8.0;
    float nDotX = clamp(dot(normal, x), 0.0, 1.0);
    return nDotX / (max((nDotX * (1.0 - k) + k), MIN_FLOAT_VALUE));
}

float GeometryShadowing(vec3 normal, vec3 viewDir, vec3 lightDir, float roughnessFactor)
{
    return SchlickBeckmannGS(normal, viewDir, roughnessFactor) * SchlickBeckmannGS(normal, lightDir, roughnessFactor);    
}

// https://rtarun9.github.io/blogs/physically_based_rendering/ reference for equations and explanations.
// matDiffuse vec3: Color of the object
// matRoughnessFactor [0, 1]: 0=perfectly reflective, 1=rough and bumpy.
// matMetallicFactor [0, 1]: 0=not metalic, 1=is metal.
vec3 cookTorranceBRDF(vec3 matDiffuse, float matRoughnessFactor, float matMetallicFactor, 
                      vec3 normal, vec3 viewDir, vec3 dirToLight, vec3 lightColor)
{
    vec3 halfway = normalize(viewDir+dirToLight);
    float vDotH = max(dot(viewDir, halfway), 0);

    // NDF = Normal Distribution Function.
    float NDF = NormalDistribution(normal, halfway, matRoughnessFactor);
    // F   = Fresnel equation.
    vec3 F = fresnelShclick(matDiffuse, matMetallicFactor, vDotH);
    // G   = Geometry shadowing / Masking function.
    float G = GeometryShadowing(normal, viewDir, dirToLight, matRoughnessFactor);
    // wi  = Incoming light direction.
    float wo = clamp(dot(viewDir, normal), 0.001, 1.0);
    // wo  = View direction.
    float wi = clamp(dot(dirToLight, normal), 0.001, 1.0);


    vec3 specular = ((NDF) * F * G)/ max(4*wi*wo, MIN_FLOAT_VALUE);
    vec3 ks = F;
    vec3 kd = mix(vec3(1.0, 1.0, 1.0) - F, vec3(0.0, 0.0, 0.0), matMetallicFactor);
    // lambertianDiffuse
    vec3 diffuse = matDiffuse / PI;

    // vec3 ambient = matDiffuse * vec3(0.30, 0.32, 0.35);
    vec3 ambient = mix(matDiffuse * vec3(0.15, 0.15, 0.15), matDiffuse * 0.12, matMetallicFactor);

    return ambient + (kd*diffuse + ks*specular)*lightColor*max(0.0,dot(normal, dirToLight));
}

vec3 blinnPhong(vec3 matAmbient, vec3 matDiffuse, vec3 matSpecular, float shininess,
                vec3 normal, vec3 lightPos, vec3 voxelPos, vec3 camPos){

    vec3 N = normalize(normal);
	vec3 L = normalize(lightPos- voxelPos);
    vec3 Vdir = normalize(camPos - voxelPos);
	vec3 H = normalize(L + Vdir);

    float diff = max(dot(N, L), 0.0);
	float spec = 0.0;
	if (diff > 0.0) {
        spec = pow(max(dot(N, H), 0.0), shininess);
    }
    
    vec3 ambient = matAmbient;
    vec3 diffuse = matDiffuse * lightColor * diff;
    vec3 specular = matSpecular * lightColor * spec;

    return ambient + diffuse + specular;
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
    vec3 matDiffuse = clamp(m.diffuse.rgb+random/17.5, 0.0, 1.0);
    float matRoughnessFactor = m.roughness;
    float matMetallicFactor = m.metallic;
    
    // vec3 matSpecular = m.specular.rgb+random/7.5;
    // vec3 matAmbient = m.ambient.rgb+random/17.5;
    // float shininess = max(m.shininess, 0);


    
    
    // vec3 dirToLight = normalize(lightPos-worldPos);
    vec3 dirToLight = normalize(lightPos-voxelPos);
    // vec3 viewDir = normalize(camPos - worldPos);
    vec3 viewDir = normalize(camPos - voxelPos);
    vec3 rgb = cookTorranceBRDF(matDiffuse, matRoughnessFactor, matMetallicFactor, normal, viewDir, dirToLight, lightColor);
    // vec3 rgb = blinnPhong(matAmbient, matDiffuse, matSpecular, shininess, normal, lightPos, voxelPos, camPos);

    // Gamma correction
    // rgb = rgb / (rgb + vec3(1.0));
    // rgb = pow(rgb, vec3(1.0 / 2.0));
    color = vec4(rgb, 1.0);

    // Testing Color Outputs.
}
