#pragma once

#include <array>
#include <string>

#include <glad/glad.h>
#include <imgui.h>

#include "../tools/ToolTypes.h"

class RadialToolMenu {
public:
    bool init(const std::string &resourceDir);
    void destroy();

    void open(const ImVec2 &center, ToolKind currentTool);
    void updateMouse(const ImVec2 &pos);
    void draw();
    bool close(ToolKind *selectedOut);

    bool isOpen() const { return open_; }

private:
    struct ToolSlice {
        ToolKind kind = ToolKind::Cube;
        const char *label = "";
        const char *iconFile = "";
        GLuint iconTexture = 0;
        int iconWidth = 0;
        int iconHeight = 0;
    };

    bool loadIcon(ToolSlice &slice, const std::string &path);
    void loadMenuFont();
    int indexForTool(ToolKind tool) const;
    int indexForMouse(const ImVec2 &pos) const;
    float outerRadius() const;
    float innerRadius() const;
    float iconDistance() const;
    float iconSize() const;
    void drawSlice(ImDrawList *drawList, int index, float innerRadius, float outerRadius, ImU32 color) const;
    void drawFallbackIcon(ImDrawList *drawList, int index, const ImVec2 &center, ImU32 color) const;

    std::array<ToolSlice, 4> slices_;
    ImFont *font_ = nullptr;
    bool initialized_ = false;
    bool open_ = false;
    ImVec2 center_ = ImVec2(0.0f, 0.0f);
    ImVec2 mousePos_ = ImVec2(0.0f, 0.0f);
    int hoveredIndex_ = -1;
    int currentIndex_ = 0;
    bool releaseAnimActive_ = false;
    int releaseAnimIndex_ = -1;
    double releaseAnimStartSeconds_ = 0.0;
};
