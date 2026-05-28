#pragma once

#include "VoxelTemplate.h"

#include <string>

class FloraAssets {
public:
    bool load(const std::string &resourceDirectory, std::string &outError);

    bool loaded() const { return loaded_; }
    const VoxelTemplate &bush() const { return bush_; }
    const VoxelTemplate &flower() const { return flower_; }

private:
    bool loaded_ = false;
    VoxelTemplate bush_;
    VoxelTemplate flower_;
};
