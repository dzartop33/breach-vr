#pragma once
// Breach VR - core OpenXR application interface.
//
// This wraps the OpenXR session lifecycle for Meta Quest 3S:
//   - instance + system creation with Quest extensions
//   - session, swapchains and the render loop
//   - hand tracking (XR_EXT_hand_tracking)
//   - scene understanding hooks (XR_FB_scene / spatial entities) used to
//     build the tactical breach map from the player's real apartment.

#include <openxr/openxr.h>
#include <android_native_app_glue.h>

namespace breachvr {

class OpenXrApp {
public:
    // Called once from android_main. Sets up instance/system/session.
    bool Init(android_app* app);

    // Pumps one frame: poll events, locate spaces, render both eyes.
    void RenderFrame();

    // True while the session is running and the app should keep looping.
    bool IsRunning() const { return running_; }

    void Shutdown();

private:
    bool CreateInstance(android_app* app);
    bool InitSystem();
    bool InitSession();
    bool InitHandTracking();
    bool QuerySceneAnchors();   // reads scanned apartment geometry

    XrInstance instance_ = XR_NULL_HANDLE;
    XrSystemId system_id_ = XR_NULL_SYSTEM_ID;
    XrSession session_ = XR_NULL_HANDLE;
    XrSpace app_space_ = XR_NULL_HANDLE;

    bool running_ = false;
    bool hand_tracking_supported_ = false;
    bool scene_supported_ = false;
};

} // namespace breachvr
