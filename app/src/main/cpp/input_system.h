#pragma once
// Breach VR - Input System: Hand Tracking (XR_EXT_hand_tracking) & Quest Controllers

#ifndef XR_USE_PLATFORM_ANDROID
#define XR_USE_PLATFORM_ANDROID
#endif
#ifndef XR_USE_GRAPHICS_API_OPENGL_ES
#define XR_USE_GRAPHICS_API_OPENGL_ES
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "math_types.h"
#include <array>

namespace breachvr {

struct HandJoint {
    Vec3 position;
    Quat orientation;
    float radius = 0.01f;
    bool isValid = false;
};

struct HandState {
    bool isHandTrackingActive = false;
    bool isTracked = false;

    // Poses
    Vec3 aimPosition{0, 1.2f, -0.3f};
    Quat aimOrientation = Quat::Identity();
    Vec3 gripPosition{0, 1.2f, -0.3f};
    Quat gripOrientation = Quat::Identity();

    // Gestures / Controller Inputs
    float triggerValue = 0.0f;
    bool isTriggerDown = false;
    bool isTriggerClicked = false; // single frame edge

    float gripValue = 0.0f;
    bool isGripDown = false;

    bool isPinching = false;
    bool isPinchClicked = false;

    bool isReloadClicked = false;
    bool isSpecialClicked = false; // Visor / Switch weapon

    // 26 Hand Joints (when hand tracking is active)
    std::array<HandJoint, XR_HAND_JOINT_COUNT_EXT> joints{};
};

class InputSystem {
public:
    InputSystem() = default;

    bool Init(XrInstance instance, XrSession session);
    void Update(XrSession session, XrSpace baseSpace, XrTime predictedDisplayTime);
    void Teardown(XrSession session);

    void TriggerHaptic(XrSession session, bool isRightHand, float durationSec = 0.1f, float frequency = 200.0f, float amplitude = 0.8f);

    const HandState& GetLeftHand()  const { return leftHand_; }
    const HandState& GetRightHand() const { return rightHand_; }

    Vec2 GetLocomotionInput() const { return locomotionInput_; }
    float GetSnapTurnInput()   const { return snapTurnInput_; }

private:
    bool InitActionSet(XrInstance instance, XrSession session);
    bool InitHandTrackers(XrInstance instance, XrSession session);

    void UpdateControllers(XrSession session, XrSpace baseSpace, XrTime predictedDisplayTime);
    void UpdateHandTracking(XrSession session, XrSpace baseSpace, XrTime predictedDisplayTime);

    XrInstance instance_ = XR_NULL_HANDLE;
    XrSession session_ = XR_NULL_HANDLE;

    // Hand tracking extension handles & function pointers
    PFN_xrCreateHandTrackerEXT xrCreateHandTrackerEXT_ = nullptr;
    PFN_xrDestroyHandTrackerEXT xrDestroyHandTrackerEXT_ = nullptr;
    PFN_xrLocateHandJointsEXT xrLocateHandJointsEXT_ = nullptr;

    XrHandTrackerEXT leftHandTracker_ = XR_NULL_HANDLE;
    XrHandTrackerEXT rightHandTracker_ = XR_NULL_HANDLE;
    bool handTrackingSupported_ = false;

    // Action Sets & Actions
    XrActionSet actionSet_ = XR_NULL_HANDLE;
    XrAction aimPoseAction_ = XR_NULL_HANDLE;
    XrAction gripPoseAction_ = XR_NULL_HANDLE;
    XrAction triggerAction_ = XR_NULL_HANDLE;
    XrAction gripAction_ = XR_NULL_HANDLE;
    XrAction moveStickAction_ = XR_NULL_HANDLE;
    XrAction turnStickAction_ = XR_NULL_HANDLE;
    XrAction reloadAction_ = XR_NULL_HANDLE;
    XrAction specialAction_ = XR_NULL_HANDLE;
    XrAction hapticAction_ = XR_NULL_HANDLE;

    XrSpace leftAimSpace_ = XR_NULL_HANDLE;
    XrSpace rightAimSpace_ = XR_NULL_HANDLE;
    XrSpace leftGripSpace_ = XR_NULL_HANDLE;
    XrSpace rightGripSpace_ = XR_NULL_HANDLE;

    XrPath leftHandPath_ = XR_NULL_PATH;
    XrPath rightHandPath_ = XR_NULL_PATH;

    HandState leftHand_;
    HandState rightHand_;
    Vec2 locomotionInput_{0.0f, 0.0f};
    float snapTurnInput_ = 0.0f;

    bool prevLeftTrigger_ = false;
    bool prevRightTrigger_ = false;
    bool prevLeftPinch_ = false;
    bool prevRightPinch_ = false;
    bool prevReload_ = false;
    bool prevSpecial_ = false;
};

} // namespace breachvr
