#version 330 core

layout(location = 0) in vec3 vertPos;

uniform mat4 lightSpaceMatrix;

void main()
{
    gl_Position = lightSpaceMatrix * vec4(vertPos, 1.0);
}
