#include "FirstPersonCamera.h"

#include "../world/ChunkManager.h"

#include <algorithm>
#include <cmath>
#include <GLFW/glfw3.h>

FirstPersonCamera::FirstPersonCamera() {
    player_pos = glm::vec3(0.0f, 1.0f, 0.0f);
    look_at = glm::vec3(0.0f, 1.0f, -1.0f);
    airborne_peak_y = player_pos.y;

    yaw = -90.0f;
    pitch = 0.0f;
    fov = base_fov;

    SetBasisVectors();
    SyncViewToPlayer(false);
}

FirstPersonCamera::FirstPersonCamera(glm::vec3 initial_pos, glm::vec3 look_at_, float height_) {
    player_pos = initial_pos;
    look_at = look_at_;
    height = height_;
    airborne_peak_y = player_pos.y;
    cam_pos = player_pos;

    glm::vec3 dir = glm::normalize(look_at - cam_pos);

    yaw = glm::degrees(atan2f(dir.z, dir.x));
    pitch = glm::degrees(asinf(dir.y));
    fov = base_fov;

    SetBasisVectors();
    SyncViewToPlayer(false);
}

void FirstPersonCamera::SetChunkManager(ChunkManager *chunk_manager)
{
    chunk_manager_ = chunk_manager;
    NudgeOutOfCollision();
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

void FirstPersonCamera::ApplyKeyboardLook()
{
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
}

void FirstPersonCamera::UpdateAirbornePeak()
{
    if (is_grounded) {
        airborne_peak_y = player_pos.y;
    } else {
        airborne_peak_y = std::max(airborne_peak_y, player_pos.y);
    }
}

FirstPersonCamera::MoveIntent FirstPersonCamera::BuildMoveIntent(float dt)
{
    MoveIntent move;
    const glm::vec3 flat_forward = glm::normalize(glm::vec3(forward.x, 0.0f, forward.z));

    float trans_speed = dt * trans_sensitivity;
    if (key_sprint) {
        trans_speed *= sprint_multiple;
    }

    roll_target = 0.0f;

    if (key_left) {
        move.delta -= trans_speed * right;
        move.is_moving = true;
        roll_target -= max_roll;
    }
    if (key_right) {
        move.delta += trans_speed * right;
        move.is_moving = true;
        roll_target += max_roll;
    }
    if (key_forward) {
        move.delta += trans_speed * flat_forward;
        move.is_moving = true;
        move.moving_forward = true;
    }
    if (key_backward) {
        move.delta -= trans_speed * flat_forward;
        move.is_moving = true;
        move.moving_backward = true;
    }

    return move;
}

void FirstPersonCamera::UpdateJumpAndVerticalVelocity(float dt)
{
    if (is_grounded && key_jump) {
        vertical_velocity = jump_velocity;
        is_grounded = false;
    }

    vertical_velocity -= gravity * dt;
}

void FirstPersonCamera::ApplyMovement(MoveIntent &move, float dt)
{
    if (chunk_manager_) {
        ApplyChunkMovement(move, dt);
        return;
    }

    ApplyFallbackMovement(move, dt);
}

void FirstPersonCamera::ApplyChunkMovement(MoveIntent &move, float dt)
{
    move.delta.y = vertical_velocity * dt;

    // Vertical motion is resolved first so ground hits can zero the fall speed
    // before lateral movement attempts to auto-step.
    is_grounded = false;
    const bool hit_y = ResolveAxisMove(1, move.delta.y);
    if (hit_y) {
        if (vertical_velocity <= 0.0f) {
            is_grounded = true;
        }
        vertical_velocity = 0.0f;
    }

    ResolveAxisMove(0, move.delta.x);
    ResolveAxisMove(2, move.delta.z);

    if (!is_grounded && vertical_velocity <= 0.0f) {
        is_grounded = ProbeGrounded();
    }
}

void FirstPersonCamera::ApplyFallbackMovement(const MoveIntent &move, float dt)
{
    player_pos += move.delta;
    player_pos.y += vertical_velocity * dt;

    if (player_pos.y <= floor_height + height) {
        player_pos.y = floor_height + height;
        vertical_velocity = 0.0f;
        is_grounded = true;
    } else {
        is_grounded = false;
    }
}

void FirstPersonCamera::ResolvePostMoveCollision()
{
    if (!chunk_manager_ || !CollidesAt(player_pos)) {
        return;
    }

    NudgeOutOfCollision();
    is_grounded = ProbeGrounded();
    if (is_grounded && vertical_velocity < 0.0f) {
        vertical_velocity = 0.0f;
    }
}

void FirstPersonCamera::UpdateBobbing(float dt, bool is_moving)
{
    if (is_moving && is_grounded) {
        bob_time += dt * bob_speed;
        bob_weight = std::min(1.0f, bob_weight + dt * bob_fade_speed);
    } else {
        bob_weight = std::max(0.0f, bob_weight - dt * bob_fade_speed);
    }
}

glm::vec3 FirstPersonCamera::ComputeBobOffset() const
{
    // Bobbing is visual-only; collision and physics stay anchored to player_pos.
    glm::vec3 bob_offset(0.0f);
    bob_offset.y = std::abs(sin(bob_time)) * bob_amount_y * bob_weight;
    bob_offset.x = sin(bob_time * 0.5f) * bob_amount_x * bob_weight;
    return bob_offset;
}

void FirstPersonCamera::UpdateLandingDip(float dt)
{
    if (!was_grounded && is_grounded) {
        const float fall_height = std::max(0.0f, airborne_peak_y - player_pos.y);
        if (fall_height >= min_landing_dip_fall_height) {
            const float landing_impulse = std::min(
                max_landing_dip_velocity,
                landing_dip_amount + fall_height * landing_dip_per_meter);
            landing_velocity = -landing_impulse;
        }
        airborne_peak_y = player_pos.y;
    }

    landing_velocity += (0.0f - landing_offset) * landing_spring_strength * dt;
    landing_velocity -= landing_velocity * landing_recover_speed * dt;
    landing_offset += landing_velocity * dt;
}

void FirstPersonCamera::UpdateCameraPose(float dt)
{
    step_up_visual_offset += (0.0f - step_up_visual_offset) * std::min(1.0f, dt * step_up_lerp_speed);

    cam_pos = player_pos + ComputeBobOffset();
    cam_pos.y += step_up_visual_offset;

    UpdateLandingDip(dt);
    cam_pos.y += landing_offset;
    was_grounded = is_grounded;

    look_at = cam_pos + forward;
}

void FirstPersonCamera::UpdateFovAndRoll(float dt, bool moving_forward, bool moving_backward)
{
    roll += (roll_target - roll) * std::min(1.0f, dt * roll_lerp_speed);

    float target_fov = base_fov;
    if (moving_forward) {
        target_fov = key_sprint ? sprint_fov : move_fov;
    } else if (moving_backward) {
        target_fov = move_back_fov;
    }

    fov += (target_fov - fov) * std::min(1.0f, dt * fov_lerp_speed);
}

void FirstPersonCamera::SyncViewToPlayer(bool reset_step_offset)
{
    if (reset_step_offset) {
        step_up_visual_offset = 0.0f;
    }

    airborne_peak_y = player_pos.y;
    cam_pos = player_pos;
    look_at = cam_pos + forward;
}

void FirstPersonCamera::UpdateCamera(float dt) {
    if (chunk_manager_ && CollidesAt(player_pos)) {
        NudgeOutOfCollision();
    }

    UpdateAirbornePeak();
    ApplyKeyboardLook();
    SetBasisVectors();

    MoveIntent move = BuildMoveIntent(dt);
    UpdateFovAndRoll(dt, move.moving_forward, move.moving_backward);
    UpdateJumpAndVerticalVelocity(dt);
    ApplyMovement(move, dt);
    ResolvePostMoveCollision();
    UpdateBobbing(dt, move.is_moving);
    UpdateCameraPose(dt);
}

void FirstPersonCamera::SetPlayerPos(glm::vec3 new_pos) {
    player_pos = new_pos;
    NudgeOutOfCollision();
    SyncViewToPlayer(true);
}

void FirstPersonCamera::UpdatePlayerPos(glm::vec3 delta_pos) {
    player_pos += delta_pos;
    NudgeOutOfCollision();
    SyncViewToPlayer(true);
}

bool FirstPersonCamera::CollidesAt(glm::vec3 eye_pos) const
{
    if (!chunk_manager_) {
        return false;
    }

    const glm::vec3 aabb_min(
        eye_pos.x - player_half_width,
        eye_pos.y - height,
        eye_pos.z - player_half_width);
    const glm::vec3 aabb_max(
        eye_pos.x + player_half_width,
        eye_pos.y - kCollisionEpsilon,
        eye_pos.z + player_half_width);

    const glm::ivec3 min_voxel = chunk_manager_->worldToVoxel(aabb_min);
    const glm::ivec3 max_voxel = chunk_manager_->worldToVoxel(aabb_max);
    return chunk_manager_->isAnyVoxelOccupied(min_voxel, max_voxel);
}

bool FirstPersonCamera::ResolveAxisMove(int axis, float delta)
{
    if (!chunk_manager_ || std::abs(delta) <= kCollisionEpsilon) {
        return false;
    }

    const float voxel_step = std::max(0.01f, chunk_manager_->voxSizeMeters * 0.5f);
    float remaining = delta;
    bool collided = false;
    bool attemptedStepUp = false;

    while (std::abs(remaining) > kCollisionEpsilon) {
        const float step = glm::clamp(remaining, -voxel_step, voxel_step);
        glm::vec3 candidate = player_pos;
        candidate[axis] += step;

        if (CollidesAt(candidate)) {
            if (!attemptedStepUp && (axis == 0 || axis == 2)) {
                attemptedStepUp = true;
                if (TryStepUpAxisMove(axis, step)) {
                    remaining -= step;
                    continue;
                }
            }
            // player_pos[axis] += ResolveCollidingStep(axis, step);
            collided = true;
            break;
        }

        player_pos = candidate;
        remaining -= step;
    }

    return collided;
}

float FirstPersonCamera::ResolveCollidingStep(int axis, float step) const
{
    float low = 0.0f;
    float high = step;

    for (int i = 0; i < 6; i++) {
        const float mid = 0.5f * (low + high);
        glm::vec3 probe = player_pos;
        probe[axis] += mid;
        if (CollidesAt(probe)) {
            high = mid;
        } else {
            low = mid;
        }
    }

    return low;
}

bool FirstPersonCamera::CanAttemptStepUp(int axis, float delta) const
{
    if (!chunk_manager_ || (axis != 0 && axis != 2)) {
        return false;
    }
    if (delta == 0.0f || vertical_velocity > 0.0f) {
        return false;
    }

    return is_grounded || ProbeGrounded();
}

glm::vec3 FirstPersonCamera::SettleStepDown(glm::vec3 stepped_pos, float lift, float probe_step) const
{
    glm::vec3 settled = stepped_pos;
    float drop = 0.0f;

    // Drop back down to the top of the stepped surface after clearing the ledge.
    while (drop + probe_step <= lift + kCollisionEpsilon) {
        glm::vec3 probe = settled;
        probe.y -= probe_step;
        if (CollidesAt(probe)) {
            break;
        }
        settled = probe;
        drop += probe_step;
    }

    return settled;
}

bool FirstPersonCamera::FindStepDestination(
    int axis,
    float delta,
    float probe_step,
    float max_step_height,
    glm::vec3 &settled_pos) const
{
    for (float lift = probe_step; lift <= max_step_height + kCollisionEpsilon; lift += probe_step) {
        glm::vec3 lifted = player_pos;
        lifted.y += lift;
        if (CollidesAt(lifted)) {
            continue;
        }

        glm::vec3 stepped = lifted;
        stepped[axis] += delta;
        if (CollidesAt(stepped)) {
            continue;
        }

        settled_pos = SettleStepDown(stepped, lift, probe_step);
        return true;
    }

    return false;
}

bool FirstPersonCamera::TryStepUpAxisMove(int axis, float delta)
{
    if (!CanAttemptStepUp(axis, delta)) {
        return false;
    }

    const float max_step_height = chunk_manager_->voxSizeMeters * 2.0f;
    const float probe_step = std::max(0.01f, chunk_manager_->voxSizeMeters * 0.25f);
    glm::vec3 settled_pos(0.0f);
    if (!FindStepDestination(axis, delta, probe_step, max_step_height, settled_pos)) {
        return false;
    }

    const float previous_visible_y = player_pos.y + step_up_visual_offset;
    player_pos = settled_pos;
    if (player_pos.y > previous_visible_y) {
        step_up_visual_offset = previous_visible_y - player_pos.y;
    }
    is_grounded = true;
    if (vertical_velocity < 0.0f) {
        vertical_velocity = 0.0f;
    }
    return true;
}

bool FirstPersonCamera::ProbeGrounded() const
{
    if (!chunk_manager_) {
        return player_pos.y <= floor_height + height + 0.001f;
    }

    const float probe_distance = std::max(0.01f, chunk_manager_->voxSizeMeters * 0.5f);
    glm::vec3 probe_pos = player_pos;
    probe_pos.y -= probe_distance;
    return CollidesAt(probe_pos);
}

void FirstPersonCamera::NudgeOutOfCollision()
{
    if (!chunk_manager_) {
        return;
    }

    const float lift = std::max(0.05f, chunk_manager_->voxSizeMeters);
    for (int i = 0; i < 256 && CollidesAt(player_pos); i++) {
        player_pos.y += lift;
    }
    cam_pos = player_pos;
    look_at = cam_pos + forward;
}
