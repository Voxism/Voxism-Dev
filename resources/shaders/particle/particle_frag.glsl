#version 330 core

in vec3 fragNormal;
in vec3 fragColor;

uniform vec3 lightDir;

out vec4 FragColor;

void main()
{
	vec3 N = normalize(fragNormal);
	float diffuse = max(dot(N, normalize(lightDir)), 0.0);
	vec3 litColor = fragColor * (0.35 + 0.65 * diffuse);
	FragColor = vec4(litColor, 1.0);
}
