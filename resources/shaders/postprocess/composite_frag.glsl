#version 330 core

// Final composite: scene + screen-space god rays + bloom.
// Applies Reinhard tone mapping, gamma correction, and a subtle vignette.

in  vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D sceneTex;
uniform sampler2D godrayTex;
uniform sampler2D bloomTex;
uniform float godrayStrength;
uniform float bloomStrength;

uniform sampler2D ssaoTex;
uniform float     ssaoIntensity;
uniform bool      ssaoEnabled;

// Reinhard per-channel tone map
vec3 toneMap(vec3 hdr)
{
    return hdr / (hdr + vec3(1.0));
}

void main()
{
    vec3 scene = texture(sceneTex,  TexCoords).rgb;

    // Apply SSAO with a contrast curve: pow(ao, k) leaves exposed pixels (ao≈1) at 1.0
    // but deepens real creases (ao<1) much more aggressively, so corners read as dark
    // without re-darkening already-lit flat faces.
    if (ssaoEnabled) {
        float ao = texture(ssaoTex, TexCoords).r;
        ao = pow(ao, 2.2);
        // floor so deep creases keep a little ambient bounce light rather than going black.
        ao = mix(0.18, 1.0, ao);
        scene *= mix(1.0, ao, clamp(ssaoIntensity, 0.0, 1.0));
    }

    vec3 rays  = texture(godrayTex, TexCoords).rgb;
    vec3 bloom = texture(bloomTex,  TexCoords).rgb;

    vec3 hdr = scene + rays * godrayStrength + bloom * bloomStrength;

    // Tone map + gamma
    vec3 ldr = toneMap(hdr);
    ldr      = pow(max(ldr, vec3(0.0)), vec3(1.0 / 2.2));

    // ---- Golden-hour colour grade ----
    // Warm the shadows toward amber, keep highlights golden-white
    float lum    = dot(ldr, vec3(0.2126, 0.7152, 0.0722));
    // shadow tint: push dark areas toward warm orange
    vec3  shadow = vec3(1.10, 0.88, 0.72);
    // highlight tint: keep brights a clean warm white
    vec3  hi     = vec3(1.05, 0.98, 0.88);
    ldr *= mix(shadow, hi, clamp(lum * 1.4, 0.0, 1.0));

    // Rein in any clipping after the grade
    ldr = clamp(ldr, 0.0, 1.0);

    // Subtle radial vignette
    vec2  uv  = TexCoords - 0.5;
    float vig = 1.0 - dot(uv, uv) * 1.4;
    ldr *= clamp(vig, 0.0, 1.0);

    FragColor = vec4(ldr, 1.0);
}
