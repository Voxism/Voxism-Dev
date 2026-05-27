#include "BreakParticleSystem.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "../world/Materials.h"

namespace {
constexpr float kGravity = 16.0f;
constexpr float kDragPerSecond = 3.0f;

std::string particleShaderPath(const std::string &resourceDir, const std::string &filename)
{
    return resourceDir + "/shaders/particle/" + filename;
}
}

constexpr std::size_t BreakParticleSystem::kMaxParticles;
constexpr std::size_t BreakParticleSystem::kMaxSpawnPerBurst;

BreakParticleSystem::~BreakParticleSystem()
{
    teardown();
}

bool BreakParticleSystem::init(const std::string &resourceDir)
{
    teardown();

    prog_ = std::make_shared<Program>();
    prog_->setVerbose(true);
    prog_->setShaderNames(
        particleShaderPath(resourceDir, "particle_vert.glsl"),
        particleShaderPath(resourceDir, "particle_frag.glsl"));
    if (!prog_->init()) {
        prog_.reset();
        return false;
    }

    prog_->addUniform("P");
    prog_->addUniform("V");
    prog_->addUniform("voxelSizeMeters");
    prog_->addUniform("lightDir");
    prog_->addAttribute("vertPos");
    prog_->addAttribute("vertNor");
    prog_->addAttribute("instanceCenter");
    prog_->addAttribute("instanceColor");

    ensureParticleStorage();
    initCubeMesh();
    return true;
}

void BreakParticleSystem::teardown()
{
    if (instanceBuffer_) {
        glDeleteBuffers(1, &instanceBuffer_);
        instanceBuffer_ = 0;
    }
    if (elementBuffer_) {
        glDeleteBuffers(1, &elementBuffer_);
        elementBuffer_ = 0;
    }
    if (normalBuffer_) {
        glDeleteBuffers(1, &normalBuffer_);
        normalBuffer_ = 0;
    }
    if (vertexBuffer_) {
        glDeleteBuffers(1, &vertexBuffer_);
        vertexBuffer_ = 0;
    }
    if (vao_) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
}

void BreakParticleSystem::ensureParticleStorage()
{
    particles_.assign(kMaxParticles, Particle {});
    instanceData_.resize(kMaxParticles);
    cursor_ = 0;
}

void BreakParticleSystem::initCubeMesh()
{
    static const std::array<GLfloat, 72> kCubeVerts = {{
        -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f
    }};

    static const std::array<GLfloat, 72> kCubeNormals = {{
         0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,
         0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,
        -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
         1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,
         0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
         0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f
    }};

    static const std::array<unsigned int, 36> kCubeIndices = {{
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        8, 9, 10, 8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23
    }};

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    glGenBuffers(1, &vertexBuffer_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kCubeVerts), kCubeVerts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);

    glGenBuffers(1, &normalBuffer_);
    glBindBuffer(GL_ARRAY_BUFFER, normalBuffer_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kCubeNormals), kCubeNormals.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);

    glGenBuffers(1, &elementBuffer_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kCubeIndices), kCubeIndices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &instanceBuffer_);
    glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(kMaxParticles * sizeof(InstanceData)), nullptr, GL_STREAM_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void *)offsetof(InstanceData, center));
    glVertexAttribDivisor(2, 1);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void *)offsetof(InstanceData, color));
    glVertexAttribDivisor(3, 1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void BreakParticleSystem::emitParticle(const glm::vec3 &position,
    const glm::vec3 &velocity,
    const glm::vec3 &color,
    float lifetime)
{
    Particle &particle = particles_[cursor_];
    particle.position = position;
    particle.velocity = velocity;
    particle.color = color;
    particle.age = 0.0f;
    particle.lifetime = lifetime;
    particle.active = true;
    cursor_ = (cursor_ + 1) % particles_.size();
}

glm::vec3 BreakParticleSystem::snapToVoxelCenter(const glm::vec3 &position) const
{
    const float voxelSize = 1.0f;
    return (glm::floor(position / voxelSize) + glm::vec3(0.5f)) * voxelSize;
}

void BreakParticleSystem::spawnDeleteBurst(const ChunkEditSummary &editSummary, float voxelSizeMeters)
{
    if (!editSummary.valid || editSummary.action != ChunkEditAction::Delete || editSummary.affectedVoxelCount == 0) {
        return;
    }

    const std::size_t spawnCount = std::min<std::size_t>(
        kMaxSpawnPerBurst,
        editSummary.deletedVoxels.size());
    if (spawnCount == 0) {
        return;
    }

    glm::vec3 burstCenter(0.0f);
    for (const ChunkEditSummary::DeletedVoxel &deletedVoxel : editSummary.deletedVoxels) {
        burstCenter += (glm::vec3(deletedVoxel.voxel) + glm::vec3(0.5f)) * voxelSizeMeters;
    }
    burstCenter /= static_cast<float>(editSummary.deletedVoxels.size());

    std::uniform_real_distribution<float> upwardDist(1.2f, 3.2f);
    std::uniform_real_distribution<float> sideDist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> lifetimeDist(0.45f, 0.9f);

    std::vector<std::size_t> indices(editSummary.deletedVoxels.size());
    for (std::size_t i = 0; i < indices.size(); ++i) {
        indices[i] = i;
    }
    std::shuffle(indices.begin(), indices.end(), rng_);

    for (std::size_t i = 0; i < spawnCount; ++i) {
        const ChunkEditSummary::DeletedVoxel &deletedVoxel = editSummary.deletedVoxels[indices[i]];
        const glm::vec3 center = (glm::vec3(deletedVoxel.voxel) + glm::vec3(0.5f)) * voxelSizeMeters;
        const glm::vec3 color = Materials::paletteColor(deletedVoxel.materialID);

        glm::vec3 outward = center - burstCenter;
        outward.y = std::max(outward.y, 0.0f);
        if (glm::dot(outward, outward) < 1e-5f) {
            outward = glm::vec3(sideDist(rng_), 0.35f, sideDist(rng_));
        }
        outward = glm::normalize(outward);

        glm::vec3 velocity = outward * upwardDist(rng_);
        velocity.y += upwardDist(rng_) * 0.7f;
        emitParticle(center, velocity, color, lifetimeDist(rng_));
    }
}

void BreakParticleSystem::update(float dt)
{
    if (dt <= 0.0f) {
        return;
    }

    const float drag = std::max(0.0f, 1.0f - kDragPerSecond * dt);
    for (Particle &particle : particles_) {
        if (!particle.active) {
            continue;
        }

        particle.age += dt;
        if (particle.age >= particle.lifetime) {
            particle.active = false;
            continue;
        }

        particle.velocity.y -= kGravity * dt;
        particle.velocity *= drag;
        particle.position += particle.velocity * dt;
    }
}

void BreakParticleSystem::draw(const glm::mat4 &P,
    const glm::mat4 &V,
    const glm::vec3 &lightDir,
    float voxelSizeMeters)
{
    if (!prog_ || vao_ == 0) {
        return;
    }

    std::size_t instanceCount = 0;
    for (const Particle &particle : particles_) {
        if (!particle.active) {
            continue;
        }

        instanceData_[instanceCount].center = snapToVoxelCenter(particle.position / voxelSizeMeters) * voxelSizeMeters;
        instanceData_[instanceCount].color = particle.color;
        ++instanceCount;
    }

    if (instanceCount == 0) {
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer_);
    glBufferData(GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(instanceCount * sizeof(InstanceData)),
        instanceData_.data(),
        GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);

    prog_->bind();
    glUniformMatrix4fv(prog_->getUniform("P"), 1, GL_FALSE, &P[0][0]);
    glUniformMatrix4fv(prog_->getUniform("V"), 1, GL_FALSE, &V[0][0]);
    glUniform1f(prog_->getUniform("voxelSizeMeters"), voxelSizeMeters);
    const glm::vec3 safeLightDir = (glm::dot(lightDir, lightDir) > 1e-5f) ? glm::normalize(lightDir) : glm::vec3(0.4f, 1.0f, 0.3f);
    glUniform3fv(prog_->getUniform("lightDir"), 1, &safeLightDir[0]);
    glBindVertexArray(vao_);
    glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void *)0, static_cast<GLsizei>(instanceCount));
    glBindVertexArray(0);
    prog_->unbind();

    glDepthMask(GL_TRUE);
    if (!cullWasEnabled) {
        glDisable(GL_CULL_FACE);
    }
}
