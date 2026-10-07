#include "egl_context.h"
#include <vector>

#define LOG_TAG "BreachVR_EGL"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace breachvr {

bool EglContext::Init() {
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay failed: 0x%x", eglGetError());
        return false;
    }

    EGLint major = 0, minor = 0;
    if (!eglInitialize(display_, &major, &minor)) {
        LOGE("eglInitialize failed: 0x%x", eglGetError());
        return false;
    }
    LOGI("EGL initialized: version %d.%d", major, minor);

    // OpenGL ES 3.0 RGBA8 config with depth
    const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT | EGL_WINDOW_BIT,
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,
        EGL_DEPTH_SIZE,      24,
        EGL_STENCIL_SIZE,    0,
        EGL_NONE
    };

    EGLint numConfigs = 0;
    if (!eglChooseConfig(display_, configAttribs, &config_, 1, &numConfigs) || numConfigs <= 0) {
        LOGE("eglChooseConfig failed: 0x%x", eglGetError());
        return false;
    }

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    context_ = eglCreateContext(display_, config_, EGL_NO_CONTEXT, contextAttribs);
    if (context_ == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed: 0x%x", eglGetError());
        return false;
    }

    // Create a 16x16 offscreen Pbuffer surface for context binding
    const EGLint pbufferAttribs[] = {
        EGL_WIDTH, 16,
        EGL_HEIGHT, 16,
        EGL_NONE
    };
    surface_ = eglCreatePbufferSurface(display_, config_, pbufferAttribs);
    if (surface_ == EGL_NO_SURFACE) {
        LOGE("eglCreatePbufferSurface failed: 0x%x", eglGetError());
        return false;
    }

    if (!MakeCurrent()) {
        return false;
    }

    LOGI("EGL/GLES3 context successfully created and made current");
    return true;
}

bool EglContext::MakeCurrent() {
    if (display_ == EGL_NO_DISPLAY || context_ == EGL_NO_CONTEXT) {
        return false;
    }
    if (!eglMakeCurrent(display_, surface_, surface_, context_)) {
        LOGE("eglMakeCurrent failed: 0x%x", eglGetError());
        return false;
    }
    return true;
}

void EglContext::Teardown() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(display_, surface_);
            surface_ = EGL_NO_SURFACE;
        }
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
        }
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }
}

} // namespace breachvr
