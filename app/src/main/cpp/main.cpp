// Breach VR - Native Activity Entry Point
// Pure Android (NDK/C++17) + OpenXR for Meta Quest 3S

#include <android_native_app_glue.h>
#include <android/log.h>
#include "openxr_engine.h"

#define LOG_TAG "BreachVR_Main"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

using breachvr::OpenXrEngine;

static void handle_cmd(android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            LOGI("NativeActivity Window Initialized");
            break;
        case APP_CMD_TERM_WINDOW:
            LOGI("NativeActivity Window Destroyed");
            break;
        case APP_CMD_GAINED_FOCUS:
            LOGI("NativeActivity Gained Focus");
            break;
        case APP_CMD_LOST_FOCUS:
            LOGI("NativeActivity Lost Focus");
            break;
        case APP_CMD_DESTROY:
            LOGI("NativeActivity Destroy Requested");
            break;
        default:
            break;
    }
}

extern "C" void android_main(android_app* app) {
    LOGI("Breach VR initializing on Meta Quest 3S");
    app->onAppCmd = handle_cmd;

    OpenXrEngine engine;
    if (!engine.Init(app)) {
        LOGI("OpenXR Engine initialization failed - shutting down");
        return;
    }

    LOGI("Entering Breach VR main render loop");

    // Main XR Loop: process native window events and render VR frames
    while (!app->destroyRequested && engine.IsRunning()) {
        int events;
        android_poll_source* source = nullptr;

        while (ALooper_pollOnce(0, nullptr, &events, (void**)&source) >= 0) {
            if (source) source->process(app, source);
            if (app->destroyRequested) break;
        }

        engine.RenderFrame();
    }

    engine.Shutdown();
    LOGI("Breach VR safely terminated");
}
