#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;

void main()
{
	vec3 sky = texture(skybox, TexCoords).rgb;
	sky *= 1.15;
	FragColor = vec4(sky, 1.0);
}
