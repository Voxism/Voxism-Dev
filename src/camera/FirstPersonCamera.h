#pragma once

#include "Camera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class ChunkManager;

class FirstPersonCamera : public Camera {
public:
    struct LandingEvent {
        bool triggered = false;
        glm::vec3 position = glm::vec3(0.0f);
        float fallHeight = 0.0f;
    };

    FirstPersonCamera();
    FirstPersonCamera(glm::vec3 initial_pos, glm::vec3 look_at, float height);
    void SetChunkManager(ChunkManager *chunk_manager);

    void ProcessMouseMovement(double dx, double dy) override;
    void ProcessScroll(double dy) override;
    void ProcessKeypress(int key, int action) override;

    glm::vec3 GetPlayerPos() const { return player_pos; }
    glm::vec3 GetFeetPos() const { return player_pos - glm::vec3(0.0f, height, 0.0f); }

    glm::vec3 GetCameraPos() const {return cam_pos;}

    glm::mat4 GetViewMatrix() const override;

    void SetState(glm::vec3 playerPosition, float yawDegrees, float pitchDegrees, float fovDegrees = 60.0f);
    void SetPlayerPos(glm::vec3 new_pos);
    void UpdatePlayerPos(glm::vec3 delta_pos);

    void UpdateCamera(float dt) override;

    float GetYaw() const {return yaw;}
    float GetPitch() const {return pitch;}
    float GetFOV() const {return fov;}

    glm::vec3 GetForward() const {return forward;}
    glm::vec3 GetRight() const {return right;}
    glm::vec3 GetUp() const {return up;}
    LandingEvent ConsumeLandingEvent();

private:
    struct MoveIntent {
        glm::vec3 delta = glm::vec3(0.0f);
        bool is_moving = false;
        bool moving_forward = false;
        bool moving_backward = false;
    };

    static constexpr float kCollisionEpsilon = 0.0001f;

    float height = 1.5f;
    float floor_height = 0.0f;
    float player_half_width = 0.3f;

    glm::vec3 player_pos;
    glm::vec3 look_at;
    ChunkManager *chunk_manager_ = nullptr;

    float roll = 0.0f;

    const float trans_sensitivity = 4.0f;
    const float sprint_multiple = 2.0f;
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
    const float bob_amount_y = 0.1f;
    const float bob_amount_x = 0.04f;
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
    float airborne_peak_y = 0.0f;
    float landing_dip_per_meter = 1.2f;
    float max_landing_dip_velocity = 20.0f;
    float min_landing_dip_fall_height = 0.75f;
    LandingEvent pending_landing_event_;

    // smooth visual rise for auto-step
    float step_up_visual_offset = 0.0f;
    float step_up_lerp_speed = 8.0f;

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

    bool CollidesAt(glm::vec3 eye_pos) const;
    bool ResolveAxisMove(int axis, float delta);
    bool ProbeGrounded() const;
    bool TryStepUpAxisMove(int axis, float delta);
    void NudgeOutOfCollision();
    bool CanAttemptStepUp(int axis, float delta) const;
    bool FindStepDestination(int axis, float delta, float probe_step, float max_step_height, glm::vec3 &settled_pos) const;
    glm::vec3 SettleStepDown(glm::vec3 stepped_pos, float lift, float probe_step) const;
    float ResolveCollidingStep(int axis, float step) const;
    void ApplyKeyboardLook();
    void UpdateAirbornePeak();
    MoveIntent BuildMoveIntent(float dt);
    void UpdateJumpAndVerticalVelocity(float dt);
    void ApplyMovement(MoveIntent &move, float dt);
    void ApplyChunkMovement(MoveIntent &move, float dt);
    void ApplyFallbackMovement(const MoveIntent &move, float dt);
    void ResolvePostMoveCollision();
    void UpdateBobbing(float dt, bool is_moving);
    glm::vec3 ComputeBobOffset() const;
    void UpdateLandingDip(float dt);
    void UpdateCameraPose(float dt);
    void UpdateFovAndRoll(float dt, bool moving_forward, bool moving_backward);
    void SyncViewToPlayer(bool reset_step_offset);
};
