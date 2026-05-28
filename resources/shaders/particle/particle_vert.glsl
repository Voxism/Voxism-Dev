#version 330 core

layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 vertNor;
layout(location = 2) in vec3 instanceCenter;
layout(location = 3) in vec3 instanceColor;

uniform mat4 P;
uniform mat4 V;
uniform float voxelSizeMeters;

out vec3 fragNormal;
out vec3 fragColor;

void main()
{
	vec3 worldPos = instanceCenter + vertPos * voxelSizeMeters;
	gl_Position = P * V * vec4(worldPos, 1.0);
	fragNormal = vertNor;
	fragColor = instanceColor;
}
