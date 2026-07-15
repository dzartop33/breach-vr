cmake_minimum_required(VERSION 3.22.1)
project(breachvr)

# Build our native immersive engine as a shared lib loaded by NativeActivity.
add_library(breachvr SHARED
        main.cpp
        openxr_app.cpp
)

target_include_directories(breachvr PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# The OpenXR loader is provided as a prefab by the Khronos AAR dependency.
find_package(OpenXR REQUIRED CONFIG)

# Android native app glue + system libs.
find_library(log-lib log)
find_library(android-lib android)

# games-activity / native_app_glue provided by AndroidX GameActivity prefab.
find_package(game-activity REQUIRED CONFIG)

target_link_libraries(breachvr
        OpenXR::openxr_loader
        game-activity::game-activity_static
        EGL
        GLESv3
        ${android-lib}
        ${log-lib}
)
