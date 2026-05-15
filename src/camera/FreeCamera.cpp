#include "FreeCamera.h"

#include <algorithm>
#include <GLFW/glfw3.h>

FreeCamera::FreeCamera() {
    SetBasisVectors();
}

void FreeCamera::SetState(glm::vec3 position, float yawDegrees, float pitchDegrees, float fovDegrees) {
    cam_pos = position;
    yaw = yawDegrees;
    pitch = pitchDegrees;
    fov = fovDegrees;

    SetBasisVectors();
}

void FreeCamera::ProcessMouseMovement(double dx, double dy) {
    yaw += static_cast<float>(dx) * rot_sensitivity;
    pitch += static_cast<float>(dy) * rot_sensitivity;

    SetBasisVectors();
}

void FreeCamera::ProcessKeypress(int key, int action) {
    bool is_pressed = action != GLFW_RELEASE;

    switch (key) {
    case GLFW_KEY_W:
        key_forward = is_pressed;
        break;
    case GLFW_KEY_S:
        key_backward = is_pressed;
        break;
    case GLFW_KEY_A:
        key_left = is_pressed;
        break;
    case GLFW_KEY_D:
        key_right = is_pressed;
        break;
    case GLFW_KEY_SPACE:
        key_up = is_pressed;
        break;
    case GLFW_KEY_LEFT_CONTROL:
        key_down = is_pressed;
        break;
    case GLFW_KEY_LEFT_SHIFT:
        key_sprint = is_pressed;
        break;
    default:
        break;
    }
}

void FreeCamera::UpdateCamera(float dt) {
    float trans_speed = dt * trans_sensitivity;

    if (key_sprint) {
        trans_speed *= sprint_multiple;
    }

    if (key_forward) {
        cam_pos += forward * trans_speed;
    }
    if (key_backward) {
        cam_pos -= forward * trans_speed;
    }
    if (key_left) {
        cam_pos -= right * trans_speed;
    }
    if (key_right) {
        cam_pos += right * trans_speed;
    }
    if (key_up) {
        cam_pos += up * trans_speed;
    }
    if (key_down) {
        cam_pos -= up * trans_speed;
    }
}

glm::mat4 FreeCamera::GetViewMatrix() const {
    return glm::lookAt(cam_pos, cam_pos + forward, up);
}