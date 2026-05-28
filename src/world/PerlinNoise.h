#pragma once

#ifndef _PERLINNOISE_H_
#define _PERLINNOISE_H_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>
// #include "TerrainGenerator.h"
#include "Octave.h"
#include <glm/glm.hpp>

class PerlinNoise {
public:
    explicit PerlinNoise(uint32_t seed = 1337u) : perm_(512, 0) {
        std::vector<int> p(256); //vector p of 256 values.
        std::iota(p.begin(), p.end(), 0); // fill p with 0-255.
        std::mt19937 rng(seed); // create RNG.
        std::shuffle(p.begin(), p.end(), rng); //randomly swaps p.
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

    // Fractal Brownian motion, normalized to approximately [-sumOctaves.amplitude, +sumOctaves.amplitude].
    float fbm2D(float x, float z, const std::vector<Octave> octaves) const {
        float value = 0.0f;

        for (int i = 0; i < octaves.size(); ++i) {
            Octave currentOctave = octaves[i];
            float octaveValue = noise2D(x * currentOctave.frequency, z * currentOctave.frequency) * currentOctave.amplitude;
            if (octaveValue < 0){
                octaveValue = octaveValue / (1.0f + (-octaveValue * currentOctave.floorFactor));
            }
            value += octaveValue;
        }
        return value;
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

#endif