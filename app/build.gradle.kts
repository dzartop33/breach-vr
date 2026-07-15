plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.dzartop33.breachvr"
    compileSdk = 34
    ndkVersion = "26.1.10909125"

    defaultConfig {
        applicationId = "com.dzartop33.breachvr"
        minSdk = 29          // Quest OS is based on Android 10 (API 29)
        targetSdk = 32       // Meta store target for Quest apps
        versionCode = 1
        versionName = "0.1.0"

        // Quest 3S runs 64-bit ARM only.
        ndk {
            abiFilters += listOf("arm64-v8a")
        }

        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++17"
                arguments += "-DANDROID_STL=c++_shared"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            // Debug signing so CI can produce an installable APK with no secrets.
            // Replace with a real release keystore before publishing.
            signingConfig = signingConfigs.getByName("debug")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildFeatures {
        prefab = true   // consume OpenXR loader as a prefab package
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")

    // Khronos OpenXR loader for Android (provides libopenxr_loader.so + headers,
    // consumed via prefab). native_app_glue comes from the NDK itself.
    implementation("org.khronos.openxr:openxr_loader_for_android:1.1.36")
}
