
#pragma once
#ifndef _MATERIALS_H_
#define _MATERIALS_H_

#include <glm/glm.hpp>

#include <glad/glad.h>

#include <vector>
#include "../Program.h"

struct Material{
    glm::vec4 ambient;
    glm::vec4 diffuse;
    glm::vec4 specular;
    float shininess;
    float padding[3];
};

class Materials {
    public:
        enum PaletteMaterial : uint8_t {
            Grass = 0,
            Stone = 1,
            Brick = 2,
            Sand = 3,
            Dirt = 4,
            Gold = 5,
            Leaf = 6,
            Stem = 7,
            Blossom = 8,
            DB16Black = 9,
            DB16Purple = 10,
            DB16Navy = 11,
            DB16Slate = 12,
            DB16Brown = 13,
            DB16DarkGreen = 14,
            DB16Red = 15,
            DB16Gray = 16,
            DB16Blue = 17,
            DB16Orange = 18,
            DB16Silver = 19,
            DB16Green = 20,
            DB16Peach = 21,
            DB16Cyan = 22,
            DB16Yellow = 23,
            DB16White = 24
        };
        const static int paletteCount = 25;

        Materials();

        void init(GLuint bindingPoint);

        void bind();
        size_t count() const { return materials.size(); }

        static const char *paletteName(int index);
        static glm::vec3 paletteColor(int index);

    private:
        std::vector<Material> materials;
        GLuint matBuffID = 0;
        GLuint bindingPoint = 0;
};

#endif
