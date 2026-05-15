#pragma once

#include "ToolTypes.h"

#include <memory>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "../Program.h"

class ToolPreviewRenderer {
public:
    bool init(const std::string &resourceDirectory);
    void draw(const ToolPreview &preview,
        float voxelSizeMeters,
        const glm::mat4 &P,
        const glm::mat4 &V);

private:
    std::shared_ptr<Program> prog_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
};
