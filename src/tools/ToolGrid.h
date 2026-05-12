#pragma once

#include <glm/glm.hpp>

int floorDiv(int value, int divisor);
int snapDownToGrid(int value, int gridSize);
glm::ivec3 snapVoxelToGrid(const glm::ivec3 &voxel, int gridSize);
void drawDiscreteSizeSelector(const char *label, int &value);
