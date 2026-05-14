#version 330 core
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 vertNor;
layout(location = 3) in vec3 vertCol;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;

out vec3 fragPos;
out vec3 fragNor;
out vec3 fragCol;

void main()
{
	vec4 wp = M * vec4(vertPos, 1.0);
	fragPos = wp.xyz;
	mat3 N = mat3(transpose(inverse(M)));
	fragNor = normalize(N * vertNor);
	fragCol = vertCol;
	gl_Position = P * V * wp;
}
