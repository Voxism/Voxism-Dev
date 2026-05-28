#version 330 core

in vec2 TexCoords;
out float FragColor;

// the raw noisy ssao output from ssao_frag
uniform sampler2D ssaoInput;
// scene depth, used to reject neighbors that lie on a different surface
uniform sampler2D depthTex;
// size of one pixel in uv space (1/screenWidth, 1/screenHeight)
uniform vec2 texelSize;
// inverse of projection — linearize depth so the threshold is in view units
uniform mat4 invProjection;

// convert a [0,1] depth sample into linear view-space z (negative-valued).
float linearizeDepth(float d, vec2 uv) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    vec4 view = invProjection * ndc;
    return view.z / view.w;
}

void main() {
    float centerD = linearizeDepth(texture(depthTex, TexCoords).r, TexCoords);

    // depth-aware (bilateral) box blur. a plain box blur smears dark AO from deep
    // crevices onto adjacent lit faces — exactly the "exposed surface looks dark"
    // artifact. by rejecting neighbors whose view-space depth differs by more than a
    // small threshold, the blur stays inside the same surface and respects edges.
    //
    // threshold scales with distance so the tolerance grows with perspective.
    float threshold = 0.05 * abs(centerD) + 0.01;

    float sum    = 0.0;
    float weight = 0.0;

    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec2 off = vec2(float(x), float(y)) * texelSize;
            vec2 uv  = TexCoords + off;
            float nD = linearizeDepth(texture(depthTex, uv).r, uv);
            float w  = step(abs(nD - centerD), threshold);
            sum    += texture(ssaoInput, uv).r * w;
            weight += w;
        }
    }

    FragColor = (weight > 0.0) ? (sum / weight) : texture(ssaoInput, TexCoords).r;
}
