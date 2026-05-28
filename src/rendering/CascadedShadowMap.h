#pragma once

#ifndef VOXISM_CASCADED_SHADOW_MAP_H
#define VOXISM_CASCADED_SHADOW_MAP_H

#include <array>
#include <functional>

#include <glm/glm.hpp>

/** Runtime toggles and quality knobs passed from the debug UI / game settings. */
struct ShadowSettings {
    bool enabled = true;
    int cascadeCount = 3;
    int resolution = 1024;
    float shadowDistance = 64.0f;
    float shadowStrength = 1.0f;
    float minShadowVisibility = 0.26f;
    float blurTexels = 0.55f;
    float normalBiasVoxels = 1.25f;
};

/**
 * Cascaded shadow maps (CSM): multiple orthographic depth passes from the sun,
 * each covering a slice of the camera view frustum. Splits use the practical
 * scheme (blend of log + uniform). Matrices are texel-snapped to reduce shimmer.
 *
 * GPU resources: one FBO + GL_TEXTURE_2D_ARRAY depth texture (one layer per cascade).
 * Sampling and cascade selection live in chunk_frag.glsl.
 */
class CascadedShadowMap {
public:
    static const int kMaxCascades = 4;

    CascadedShadowMap() = default;
    ~CascadedShadowMap();

    CascadedShadowMap(const CascadedShadowMap&) = delete;
    CascadedShadowMap& operator=(const CascadedShadowMap&) = delete;

    /** Allocate depth texture array and a single FBO used for all cascade layers. */
    bool init(int resolution, int cascadeCount);
    bool recreate(int resolution, int cascadeCount);
    void destroy();

    /**
     * Fit one light view + ortho projection per cascade from the active camera.
     * Must be called before renderDepthPass when the camera or sun moves.
     */
    void updateMatrices(const glm::mat4 &P,
        const glm::mat4 &V,
        const glm::vec3 &cameraPos,
        const glm::vec3 &sunDir,
        float shadowDistance,
        float voxelSizeMeters);

    /** Render depth into each array layer; drawCallback receives the cascade index. */
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
    /** View-space Z distances along the camera forward axis where each cascade ends. */
    const std::array<float, kMaxCascades> &cascadeEnds() const { return cascadeEnds_; }

    /**
     * Practical split scheme (PSS): per-cascade far bound is a mix of logarithmic
     * and uniform splits. lambda=1 is pure log, lambda=0 is pure linear.
     */
    static std::array<float, kMaxCascades> computePracticalSplits(
        float nearPlane,
        float farPlane,
        int cascadeCount,
        float lambda = 0.65f);

    /** Quantize a world-space coordinate to the shadow map texel grid (reduces swimming). */
    static float snapToTexel(float value, float texelWorldSize);
    static glm::vec3 snapWorldToVoxelCenter(const glm::vec3 &position, float voxelSizeMeters);

private:
    struct ProjectionParams {
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
        float fovYRadians = 1.0f;
        float aspect = 1.0f;
    };

    /** Recover near/far/fov/aspect from a standard perspective projection matrix. */
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
