#pragma once

#ifndef _FRUSTUM_H_
#define _FRUSTUM_H_

#include "Frustum.h"
#include <glm/glm.hpp>

class Frustum 
{
    public:
        // Extract frustum on initialization.
        Frustum(glm::mat4 P, glm::mat4 V);

        // Culling test functions. 
        // true = cull, (outside frustum)
        // false = don't cull. (inside frustum)
        bool cullCube(glm::vec3 minPos, glm::vec3 maxPos) const;
        // bool cullSphere(glm::vec3 center, float radius);


    private:
        // Planes
        glm::vec4 Left, Right, Bottom, Top, Near, Far;

};

#endif