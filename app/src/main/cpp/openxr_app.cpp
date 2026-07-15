#include "openxr_app.h"

#include <android/log.h>
#include <vector>
#include <cstring>

#define LOG_TAG "BreachVR"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace breachvr {

bool OpenXrApp::Init(android_app* app) {
    if (!CreateInstance(app)) return false;
    if (!InitSystem())        return false;
    if (!InitSession())       return false;
    InitHandTracking();       // optional - controllers still work without it
    QuerySceneAnchors();      // optional - falls back to a test room
    running_ = true;
    LOGI("Breach VR OpenXR app initialized");
    return true;
}

bool OpenXrApp::CreateInstance(android_app* app) {
    // Extensions we need on Quest 3S.
    std::vector<const char*> extensions = {
        XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
        XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
        XR_EXT_HAND_TRACKING_EXTENSION_NAME,
        // Scene / spatial-entity extensions are checked at runtime; they are
        // requested here so we can build the map from the scanned apartment.
        "XR_FB_scene",
        "XR_FB_spatial_entity",
        "XR_FB_passthrough",
    };

    // Android-specific instance creation info (loader initialization).
    PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR = nullptr;
    xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                          (PFN_xrVoidFunction*)(&xrInitializeLoaderKHR));
    if (xrInitializeLoaderKHR) {
        XrLoaderInitInfoAndroidKHR loaderInfo{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        loaderInfo.applicationVM = app->activity->vm;
        loaderInfo.applicationContext = app->activity->clazz;
        xrInitializeLoaderKHR((const XrLoaderInitInfoBaseHeaderKHR*)&loaderInfo);
    }

    XrInstanceCreateInfoAndroidKHR androidInfo{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
    androidInfo.applicationVM = app->activity->vm;
    androidInfo.applicationActivity = app->activity->clazz;

    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    createInfo.next = &androidInfo;
    createInfo.enabledExtensionCount = (uint32_t)extensions.size();
    createInfo.enabledExtensionNames = extensions.data();
    std::strcpy(createInfo.applicationInfo.applicationName, "Breach VR");
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

    XrResult r = xrCreateInstance(&createInfo, &instance_);
    if (XR_FAILED(r)) {
        LOGE("xrCreateInstance failed: %d", r);
        return false;
    }
    return true;
}

bool OpenXrApp::InitSystem() {
    XrSystemGetInfo info{XR_TYPE_SYSTEM_GET_INFO};
    info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrResult r = xrGetSystem(instance_, &info, &system_id_);
    if (XR_FAILED(r)) {
        LOGE("xrGetSystem failed: %d", r);
        return false;
    }
    return true;
}

bool OpenXrApp::InitSession() {
    // NOTE: full session creation requires a valid OpenGL ES graphics binding
    // (XrGraphicsBindingOpenGLESAndroidKHR) built from an EGL context. That EGL
    // setup lives in main.cpp's render bootstrap; here we mark the milestone.
    LOGI("Session/swapchain setup handled by the render bootstrap");
    return true;
}

bool OpenXrApp::InitHandTracking() {
    // Query whether XR_EXT_hand_tracking is usable, then create left/right
    // hand trackers. Pinch = primary action in hand mode.
    XrSystemHandTrackingPropertiesEXT handProps{XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT};
    XrSystemProperties sysProps{XR_TYPE_SYSTEM_PROPERTIES};
    sysProps.next = &handProps;
    if (XR_SUCCEEDED(xrGetSystemProperties(instance_, system_id_, &sysProps))) {
        hand_tracking_supported_ = handProps.supportsHandTracking == XR_TRUE;
    }
    LOGI("Hand tracking supported: %s", hand_tracking_supported_ ? "yes" : "no");
    return hand_tracking_supported_;
}

bool OpenXrApp::QuerySceneAnchors() {
    // On device this enumerates scene anchors (walls, floor, furniture) from
    // the Quest room setup and hands them to the gameplay map builder so the
    // breach map matches the player's real apartment. Stubbed until session
    // graphics are wired up.
    LOGI("Scene anchor query stub - map will use scanned apartment when available");
    scene_supported_ = false;
    return scene_supported_;
}

void OpenXrApp::RenderFrame() {
    // Real frame loop: xrPollEvent -> xrWaitFrame -> xrBeginFrame ->
    // xrLocateViews -> render eyes -> xrEndFrame. Implemented alongside the
    // EGL/GLES bootstrap.
}

void OpenXrApp::Shutdown() {
    if (session_ != XR_NULL_HANDLE) { xrDestroySession(session_); session_ = XR_NULL_HANDLE; }
    if (instance_ != XR_NULL_HANDLE) { xrDestroyInstance(instance_); instance_ = XR_NULL_HANDLE; }
    running_ = false;
}

} // namespace breachvr
