cmake_minimum_required(VERSION 3.22.1)
project(breachvr)

# --- Android native_app_glue (ships with the NDK) ---
set(GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)

add_library(breachvr SHARED
        main.cpp
        openxr_app.cpp
        ${GLUE_DIR}/android_native_app_glue.c
)

target_include_directories(breachvr PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${GLUE_DIR}
)

# Export ANativeActivity_onCreate so the NativeActivity loader finds it.
set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

# --- OpenXR loader (prefab from the Khronos AAR) ---
find_package(OpenXR REQUIRED CONFIG)

# --- System libraries ---
find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(breachvr
        OpenXR::openxr_loader
        EGL
        GLESv3
        ${android-lib}
        ${log-lib}
)
