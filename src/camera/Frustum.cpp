#include "Frustum.h"
#include <functional>

Frustum::Frustum(glm::mat4 P, glm::mat4 V){
    // Extract view frustum planes.

    /* composite matrix */
    glm::mat4 comp = P*V;
    glm::vec3 norm; //use to pull out normal
    float normMag; //length of normal for plane normalization
    std::function<void(glm::vec4*)> NormalizePlane = [&](glm::vec4* plane){
        norm = glm::vec3(plane->x, plane->y, plane->z);
        normMag = glm::length(norm);
        *plane = (*plane)/normMag;
    };
    
    // Extract Left Plane.
    Left.x = comp[0][3] + comp[0][0]; 
    Left.y = comp[1][3] + comp[1][0]; 
    Left.z = comp[2][3] + comp[2][0]; 
    Left.w = comp[3][3] + comp[3][0];
    NormalizePlane(&Left);
    // norm = glm::vec3(Left.x, Left.y, Left.z);
    // normMag = glm::length(norm);
    // Left = Left/normMag;

    // Extract Right Plane
    Right.x = comp[0][3] - comp[0][0]; 
    Right.y = comp[1][3] - comp[1][0]; 
    Right.z = comp[2][3] - comp[2][0]; 
    Right.w = comp[3][3] - comp[3][0]; 
    NormalizePlane(&Right);

    // Extract Bottom Plane
    Bottom.x = comp[0][3] + comp[0][1];
    Bottom.y = comp[1][3] + comp[1][1];
    Bottom.z = comp[2][3] + comp[2][1];
    Bottom.w = comp[3][3] + comp[3][1];
    NormalizePlane(&Bottom);

    // Extract Top Plane
    Top.x = comp[0][3] - comp[0][1];
    Top.y = comp[1][3] - comp[1][1];
    Top.z = comp[2][3] - comp[2][1];
    Top.w = comp[3][3] - comp[3][1];
    NormalizePlane(&Top);

    // Extract Near Plane
    Near.x = comp[0][2];
    Near.y = comp[1][2];
    Near.z = comp[2][2];
    Near.w = comp[3][2];
    NormalizePlane(&Near);

    // Extract Far Plane
    Far.x = comp[0][3] - comp[0][2];
    Far.y = comp[1][3] - comp[1][2];
    Far.z = comp[2][3] - comp[2][2];
    Far.w = comp[3][3] - comp[3][2];
    NormalizePlane(&Far);
};

bool Frustum::cullCube(glm::vec3 minPos, glm::vec3 maxPos) const {

    std::function<bool(glm::vec4)> testPlane = [&](glm::vec4 plane){
        // AABB test
        glm::vec3 p;

        p.x = (plane.x >= 0.0f) ? maxPos.x : minPos.x;
        p.y = (plane.y >= 0.0f) ? maxPos.y : minPos.y;
        p.z = (plane.z >= 0.0f) ? maxPos.z : minPos.z;

        float dist =
            plane.x * p.x +
            plane.y * p.y +
            plane.z * p.z +
            plane.w;

        return dist < 0.0;
            
    };

    return (testPlane(Left) || testPlane(Right) || testPlane(Top) || testPlane(Bottom) || testPlane(Near) || testPlane(Far));
}