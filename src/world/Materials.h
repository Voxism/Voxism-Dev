
#pragma once
#ifndef _MATERIALS_H_
#define _MATERIALS_H_

#include <glm/glm.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

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
            Gold = 5
        };
        const static int paletteCount = 6;

        Materials();

        void init(GLuint bindingPoint);

        void bind();
        size_t count() const { return materials.size(); }

        static const char *paletteName(int index);

    private:
        std::vector<Material> materials;
        GLuint matBuffID = 0;
        GLuint bindingPoint = 0;
};

#endif
