#version 330 core

// Attribute ins
layout(location = 0) in vec3 vertPos;
layout(location = 1) in uint normalID;

// Uniform ins
uniform mat4 P;
uniform mat4 V;
uniform mat4 M;

// flat un-interpolated outs
flat out uint frag_normalID; // no need to interpolate since each vertex in a face has the same normals.

// interpolated outs
out vec3 worldPos;
out float viewDepth;

void main()
{
    vec4 world = M * vec4(vertPos, 1.0);
    worldPos = world.xyz;
    viewDepth = -(V * world).z;
    gl_Position = P * V * world;
    //normal = -vertNormal; 
    frag_normalID = normalID;
}
