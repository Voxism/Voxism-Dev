#pragma once

#include "Camera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class FirstPersonCamera : public Camera {
public:
    FirstPersonCamera();
    FirstPersonCamera(glm::vec3 initial_pos, glm::vec3 look_at, float height);

    void ProcessMouseMovement(double dx, double dy) override;
    void ProcessScroll(double dy) override;
    void ProcessKeypress(int key, int action) override;

    glm::vec3 GetPlayerPos() const { return player_pos; }

    glm::vec3 GetCameraPos() const {return cam_pos;}

    glm::mat4 GetViewMatrix() const override;

    void SetPlayerPos(glm::vec3 new_pos);
    void UpdatePlayerPos(glm::vec3 delta_pos);

    void UpdateCamera(float dt) override;

    float GetYaw() const {return yaw;}
    float GetPitch() const {return pitch;}
    float GetFOV() const {return fov;}

    glm::vec3 GetForward() const {return forward;}
    glm::vec3 GetRight() const {return right;}
    glm::vec3 GetUp() const {return up;}

private:
    float height = 1.5f;
    float floor_height = 0.0f;

    glm::vec3 player_pos;
    glm::vec3 look_at;

    float roll = 0.0f;

    const float trans_sensitivity = 10.0f;
    const float sprint_multiple = 1.5f;
    const float rot_sensitivity = 0.5f;
    const float rot_sensitivity_key = 2.0f;

    bool key_forward = false;
    bool key_backward = false;
    bool key_left = false;
    bool key_right = false;
    bool key_yaw_left = false;
    bool key_yaw_right = false;
    bool key_pitch_up = false;
    bool key_pitch_down = false;
    bool key_jump = false;
    bool key_sprint = false;

    // view bobbing
    float bob_time = 0.0f;
    const float bob_speed = 10.0f;
    const float bob_amount_y = 0.05f;
    const float bob_amount_x = 0.02f;
    float bob_weight = 0.0f;
    const float bob_fade_speed = 8.0f;

    // jumping
    float vertical_velocity = 0.0f;
    const float gravity = 30.0f;
    const float jump_velocity = 8.0f;
    bool is_grounded = false;

    // jump landing dip
    float landing_offset = 0.0f;
    float landing_velocity = 0.0f;
    float landing_dip_amount = 2.4f;
    float landing_spring_strength = 75.0f;
    float landing_recover_speed = 12.0f;
    bool was_grounded = false;

    // roll on strafe
    float roll_target = 0.0f;
    float max_roll = 1.0f;
    float roll_lerp_speed = 10.0f;

    // dynamic fov
    const float base_fov = 60.0f;
    const float move_fov = 66.0f;
    const float move_back_fov = 57.0f;
    const float sprint_fov = 72.0f;
    const float fov_lerp_speed = 8.0f;
};