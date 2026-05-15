#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    virtual ~Camera() = default;

    virtual void ProcessMouseMovement(double dx, double dy) = 0;
    virtual void ProcessKeypress(int key, int action) = 0;
    virtual void ProcessScroll(double dy) = 0;
    virtual void UpdateCamera(float dt) = 0;
    virtual glm::mat4 GetViewMatrix() const = 0;

    glm::vec3 GetCameraPos() const { return cam_pos; }
    glm::vec3 GetForward() const { return forward; }
    glm::vec3 GetRight() const { return right; }
    glm::vec3 GetUp() const { return up; }

    float GetYaw() const { return yaw; }
    float GetPitch() const { return pitch; }
    float GetFOV() const { return fov; }

protected:
    glm::vec3 cam_pos = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    float yaw = -90.0f;
    float pitch = 0.0f;
    float fov = 60.0f;

    const float min_pitch = -80.0f;
    const float max_pitch = 80.0f;

    void SetBasisVectors();
};