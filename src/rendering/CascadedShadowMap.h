#pragma once

#ifndef VOXISM_CASCADED_SHADOW_MAP_H
#define VOXISM_CASCADED_SHADOW_MAP_H

#include <array>
#include <functional>

#include <glm/glm.hpp>

struct ShadowSettings {
    bool enabled = true;
    int cascadeCount = 3;
    int resolution = 2048;
    float shadowDistance = 64.0f;
    float shadowStrength = 0.55f;
    float minShadowVisibility = 0.45f;
    float blurTexels = 1.5f;
    float normalBiasVoxels = 1.25f;
};

class CascadedShadowMap {
public:
    static const int kMaxCascades = 4;

    CascadedShadowMap() = default;
    ~CascadedShadowMap();

    CascadedShadowMap(const CascadedShadowMap&) = delete;
    CascadedShadowMap& operator=(const CascadedShadowMap&) = delete;

    bool init(int resolution, int cascadeCount);
    bool recreate(int resolution, int cascadeCount);
    void destroy();

    void updateMatrices(const glm::mat4 &P,
        const glm::mat4 &V,
        const glm::vec3 &cameraPos,
        const glm::vec3 &sunDir,
        float shadowDistance,
        float voxelSizeMeters);

    void renderDepthPass(const std::function<void(int cascadeIndex)> &drawCallback) const;

    bool isReady() const { return depthTex_ != 0 && fbo_ != 0; }
    int resolution() const { return resolution_; }
    int cascadeCount() const { return cascadeCount_; }
    unsigned int depthTexture() const { return depthTex_; }

    const glm::mat4 &lightSpaceMatrix(int cascadeIndex) const { return lightSpaceMatrices_[cascadeIndex]; }
    const glm::mat4 &lightProjection(int cascadeIndex) const { return lightProjections_[cascadeIndex]; }
    const glm::mat4 &lightView(int cascadeIndex) const { return lightViews_[cascadeIndex]; }
    const glm::vec3 &cascadeCenter(int cascadeIndex) const { return cascadeCenters_[cascadeIndex]; }
    float cascadeRadius(int cascadeIndex) const { return cascadeRadii_[cascadeIndex]; }
    const std::array<float, kMaxCascades> &cascadeEnds() const { return cascadeEnds_; }

    static std::array<float, kMaxCascades> computePracticalSplits(
        float nearPlane,
        float farPlane,
        int cascadeCount,
        float lambda = 0.65f);
    static float snapToTexel(float value, float texelWorldSize);
    static glm::vec3 snapWorldToVoxelCenter(const glm::vec3 &position, float voxelSizeMeters);

private:
    struct ProjectionParams {
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
        float fovYRadians = 1.0f;
        float aspect = 1.0f;
    };

    static ProjectionParams extractPerspectiveParams(const glm::mat4 &P);

    int resolution_ = 0;
    int cascadeCount_ = 0;
    unsigned int fbo_ = 0;
    unsigned int depthTex_ = 0;

    std::array<glm::mat4, kMaxCascades> lightSpaceMatrices_;
    std::array<glm::mat4, kMaxCascades> lightProjections_;
    std::array<glm::mat4, kMaxCascades> lightViews_;
    std::array<glm::vec3, kMaxCascades> cascadeCenters_;
    std::array<float, kMaxCascades> cascadeRadii_;
    std::array<float, kMaxCascades> cascadeEnds_;
};

#endif
