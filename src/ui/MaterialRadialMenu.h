#pragma once

#include <array>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <imgui.h>

struct MaterialMenuOption {
    int materialIndex = 0;
    std::string label;
    std::array<glm::vec3, 4> swatchColors;
};

class MaterialRadialMenu {
public:
    void init(std::vector<MaterialMenuOption> options);
    void open(const ImVec2 &center, int currentMaterialIndex);
    void updateMouse(const ImVec2 &pos);
    void draw();
    void dismiss();
    bool confirm(int *selectedMaterialIndex);

    bool isOpen() const { return open_; }
    int hoveredIndex() const { return hoveredIndex_; }

private:
    void loadMenuFont();
    int indexForMaterial(int materialIndex) const;
    int indexForMouse(const ImVec2 &pos) const;
    float outerRadius() const;
    float innerRadius() const;
    float iconDistance() const;
    float iconSize() const;
    float sliceCenterDegrees(int index) const;
    void drawSlice(ImDrawList *drawList, int index, float innerRadius, float outerRadius, ImU32 color) const;
    void drawVoxelCube(ImDrawList *drawList, const MaterialMenuOption &option, const ImVec2 &center, float size, float alpha) const;

    std::vector<MaterialMenuOption> options_;
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
