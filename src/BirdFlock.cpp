#include "BirdFlock.h"

#include "Program.h"
#include "Shape.h"
#include "world/MagicaVoxelLoader.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>

namespace {

struct RgbColor {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
};

uint32_t voxelKey(const glm::ivec3 &v)
{
    const uint32_t x = static_cast<uint32_t>(v.x + 1024);
    const uint32_t y = static_cast<uint32_t>(v.y + 1024);
    const uint32_t z = static_cast<uint32_t>(v.z + 1024);
    return (x << 20) ^ (y << 10) ^ z;
}

RgbColor rgbaToRgb(uint32_t rgba)
{
    return RgbColor {
        static_cast<float>(rgba & 0xffu) / 255.0f,
        static_cast<float>((rgba >> 8) & 0xffu) / 255.0f,
        static_cast<float>((rgba >> 16) & 0xffu) / 255.0f
    };
}

glm::ivec3 toEngineVoxel(const MagicaVoxelCell &cell, int anchorX, int anchorZ, int minY)
{
    return glm::ivec3(
        static_cast<int>(cell.x) - anchorX,
        static_cast<int>(cell.z) - minY,
        static_cast<int>(cell.y) - anchorZ);
}

void appendFace(std::vector<float> &positions,
    std::vector<float> &normals,
    std::vector<float> &colors,
    std::vector<unsigned int> &indices,
    const std::array<glm::vec3, 4> &verts,
    const glm::vec3 &normal,
    const RgbColor &color)
{
    const unsigned int base = static_cast<unsigned int>(positions.size() / 3);
    for (const glm::vec3 &v : verts) {
        positions.push_back(v.x);
        positions.push_back(v.y);
        positions.push_back(v.z);
        normals.push_back(normal.x);
        normals.push_back(normal.y);
        normals.push_back(normal.z);
        colors.push_back(color.r);
        colors.push_back(color.g);
        colors.push_back(color.b);
    }

    indices.push_back(base + 0);
    indices.push_back(base + 2);
    indices.push_back(base + 1);
    indices.push_back(base + 0);
    indices.push_back(base + 3);
    indices.push_back(base + 2);
}

std::shared_ptr<Shape> buildShapeFromVoxelModel(const MagicaVoxelModel &model)
{
    if (model.cells.empty()) {
        return nullptr;
    }

    int minY = std::numeric_limits<int>::max();
    for (const auto &cell : model.cells) {
        minY = std::min(minY, static_cast<int>(cell.z));
    }

    const int anchorX = model.size.x / 2;
    const int anchorZ = model.size.y / 2;

    std::unordered_map<uint32_t, RgbColor> filled;
    filled.reserve(model.cells.size() * 2);
    for (const auto &cell : model.cells) {
        const glm::ivec3 pos = toEngineVoxel(cell, anchorX, anchorZ, minY);
        RgbColor color;
        if (model.hasPalette && cell.colorIndex > 0) {
            color = rgbaToRgb(model.palette[static_cast<std::size_t>(cell.colorIndex - 1)]);
        }
        filled[voxelKey(pos)] = color;
    }

    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> colors;
    std::vector<unsigned int> indices;
    positions.reserve(model.cells.size() * 6 * 4 * 3);
    normals.reserve(model.cells.size() * 6 * 4 * 3);
    colors.reserve(model.cells.size() * 6 * 4 * 3);
    indices.reserve(model.cells.size() * 6 * 6);

    const glm::vec3 faceNormals[] = {
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(-1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, -1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, -1.0f)
    };
    const glm::ivec3 offsets[] = {
        glm::ivec3(1, 0, 0),
        glm::ivec3(-1, 0, 0),
        glm::ivec3(0, 1, 0),
        glm::ivec3(0, -1, 0),
        glm::ivec3(0, 0, 1),
        glm::ivec3(0, 0, -1)
    };

    for (const auto &cell : model.cells) {
        const glm::ivec3 v = toEngineVoxel(cell, anchorX, anchorZ, minY);
        const RgbColor color = filled[voxelKey(v)];
        const float x0 = static_cast<float>(v.x);
        const float y0 = static_cast<float>(v.y);
        const float z0 = static_cast<float>(v.z);
        const float x1 = x0 + 1.0f;
        const float y1 = y0 + 1.0f;
        const float z1 = z0 + 1.0f;

        for (int face = 0; face < 6; ++face) {
            if (filled.find(voxelKey(v + offsets[face])) != filled.end()) {
                continue;
            }

            switch (face) {
            case 0:
                appendFace(positions, normals, colors, indices,
                    {glm::vec3(x1, y0, z0), glm::vec3(x1, y0, z1), glm::vec3(x1, y1, z1), glm::vec3(x1, y1, z0)},
                    faceNormals[face],
                    color);
                break;
            case 1:
                appendFace(positions, normals, colors, indices,
                    {glm::vec3(x0, y0, z1), glm::vec3(x0, y0, z0), glm::vec3(x0, y1, z0), glm::vec3(x0, y1, z1)},
                    faceNormals[face],
                    color);
                break;
            case 2:
                appendFace(positions, normals, colors, indices,
                    {glm::vec3(x0, y1, z0), glm::vec3(x1, y1, z0), glm::vec3(x1, y1, z1), glm::vec3(x0, y1, z1)},
                    faceNormals[face],
                    color);
                break;
            case 3:
                appendFace(positions, normals, colors, indices,
                    {glm::vec3(x0, y0, z1), glm::vec3(x1, y0, z1), glm::vec3(x1, y0, z0), glm::vec3(x0, y0, z0)},
                    faceNormals[face],
                    color);
                break;
            case 4:
                appendFace(positions, normals, colors, indices,
                    {glm::vec3(x1, y0, z1), glm::vec3(x0, y0, z1), glm::vec3(x0, y1, z1), glm::vec3(x1, y1, z1)},
                    faceNormals[face],
                    color);
                break;
            case 5:
                appendFace(positions, normals, colors, indices,
                    {glm::vec3(x0, y0, z0), glm::vec3(x1, y0, z0), glm::vec3(x1, y1, z0), glm::vec3(x0, y1, z0)},
                    faceNormals[face],
                    color);
                break;
            default:
                break;
            }
        }
    }

    auto shape = std::make_shared<Shape>();
    shape->setMeshData(std::move(positions), std::move(normals), std::move(indices), std::move(colors));
    shape->measure();
    shape->init();
    return shape;
}

} // namespace

bool BirdFlock::init(const std::string &resourceDirectory)
{
    variants_.clear();
    birds_.clear();

    const std::array<std::array<std::string, 3>, 2> variantPaths = {{
        {
            resourceDirectory + "/fauna/bird_a_frame_1.vox",
            resourceDirectory + "/fauna/bird_a_frame_2.vox",
            resourceDirectory + "/fauna/bird_a_frame_3.vox"
        },
        {
            resourceDirectory + "/fauna/bird_b_frame_1.vox",
            resourceDirectory + "/fauna/bird_b_frame_2.vox",
            resourceDirectory + "/fauna/bird_b_frame_3.vox"
        }
    }};

    for (const auto &framePaths : variantPaths) {
        std::vector<std::shared_ptr<Shape>> frames;
        frames.reserve(framePaths.size());
        for (const std::string &path : framePaths) {
            MagicaVoxelModel model;
            std::string error;
            if (!MagicaVoxelLoader::load(path, model, error)) {
                return false;
            }
            auto shape = buildShapeFromVoxelModel(model);
            if (!shape) {
                return false;
            }
            frames.push_back(shape);
        }
        variants_.push_back(std::move(frames));
    }

    if (variants_.empty()) {
        return false;
    }

    for (const auto &frames : variants_) {
        if (frames.empty()) {
            return false;
        }
    }

    spawnBirds();
    return true;
}

void BirdFlock::spawnBirds()
{
    birds_.clear();
    static constexpr int kBirdCount = 10;
    birds_.reserve(kBirdCount);
    for (int i = 0; i < kBirdCount; ++i) {
        birds_.push_back(makeBird());
    }
}

BirdFlock::Bird BirdFlock::makeBird()
{
    Bird bird;
    bird.position = glm::vec3(distXZ_(rng_), distY_(rng_), distXZ_(rng_));
    bird.variantIndex = birds_.empty() ? 0 : (birds_.size() % variants_.size());
    if (!variants_.empty()) {
        std::uniform_int_distribution<std::size_t> variantDist(0, variants_.size() - 1);
        bird.variantIndex = variantDist(rng_);
    }
    bird.speed = distSpeed_(rng_);
    bird.flapTime = distPhase_(rng_);
    bird.bobPhase = distPhase_(rng_);
    retargetBird(bird, glm::vec3(0.0f), false);
    bird.position += bird.velocity * (0.15f * static_cast<float>(birds_.size()));
    return bird;
}

glm::vec3 BirdFlock::randomAirPositionNear(const glm::vec3 &playerPos)
{
    return glm::vec3(
        playerPos.x + distXZ_(rng_),
        playerPos.y + distY_(rng_),
        playerPos.z + distXZ_(rng_)
    );
}

void BirdFlock::retargetBird(Bird &bird, const glm::vec3 &playerPos, bool resetPositionBounds)
{
    static constexpr float kHorizontalRange = 44.0f;
    static constexpr float kMinHeightAbovePlayer = 5.0f;
    static constexpr float kMaxHeightAbovePlayer = 15.0f;

    if (resetPositionBounds) {
        bird.position.x = glm::clamp(
            bird.position.x,
            playerPos.x - kHorizontalRange,
            playerPos.x + kHorizontalRange
        );

        bird.position.y = glm::clamp(
            bird.position.y,
            playerPos.y + kMinHeightAbovePlayer,
            playerPos.y + kMaxHeightAbovePlayer
        );

        bird.position.z = glm::clamp(
            bird.position.z,
            playerPos.z - kHorizontalRange,
            playerPos.z + kHorizontalRange
        );
    }

    bird.target = randomAirPositionNear(playerPos);
    bird.speed = distSpeed_(rng_);
    bird.retargetTimer = distRetarget_(rng_);

    glm::vec3 toTarget = bird.target - bird.position;
    if (glm::dot(toTarget, toTarget) < 0.0001f) {
        toTarget = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    bird.velocity = glm::normalize(toTarget) * bird.speed;
    bird.yaw = std::atan2(bird.velocity.x, bird.velocity.z);
}

void BirdFlock::update(float dt, const glm::vec3 &playerPos)
{
    if (variants_.empty()) {
        return;
    }

    static constexpr float kMaxDistanceFromPlayer = 56.0f;
    static constexpr float kMinHeightAbovePlayer = 5.0f;
    static constexpr float kMaxHeightAbovePlayer = 15.0f;

    for (Bird &bird : birds_) {
        bird.retargetTimer -= dt;
        bird.flapTime += dt * 8.5f;
        bird.bobPhase += dt * 2.4f;

        glm::vec3 flatOffset = bird.position - playerPos;
        flatOffset.y = 0.0f;

        const float flatDistanceFromPlayer = glm::length(flatOffset);
        const bool tooFarFromPlayer = flatDistanceFromPlayer > kMaxDistanceFromPlayer;
        const bool tooLow = bird.position.y < playerPos.y + kMinHeightAbovePlayer;
        const bool tooHigh = bird.position.y > playerPos.y + kMaxHeightAbovePlayer;

        if (tooFarFromPlayer || tooLow || tooHigh) {
            retargetBird(bird, playerPos, true);
        }

        glm::vec3 toTarget = bird.target - bird.position;
        const float distanceToTarget = glm::length(toTarget);

        if (bird.retargetTimer <= 0.0f || distanceToTarget < 2.0f) {
            retargetBird(bird, playerPos, false);
            toTarget = bird.target - bird.position;
        }

        if (glm::dot(toTarget, toTarget) > 0.0001f) {
            glm::vec3 desiredVel = glm::normalize(toTarget) * bird.speed;

            if (tooFarFromPlayer) {
                glm::vec3 backToPlayer = playerPos - bird.position;
                backToPlayer.y = 0.0f;

                if (glm::dot(backToPlayer, backToPlayer) > 0.0001f) {
                    desiredVel = glm::normalize(backToPlayer) * bird.speed;
                }
            }

            const float steer = glm::clamp(dt * 1.6f, 0.0f, 1.0f);
            bird.velocity = glm::mix(bird.velocity, desiredVel, steer);
        }

        bird.position += bird.velocity * dt;

        const float horizLen2 =
            bird.velocity.x * bird.velocity.x +
            bird.velocity.z * bird.velocity.z;

        if (horizLen2 > 0.0001f) {
            bird.yaw = std::atan2(bird.velocity.x, bird.velocity.z);
        }
    }
}

void BirdFlock::draw(const std::shared_ptr<Program> &prog,
    const glm::mat4 &P,
    const glm::mat4 &V,
    const glm::vec3 &lightPos,
    const glm::vec3 &lightColor,
    const glm::vec3 &cameraPos) const
{
    if (!prog || variants_.empty()) {
        return;
    }

    prog->bind();
    glUniformMatrix4fv(prog->getUniform("P"), 1, GL_FALSE, glm::value_ptr(P));
    glUniformMatrix4fv(prog->getUniform("V"), 1, GL_FALSE, glm::value_ptr(V));
    glUniform3fv(prog->getUniform("lightPos"), 1, glm::value_ptr(lightPos));
    glUniform3fv(prog->getUniform("camPos"), 1, glm::value_ptr(cameraPos));
    glUniform3fv(prog->getUniform("lightColor"), 1, glm::value_ptr(lightColor));
    glUniform3fv(prog->getUniform("matAmbient"), 1, glm::value_ptr(glm::vec3(0.28f)));
    glUniform3fv(prog->getUniform("matDiffuse"), 1, glm::value_ptr(glm::vec3(0.92f)));
    glUniform3fv(prog->getUniform("matSpecular"), 1, glm::value_ptr(glm::vec3(0.18f)));
    glUniform1f(prog->getUniform("shininess"), 10.0f);
    glUniform3fv(prog->getUniform("tintColor"), 1, glm::value_ptr(glm::vec3(1.0f)));

    for (const Bird &bird : birds_) {
        const auto &frames = variants_[bird.variantIndex % variants_.size()];
        const std::size_t frameIndex = static_cast<std::size_t>(bird.flapTime) % frames.size();
        glm::mat4 M = glm::translate(glm::mat4(1.0f),
            bird.position + glm::vec3(0.0f, std::sin(bird.bobPhase) * 0.18f, 0.0f));
        M = glm::rotate(M, bird.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        M = glm::scale(M, glm::vec3(scale_));
        glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, glm::value_ptr(M));
        frames[frameIndex]->draw(prog);
    }

    prog->unbind();
}
