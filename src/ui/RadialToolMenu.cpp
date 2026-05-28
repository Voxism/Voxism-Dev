#include "RadialToolMenu.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

#define NANOSVG_IMPLEMENTATION
#include <nanosvg/nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvg/nanosvgrast.h>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr int kArcSegments = 40;
constexpr float kSliceGapDegrees = 3.0f;
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

const char *toolLabel(ToolKind tool)
{
    switch (tool) {
    case ToolKind::Cube:
        return "Cube";
    case ToolKind::Area:
        return "Area";
    case ToolKind::Sphere:
        return "Sphere";
    case ToolKind::OrganicSphere:
        return "Organic";
    default:
        return "Tool";
    }
}
}

bool RadialToolMenu::init(const std::string &resourceDir)
{
    slices_ = {{
        {ToolKind::Cube, "Cube", "tool_cube.svg", 0, 0, 0},
        {ToolKind::Area, "Area", "tool_area.svg", 0, 0, 0},
        {ToolKind::Sphere, "Sphere", "tool_sphere.svg", 0, 0, 0},
        {ToolKind::OrganicSphere, "Organic", "tool_organic.svg", 0, 0, 0},
    }};

    loadMenuFont();

    bool loadedAll = true;
    const std::string iconDir = resourceDir + "/ui/icons/";
    for (ToolSlice &slice : slices_) {
        loadedAll = loadIcon(slice, iconDir + slice.iconFile) && loadedAll;
    }

    initialized_ = true;
    return loadedAll;
}

void RadialToolMenu::destroy()
{
    for (ToolSlice &slice : slices_) {
        if (slice.iconTexture != 0) {
            glDeleteTextures(1, &slice.iconTexture);
            slice.iconTexture = 0;
        }
    }
    initialized_ = false;
    open_ = false;
    releaseAnimActive_ = false;
}

void RadialToolMenu::open(const ImVec2 &center, ToolKind currentTool)
{
    if (!initialized_) {
        return;
    }

    center_ = center;
    mousePos_ = center;
    currentIndex_ = std::max(0, indexForTool(currentTool));
    hoveredIndex_ = -1;
    releaseAnimActive_ = false;
    open_ = true;
}

void RadialToolMenu::updateMouse(const ImVec2 &pos)
{
    mousePos_ = pos;
    hoveredIndex_ = open_ ? indexForMouse(pos) : -1;
}

void RadialToolMenu::dismiss()
{
    open_ = false;
    hoveredIndex_ = -1;
}

bool RadialToolMenu::confirm(ToolKind *selectedOut)
{
    const int selectedIndex = hoveredIndex_;
    open_ = false;
    hoveredIndex_ = -1;

    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(slices_.size())) {
        releaseAnimActive_ = false;
        return false;
    }

    currentIndex_ = selectedIndex;
    releaseAnimIndex_ = selectedIndex;
    releaseAnimStartSeconds_ = nowSeconds();
    releaseAnimActive_ = true;

    if (selectedOut) {
        *selectedOut = slices_[selectedIndex].kind;
    }
    return true;
}

void RadialToolMenu::loadMenuFont()
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

bool RadialToolMenu::loadIcon(ToolSlice &slice, const std::string &path)
{
    NSVGimage *image = nsvgParseFromFile(path.c_str(), "px", 96.0f);
    if (!image) {
        std::cerr << "RadialToolMenu: could not parse SVG icon '" << path << "'" << std::endl;
        return false;
    }

    NSVGrasterizer *rasterizer = nsvgCreateRasterizer();
    if (!rasterizer) {
        nsvgDelete(image);
        std::cerr << "RadialToolMenu: could not create SVG rasterizer" << std::endl;
        return false;
    }

    constexpr int kTextureSize = 128;
    std::vector<unsigned char> pixels(kTextureSize * kTextureSize * 4, 0);
    const float imageWidth = std::max(1.0f, image->width);
    const float imageHeight = std::max(1.0f, image->height);
    const float scale = std::min(
        (kTextureSize - 16.0f) / imageWidth,
        (kTextureSize - 16.0f) / imageHeight);
    const float offsetX = (kTextureSize - imageWidth * scale) * 0.5f;
    const float offsetY = (kTextureSize - imageHeight * scale) * 0.5f;
    nsvgRasterize(rasterizer, image, offsetX, offsetY, scale, pixels.data(), kTextureSize, kTextureSize, kTextureSize * 4);

    glGenTextures(1, &slice.iconTexture);
    glBindTexture(GL_TEXTURE_2D, slice.iconTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kTextureSize, kTextureSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    slice.iconWidth = kTextureSize;
    slice.iconHeight = kTextureSize;

    nsvgDeleteRasterizer(rasterizer);
    nsvgDelete(image);
    return true;
}

int RadialToolMenu::indexForTool(ToolKind tool) const
{
    for (int i = 0; i < static_cast<int>(slices_.size()); ++i) {
        if (slices_[i].kind == tool) {
            return i;
        }
    }
    return -1;
}

int RadialToolMenu::indexForMouse(const ImVec2 &pos) const
{
    const ImVec2 delta = pos - center_;
    if (lengthSquared(delta) < innerRadius() * innerRadius()) {
        return -1;
    }

    float degrees = std::atan2(delta.y, delta.x) * 180.0f / kPi;
    if (degrees < 0.0f) {
        degrees += 360.0f;
    }

    if (degrees >= 225.0f && degrees < 315.0f) {
        return 0;
    }
    if (degrees >= 315.0f || degrees < 45.0f) {
        return 1;
    }
    if (degrees >= 45.0f && degrees < 135.0f) {
        return 2;
    }
    return 3;
}

float RadialToolMenu::outerRadius() const
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float shortest = std::max(1.0f, std::min(display.x, display.y));
    return std::max(220.0f, std::min(430.0f, shortest * 0.44f));
}

float RadialToolMenu::innerRadius() const
{
    return outerRadius() * 0.34f;
}

float RadialToolMenu::iconDistance() const
{
    return (innerRadius() + outerRadius()) * 0.52f;
}

float RadialToolMenu::iconSize() const
{
    return std::max(46.0f, outerRadius() * 0.17f);
}

void RadialToolMenu::drawSlice(ImDrawList *drawList, int index, float inner, float outer, ImU32 color) const
{
    constexpr float starts[] = {-135.0f, -45.0f, 45.0f, 135.0f};
    constexpr float ends[] = {-45.0f, 45.0f, 135.0f, 225.0f};

    const float start = starts[index] + kSliceGapDegrees;
    const float end = ends[index] - kSliceGapDegrees;

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

void RadialToolMenu::draw()
{
    if (!open_ && !releaseAnimActive_) {
        return;
    }

    ImDrawList *drawList = ImGui::GetForegroundDrawList();
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImFont *font = font_ ? font_ : ImGui::GetFont();
    const float outer = outerRadius();
    const float inner = innerRadius();
    const float iconDist = iconDistance();
    const float icon = iconSize();

    constexpr float sliceCenters[] = {-90.0f, 0.0f, 90.0f, 180.0f};

    if (open_) {
        drawList->AddRectFilled(ImVec2(0.0f, 0.0f), displaySize, IM_COL32(3, 5, 9, 76));

        for (int i = 0; i < static_cast<int>(slices_.size()); ++i) {
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

        const char *centerLabel = (hoveredIndex_ >= 0) ? slices_[hoveredIndex_].label : toolLabel(slices_[currentIndex_].kind);
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

        for (int i = 0; i < static_cast<int>(slices_.size()); ++i) {
            const ImVec2 direction = normalizedDirection(sliceCenters[i]);
            const ImVec2 iconCenter = center_ + direction * iconDist;
            const bool highlighted = (i == hoveredIndex_);
            const bool current = (i == currentIndex_);
            const float iconScale = highlighted ? 1.14f : 1.0f;
            const ImU32 iconTint = highlighted
                ? rgba(255, 255, 255, 1.0f)
                : (current ? rgba(230, 244, 255, 0.90f) : rgba(204, 214, 224, 0.72f));

            if (slices_[i].iconTexture != 0) {
                const float drawSize = icon * iconScale;
                const ImVec2 iconMin(iconCenter.x - drawSize * 0.5f, iconCenter.y - drawSize * 0.5f);
                const ImVec2 iconMax(iconCenter.x + drawSize * 0.5f, iconCenter.y + drawSize * 0.5f);
                drawList->AddImage(
                    static_cast<ImTextureID>(static_cast<uintptr_t>(slices_[i].iconTexture)),
                    iconMin,
                    iconMax,
                    ImVec2(0.0f, 0.0f),
                    ImVec2(1.0f, 1.0f),
                    iconTint);
            } else {
                drawFallbackIcon(drawList, i, iconCenter, iconTint);
            }

            const float labelSize = std::max(18.0f, outer * 0.060f);
            const ImVec2 labelText(
                textWidth(font, labelSize, slices_[i].label),
                textHeight(font, labelSize, slices_[i].label));
            const ImVec2 labelPos(iconCenter.x - labelText.x * 0.5f, iconCenter.y + icon * 0.58f);
            drawList->AddText(
                font,
                labelSize,
                labelPos,
                highlighted ? rgba(244, 249, 249, 0.96f) : rgba(204, 214, 224, 0.70f),
                slices_[i].label);
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

        const ImVec2 direction = normalizedDirection(sliceCenters[releaseAnimIndex_]);
        const ImVec2 iconCenter = center_ + direction * iconDist * (1.0f + 0.10f * eased);
        const float drawSize = icon * (1.18f + 0.18f * eased);
        const ImU32 tint = rgba(255, 255, 255, alpha);
        if (slices_[releaseAnimIndex_].iconTexture != 0) {
            drawList->AddImage(
                static_cast<ImTextureID>(static_cast<uintptr_t>(slices_[releaseAnimIndex_].iconTexture)),
                ImVec2(iconCenter.x - drawSize * 0.5f, iconCenter.y - drawSize * 0.5f),
                ImVec2(iconCenter.x + drawSize * 0.5f, iconCenter.y + drawSize * 0.5f),
                ImVec2(0.0f, 0.0f),
                ImVec2(1.0f, 1.0f),
                tint);
        }
    }
}

void RadialToolMenu::drawFallbackIcon(ImDrawList *drawList, int index, const ImVec2 &center, ImU32 color) const
{
    const float s = iconSize() * 0.5f;
    if (index == 0) {
        drawList->AddRect(ImVec2(center.x - s, center.y - s), ImVec2(center.x + s, center.y + s), color, 2.0f, 0, 2.0f);
    } else if (index == 1) {
        drawList->AddRect(ImVec2(center.x - s, center.y - s), ImVec2(center.x + s, center.y + s), color, 1.0f, 0, 2.0f);
        drawList->AddLine(ImVec2(center.x, center.y - s), ImVec2(center.x, center.y + s), color, 1.5f);
        drawList->AddLine(ImVec2(center.x - s, center.y), ImVec2(center.x + s, center.y), color, 1.5f);
    } else if (index == 2) {
        drawList->AddCircle(center, s, color, 36, 2.0f);
        drawList->AddLine(ImVec2(center.x - s, center.y), ImVec2(center.x + s, center.y), color, 1.5f);
    } else {
        drawList->AddCircle(center, s * 0.8f, color, 9, 2.0f);
    }
}
