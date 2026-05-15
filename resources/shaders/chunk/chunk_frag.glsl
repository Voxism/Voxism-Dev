#version 330 core
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
    
    vec3 rgb = ambient + diffuse + specular;
    color = vec4(rgb, 1.0);

    // Testing Color Outputs.
    //color = vec4(vec3(max(dot(N,H),0.0)),1.0);
    //color = vec4((normal+1)/2.0, 0);
    //color = vec4(matDiffuse, 1.0);
    //color = vec4(vec3(spec), 1.0);
    //color = texelFetch(colorTex, localCoord, 0);
}
