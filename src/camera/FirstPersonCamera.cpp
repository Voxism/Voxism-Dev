#include "FirstPersonCamera.h"

#include <algorithm>
#include <cmath>
#include <GLFW/glfw3.h>

FirstPersonCamera::FirstPersonCamera() {
    player_pos = glm::vec3(0.0f, 1.0f, 0.0f);
    cam_pos = glm::vec3(0.0f, 1.0f, 0.0f);
    look_at = glm::vec3(0.0f, 1.0f, -1.0f);

    yaw = -90.0f;
    pitch = 0.0f;
    fov = base_fov;

    SetBasisVectors();
}

FirstPersonCamera::FirstPersonCamera(glm::vec3 initial_pos, glm::vec3 look_at_, float height_) {
    player_pos = initial_pos;
    cam_pos = initial_pos;
    look_at = look_at_;
    height = height_;

    glm::vec3 dir = glm::normalize(look_at - cam_pos);

    yaw = glm::degrees(atan2f(dir.z, dir.x));
    pitch = glm::degrees(asinf(dir.y));
    fov = base_fov;

    SetBasisVectors();
}

void FirstPersonCamera::ProcessMouseMovement(double dx, double dy) {
    yaw += static_cast<float>(dx) * rot_sensitivity;
    pitch += static_cast<float>(dy) * rot_sensitivity;

    SetBasisVectors();
}

void FirstPersonCamera::ProcessScroll(double dy) {
}

void FirstPersonCamera::ProcessKeypress(int key, int action) {
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
    case GLFW_KEY_Q:
        key_yaw_left = is_pressed;
        break;
    case GLFW_KEY_E:
        key_yaw_right = is_pressed;
        break;
    case GLFW_KEY_R:
        key_pitch_up = is_pressed;
        break;
    case GLFW_KEY_F:
        key_pitch_down = is_pressed;
        break;
    case GLFW_KEY_SPACE:
        key_jump = is_pressed;
        break;
    case GLFW_KEY_LEFT_SHIFT:
        key_sprint = is_pressed;
        break;
    default:
        return;
    }
}

glm::mat4 FirstPersonCamera::GetViewMatrix() const {
    glm::vec3 view_forward = glm::normalize(look_at - cam_pos);
    glm::vec3 view_right = glm::normalize(glm::cross(view_forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 view_up = glm::normalize(glm::cross(view_right, view_forward));

    float roll_rad = glm::radians(roll);
    glm::vec3 rolled_up =
        static_cast<float>(cos(roll_rad)) * view_up +
        static_cast<float>(sin(roll_rad)) * view_right;

    return glm::lookAt(cam_pos, look_at, rolled_up);
}

void FirstPersonCamera::UpdateCamera(float dt) {
    if (key_yaw_left) {
        yaw -= rot_sensitivity_key;
    }
    if (key_yaw_right) {
        yaw += rot_sensitivity_key;
    }
    if (key_pitch_up) {
        pitch += rot_sensitivity_key;
    }
    if (key_pitch_down) {
        pitch -= rot_sensitivity_key;
    }

    SetBasisVectors();

    glm::vec3 flat_forward = glm::normalize(glm::vec3(forward.x, 0.0f, forward.z));

    float trans_speed = dt * trans_sensitivity;
    if (key_sprint) {
        trans_speed *= sprint_multiple;
    }

    roll_target = 0.0f;

    bool is_moving = false;
    float target_fov = base_fov;

    if (key_left) {
        player_pos -= trans_speed * right;
        is_moving = true;
        roll_target -= max_roll;
    }
    if (key_right) {
        player_pos += trans_speed * right;
        is_moving = true;
        roll_target += max_roll;
    }
    if (key_forward) {
        player_pos += trans_speed * flat_forward;
        is_moving = true;
        target_fov = move_fov;
    }
    if (key_backward) {
        player_pos -= trans_speed * flat_forward;
        is_moving = true;
        target_fov = move_back_fov;
    }

    roll += (roll_target - roll) * std::min(1.0f, dt * roll_lerp_speed);

    if (key_sprint && target_fov == move_fov) {
        target_fov = sprint_fov;
    }

    fov += (target_fov - fov) * std::min(1.0f, dt * fov_lerp_speed);

    if (is_grounded && key_jump) {
        vertical_velocity = jump_velocity;
        is_grounded = false;
    }

    vertical_velocity -= gravity * dt;
    player_pos.y += vertical_velocity * dt;

    if (player_pos.y <= floor_height + height) {
        player_pos.y = floor_height + height;
        vertical_velocity = 0.0f;
        is_grounded = true;
    } else {
        is_grounded = false;
    }

    if (is_moving && is_grounded) {
        bob_time += dt * bob_speed;
        bob_weight = std::min(1.0f, bob_weight + dt * bob_fade_speed);
    } else {
        bob_weight = std::max(0.0f, bob_weight - dt * bob_fade_speed);
    }

    glm::vec3 bob_offset(0.0f);
    bob_offset.y = std::abs(sin(bob_time)) * bob_amount_y * bob_weight;
    bob_offset.x = sin(bob_time * 0.5f) * bob_amount_x * bob_weight;

    cam_pos = player_pos + bob_offset;

    if (!was_grounded && is_grounded) {
        landing_velocity = -landing_dip_amount;
    }

    landing_velocity += (0.0f - landing_offset) * landing_spring_strength * dt;
    landing_velocity -= landing_velocity * landing_recover_speed * dt;
    landing_offset += landing_velocity * dt;

    cam_pos.y += landing_offset;
    was_grounded = is_grounded;

    look_at = cam_pos + forward;
}

void FirstPersonCamera::SetPlayerPos(glm::vec3 new_pos) {
    player_pos = new_pos;
    cam_pos = player_pos;
    look_at = cam_pos + forward;
}

void FirstPersonCamera::UpdatePlayerPos(glm::vec3 delta_pos) {
    player_pos += delta_pos;
    cam_pos = player_pos;
    look_at = cam_pos + forward;
}