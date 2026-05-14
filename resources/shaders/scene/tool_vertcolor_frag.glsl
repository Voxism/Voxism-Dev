#version 330 core
in vec3 fragPos;
in vec3 fragNor;
in vec3 fragCol;

uniform vec3 lightPos;
uniform vec3 camPos;
uniform vec3 lightColor;
uniform vec3 matAmbient;
uniform vec3 matDiffuse;
uniform vec3 matSpecular;
uniform float shininess;
uniform vec3 tintColor;

out vec4 color;

void main()
{
	vec3 N = normalize(fragNor);
	vec3 L = normalize(lightPos - fragPos);
	vec3 Vdir = normalize(camPos - fragPos);
	vec3 H = normalize(L + Vdir);

	vec3 albedo = fragCol * tintColor;

	float diff = max(dot(N, L), 0.0);
	float spec = 0.0;
	if (diff > 0.0) {
		spec = pow(max(dot(N, H), 0.0), shininess);
	}

	vec3 ambient = matAmbient * albedo * lightColor;
	vec3 diffuse = matDiffuse * albedo * lightColor * diff;
	vec3 specular = matSpecular * lightColor * spec;

	color = vec4(ambient + diffuse + specular, 1.0);
}
