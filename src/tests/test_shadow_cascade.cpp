#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include <glm/glm.hpp>

#include "../rendering/CascadedShadowMap.h"

static int gPassed = 0;
static int gFailed = 0;

static void reportCheck(const std::string &testName, bool passed, const std::string &detail = "")
{
    if (passed) {
        std::cout << "[PASS] " << testName << "\n";
        ++gPassed;
    } else {
        std::cout << "[FAIL] " << testName;
        if (!detail.empty()) {
            std::cout << "\n       " << detail;
        }
        std::cout << "\n";
        ++gFailed;
    }
}

static bool nearlyEqual(float a, float b, float epsilon = 1e-4f)
{
    return std::abs(a - b) <= epsilon;
}

static void verifyCascadeSplits()
{
    const float nearPlane = 0.1f;
    const float farPlane = 64.0f;
    const int cascadeCount = 3;
    const std::array<float, CascadedShadowMap::kMaxCascades> splits =
        CascadedShadowMap::computePracticalSplits(nearPlane, farPlane, cascadeCount);

    bool monotonic = true;
    float previous = nearPlane;
    for (int i = 0; i < cascadeCount; ++i) {
        if (splits[i] <= previous) {
            monotonic = false;
            break;
        }
        previous = splits[i];
    }
    reportCheck("cascade splits are monotonic", monotonic);
    reportCheck("last cascade ends at shadow distance",
                nearlyEqual(splits[cascadeCount - 1], farPlane),
                "expected " + std::to_string(farPlane) + ", got " + std::to_string(splits[cascadeCount - 1]));
}

static void verifyTexelSnapping()
{
    const float texel = 0.25f;
    bool allOnTexelGrid = true;
    bool deltasAreTexelMultiples = true;
    float previous = CascadedShadowMap::snapToTexel(-2.0f, texel);

    for (int i = -19; i <= 19; ++i) {
        const float value = static_cast<float>(i) * 0.11f;
        const float snapped = CascadedShadowMap::snapToTexel(value, texel);
        const float normalized = snapped / texel;
        allOnTexelGrid = allOnTexelGrid && nearlyEqual(normalized, std::round(normalized));

        const float delta = (snapped - previous) / texel;
        deltasAreTexelMultiples = deltasAreTexelMultiples && nearlyEqual(delta, std::round(delta));
        previous = snapped;
    }

    reportCheck("stable snapping lands on light texel grid", allOnTexelGrid);
    reportCheck("stable snapping changes only by texel increments", deltasAreTexelMultiples);
}

static void verifyVoxelCenterSnapping()
{
    const float voxelSize = 0.0625f;
    const glm::ivec3 voxel(10, -4, 55);
    const glm::vec3 p0 = (glm::vec3(voxel) + glm::vec3(0.10f, 0.20f, 0.30f)) * voxelSize;
    const glm::vec3 p1 = (glm::vec3(voxel) + glm::vec3(0.90f, 0.80f, 0.70f)) * voxelSize;
    const glm::vec3 expected = (glm::vec3(voxel) + glm::vec3(0.5f)) * voxelSize;

    const glm::vec3 snapped0 = CascadedShadowMap::snapWorldToVoxelCenter(p0, voxelSize);
    const glm::vec3 snapped1 = CascadedShadowMap::snapWorldToVoxelCenter(p1, voxelSize);

    const bool sameCenter = glm::length(snapped0 - snapped1) <= 1e-6f;
    const bool matchesExpected = glm::length(snapped0 - expected) <= 1e-6f;
    reportCheck("positions inside one voxel snap to same center", sameCenter);
    reportCheck("voxel snapping matches floor-plus-half rule", matchesExpected);
}

int main()
{
    std::cout << "=== Shadow cascade CPU tests ===\n\n";

    verifyCascadeSplits();
    verifyTexelSnapping();
    verifyVoxelCenterSnapping();

    std::cout << "\n=== Results: " << gPassed << " passed, " << gFailed << " failed ===\n";
    return (gFailed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
