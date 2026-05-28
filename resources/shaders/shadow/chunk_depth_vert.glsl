#version 330 core

// Depth-only pass: one draw per cascade layer. lightSpaceMatrix is set per layer
// from CascadedShadowMap::lightSpaceMatrix(cascadeIndex).

layout(location = 0) in vec3 vertPos;

uniform mat4 lightSpaceMatrix;

void main()
{
    gl_Position = lightSpaceMatrix * vec4(vertPos, 1.0);
}
