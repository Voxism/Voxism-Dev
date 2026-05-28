#include "MaterialRadialMenu.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <vector>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr int kArcSegments = 40;
constexpr float kSliceGapDegrees = 2.4f;
constexpr float kReleaseDuration = 0.42f;

ImVec2 operator+(const ImVec2 &a, const ImVec2 &b)
{
    return ImVec2(a.x + b.x, a.y + b.y);
}

ImVec2 operator-(const ImVec2 &a, const ImVec2 &b)
{
    return ImVec2(a.x - b.x, a.y - b.y);
}

ImVec2 operator*(const ImVec2 &a, float scale)
{
    return ImVec2(a.x * scale, a.y * scale);
}

float lengthSquared(const ImVec2 &v)
{
    return v.x * v.x + v.y * v.y;
}

float clamp01(float value)
{
    return std::max(0.0f, std::min(1.0f, value));
}

float smoothStep(float value)
{
    const float t = clamp01(value);
    return t * t * (3.0f - 2.0f * t);
}

float degreesToRadians(float degrees)
{
    return degrees * kPi / 180.0f;
}

ImVec2 radialPoint(const ImVec2 &center, float degrees, float radius)
{
    const float radians = degreesToRadians(degrees);
    return ImVec2(
        center.x + std::cos(radians) * radius,
        center.y + std::sin(radians) * radius);
}

ImVec2 normalizedDirection(float degrees)
{
    const float radians = degreesToRadians(degrees);
    return ImVec2(std::cos(radians), std::sin(radians));
}

ImU32 rgba(int r, int g, int b, float alpha)
{
    return IM_COL32(r, g, b, static_cast<int>(255.0f * clamp01(alpha)));
}

ImU32 colorFromVec3(const glm::vec3 &color, float alpha)
{
    return IM_COL32(
        static_cast<int>(255.0f * clamp01(color.r)),
        static_cast<int>(255.0f * clamp01(color.g)),
        static_cast<int>(255.0f * clamp01(color.b)),
        static_cast<int>(255.0f * clamp01(alpha)));
}

glm::vec3 shadeColor(const glm::vec3 &color, float factor, float lift = 0.0f)
{
    return glm::clamp(color * factor + glm::vec3(lift), glm::vec3(0.0f), glm::vec3(1.0f));
}

double nowSeconds()
{
    using Clock = std::chrono::steady_clock;
    static const Clock::time_point start = Clock::now();
    return std::chrono::duration<double>(Clock::now() - start).count();
}

float textWidth(ImFont *font, float size, const char *text)
{
    return font->CalcTextSizeA(size, FLT_MAX, 0.0f, text).x;
}

float textHeight(ImFont *font, float size, const char *text)
{
    return font->CalcTextSizeA(size, FLT_MAX, 0.0f, text).y;
}
}

void MaterialRadialMenu::init(std::vector<MaterialMenuOption> options)
{
    options_ = options;
    loadMenuFont();
    initialized_ = true;
}

void MaterialRadialMenu::open(const ImVec2 &center, int currentMaterialIndex)
{
    if (!initialized_ || options_.empty()) {
        return;
    }

    center_ = center;
    mousePos_ = center;
    currentIndex_ = std::max(0, indexForMaterial(currentMaterialIndex));
    hoveredIndex_ = -1;
    releaseAnimActive_ = false;
    open_ = true;
}

void MaterialRadialMenu::updateMouse(const ImVec2 &pos)
{
    mousePos_ = pos;
    hoveredIndex_ = open_ ? indexForMouse(pos) : -1;
}

void MaterialRadialMenu::dismiss()
{
    open_ = false;
    hoveredIndex_ = -1;
}

bool MaterialRadialMenu::confirm(int *selectedMaterialIndex)
{
    const int selectedIndex = hoveredIndex_;
    open_ = false;
    hoveredIndex_ = -1;

    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(options_.size())) {
        releaseAnimActive_ = false;
        return false;
    }

    currentIndex_ = selectedIndex;
    releaseAnimIndex_ = selectedIndex;
    releaseAnimStartSeconds_ = nowSeconds();
    releaseAnimActive_ = true;

    if (selectedMaterialIndex) {
        *selectedMaterialIndex = options_[selectedIndex].materialIndex;
    }
    return true;
}

void MaterialRadialMenu::loadMenuFont()
{
    struct FontCandidate {
        const char *path;
        float size;
    };

    const FontCandidate candidates[] = {
        {"C:/Windows/Fonts/GIL_____.TTF", 28.0f},
        {"C:/Windows/Fonts/segoeui.ttf", 27.0f},
        {"C:/Windows/Fonts/arial.ttf", 27.0f},
        {"/System/Library/Fonts/Supplemental/Gill Sans.ttc", 28.0f},
        {"/System/Library/Fonts/Supplemental/Avenir Next.ttc", 27.0f},
    };

    ImGuiIO &io = ImGui::GetIO();
    for (const FontCandidate &candidate : candidates) {
        std::ifstream file(candidate.path);
        if (!file.good()) {
            continue;
        }

        font_ = io.Fonts->AddFontFromFileTTF(candidate.path, candidate.size);
        if (font_) {
            return;
        }
    }

    font_ = io.Fonts->Fonts.empty() ? nullptr : io.Fonts->Fonts[0];
}

int MaterialRadialMenu::indexForMaterial(int materialIndex) const
{
    for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
        if (options_[i].materialIndex == materialIndex) {
            return i;
        }
    }
    return -1;
}

int MaterialRadialMenu::indexForMouse(const ImVec2 &pos) const
{
    if (options_.empty()) {
        return -1;
    }

    const ImVec2 delta = pos - center_;
    if (lengthSquared(delta) < innerRadius() * innerRadius()) {
        return -1;
    }

    float degrees = std::atan2(delta.y, delta.x) * 180.0f / kPi;
    if (degrees < 0.0f) {
        degrees += 360.0f;
    }

    const float sector = 360.0f / static_cast<float>(options_.size());
    const float adjusted = std::fmod(degrees + 90.0f + sector * 0.5f, 360.0f);
    return static_cast<int>(adjusted / sector);
}

float MaterialRadialMenu::outerRadius() const
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float shortest = std::max(1.0f, std::min(display.x, display.y));
    return std::max(220.0f, std::min(430.0f, shortest * 0.44f));
}

float MaterialRadialMenu::innerRadius() const
{
    return outerRadius() * 0.34f;
}

float MaterialRadialMenu::iconDistance() const
{
    return (innerRadius() + outerRadius()) * 0.52f;
}

float MaterialRadialMenu::iconSize() const
{
    return std::max(46.0f, outerRadius() * 0.16f);
}

float MaterialRadialMenu::sliceCenterDegrees(int index) const
{
    if (options_.empty()) {
        return -90.0f;
    }
    return -90.0f + (360.0f / static_cast<float>(options_.size())) * static_cast<float>(index);
}

void MaterialRadialMenu::drawSlice(ImDrawList *drawList, int index, float inner, float outer, ImU32 color) const
{
    if (options_.empty()) {
        return;
    }

    const float sector = 360.0f / static_cast<float>(options_.size());
    const float gap = std::min(kSliceGapDegrees, sector * 0.18f);
    const float center = sliceCenterDegrees(index);
    const float start = center - sector * 0.5f + gap;
    const float end = center + sector * 0.5f - gap;

    std::vector<ImVec2> points;
    points.reserve((kArcSegments + 1) * 2);
    for (int segment = 0; segment <= kArcSegments; ++segment) {
        const float t = static_cast<float>(segment) / static_cast<float>(kArcSegments);
        points.push_back(radialPoint(center_, start + (end - start) * t, outer));
    }
    for (int segment = kArcSegments; segment >= 0; --segment) {
        const float t = static_cast<float>(segment) / static_cast<float>(kArcSegments);
        points.push_back(radialPoint(center_, start + (end - start) * t, inner));
    }

    drawList->AddConcavePolyFilled(points.data(), static_cast<int>(points.size()), color);
}

void MaterialRadialMenu::drawVoxelCube(ImDrawList *drawList, const MaterialMenuOption &option, const ImVec2 &center, float size, float alpha) const
{
    const float w = size * 0.48f;
    const float h = size * 0.28f;
    const float drop = size * 0.42f;
    const float yOffset = size * 0.12f;

    const ImVec2 top(center.x, center.y - h - yOffset);
    const ImVec2 right(center.x + w, center.y - yOffset);
    const ImVec2 bottom(center.x, center.y + h - yOffset);
    const ImVec2 left(center.x - w, center.y - yOffset);
    const ImVec2 bottomDrop(center.x, center.y + h + drop - yOffset);
    const ImVec2 leftDrop(center.x - w, center.y + drop - yOffset);
    const ImVec2 rightDrop(center.x + w, center.y + drop - yOffset);

    const ImVec2 topFace[] = {top, right, bottom, left};
    const ImVec2 leftFace[] = {left, bottom, bottomDrop, leftDrop};
    const ImVec2 rightFace[] = {bottom, right, rightDrop, bottomDrop};

    const glm::vec3 topColor = shadeColor(option.swatchColors[3], 1.10f, 0.045f);
    const glm::vec3 leftColor = shadeColor(option.swatchColors[0], 0.68f);
    const glm::vec3 rightColor = shadeColor(option.swatchColors[2], 0.86f, 0.010f);

    drawList->AddConvexPolyFilled(leftFace, 4, colorFromVec3(leftColor, alpha));
    drawList->AddConvexPolyFilled(rightFace, 4, colorFromVec3(rightColor, alpha));
    drawList->AddConvexPolyFilled(topFace, 4, colorFromVec3(topColor, alpha));

    const ImVec2 topPatch[] = {
        radialPoint(center, -112.0f, size * 0.15f),
        radialPoint(center, -62.0f, size * 0.24f),
        radialPoint(center, -14.0f, size * 0.15f),
        radialPoint(center, -166.0f, size * 0.10f),
    };
    const ImVec2 leftPatch[] = {
        ImVec2(left.x + w * 0.24f, left.y + drop * 0.24f),
        ImVec2(bottom.x - w * 0.26f, bottom.y + drop * 0.19f),
        ImVec2(bottom.x - w * 0.26f, bottom.y + drop * 0.48f),
        ImVec2(left.x + w * 0.24f, left.y + drop * 0.54f),
    };
    const ImVec2 rightPatch[] = {
        ImVec2(bottom.x + w * 0.24f, bottom.y + drop * 0.16f),
        ImVec2(right.x - w * 0.24f, right.y + drop * 0.22f),
        ImVec2(right.x - w * 0.24f, right.y + drop * 0.52f),
        ImVec2(bottom.x + w * 0.24f, bottom.y + drop * 0.45f),
    };

    drawList->AddConvexPolyFilled(topPatch, 4, colorFromVec3(shadeColor(option.swatchColors[1], 1.08f, 0.035f), alpha * 0.70f));
    drawList->AddConvexPolyFilled(leftPatch, 4, colorFromVec3(shadeColor(option.swatchColors[1], 0.62f), alpha * 0.55f));
    drawList->AddConvexPolyFilled(rightPatch, 4, colorFromVec3(shadeColor(option.swatchColors[3], 0.82f, 0.008f), alpha * 0.55f));
}

void MaterialRadialMenu::draw()
{
    if ((!open_ && !releaseAnimActive_) || options_.empty()) {
        return;
    }

    ImDrawList *drawList = ImGui::GetForegroundDrawList();
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImFont *font = font_ ? font_ : ImGui::GetFont();
    const float outer = outerRadius();
    const float inner = innerRadius();
    const float iconDist = iconDistance();
    const float icon = iconSize();

    if (open_) {
        drawList->AddRectFilled(ImVec2(0.0f, 0.0f), displaySize, IM_COL32(3, 5, 9, 76));

        for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
            ImU32 fill = rgba(18, 25, 32, 0.50f);
            if (i == currentIndex_) {
                fill = rgba(238, 244, 248, 0.18f);
            }
            if (i == hoveredIndex_) {
                fill = rgba(255, 255, 255, 0.34f);
            }
            drawSlice(drawList, i, inner, outer, fill);
        }

        drawList->AddCircleFilled(center_, inner * 0.78f, rgba(18, 25, 32, 0.50f), 72);

        const char *centerLabel = (hoveredIndex_ >= 0) ? options_[hoveredIndex_].label.c_str() : options_[currentIndex_].label.c_str();
        const float centerSize = std::max(23.0f, outer * 0.082f);
        const ImVec2 centerText(
            textWidth(font, centerSize, centerLabel),
            textHeight(font, centerSize, centerLabel));
        drawList->AddText(
            font,
            centerSize,
            center_ - centerText * 0.5f + ImVec2(2.0f, 2.0f),
            rgba(2, 4, 7, 0.42f),
            centerLabel);
        drawList->AddText(
            font,
            centerSize,
            center_ - centerText * 0.5f,
            rgba(244, 249, 249, 0.92f),
            centerLabel);

        for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
            const ImVec2 direction = normalizedDirection(sliceCenterDegrees(i));
            const ImVec2 iconCenter = center_ + direction * iconDist;
            const bool highlighted = (i == hoveredIndex_);
            const float iconScale = highlighted ? 1.14f : 1.0f;
            drawVoxelCube(drawList, options_[i], iconCenter, icon * iconScale, highlighted ? 1.0f : 0.84f);

            const float labelSize = std::max(18.0f, outer * 0.052f);
            const ImVec2 labelText(
                textWidth(font, labelSize, options_[i].label.c_str()),
                textHeight(font, labelSize, options_[i].label.c_str()));
            const ImVec2 labelPos(iconCenter.x - labelText.x * 0.5f, iconCenter.y + icon * 0.58f);
            drawList->AddText(
                font,
                labelSize,
                labelPos,
                highlighted ? rgba(244, 249, 249, 0.96f) : rgba(204, 214, 224, 0.70f),
                options_[i].label.c_str());
        }
    }

    if (releaseAnimActive_) {
        const float t = static_cast<float>((nowSeconds() - releaseAnimStartSeconds_) / kReleaseDuration);
        if (t >= 1.0f) {
            releaseAnimActive_ = false;
            return;
        }

        const float eased = smoothStep(t);
        const float alpha = 1.0f - eased;
        const float grow = 1.0f + 0.22f * eased;
        drawSlice(drawList, releaseAnimIndex_, inner * (0.98f - 0.08f * eased), outer * grow, rgba(255, 255, 255, 0.36f * alpha));

        const ImVec2 direction = normalizedDirection(sliceCenterDegrees(releaseAnimIndex_));
        const ImVec2 iconCenter = center_ + direction * iconDist * (1.0f + 0.10f * eased);
        drawVoxelCube(drawList, options_[releaseAnimIndex_], iconCenter, icon * (1.18f + 0.18f * eased), alpha);
    }
}
