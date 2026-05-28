#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D currentTex;   // jittered scene HDR (this frame)
uniform sampler2D historyTex;   // resolved output from previous frame
uniform sampler2D depthTex;     // scene hardware depth (this frame)

uniform mat4 invViewProj;       // inverse of unjittered current view-projection
uniform mat4 prevViewProj;      // unjittered view-projection from previous frame
uniform vec2 texelSize;         // 1/width, 1/height
uniform int  historyValid;      // 0 = no usable history (first frame / resize)
uniform float blendFactor;      // weight toward history (~0.9)

void main()
{
    vec3 current = texture(currentTex, TexCoords).rgb;

    float depth = texture(depthTex, TexCoords).r;
    // Sky / cleared pixels: nothing to reproject, keep current to protect god-ray mask.
    if (historyValid == 0 || depth >= 1.0) {
        FragColor = vec4(current, 1.0);
        return;
    }

    // Reconstruct world position from depth using the unjittered current VP.
    vec3 ndc = vec3(TexCoords, depth) * 2.0 - 1.0;
    vec4 world = invViewProj * vec4(ndc, 1.0);
    world /= world.w;

    // Reproject into the previous frame.
    vec4 prevClip = prevViewProj * vec4(world.xyz, 1.0);
    vec2 prevUV = (prevClip.xy / prevClip.w) * 0.5 + 0.5;

    if (prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0) {
        FragColor = vec4(current, 1.0);
        return;
    }

    // Neighborhood color AABB (3x3) for history clamping.
    vec3 cmin = current;
    vec3 cmax = current;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            if (x == 0 && y == 0) continue;
            vec3 c = texture(currentTex, TexCoords + vec2(x, y) * texelSize).rgb;
            cmin = min(cmin, c);
            cmax = max(cmax, c);
        }
    }

    vec3 history = texture(historyTex, prevUV).rgb;
    history = clamp(history, cmin, cmax);

    FragColor = vec4(mix(current, history, blendFactor), 1.0);
}
