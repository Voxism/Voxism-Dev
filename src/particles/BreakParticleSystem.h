#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "../Program.h"
#include "../world/ChunkEdit.h"

class BreakParticleSystem {
public:
    ~BreakParticleSystem();

    bool init(const std::string &resourceDir);
    void spawnDeleteBurst(const ChunkEditSummary &editSummary, float voxelSizeMeters);
    void update(float dt);
    void draw(const glm::mat4 &P, const glm::mat4 &V, const glm::vec3 &lightDir, float voxelSizeMeters);

private:
    struct Particle {
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 velocity = glm::vec3(0.0f);
        glm::vec3 color = glm::vec3(1.0f);
        float age = 0.0f;
        float lifetime = 0.0f;
        bool active = false;
    };

    struct InstanceData {
        glm::vec3 center = glm::vec3(0.0f);
        glm::vec3 color = glm::vec3(1.0f);
    };

    void teardown();
    void initCubeMesh();
    void ensureParticleStorage();
    void emitParticle(const glm::vec3 &position, const glm::vec3 &velocity, const glm::vec3 &color, float lifetime);
    glm::vec3 snapToVoxelCenter(const glm::vec3 &position) const;

    std::shared_ptr<Program> prog_;
    GLuint vao_ = 0;
    GLuint vertexBuffer_ = 0;
    GLuint normalBuffer_ = 0;
    GLuint elementBuffer_ = 0;
    GLuint instanceBuffer_ = 0;

    std::vector<Particle> particles_;
    std::vector<InstanceData> instanceData_;
    std::size_t cursor_ = 0;
    std::mt19937 rng_{std::random_device{}()};

    static constexpr std::size_t kMaxParticles = 512;
    static constexpr std::size_t kMaxSpawnPerBurst = 96;
};
