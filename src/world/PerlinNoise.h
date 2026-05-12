#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

class PerlinNoise {
public:
    explicit PerlinNoise(uint32_t seed = 1337u) : perm_(512, 0) {
        std::vector<int> p(256);
        std::iota(p.begin(), p.end(), 0);
        std::mt19937 rng(seed);
        std::shuffle(p.begin(), p.end(), rng);
        for (int i = 0; i < 512; ++i) {
            perm_[i] = p[i & 255];
        }
    }

    // 2D improved Perlin noise in [-1, 1].
    float noise2D(float x, float z) const {
        const int xi = static_cast<int>(std::floor(x)) & 255;
        const int zi = static_cast<int>(std::floor(z)) & 255;

        const float xf = x - std::floor(x);
        const float zf = z - std::floor(z);

        const float u = fade(xf);
        const float v = fade(zf);

        const int aa = perm_[perm_[xi] + zi];
        const int ab = perm_[perm_[xi] + zi + 1];
        const int ba = perm_[perm_[xi + 1] + zi];
        const int bb = perm_[perm_[xi + 1] + zi + 1];

        const float x1 = lerp(grad(aa, xf, zf), grad(ba, xf - 1.0f, zf), u);
        const float x2 = lerp(grad(ab, xf, zf - 1.0f), grad(bb, xf - 1.0f, zf - 1.0f), u);

        return lerp(x1, x2, v);
    }

    // Fractal Brownian motion, normalized to approximately [-1, 1].
    float fbm2D(float x, float z, int octaves, float lacunarity, float persistence) const {
        float value = 0.0f;
        float amplitude = 10.0f;
        float frequency = 2.0f;
        float norm = 0.0f;

        for (int i = 0; i < octaves; ++i) {
            value += noise2D(x * frequency, z * frequency) * amplitude;
            norm += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }

        if (norm <= 0.0f) {
            return 0.0f;
        }
        return value / norm;
    }

private:
    static float fade(float t) {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    static float lerp(float a, float b, float t) {
        return a + t * (b - a);
    }

    static float grad(int hash, float x, float z) {
        switch (hash & 0x3) {
            case 0: return x + z;
            case 1: return -x + z;
            case 2: return x - z;
            default: return -x - z;
        }
    }

    std::vector<int> perm_;
};
