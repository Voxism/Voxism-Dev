#include "ToolPreviewRenderer.h"

#include <glm/gtc/type_ptr.hpp>

bool ToolPreviewRenderer::init(const std::string &resourceDirectory)
{
    prog_ = std::make_shared<Program>();
    prog_->setVerbose(true);
    prog_->setShaderNames(
        resourceDirectory + "/shaders/scene/preview_vert.glsl",
        resourceDirectory + "/shaders/scene/preview_frag.glsl");
    if (!prog_->init()) {
        return false;
    }
    prog_->addUniform("P");
    prog_->addUniform("V");
    prog_->addUniform("previewColor");
    prog_->addAttribute("vertPos");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * 24, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

void ToolPreviewRenderer::draw(const ToolPreview &preview,
    float voxelSizeMeters,
    const glm::mat4 &P,
    const glm::mat4 &V)
{
    if (!preview.valid || !prog_ || vao_ == 0 || vbo_ == 0) {
        return;
    }

    const glm::vec3 minP = glm::vec3(preview.minVoxel) * voxelSizeMeters;
    const glm::vec3 maxP = glm::vec3(preview.maxVoxel + glm::ivec3(1)) * voxelSizeMeters;

    const glm::vec3 p000(minP.x, minP.y, minP.z);
    const glm::vec3 p100(maxP.x, minP.y, minP.z);
    const glm::vec3 p010(minP.x, maxP.y, minP.z);
    const glm::vec3 p110(maxP.x, maxP.y, minP.z);
    const glm::vec3 p001(minP.x, minP.y, maxP.z);
    const glm::vec3 p101(maxP.x, minP.y, maxP.z);
    const glm::vec3 p011(minP.x, maxP.y, maxP.z);
    const glm::vec3 p111(maxP.x, maxP.y, maxP.z);

    const glm::vec3 lines[] = {
        p000, p100, p100, p110, p110, p010, p010, p000,
        p001, p101, p101, p111, p111, p011, p011, p001,
        p000, p001, p100, p101, p110, p111, p010, p011
    };

    const glm::vec3 color = (preview.mode == ToolMode::Build) ?
        (preview.anchored ? glm::vec3(0.2f, 0.95f, 0.45f) : glm::vec3(0.85f, 0.95f, 0.25f)) :
        glm::vec3(0.95f, 0.3f, 0.25f);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(lines), lines);

    prog_->bind();
    glUniformMatrix4fv(prog_->getUniform("P"), 1, GL_FALSE, glm::value_ptr(P));
    glUniformMatrix4fv(prog_->getUniform("V"), 1, GL_FALSE, glm::value_ptr(V));
    glUniform3fv(prog_->getUniform("previewColor"), 1, glm::value_ptr(color));

    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_LINES, 0, 24);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);

    prog_->unbind();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}
