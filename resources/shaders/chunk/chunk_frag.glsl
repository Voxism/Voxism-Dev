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

layout(std140) uniform materials {
    Material materialArray[256];
};

vec3 normalLookup[6] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(-1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, -1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(0.0, 0.0, -1.0)
);

out vec4 color;

float rand(float n) {
    return fract(sin(n) * 43758.5453123);
}
float rand(vec3 p) {
    return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453123);
}

void main()
{
    vec3 normal = normalLookup[frag_normalID];

    // m / m per v
    ivec3 localCoord = ivec3(((((worldPos-chunkWorldPos)/(voxelSizeMeters))-normal*0.5)));
    ivec3 textureCoord = ivec3(((((worldPos-chunkWorldPos)/(voxelSizeMeters*2+0.001))-normal*0.5)-0.5*voxelSizeMeters));
    uint matID = texelFetch(matIDTex, textureCoord, 0).x;
    
    
    // hard coded until ID lookup can be used.
    //vec3 matSpecular = vec3(0.15);
    //vec3 matDiffuse = voxColor.rgb*1.0;
    //vec3 matAmbient = voxColor.rgb*0.08;
    //float shininess = 30;

    Material m = materialArray[matID];
    vec3 matSpecular = m.specular.rgb + (rand(localCoord+1)-0.5)/6;
    vec3 matDiffuse = m.diffuse.rgb + (rand(localCoord-1)-0.5)/10.0;
    vec3 matAmbient = m.ambient.rgb + (rand(localCoord)-0.5)/20.0;
    float shininess = m.shininess + (rand(localCoord+2)-0.5)*10;

    // Lighting Equations
    vec3 N = normalize(normal);
    //used to calculate the blinn-phong color per voxel.
    //vec3 voxelPos = worldPos;
    vec3 voxelPos = (floor(worldPos/voxelSizeMeters))*voxelSizeMeters+0.5*voxelSizeMeters;
    //vec3 voxelPos = (floor(worldPos/(voxelSizeMeters*2)))*(voxelSizeMeters*2)+0.5*(voxelSizeMeters*2); //lighting 2x voxel size.
    
	vec3 L = normalize(lightPos- voxelPos);
	vec3 Vdir = normalize(camPos - voxelPos);
    //used to calculate the blinn-phong color normally
    //vec3 L = normalize(lightPos- worldPos);
	//vec3 Vdir = normalize(camPos - worldPos);

	vec3 H = normalize(L + Vdir);

	float diff = max(dot(N, L), 0.0);
	float spec = 0.0;
	if (diff > 0.0) {
        spec = pow(max(dot(N, H), 0.0), shininess);
        //spec = smoothstep(0.2, 1.0, spec);
    }
        
	vec3 ambient = matAmbient;
	vec3 diffuse = matDiffuse * normalize(lightColor) * diff;
	vec3 specular = matSpecular * normalize(lightColor) * spec;
    
    vec3 rgb = ambient + diffuse + specular;
    color = vec4(rgb, 1.0);
    //color = vec4(matDiffuse, 1.0);
    
    //color = vec4(vec3(spec), 1.0);
    //color = texelFetch(colorTex, localCoord, 0);
}
