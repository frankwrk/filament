/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <filament/driver/Platform.h>

#if defined(ANDROID)
    #ifndef USE_EXTERNAL_GLES3
        #include "driver/opengl/PlatformEGL.h"
    #endif
    #if defined (FILAMENT_DRIVER_SUPPORTS_VULKAN)
        #include "driver/vulkan/PlatformVkAndroid.h"
    #endif
#elif defined(IOS)
    #ifndef USE_EXTERNAL_GLES3
        #include "driver/opengl/PlatformCocoaTouchGL.h"
    #endif
    #if defined (FILAMENT_DRIVER_SUPPORTS_VULKAN)
        #include "driver/vulkan/PlatformVkCocoaTouch.h"
    #endif
#elif defined(__APPLE__)
    #ifndef USE_EXTERNAL_GLES3
        #include "driver/opengl/PlatformCocoaGL.h"
    #endif
    #if defined (FILAMENT_DRIVER_SUPPORTS_VULKAN)
        #include "driver/vulkan/PlatformVkCocoa.h"
    #endif
#elif defined(__linux__)
    #ifndef USE_EXTERNAL_GLES3
        #include "driver/opengl/PlatformGLX.h"
    #endif
    #if defined (FILAMENT_DRIVER_SUPPORTS_VULKAN)
        #include "driver/vulkan/PlatformVkLinux.h"
    #endif
#elif defined(WIN32)
    #ifndef USE_EXTERNAL_GLES3
        #include "driver/opengl/PlatformWGL.h"
    #endif
    #if defined (FILAMENT_DRIVER_SUPPORTS_VULKAN)
        #include "driver/vulkan/PlatformVkWindows.h"
    #endif
#elif defined(__EMSCRIPTEN__)
    #include "driver/opengl/PlatformWebGL.h"
#else
    #ifndef USE_EXTERNAL_GLES3
        #include "driver/opengl/PlatformDummyGL.h"
    #endif
#endif

#if defined (FILAMENT_SUPPORTS_METAL)
    #include "driver/metal/PlatformMetal.h"
#endif

#ifndef NDEBUG
    #include "driver/noop/PlatformNoop.h"
#endif

namespace filament {
namespace driver {

// this generates the vtable in this translation unit
Platform::~Platform() noexcept = default;

OpenGLPlatform::~OpenGLPlatform() noexcept = default;

VulkanPlatform::~VulkanPlatform() noexcept = default;

MetalPlatform::~MetalPlatform() noexcept = default;

// Creates the platform-specific Platform object. The caller takes ownership and is
// responsible for destroying it. Initialization of the backend API is deferred until
// createDriver(). The passed-in backend hint is replaced with the resolved backend.
Platform* Platform::create(Backend* backend) noexcept {
    assert(backend);
    if (*backend == Backend::DEFAULT) {
        *backend = Backend::OPENGL;
    }
    #ifndef NDEBUG
    if (*backend == Backend::NOOP) {
        return new PlatformNoop();
    }
    #endif
    if (*backend == Backend::VULKAN) {
        #if defined(FILAMENT_DRIVER_SUPPORTS_VULKAN)
            #if defined(ANDROID)
                return new PlatformVkAndroid();
            #elif defined(IOS)
                return new PlatformVkCocoaTouch();
            #elif defined(__linux__)
                return new PlatformVkLinux();
            #elif defined(__APPLE__)
                return new PlatformVkCocoa();
            #elif defined(WIN32)
                return new PlatformVkWindows();
            #else
                return nullptr;
            #endif
        #else
            return nullptr;
        #endif
    }
    if (*backend == Backend::METAL) {
#if defined(FILAMENT_SUPPORTS_METAL)
        return new PlatformMetal();
#else
        return nullptr;
#endif
    }
    #if defined(USE_EXTERNAL_GLES3)
        return nullptr;
    #elif defined(ANDROID)
        return new PlatformEGL();
    #elif defined(IOS)
        return new PlatformCocoaTouchGL();
    #elif defined(__APPLE__)
        return new PlatformCocoaGL();
    #elif defined(__linux__)
        return new PlatformGLX();
    #elif defined(WIN32)
        return new PlatformWGL();
    #elif defined(__EMSCRIPTEN__)
        return new PlatformWebGL();
    #else
        return new PlatformDummyGL();
    #endif
    return nullptr;
}

// destroys an Platform create by create()
void Platform::destroy(Platform** context) noexcept {
    delete *context;
    *context = nullptr;
}

} // namespace driver
} // namespace filament
