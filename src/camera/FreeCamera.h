#pragma once

#include "Camera.h"

class FreeCamera : public Camera {
public:
    FreeCamera();

    void SetState(glm::vec3 position, float yawDegrees, float pitchDegrees, float fovDegrees = 60.0f);

    void ProcessMouseMovement(double dx, double dy) override;
    void ProcessKeypress(int key, int action) override;
    void ProcessScroll(double dy) override {return;}
    void UpdateCamera(float dt) override;

    glm::mat4 GetViewMatrix() const override;

private:
    float trans_sensitivity = 10.0f;
    const float sprint_multiple = 1.5f;
    const float rot_sensitivity = 0.5f;

    bool key_forward = false;
    bool key_backward = false;
    bool key_left = false;
    bool key_right = false;
    bool key_up = false;
    bool key_down = false;
    bool key_sprint = false;
};