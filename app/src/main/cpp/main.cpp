// Breach VR - native entry point.
//
// NativeActivity calls android_main on a dedicated thread. We drive the
// OpenXR application lifecycle from here.

#include <android_native_app_glue.h>
#include <android/log.h>

#include "openxr_app.h"

#define LOG_TAG "BreachVR"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

using breachvr::OpenXrApp;

static void handle_cmd(android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            LOGI("Window ready");
            break;
        case APP_CMD_TERM_WINDOW:
            LOGI("Window destroyed");
            break;
        case APP_CMD_DESTROY:
            LOGI("App destroy requested");
            break;
        default:
            break;
    }
}

void android_main(android_app* app) {
    LOGI("Breach VR starting");
    app->onAppCmd = handle_cmd;

    OpenXrApp xr;
    if (!xr.Init(app)) {
        LOGI("OpenXR init failed - exiting");
        return;
    }

    // Main loop: process Android events, then render an XR frame.
    while (!app->destroyRequested && xr.IsRunning()) {
        int events;
        android_poll_source* source;
        while (ALooper_pollAll(0, nullptr, &events, (void**)&source) >= 0) {
            if (source) source->process(app, source);
            if (app->destroyRequested) break;
        }
        xr.RenderFrame();
    }

    xr.Shutdown();
    LOGI("Breach VR shutting down");
}
