#include "openxr_engine.h"
#include <android/log.h>
#include <time.h>
#include <cstring>
#include <algorithm>

#define LOG_TAG "BreachVR_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace breachvr {

static double GetCurrentTimeSec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

bool OpenXrEngine::Init(android_app* app) {
    app_ = app;

    if (!CreateInstance(app)) return false;
    if (!InitSystem())        return false;
    if (!InitSession())       return false;

    lastFrameTime_ = GetCurrentTimeSec();
    LOGI("OpenXrEngine successfully initialized");
    return true;
}

bool OpenXrEngine::CreateInstance(android_app* app) {
    PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR = nullptr;
    xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                          (PFN_xrVoidFunction*)(&xrInitializeLoaderKHR));
    if (xrInitializeLoaderKHR) {
        XrLoaderInitInfoAndroidKHR loaderInfo{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        loaderInfo.applicationVM = app->activity->vm;
        loaderInfo.applicationContext = app->activity->clazz;
        xrInitializeLoaderKHR((const XrLoaderInitInfoBaseHeaderKHR*)&loaderInfo);
    }

    // Enumerate supported instance extensions
    uint32_t extCount = 0;
    xrEnumerateInstanceExtensionProperties(nullptr, 0, &extCount, nullptr);
    std::vector<XrExtensionProperties> availableExts(extCount, {XR_TYPE_EXTENSION_PROPERTIES});
    xrEnumerateInstanceExtensionProperties(nullptr, extCount, &extCount, availableExts.data());

    auto IsExtAvailable = [&](const char* name) -> bool {
        for (const auto& ext : availableExts) {
            if (std::strcmp(ext.extensionName, name) == 0) return true;
        }
        return false;
    };

    std::vector<const char*> enabledExts;
    auto EnableIfAvail = [&](const char* name) {
        if (IsExtAvailable(name)) {
            enabledExts.push_back(name);
            LOGI("Enabled extension: %s", name);
        } else {
            LOGI("Extension not available (optional): %s", name);
        }
    };

    EnableIfAvail(XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME);
    EnableIfAvail(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME);
    EnableIfAvail(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
    EnableIfAvail("XR_FB_scene");
    EnableIfAvail("XR_FB_spatial_entity");
    EnableIfAvail("XR_FB_passthrough");

    XrInstanceCreateInfoAndroidKHR androidInfo{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
    androidInfo.applicationVM = app->activity->vm;
    androidInfo.applicationActivity = app->activity->clazz;

    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    createInfo.next = &androidInfo;
    createInfo.enabledExtensionCount = (uint32_t)enabledExts.size();
    createInfo.enabledExtensionNames = enabledExts.data();
    std::strcpy(createInfo.applicationInfo.applicationName, "Breach VR");
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

    XrResult r = xrCreateInstance(&createInfo, &instance_);
    if (XR_FAILED(r)) {
        LOGE("xrCreateInstance failed: %d", r);
        return false;
    }
    return true;
}

bool OpenXrEngine::InitSystem() {
    XrSystemGetInfo info{XR_TYPE_SYSTEM_GET_INFO};
    info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrResult r = xrGetSystem(instance_, &info, &systemId_);
    if (XR_FAILED(r)) {
        LOGE("xrGetSystem failed: %d", r);
        return false;
    }

    uint32_t viewCount = 0;
    xrEnumerateViewConfigurationViews(instance_, systemId_, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                     0, &viewCount, nullptr);
    configViews_.resize(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    xrEnumerateViewConfigurationViews(instance_, systemId_, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                     viewCount, &viewCount, configViews_.data());

    LOGI("System initialized, stereo views count: %u", viewCount);
    return true;
}

bool OpenXrEngine::InitSession() {
    if (!eglContext_.Init()) {
        LOGE("Failed to initialize EGL context");
        return false;
    }

    PFN_xrGetOpenGLESGraphicsRequirementsKHR xrGetOpenGLESGraphicsRequirementsKHR = nullptr;
    xrGetInstanceProcAddr(instance_, "xrGetOpenGLESGraphicsRequirementsKHR",
                          (PFN_xrVoidFunction*)(&xrGetOpenGLESGraphicsRequirementsKHR));
    if (xrGetOpenGLESGraphicsRequirementsKHR) {
        XrGraphicsRequirementsOpenGLESKHR reqs{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
        xrGetOpenGLESGraphicsRequirementsKHR(instance_, systemId_, &reqs);
    }

    XrGraphicsBindingOpenGLESAndroidKHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
    graphicsBinding.display = eglContext_.GetDisplay();
    graphicsBinding.config  = eglContext_.GetConfig();
    graphicsBinding.context = eglContext_.GetContext();

    XrSessionCreateInfo createInfo{XR_TYPE_SESSION_CREATE_INFO};
    createInfo.next = &graphicsBinding;
    createInfo.systemId = systemId_;

    XrResult r = xrCreateSession(instance_, &createInfo, &session_);
    if (XR_FAILED(r)) {
        LOGE("xrCreateSession failed: %d", r);
        return false;
    }

    if (!CreateReferenceSpaces()) return false;
    if (!CreateSwapchains())      return false;

    // Initialize Subsystems
    ShaderLibrary::Instance().Init();
    AudioEngine::Instance().Init();
    inputSystem_.Init(instance_, session_);
    apartmentScene_.Init(instance_, session_);
    enemyManager_.Init(apartmentScene_);
    weaponSystem_.Init();
    gameplayManager_.Init();

    isSessionRunning_ = true;
    return true;
}

bool OpenXrEngine::CreateReferenceSpaces() {
    // Stage Space (floor-level room-scale for real apartment tracking)
    XrReferenceSpaceCreateInfo stageInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    stageInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    stageInfo.poseInReferenceSpace.orientation.w = 1.0f;
    XrResult r = xrCreateReferenceSpace(session_, &stageInfo, &stageSpace_);
    if (XR_FAILED(r)) {
        LOGI("Stage space not supported, falling back to LOCAL reference space");
        stageInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        xrCreateReferenceSpace(session_, &stageInfo, &stageSpace_);
    }

    // Local Space
    XrReferenceSpaceCreateInfo localInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    localInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    localInfo.poseInReferenceSpace.orientation.w = 1.0f;
    xrCreateReferenceSpace(session_, &localInfo, &localSpace_);

    // View Space
    XrReferenceSpaceCreateInfo viewInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    viewInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    viewInfo.poseInReferenceSpace.orientation.w = 1.0f;
    xrCreateReferenceSpace(session_, &viewInfo, &viewSpace_);

    return true;
}

bool OpenXrEngine::CreateSwapchains() {
    uint32_t formatCount = 0;
    xrEnumerateSwapchainFormats(session_, 0, &formatCount, nullptr);
    std::vector<int64_t> formats(formatCount);
    xrEnumerateSwapchainFormats(session_, formatCount, &formatCount, formats.data());

    int64_t chosenFormat = GL_RGBA8;
    for (int64_t f : formats) {
        if (f == GL_SRGB8_ALPHA8) {
            chosenFormat = f;
            break;
        }
    }

    eyeSwapchains_.resize(configViews_.size());

    for (size_t i = 0; i < configViews_.size(); ++i) {
        auto& eye = eyeSwapchains_[i];
        eye.width = configViews_[i].recommendedImageRectWidth;
        eye.height = configViews_[i].recommendedImageRectHeight;

        XrSwapchainCreateInfo swapchainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        swapchainInfo.format = chosenFormat;
        swapchainInfo.sampleCount = 1;
        swapchainInfo.width = eye.width;
        swapchainInfo.height = eye.height;
        swapchainInfo.faceCount = 1;
        swapchainInfo.arraySize = 1;
        swapchainInfo.mipCount = 1;

        XrResult r = xrCreateSwapchain(session_, &swapchainInfo, &eye.swapchain);
        if (XR_FAILED(r)) {
            LOGE("xrCreateSwapchain failed: %d", r);
            return false;
        }

        uint32_t imgCount = 0;
        xrEnumerateSwapchainImages(eye.swapchain, 0, &imgCount, nullptr);
        std::vector<XrSwapchainImageOpenGLESKHR> images(imgCount, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
        xrEnumerateSwapchainImages(eye.swapchain, imgCount, &imgCount, (XrSwapchainImageBaseHeader*)images.data());

        eye.colorTextures.resize(imgCount);
        eye.framebuffers.resize(imgCount);
        eye.depthBuffers.resize(imgCount);

        for (uint32_t j = 0; j < imgCount; ++j) {
            eye.colorTextures[j] = images[j].image;

            // Generate Depth Renderbuffer
            glGenRenderbuffers(1, &eye.depthBuffers[j]);
            glBindRenderbuffer(GL_RENDERBUFFER, eye.depthBuffers[j]);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, eye.width, eye.height);

            // Generate Framebuffer
            glGenFramebuffers(1, &eye.framebuffers[j]);
            glBindFramebuffer(GL_FRAMEBUFFER, eye.framebuffers[j]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, eye.colorTextures[j], 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, eye.depthBuffers[j]);

            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE) {
                LOGE("Eye %zu FBO %u incomplete: 0x%x", i, j, status);
            }
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    LOGI("Stereo swapchains created successfully");
    return true;
}

void OpenXrEngine::PollEvents() {
    XrEventDataBuffer eventData{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(instance_, &eventData) == XR_SUCCESS) {
        if (eventData.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto* change = reinterpret_cast<XrEventDataSessionStateChanged*>(&eventData);
            ProcessSessionStateChange(change->state);
        }
        eventData.type = XR_TYPE_EVENT_DATA_BUFFER;
    }
}

void OpenXrEngine::ProcessSessionStateChange(XrSessionState state) {
    sessionState_ = state;
    switch (state) {
        case XR_SESSION_STATE_READY: {
            XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
            beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            xrBeginSession(session_, &beginInfo);
            isSessionRunning_ = true;
            LOGI("OpenXR Session Begun");
            break;
        }
        case XR_SESSION_STATE_STOPPING: {
            xrEndSession(session_);
            isSessionRunning_ = false;
            LOGI("OpenXR Session Stopped");
            break;
        }
        case XR_SESSION_STATE_LOSS_PENDING:
        case XR_SESSION_STATE_EXITING: {
            exitRequested_ = true;
            isSessionRunning_ = false;
            LOGI("OpenXR Session Exit requested");
            break;
        }
        default:
            break;
    }
}

void OpenXrEngine::RenderFrame() {
    PollEvents();
    if (!isSessionRunning_ || exitRequested_) return;

    double now = GetCurrentTimeSec();
    float dt = static_cast<float>(now - lastFrameTime_);
    dt = std::clamp(dt, 0.001f, 0.1f);
    lastFrameTime_ = now;

    XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    if (XR_FAILED(xrWaitFrame(session_, &waitInfo, &frameState))) return;

    XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    if (XR_FAILED(xrBeginFrame(session_, &beginInfo))) return;

    std::vector<XrCompositionLayerBaseHeader*> layers;
    XrCompositionLayerProjection projectionLayer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    std::vector<XrCompositionLayerProjectionView> projectionViews(2, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});

    if (frameState.shouldRender) {
        // 1. Locate Eyes Views
        uint32_t viewCount = 2;
        std::vector<XrView> views(2, {XR_TYPE_VIEW});
        XrViewState viewState{XR_TYPE_VIEW_STATE};
        XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locateInfo.displayTime = frameState.predictedDisplayTime;
        locateInfo.space = stageSpace_;

        xrLocateViews(session_, &locateInfo, &viewState, viewCount, &viewCount, views.data());

        // 2. Update Input System
        inputSystem_.Update(session_, stageSpace_, frameState.predictedDisplayTime);

        // Locomotion & Turning
        Vec2 moveInput = inputSystem_.GetLocomotionInput();
        float turnInput = inputSystem_.GetSnapTurnInput();

        if (std::abs(turnInput) > 0.5f) {
            playerYaw_ += (turnInput > 0 ? -1.0f : 1.0f) * PI * 0.25f; // snap 45 deg
        }

        // Apply movement relative to head forward
        Vec3 headFwd = Quat(views[0].pose.orientation.x, views[0].pose.orientation.y,
                            views[0].pose.orientation.z, views[0].pose.orientation.w)
                           .Rotate(Vec3(0, 0, 1));
        headFwd.y = 0.0f;
        headFwd = headFwd.Normalized();
        Vec3 headRight = Vec3(headFwd.z, 0, -headFwd.x);

        Vec3 moveDelta = (headFwd * moveInput.y + headRight * moveInput.x) * (dt * 2.2f);
        playerVirtualOffset_ += moveDelta;

        // Player Head Position in World
        Vec3 headWorldPos = Vec3(views[0].pose.position.x, views[0].pose.position.y, views[0].pose.position.z) + playerVirtualOffset_;
        Quat headWorldRot = Quat(views[0].pose.orientation.x, views[0].pose.orientation.y,
                                 views[0].pose.orientation.z, views[0].pose.orientation.w);

        // Apartment Collision
        apartmentScene_.ResolveCollision(headWorldPos, 0.35f);

        // 3. Update Game Logic
        const auto& rightHand = inputSystem_.GetRightHand();
        const auto& leftHand  = inputSystem_.GetLeftHand();

        if (rightHand.isTriggerClicked || rightHand.isPinchClicked) {
            weaponSystem_.FireCurrentWeapon(rightHand, apartmentScene_, enemyManager_);
            inputSystem_.TriggerHaptic(session_, true, 0.08f, 220.0f, 0.85f);
        }

        if (rightHand.isReloadClicked) {
            weaponSystem_.Reload();
        }

        apartmentScene_.Update(dt);
        weaponSystem_.Update(dt, apartmentScene_, enemyManager_);
        enemyManager_.Update(dt, headWorldPos, apartmentScene_);
        gameplayManager_.Update(dt, headWorldPos, headWorldRot, apartmentScene_,
                                enemyManager_, weaponSystem_, inputSystem_);

        // 4. Render Stereo Eyes
        for (uint32_t i = 0; i < 2; ++i) {
            auto& eye = eyeSwapchains_[i];
            const auto& view = views[i];

            uint32_t imgIndex = 0;
            XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            xrAcquireSwapchainImage(eye.swapchain, &acquireInfo, &imgIndex);

            XrSwapchainImageWaitInfo waitImageInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            waitImageInfo.timeout = XR_INFINITE_DURATION;
            xrWaitSwapchainImage(eye.swapchain, &waitImageInfo);

            glBindFramebuffer(GL_FRAMEBUFFER, eye.framebuffers[imgIndex]);
            glViewport(0, 0, eye.width, eye.height);
            glScissor(0, 0, eye.width, eye.height);
            glEnable(GL_SCISSOR_TEST);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);

            // Background tactical dark clear color
            glClearColor(0.04f, 0.05f, 0.07f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Compute View and Projection Matrices
            Vec3 eyePos = Vec3(view.pose.position.x, view.pose.position.y, view.pose.position.z) + playerVirtualOffset_;
            Quat eyeRot(view.pose.orientation.x, view.pose.orientation.y, view.pose.orientation.z, view.pose.orientation.w);

            Mat4 eyeMat = Mat4::TRS(eyePos, eyeRot, Vec3(1, 1, 1));
            Mat4 viewMatrix = Mat4::InvertRigid(eyeMat);

            Mat4 projMatrix = Mat4::CreateProjection(
                view.fov.angleLeft, view.fov.angleRight,
                view.fov.angleUp,   view.fov.angleDown,
                0.05f, 100.0f
            );

            // Render Apartment Scene
            apartmentScene_.Render(viewMatrix, projMatrix, gameplayManager_.IsTacticalVisorActive());

            // Render CQB Enemies
            enemyManager_.Render(viewMatrix, projMatrix);

            // Render Weapons, Projectiles, Flashbangs, and Hand Tracking
            weaponSystem_.Render(viewMatrix, projMatrix, rightHand, leftHand);

            // Render Helmet Visor HUD
            gameplayManager_.RenderHUD(viewMatrix, projMatrix, headWorldPos, headWorldRot,
                                      weaponSystem_, enemyManager_, apartmentScene_);

            glDisable(GL_SCISSOR_TEST);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            xrReleaseSwapchainImage(eye.swapchain, &releaseInfo);

            projectionViews[i].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
            projectionViews[i].pose = view.pose;
            projectionViews[i].fov = view.fov;
            projectionViews[i].subImage.swapchain = eye.swapchain;
            projectionViews[i].subImage.imageRect.offset = { 0, 0 };
            projectionViews[i].subImage.imageRect.extent = { (int32_t)eye.width, (int32_t)eye.height };
            projectionViews[i].subImage.imageArrayIndex = 0;
        }

        projectionLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        projectionLayer.space = stageSpace_;
        projectionLayer.viewCount = 2;
        projectionLayer.views = projectionViews.data();
        layers.push_back((XrCompositionLayerBaseHeader*)&projectionLayer);
    }

    XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime = frameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = (uint32_t)layers.size();
    endInfo.layers = layers.data();
    xrEndFrame(session_, &endInfo);
}

void OpenXrEngine::Shutdown() {
    AudioEngine::Instance().Teardown();
    ShaderLibrary::Instance().Teardown();
    inputSystem_.Teardown(session_);

    for (auto& eye : eyeSwapchains_) {
        for (auto fb : eye.framebuffers)  glDeleteFramebuffers(1, &fb);
        for (auto rb : eye.depthBuffers)   glDeleteRenderbuffers(1, &rb);
        if (eye.swapchain != XR_NULL_HANDLE) xrDestroySwapchain(eye.swapchain);
    }
    eyeSwapchains_.clear();

    if (stageSpace_ != XR_NULL_HANDLE) { xrDestroySpace(stageSpace_); stageSpace_ = XR_NULL_HANDLE; }
    if (localSpace_ != XR_NULL_HANDLE) { xrDestroySpace(localSpace_); localSpace_ = XR_NULL_HANDLE; }
    if (viewSpace_ != XR_NULL_HANDLE)  { xrDestroySpace(viewSpace_);  viewSpace_ = XR_NULL_HANDLE; }

    if (session_ != XR_NULL_HANDLE)  { xrDestroySession(session_);   session_ = XR_NULL_HANDLE; }
    if (instance_ != XR_NULL_HANDLE) { xrDestroyInstance(instance_); instance_ = XR_NULL_HANDLE; }

    eglContext_.Teardown();
    isSessionRunning_ = false;
    LOGI("OpenXrEngine cleanly shut down");
}

} // namespace breachvr
