#pragma once

#include <glm/glm.hpp>

#include <memory>
#include <random>
#include <string>
#include <vector>

class Program;
class Shape;

class BirdFlock {
public:
    bool init(const std::string &resourceDirectory);
    void update(float dt, const glm::vec3 &playerPos);
    void draw(const std::shared_ptr<Program> &prog,
        const glm::mat4 &P,
        const glm::mat4 &V,
        const glm::vec3 &lightPos,
        const glm::vec3 &lightColor,
        const glm::vec3 &cameraPos) const;

    std::size_t birdCount() const { return birds_.size(); }

private:
    struct Bird {
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 velocity = glm::vec3(0.0f);
        glm::vec3 target = glm::vec3(0.0f);
        std::size_t variantIndex = 0;
        float speed = 0.0f;
        float yaw = 0.0f;
        float flapTime = 0.0f;
        float retargetTimer = 0.0f;
        float bobPhase = 0.0f;
    };

    void spawnBirds();
    Bird makeBird();
    glm::vec3 randomAirPositionNear(const glm::vec3 &playerPos);
    void retargetBird(Bird &bird, const glm::vec3 &playerPos, bool resetPositionBounds);

    std::vector<std::vector<std::shared_ptr<Shape>>> variants_;
    std::vector<Bird> birds_;
    std::mt19937 rng_ {std::random_device{}()};
    std::uniform_real_distribution<float> distXZ_ {-42.0f, 42.0f};
    std::uniform_real_distribution<float> distY_ {5.0f, 15.0f};
    std::uniform_real_distribution<float> distSpeed_ {2.2f, 4.6f};
    std::uniform_real_distribution<float> distRetarget_ {1.5f, 4.0f};
    std::uniform_real_distribution<float> distPhase_ {0.0f, 6.28318530718f};
    float scale_ = 0.18f;
};
