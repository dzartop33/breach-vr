#include "input_system.h"
#include <android/log.h>
#include <cstring>

#define LOG_TAG "BreachVR_Input"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace breachvr {

bool InputSystem::Init(XrInstance instance, XrSession session) {
    instance_ = instance;
    session_ = session;

    if (!InitActionSet(instance, session)) {
        LOGE("Failed to init ActionSet");
        return false;
    }

    InitHandTrackers(instance, session);

    LOGI("InputSystem initialized successfully");
    return true;
}

bool InputSystem::InitActionSet(XrInstance instance, XrSession session) {
    xrStringToPath(instance, "/user/hand/left", &leftHandPath_);
    xrStringToPath(instance, "/user/hand/right", &rightHandPath_);

    XrActionSetCreateInfo setInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
    std::strcpy(setInfo.actionSetName, "tactical_breach_set");
    std::strcpy(setInfo.localizedActionSetName, "Tactical Breach Actions");
    setInfo.priority = 0;

    XrResult res = xrCreateActionSet(instance, &setInfo, &actionSet_);
    if (XR_FAILED(res)) {
        LOGE("xrCreateActionSet failed: %d", res);
        return false;
    }

    auto CreateAction = [&](const char* name, const char* locName, XrActionType type,
                           XrAction* action, const std::vector<XrPath>& subpaths = {}) -> bool {
        XrActionCreateInfo actInfo{XR_TYPE_ACTION_CREATE_INFO};
        std::strcpy(actInfo.actionName, name);
        std::strcpy(actInfo.localizedActionName, locName);
        actInfo.actionType = type;
        actInfo.countSubactionPaths = (uint32_t)subpaths.size();
        actInfo.subactionPaths = subpaths.data();
        return XR_SUCCEEDED(xrCreateAction(actionSet_, &actInfo, action));
    };

    std::vector<XrPath> handSubpaths = { leftHandPath_, rightHandPath_ };

    CreateAction("aim_pose", "Aim Pose", XR_ACTION_TYPE_POSE_INPUT, &aimPoseAction_, handSubpaths);
    CreateAction("grip_pose", "Grip Pose", XR_ACTION_TYPE_POSE_INPUT, &gripPoseAction_, handSubpaths);
    CreateAction("trigger", "Trigger", XR_ACTION_TYPE_FLOAT_INPUT, &triggerAction_, handSubpaths);
    CreateAction("squeeze", "Squeeze Grip", XR_ACTION_TYPE_FLOAT_INPUT, &gripAction_, handSubpaths);
    CreateAction("move_stick", "Move Stick", XR_ACTION_TYPE_VECTOR2F_INPUT, &moveStickAction_);
    CreateAction("turn_stick", "Turn Stick", XR_ACTION_TYPE_VECTOR2F_INPUT, &turnStickAction_);
    CreateAction("reload", "Reload", XR_ACTION_TYPE_BOOLEAN_INPUT, &reloadAction_, handSubpaths);
    CreateAction("special", "Special/Visor", XR_ACTION_TYPE_BOOLEAN_INPUT, &specialAction_, handSubpaths);
    CreateAction("haptic", "Haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, &hapticAction_, handSubpaths);

    // Bindings for Oculus Touch Controllers (Quest 3S / Quest 3)
    XrPath touchProfilePath;
    xrStringToPath(instance, "/interaction_profiles/oculus/touch_controller", &touchProfilePath);

    std::vector<XrActionSuggestedBinding> bindings;
    auto Bind = [&](XrAction action, const char* pathStr) {
        XrPath path;
        if (XR_SUCCEEDED(xrStringToPath(instance, pathStr, &path))) {
            bindings.push_back({ action, path });
        }
    };

    // Poses
    Bind(aimPoseAction_,  "/user/hand/left/input/aim/pose");
    Bind(aimPoseAction_,  "/user/hand/right/input/aim/pose");
    Bind(gripPoseAction_, "/user/hand/left/input/grip/pose");
    Bind(gripPoseAction_, "/user/hand/right/input/grip/pose");

    // Triggers and Grips
    Bind(triggerAction_, "/user/hand/left/input/trigger/value");
    Bind(triggerAction_, "/user/hand/right/input/trigger/value");
    Bind(gripAction_,    "/user/hand/left/input/squeeze/value");
    Bind(gripAction_,    "/user/hand/right/input/squeeze/value");

    // Thumbsticks
    Bind(moveStickAction_, "/user/hand/left/input/thumbstick");
    Bind(turnStickAction_, "/user/hand/right/input/thumbstick");

    // Buttons: A (right hand) for reload, X (left hand) for visor
    Bind(reloadAction_,  "/user/hand/right/input/a/click");
    Bind(reloadAction_,  "/user/hand/left/input/x/click");
    Bind(specialAction_, "/user/hand/right/input/b/click");
    Bind(specialAction_, "/user/hand/left/input/y/click");

    // Haptics
    Bind(hapticAction_, "/user/hand/left/output/haptic");
    Bind(hapticAction_, "/user/hand/right/output/haptic");

    XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindings.interactionProfile = touchProfilePath;
    suggestedBindings.suggestedBindings = bindings.data();
    suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();

    res = xrSuggestInteractionProfileBindings(instance, &suggestedBindings);
    if (XR_FAILED(res)) {
        LOGE("xrSuggestInteractionProfileBindings failed: %d", res);
        return false;
    }

    // Attach ActionSet to Session
    XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &actionSet_;

    res = xrAttachSessionActionSets(session, &attachInfo);
    if (XR_FAILED(res)) {
        LOGE("xrAttachSessionActionSets failed: %d", res);
        return false;
    }

    // Create Action Spaces for Aim and Grip Poses
    auto CreateActionSpace = [&](XrAction action, XrPath subpath, XrSpace* space) {
        XrActionSpaceCreateInfo createInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        createInfo.action = action;
        createInfo.subactionPath = subpath;
        createInfo.poseInActionSpace.orientation.w = 1.0f;
        xrCreateActionSpace(session, &createInfo, space);
    };

    CreateActionSpace(aimPoseAction_,  leftHandPath_,  &leftAimSpace_);
    CreateActionSpace(aimPoseAction_,  rightHandPath_, &rightAimSpace_);
    CreateActionSpace(gripPoseAction_, leftHandPath_,  &leftGripSpace_);
    CreateActionSpace(gripPoseAction_, rightHandPath_, &rightGripSpace_);

    return true;
}

bool InputSystem::InitHandTrackers(XrInstance instance, XrSession session) {
    xrGetInstanceProcAddr(instance, "xrCreateHandTrackerEXT",  (PFN_xrVoidFunction*)&xrCreateHandTrackerEXT_);
    xrGetInstanceProcAddr(instance, "xrDestroyHandTrackerEXT", (PFN_xrVoidFunction*)&xrDestroyHandTrackerEXT_);
    xrGetInstanceProcAddr(instance, "xrLocateHandJointsEXT",   (PFN_xrVoidFunction*)&xrLocateHandJointsEXT_);

    if (!xrCreateHandTrackerEXT_ || !xrDestroyHandTrackerEXT_ || !xrLocateHandJointsEXT_) {
        LOGI("Hand tracking extension functions not present; using controllers only");
        handTrackingSupported_ = false;
        return false;
    }

    XrHandTrackerCreateInfoEXT leftInfo{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
    leftInfo.hand = XR_HAND_LEFT_EXT;
    leftInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;

    XrResult r1 = xrCreateHandTrackerEXT_(session, &leftInfo, &leftHandTracker_);

    XrHandTrackerCreateInfoEXT rightInfo{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
    rightInfo.hand = XR_HAND_RIGHT_EXT;
    rightInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;

    XrResult r2 = xrCreateHandTrackerEXT_(session, &rightInfo, &rightHandTracker_);

    handTrackingSupported_ = (XR_SUCCEEDED(r1) && XR_SUCCEEDED(r2));
    LOGI("Hand trackers initialized: %s", handTrackingSupported_ ? "success" : "failed");
    return handTrackingSupported_;
}

void InputSystem::Update(XrSession session, XrSpace baseSpace, XrTime predictedDisplayTime) {
    // 1. Sync Actions
    XrActiveActionSet activeSet{ actionSet_, XR_NULL_PATH };
    XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeSet;

    xrSyncActions(session, &syncInfo);

    // 2. Read Controller state
    UpdateControllers(session, baseSpace, predictedDisplayTime);

    // 3. Read Hand Tracking if enabled
    if (handTrackingSupported_) {
        UpdateHandTracking(session, baseSpace, predictedDisplayTime);
    }
}

void InputSystem::UpdateControllers(XrSession session, XrSpace baseSpace, XrTime predictedDisplayTime) {
    // Helper to query float action
    auto GetFloat = [&](XrAction action, XrPath subpath) -> float {
        XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        getInfo.subactionPath = subpath;
        if (XR_SUCCEEDED(xrGetActionStateFloat(session, &getInfo, &state)) && state.isActive) {
            return state.currentState;
        }
        return 0.0f;
    };

    // Helper to query bool action
    auto GetBool = [&](XrAction action, XrPath subpath) -> bool {
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        getInfo.subactionPath = subpath;
        if (XR_SUCCEEDED(xrGetActionStateBoolean(session, &getInfo, &state)) && state.isActive) {
            return state.currentState == XR_TRUE;
        }
        return false;
    };

    // Left Controller
    leftHand_.triggerValue = GetFloat(triggerAction_, leftHandPath_);
    leftHand_.isTriggerDown = leftHand_.triggerValue > 0.4f;
    leftHand_.isTriggerClicked = leftHand_.isTriggerDown && !prevLeftTrigger_;
    prevLeftTrigger_ = leftHand_.isTriggerDown;

    leftHand_.gripValue = GetFloat(gripAction_, leftHandPath_);
    leftHand_.isGripDown = leftHand_.gripValue > 0.4f;

    // Right Controller
    rightHand_.triggerValue = GetFloat(triggerAction_, rightHandPath_);
    rightHand_.isTriggerDown = rightHand_.triggerValue > 0.4f;
    rightHand_.isTriggerClicked = rightHand_.isTriggerDown && !prevRightTrigger_;
    prevRightTrigger_ = rightHand_.isTriggerDown;

    rightHand_.gripValue = GetFloat(gripAction_, rightHandPath_);
    rightHand_.isGripDown = rightHand_.gripValue > 0.4f;

    // Locomotion & Turning
    XrActionStateVector2f moveState{XR_TYPE_ACTION_STATE_VECTOR2F};
    XrActionStateGetInfo getMoveInfo{XR_TYPE_ACTION_STATE_GET_INFO};
    getMoveInfo.action = moveStickAction_;
    if (XR_SUCCEEDED(xrGetActionStateVector2f(session, &getMoveInfo, &moveState)) && moveState.isActive) {
        locomotionInput_ = Vec2(moveState.currentState.x, moveState.currentState.y);
    } else {
        locomotionInput_ = Vec2(0, 0);
    }

    XrActionStateVector2f turnState{XR_TYPE_ACTION_STATE_VECTOR2F};
    XrActionStateGetInfo getTurnInfo{XR_TYPE_ACTION_STATE_GET_INFO};
    getTurnInfo.action = turnStickAction_;
    if (XR_SUCCEEDED(xrGetActionStateVector2f(session, &getTurnInfo, &turnState)) && turnState.isActive) {
        snapTurnInput_ = turnState.currentState.x;
    } else {
        snapTurnInput_ = 0.0f;
    }

    // Buttons
    bool reloadPressed = GetBool(reloadAction_, rightHandPath_) || GetBool(reloadAction_, leftHandPath_);
    rightHand_.isReloadClicked = reloadPressed && !prevReload_;
    prevReload_ = reloadPressed;

    bool specialPressed = GetBool(specialAction_, rightHandPath_) || GetBool(specialAction_, leftHandPath_);
    rightHand_.isSpecialClicked = specialPressed && !prevSpecial_;
    prevSpecial_ = specialPressed;

    // Locate Controller Spaces
    auto LocatePose = [&](XrSpace space, Vec3& posOut, Quat& rotOut) -> bool {
        XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
        if (XR_SUCCEEDED(xrLocateSpace(space, baseSpace, predictedDisplayTime, &loc))) {
            if ((loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) &&
                (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
                posOut = Vec3(loc.pose.position.x, loc.pose.position.y, loc.pose.position.z);
                rotOut = Quat(loc.pose.orientation.x, loc.pose.orientation.y, loc.pose.orientation.z, loc.pose.orientation.w);
                return true;
            }
        }
        return false;
    };

    if (!leftHand_.isHandTrackingActive) {
        leftHand_.isTracked = LocatePose(leftAimSpace_, leftHand_.aimPosition, leftHand_.aimOrientation);
        LocatePose(leftGripSpace_, leftHand_.gripPosition, leftHand_.gripOrientation);
    }

    if (!rightHand_.isHandTrackingActive) {
        rightHand_.isTracked = LocatePose(rightAimSpace_, rightHand_.aimPosition, rightHand_.aimOrientation);
        LocatePose(rightGripSpace_, rightHand_.gripPosition, rightHand_.gripOrientation);
    }
}

void InputSystem::UpdateHandTracking(XrSession session, XrSpace baseSpace, XrTime predictedDisplayTime) {
    auto UpdateOneHand = [&](XrHandTrackerEXT tracker, HandState& state, bool isRight) {
        XrHandJointLocationEXT jointLocations[XR_HAND_JOINT_COUNT_EXT];
        XrHandJointLocationsEXT locations{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
        locations.jointCount = XR_HAND_JOINT_COUNT_EXT;
        locations.jointLocations = jointLocations;

        XrHandJointsLocateInfoEXT locateInfo{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
        locateInfo.baseSpace = baseSpace;
        locateInfo.time = predictedDisplayTime;

        XrResult res = xrLocateHandJointsEXT_(tracker, &locateInfo, &locations);
        if (XR_SUCCEEDED(res) && locations.isActive == XR_TRUE) {
            state.isHandTrackingActive = true;
            state.isTracked = true;

            for (uint32_t j = 0; j < XR_HAND_JOINT_COUNT_EXT; ++j) {
                const auto& loc = jointLocations[j];
                bool valid = (loc.locationFlags & XR_HAND_JOINT_LOCATION_POSITION_VALID_BIT);
                state.joints[j].isValid = valid;
                if (valid) {
                    state.joints[j].position = Vec3(loc.pose.position.x, loc.pose.position.y, loc.pose.position.z);
                    state.joints[j].orientation = Quat(loc.pose.orientation.x, loc.pose.orientation.y,
                                                       loc.pose.orientation.z, loc.pose.orientation.w);
                    state.joints[j].radius = loc.radius;
                }
            }

            // Aim pose = ray along wrist to index knuckle
            const auto& wrist = state.joints[XR_HAND_JOINT_WRIST_EXT];
            const auto& indexKnuckle = state.joints[XR_HAND_JOINT_INDEX_INTERMEDIATE_EXT];
            const auto& thumbTip = state.joints[XR_HAND_JOINT_THUMB_TIP_EXT];
            const auto& indexTip = state.joints[XR_HAND_JOINT_INDEX_TIP_EXT];
            const auto& middleTip = state.joints[XR_HAND_JOINT_MIDDLE_TIP_EXT];

            if (wrist.isValid && indexKnuckle.isValid) {
                state.aimPosition = indexKnuckle.position;
                Vec3 dir = (indexKnuckle.position - wrist.position).Normalized();
                state.aimOrientation = wrist.orientation;
                state.gripPosition = wrist.position;
                state.gripOrientation = wrist.orientation;
            }

            // Pinch Detection: distance(ThumbTip, IndexTip) < 0.025m (2.5 cm)
            if (thumbTip.isValid && indexTip.isValid) {
                float pinchDist = Vec3::Distance(thumbTip.position, indexTip.position);
                state.isPinching = (pinchDist < 0.025f);
                if (isRight) {
                    state.isPinchClicked = state.isPinching && !prevRightPinch_;
                    prevRightPinch_ = state.isPinching;
                } else {
                    state.isPinchClicked = state.isPinching && !prevLeftPinch_;
                    prevLeftPinch_ = state.isPinching;
                }

                // In hand tracking, pinch acts as trigger!
                state.triggerValue = state.isPinching ? 1.0f : 0.0f;
                state.isTriggerDown = state.isPinching;
                state.isTriggerClicked = state.isPinchClicked;
            }

            // Middle Finger Pinch: Reload gesture
            if (thumbTip.isValid && middleTip.isValid) {
                float middlePinch = Vec3::Distance(thumbTip.position, middleTip.position);
                if (middlePinch < 0.025f) {
                    state.isReloadClicked = true;
                }
            }
        } else {
            state.isHandTrackingActive = false;
        }
    };

    UpdateOneHand(leftHandTracker_,  leftHand_,  false);
    UpdateOneHand(rightHandTracker_, rightHand_, true);
}

void InputSystem::TriggerHaptic(XrSession session, bool isRightHand, float durationSec, float frequency, float amplitude) {
    XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
    vibration.duration = static_cast<XrDuration>(durationSec * 1e9f);
    vibration.frequency = frequency;
    vibration.amplitude = amplitude;

    XrHapticActionInfo hapticInfo{XR_TYPE_HAPTIC_ACTION_INFO};
    hapticInfo.action = hapticAction_;
    hapticInfo.subactionPath = isRightHand ? rightHandPath_ : leftHandPath_;

    xrApplyHapticFeedback(session, &hapticInfo, (XrHapticBaseHeader*)&vibration);
}

void InputSystem::Teardown(XrSession session) {
    if (handTrackingSupported_) {
        if (leftHandTracker_ != XR_NULL_HANDLE && xrDestroyHandTrackerEXT_) {
            xrDestroyHandTrackerEXT_(leftHandTracker_);
            leftHandTracker_ = XR_NULL_HANDLE;
        }
        if (rightHandTracker_ != XR_NULL_HANDLE && xrDestroyHandTrackerEXT_) {
            xrDestroyHandTrackerEXT_(rightHandTracker_);
            rightHandTracker_ = XR_NULL_HANDLE;
        }
    }

    if (leftAimSpace_ != XR_NULL_HANDLE)  { xrDestroySpace(leftAimSpace_);  leftAimSpace_ = XR_NULL_HANDLE; }
    if (rightAimSpace_ != XR_NULL_HANDLE) { xrDestroySpace(rightAimSpace_); rightAimSpace_ = XR_NULL_HANDLE; }
    if (leftGripSpace_ != XR_NULL_HANDLE) { xrDestroySpace(leftGripSpace_); leftGripSpace_ = XR_NULL_HANDLE; }
    if (rightGripSpace_ != XR_NULL_HANDLE){ xrDestroySpace(rightGripSpace_);rightGripSpace_ = XR_NULL_HANDLE; }

    if (actionSet_ != XR_NULL_HANDLE) {
        xrDestroyActionSet(actionSet_);
        actionSet_ = XR_NULL_HANDLE;
    }
}

} // namespace breachvr
