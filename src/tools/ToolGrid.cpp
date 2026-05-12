#include "ToolGrid.h"

#include <imgui.h>

#include <cstdio>

namespace {
constexpr int kDiscreteToolSizes[] = {2, 4, 8, 16, 32, 64};
constexpr int kDiscreteToolSizeCount = sizeof(kDiscreteToolSizes) / sizeof(kDiscreteToolSizes[0]);
}

int floorDiv(int value, int divisor)
{
    int result = value / divisor;
    const int remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
        --result;
    }
    return result;
}

int snapDownToGrid(int value, int gridSize)
{
    return floorDiv(value, gridSize) * gridSize;
}

glm::ivec3 snapVoxelToGrid(const glm::ivec3 &voxel, int gridSize)
{
    return glm::ivec3(
        snapDownToGrid(voxel.x, gridSize),
        snapDownToGrid(voxel.y, gridSize),
        snapDownToGrid(voxel.z, gridSize));
}

void drawDiscreteSizeSelector(const char *label, int &value)
{
    int selectedIndex = 0;
    for (int i = 0; i < kDiscreteToolSizeCount; ++i) {
        if (kDiscreteToolSizes[i] == value) {
            selectedIndex = i;
            break;
        }
    }

    char previewValue[8];
    snprintf(previewValue, sizeof(previewValue), "%d", kDiscreteToolSizes[selectedIndex]);

    if (ImGui::BeginCombo(label, previewValue)) {
        for (int i = 0; i < kDiscreteToolSizeCount; ++i) {
            const bool selected = (kDiscreteToolSizes[i] == value);
            char itemLabel[8];
            snprintf(itemLabel, sizeof(itemLabel), "%d", kDiscreteToolSizes[i]);
            if (ImGui::Selectable(itemLabel, selected)) {
                value = kDiscreteToolSizes[i];
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}
