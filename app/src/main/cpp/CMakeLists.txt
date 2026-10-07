cmake_minimum_required(VERSION 3.22.1)
project(breachvr)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Android native_app_glue (ships with the NDK) ---------------------------
set(GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)

add_library(breachvr SHARED
        main.cpp
        openxr_engine.cpp
        egl_context.cpp
        gl_shaders.cpp
        mesh_builder.cpp
        audio_engine.cpp
        input_system.cpp
        apartment_scene.cpp
        weapon_system.cpp
        enemy_ai.cpp
        gameplay_manager.cpp
        ${GLUE_DIR}/android_native_app_glue.c
)

target_include_directories(breachvr PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${GLUE_DIR}
)

# Export ANativeActivity_onCreate so the NativeActivity loader finds it.
set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

# --- OpenXR loader (provided as a prefab by the Khronos AAR) -----------------
find_package(OpenXR REQUIRED CONFIG)

# --- System libraries -------------------------------------------------------
find_library(log-lib log)
find_library(android-lib android)
find_library(sles-lib OpenSLES)

target_link_libraries(breachvr
        OpenXR::openxr_loader
        EGL
        GLESv3
        ${sles-lib}
        ${android-lib}
        ${log-lib}
)
