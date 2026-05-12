#version 330 core

uniform vec3 previewColor;

out vec4 color;

void main()
{
    color = vec4(previewColor, 0.9);
}
