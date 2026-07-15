#pragma once
// Breach VR - core OpenXR application interface.

#ifndef XR_USE_PLATFORM_ANDROID
#define XR_USE_PLATFORM_ANDROID
#endif
#ifndef XR_USE_GRAPHICS_API_OPENGL_ES
#define XR_USE_GRAPHICS_API_OPENGL_ES
#endif

#include <jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <android_native_app_glue.h>

namespace breachvr {

class OpenXrApp {
public:
    bool Init(android_app* app);
    void RenderFrame();
    bool IsRunning() const { return running_; }
    void Shutdown();

private:
    bool CreateInstance(android_app* app);
    bool InitSystem();
    bool InitSession();
    bool InitHandTracking();
    bool QuerySceneAnchors();

    XrInstance instance_ = XR_NULL_HANDLE;
    XrSystemId system_id_ = XR_NULL_SYSTEM_ID;
    XrSession session_ = XR_NULL_HANDLE;
    XrSpace app_space_ = XR_NULL_HANDLE;

    bool running_ = false;
    bool hand_tracking_supported_ = false;
    bool scene_supported_ = false;
};

} // namespace breachvr
