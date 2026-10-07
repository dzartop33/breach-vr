#pragma once
// Breach VR - EGL / OpenGL ES 3.0 Context Management

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <android/log.h>

namespace breachvr {

class EglContext {
public:
    EglContext() = default;
    ~EglContext() { Teardown(); }

    bool Init();
    bool MakeCurrent();
    void Teardown();

    EGLDisplay GetDisplay() const { return display_; }
    EGLConfig  GetConfig()  const { return config_; }
    EGLContext GetContext() const { return context_; }

private:
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLConfig  config_  = nullptr;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLSurface surface_ = EGL_NO_SURFACE;
};

} // namespace breachvr
