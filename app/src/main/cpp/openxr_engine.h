#pragma once
// Breach VR - Core OpenXR Engine Architecture for Meta Quest 3S
// Drives EGL context, OpenXR session, stereo swapchains, render loop, and subsystems.

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

#include "math_types.h"
#include "egl_context.h"
#include "gl_shaders.h"
#include "mesh_builder.h"
#include "audio_engine.h"
#include "input_system.h"
#include "apartment_scene.h"
#include "enemy_ai.h"
#include "weapon_system.h"
#include "gameplay_manager.h"

#include <vector>
#include <memory>

namespace breachvr {

struct SwapchainEye {
    XrSwapchain swapchain = XR_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<GLuint> colorTextures;
    std::vector<GLuint> framebuffers;
    std::vector<GLuint> depthBuffers;
};

class OpenXrEngine {
public:
    OpenXrEngine() = default;
    ~OpenXrEngine() { Shutdown(); }

    bool Init(android_app* app);
    void RenderFrame();
    void Shutdown();

    bool IsRunning() const { return isSessionRunning_ && !exitRequested_; }

private:
    bool CreateInstance(android_app* app);
    bool InitSystem();
    bool InitSession();
    bool CreateReferenceSpaces();
    bool CreateSwapchains();
    void PollEvents();
    void ProcessSessionStateChange(XrSessionState state);

    void RenderEye(int eyeIndex, const XrView& view);

    android_app* app_ = nullptr;
    EglContext eglContext_;

    XrInstance instance_ = XR_NULL_HANDLE;
    XrSystemId systemId_ = XR_NULL_SYSTEM_ID;
    XrSession session_ = XR_NULL_HANDLE;
    XrSpace stageSpace_ = XR_NULL_HANDLE;
    XrSpace localSpace_ = XR_NULL_HANDLE;
    XrSpace viewSpace_ = XR_NULL_HANDLE;

    XrSessionState sessionState_ = XR_SESSION_STATE_UNKNOWN;
    bool isSessionRunning_ = false;
    bool exitRequested_ = false;

    std::vector<XrViewConfigurationView> configViews_;
    std::vector<SwapchainEye> eyeSwapchains_;

    // Subsystems
    InputSystem inputSystem_;
    ApartmentScene apartmentScene_;
    EnemyManager enemyManager_;
    WeaponSystem weaponSystem_;
    GameplayManager gameplayManager_;

    // Timing
    double lastFrameTime_ = 0.0;
    Vec3 playerVirtualOffset_{0, 0, 0};
    float playerYaw_ = 0.0f;
};

} // namespace breachvr
