#include "CascadedShadowMap.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

const int CascadedShadowMap::kMaxCascades;

CascadedShadowMap::~CascadedShadowMap()
{
    destroy();
}

bool CascadedShadowMap::init(int resolution, int cascadeCount)
{
    resolution_ = std::max(1, resolution);
    cascadeCount_ = std::max(1, std::min(cascadeCount, kMaxCascades));

    glGenFramebuffers(1, &fbo_);
    glGenTextures(1, &depthTex_);
    glBindTexture(GL_TEXTURE_2D_ARRAY, depthTex_);
    glTexImage3D(GL_TEXTURE_2D_ARRAY,
        0,
        GL_DEPTH_COMPONENT24,
        resolution_,
        resolution_,
        cascadeCount_,
        0,
        GL_DEPTH_COMPONENT,
        GL_FLOAT,
        nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTex_, 0, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    const bool complete = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (!complete) {
        std::cerr << "[Shadow] Cascaded shadow framebuffer is incomplete" << std::endl;
        destroy();
        return false;
    }

    return true;
}

bool CascadedShadowMap::recreate(int resolution, int cascadeCount)
{
    destroy();
    return init(resolution, cascadeCount);
}

void CascadedShadowMap::destroy()
{
    if (depthTex_) {
        glDeleteTextures(1, &depthTex_);
        depthTex_ = 0;
    }
    if (fbo_) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
    resolution_ = 0;
    cascadeCount_ = 0;
}

std::array<float, CascadedShadowMap::kMaxCascades> CascadedShadowMap::computePracticalSplits(
    float nearPlane,
    float farPlane,
    int cascadeCount,
    float lambda)
{
    std::array<float, kMaxCascades> splits = {{0.0f, 0.0f, 0.0f, 0.0f}};
    const int count = std::max(1, std::min(cascadeCount, kMaxCascades));
    const float nearZ = std::max(0.001f, nearPlane);
    const float farZ = std::max(nearZ + 0.001f, farPlane);
    const float safeLambda = std::max(0.0f, std::min(lambda, 1.0f));

    for (int i = 1; i <= count; ++i) {
        const float p = static_cast<float>(i) / static_cast<float>(count);
        const float logSplit = nearZ * std::pow(farZ / nearZ, p);
        const float uniformSplit = nearZ + (farZ - nearZ) * p;
        splits[i - 1] = safeLambda * logSplit + (1.0f - safeLambda) * uniformSplit;
    }
    splits[count - 1] = farZ;
    return splits;
}

float CascadedShadowMap::snapToTexel(float value, float texelWorldSize)
{
    if (texelWorldSize <= 0.0f) {
        return value;
    }
    return std::round(value / texelWorldSize) * texelWorldSize;
}

glm::vec3 CascadedShadowMap::snapWorldToVoxelCenter(const glm::vec3 &position, float voxelSizeMeters)
{
    if (voxelSizeMeters <= 0.0f) {
        return position;
    }
    return (glm::floor(position / voxelSizeMeters) + glm::vec3(0.5f)) * voxelSizeMeters;
}

CascadedShadowMap::ProjectionParams CascadedShadowMap::extractPerspectiveParams(const glm::mat4 &P)
{
    ProjectionParams out;
    const float a = P[0][0];
    const float b = P[1][1];
    const float A = P[2][2];
    const float B = P[3][2];

    if (std::abs(A - 1.0f) > 1e-6f) {
        out.nearPlane = std::abs(B / (A - 1.0f));
    }
    if (std::abs(A + 1.0f) > 1e-6f) {
        out.farPlane = std::abs(B / (A + 1.0f));
    }
    if (std::abs(b) > 1e-6f) {
        out.fovYRadians = 2.0f * std::atan(1.0f / b);
    }
    if (std::abs(a) > 1e-6f) {
        out.aspect = b / a;
    }
    return out;
}

void CascadedShadowMap::updateMatrices(const glm::mat4 &P,
    const glm::mat4 &V,
    const glm::vec3 &cameraPos,
    const glm::vec3 &sunDir,
    float shadowDistance,
    float voxelSizeMeters)
{
    if (cascadeCount_ <= 0) {
        return;
    }

    const ProjectionParams projection = extractPerspectiveParams(P);
    const float nearPlane = projection.nearPlane;
    const float farPlane = std::max(nearPlane + 1.0f, std::min(projection.farPlane, shadowDistance));
    cascadeEnds_ = computePracticalSplits(nearPlane, farPlane, cascadeCount_);

    const glm::mat4 invV = glm::inverse(V);
    const glm::vec3 right = glm::normalize(glm::vec3(invV[0]));
    const glm::vec3 up = glm::normalize(glm::vec3(invV[1]));
    const glm::vec3 forward = glm::normalize(-glm::vec3(invV[2]));
    const glm::vec3 safeSunDir = (glm::dot(sunDir, sunDir) > 0.0001f)
        ? glm::normalize(sunDir)
        : glm::normalize(glm::vec3(0.35f, 0.85f, 0.25f));
    const glm::vec3 lightUp = (std::abs(glm::dot(safeSunDir, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.95f)
        ? glm::vec3(0.0f, 0.0f, 1.0f)
        : glm::vec3(0.0f, 1.0f, 0.0f);

    float previousSplit = nearPlane;
    for (int cascade = 0; cascade < cascadeCount_; ++cascade) {
        const float cascadeEnd = cascadeEnds_[cascade];
        const float tanHalfFov = std::tan(projection.fovYRadians * 0.5f);
        const float nearHalfH = tanHalfFov * previousSplit;
        const float nearHalfW = nearHalfH * projection.aspect;
        const float farHalfH = tanHalfFov * cascadeEnd;
        const float farHalfW = farHalfH * projection.aspect;
        const glm::vec3 nearCenter = cameraPos + forward * previousSplit;
        const glm::vec3 farCenter = cameraPos + forward * cascadeEnd;

        glm::vec3 corners[8] = {
            nearCenter - right * nearHalfW - up * nearHalfH,
            nearCenter + right * nearHalfW - up * nearHalfH,
            nearCenter + right * nearHalfW + up * nearHalfH,
            nearCenter - right * nearHalfW + up * nearHalfH,
            farCenter - right * farHalfW - up * farHalfH,
            farCenter + right * farHalfW - up * farHalfH,
            farCenter + right * farHalfW + up * farHalfH,
            farCenter - right * farHalfW + up * farHalfH
        };

        glm::vec3 center(0.0f);
        for (int i = 0; i < 8; ++i) {
            center += corners[i];
        }
        center /= 8.0f;

        float radius = 0.0f;
        for (int i = 0; i < 8; ++i) {
            radius = std::max(radius, glm::length(corners[i] - center));
        }
        // Keep the light volume a little larger than the exact camera slice so
        // nearby offscreen voxels can still cast into the visible view while
        // the camera rotates in place.
        radius *= 1.35f;
        const float snapUnit = (voxelSizeMeters > 0.0f) ? voxelSizeMeters : 0.0625f;
        radius = std::ceil(radius / snapUnit) * snapUnit;
        radius = std::max(radius, snapUnit);

        const float texelWorldSize = (2.0f * radius) / static_cast<float>(std::max(1, resolution_));
        glm::mat4 snapView = glm::lookAt(center + safeSunDir * radius * 3.0f, center, lightUp);
        glm::vec4 lightCenter = snapView * glm::vec4(center, 1.0f);
        lightCenter.x = snapToTexel(lightCenter.x, texelWorldSize);
        lightCenter.y = snapToTexel(lightCenter.y, texelWorldSize);
        const glm::vec3 snappedCenter = glm::vec3(glm::inverse(snapView) * lightCenter);

        const glm::mat4 lightView = glm::lookAt(
            snappedCenter + safeSunDir * radius * 4.0f,
            snappedCenter,
            lightUp);
        const glm::mat4 lightProjection = glm::ortho(
            -radius, radius,
            -radius, radius,
            0.1f, radius * 8.0f);

        cascadeCenters_[cascade] = snappedCenter;
        cascadeRadii_[cascade] = radius;
        lightViews_[cascade] = lightView;
        lightProjections_[cascade] = lightProjection;
        lightSpaceMatrices_[cascade] = lightProjection * lightView;

        previousSplit = cascadeEnd;
    }
}

void CascadedShadowMap::renderDepthPass(const std::function<void(int cascadeIndex)> &drawCallback) const
{
    if (!isReady() || !drawCallback) {
        return;
    }

    GLint previousFbo = 0;
    GLint previousViewport[4] = {0, 0, 0, 0};
    GLboolean previousColorMask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    glGetBooleanv(GL_COLOR_WRITEMASK, previousColorMask);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, resolution_, resolution_);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    for (int cascade = 0; cascade < cascadeCount_; ++cascade) {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTex_, 0, cascade);
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
        drawCallback(cascade);
    }

    glColorMask(previousColorMask[0], previousColorMask[1], previousColorMask[2], previousColorMask[3]);
    if (cullWasEnabled) {
        glEnable(GL_CULL_FACE);
    } else {
        glDisable(GL_CULL_FACE);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, previousFbo);
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
}
