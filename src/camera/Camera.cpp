#include "Camera.h"

#include <algorithm>
#include <cmath>

void Camera::SetBasisVectors() {
    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);

    pitch = std::max(min_pitch, std::min(max_pitch, pitch));

    float yawRad = glm::radians(yaw);
    float pitchRad = glm::radians(pitch);

    forward.x = cosf(pitchRad) * cosf(yawRad);
    forward.y = sinf(pitchRad);
    forward.z = cosf(pitchRad) * sinf(yawRad);
    forward = glm::normalize(forward);

    right = glm::normalize(glm::cross(forward, worldUp));
    up = glm::normalize(glm::cross(right, forward));
}