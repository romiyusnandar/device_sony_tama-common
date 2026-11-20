/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dlfcn.h>
#include <stdint.h>
#include <string.h>

#include <mutex>

namespace {

typedef unsigned int EGLBoolean;
typedef int32_t EGLint;
typedef unsigned int EGLenum;
typedef intptr_t EGLAttrib;
typedef uint64_t EGLTimeKHR;
typedef void* EGLDisplay;
typedef void* EGLConfig;
typedef void* EGLSurface;
typedef void* EGLContext;
typedef void* EGLClientBuffer;
typedef void* EGLImageKHR;
typedef void* EGLImage;
typedef void* EGLSyncKHR;
typedef void* EGLNativeDisplayType;
typedef void* EGLNativeWindowType;
typedef void (*__eglMustCastToProperFunctionPointerType)(void);

constexpr EGLDisplay kEglNoDisplay = nullptr;
constexpr EGLBoolean kEglFalse = 0;
constexpr size_t kNumDisplays = 2;

std::mutex gDisplayMutex;
EGLDisplay gRealDisplays[kNumDisplays] = {nullptr, nullptr};

template <typename Fn>
Fn resolveSymbol(const char* name) {
    void* sym = dlsym(RTLD_NEXT, name);
    if (!sym) {
        static void* eglHandle = dlopen("libEGL.so", RTLD_LAZY | RTLD_NOLOAD);
        if (!eglHandle) {
            eglHandle = dlopen("libEGL.so", RTLD_LAZY);
        }
        if (eglHandle) {
            sym = dlsym(eglHandle, name);
        }
    }
    return reinterpret_cast<Fn>(sym);
}

__eglMustCastToProperFunctionPointerType realGetProcAddress(const char* procname) {
    using Fn = __eglMustCastToProperFunctionPointerType (*)(const char*);
    static Fn fn = resolveSymbol<Fn>("eglGetProcAddress");
    return fn ? fn(procname) : nullptr;
}

template <typename Fn>
Fn resolveEglExt(const char* name) {
    Fn fn = resolveSymbol<Fn>(name);
    if (!fn) {
        fn = reinterpret_cast<Fn>(realGetProcAddress(name));
    }
    return fn;
}

EGLDisplay toFakeDisplay(uintptr_t index, EGLDisplay realDpy) {
    if (realDpy == kEglNoDisplay) {
        return kEglNoDisplay;
    }
    if (index < kNumDisplays) {
        std::lock_guard<std::mutex> lock(gDisplayMutex);
        gRealDisplays[index] = realDpy;
        return reinterpret_cast<EGLDisplay>(index + 1U);
    }
    return realDpy;
}

EGLDisplay toRealDisplay(EGLDisplay dpy) {
    uintptr_t val = reinterpret_cast<uintptr_t>(dpy);
    if (val >= 1U && val <= kNumDisplays) {
        std::lock_guard<std::mutex> lock(gDisplayMutex);
        EGLDisplay realDpy = gRealDisplays[val - 1U];
        if (realDpy != kEglNoDisplay) {
            return realDpy;
        }
    }
    return dpy;
}

EGLDisplay fromRealDisplay(EGLDisplay realDpy) {
    if (realDpy == kEglNoDisplay) {
        return kEglNoDisplay;
    }
    std::lock_guard<std::mutex> lock(gDisplayMutex);
    for (size_t i = 0; i < kNumDisplays; ++i) {
        if (gRealDisplays[i] == realDpy) {
            return reinterpret_cast<EGLDisplay>(i + 1U);
        }
    }
    return realDpy;
}

}  // namespace

extern "C" {

EGLDisplay eglGetDisplay(EGLNativeDisplayType display_id) {
    using Fn = EGLDisplay (*)(EGLNativeDisplayType);
    static Fn fn = resolveSymbol<Fn>("eglGetDisplay");
    if (!fn) return kEglNoDisplay;

    EGLDisplay realDpy = fn(display_id);
    uintptr_t idx = reinterpret_cast<uintptr_t>(display_id);
    return toFakeDisplay(idx, realDpy);
}

EGLDisplay eglGetPlatformDisplay(EGLenum platform, void* native_display,
                                 const EGLAttrib* attrib_list) {
    using Fn = EGLDisplay (*)(EGLenum, void*, const EGLAttrib*);
    static Fn fn = resolveSymbol<Fn>("eglGetPlatformDisplay");
    if (!fn) return kEglNoDisplay;

    EGLDisplay realDpy = fn(platform, native_display, attrib_list);
    uintptr_t idx = reinterpret_cast<uintptr_t>(native_display);
    return toFakeDisplay(idx, realDpy);
}

EGLDisplay eglGetPlatformDisplayEXT(EGLenum platform, void* native_display,
                                    const EGLint* attrib_list) {
    using Fn = EGLDisplay (*)(EGLenum, void*, const EGLint*);
    static Fn fn = resolveEglExt<Fn>("eglGetPlatformDisplayEXT");
    if (!fn) return kEglNoDisplay;

    EGLDisplay realDpy = fn(platform, native_display, attrib_list);
    uintptr_t idx = reinterpret_cast<uintptr_t>(native_display);
    return toFakeDisplay(idx, realDpy);
}

EGLDisplay eglGetCurrentDisplay(void) {
    using Fn = EGLDisplay (*)(void);
    static Fn fn = resolveSymbol<Fn>("eglGetCurrentDisplay");
    return fn ? fromRealDisplay(fn()) : kEglNoDisplay;
}

EGLBoolean eglInitialize(EGLDisplay dpy, EGLint* major, EGLint* minor) {
    using Fn = EGLBoolean (*)(EGLDisplay, EGLint*, EGLint*);
    static Fn fn = resolveSymbol<Fn>("eglInitialize");
    return fn ? fn(toRealDisplay(dpy), major, minor) : kEglFalse;
}

EGLBoolean eglTerminate(EGLDisplay dpy) {
    using Fn = EGLBoolean (*)(EGLDisplay);
    static Fn fn = resolveSymbol<Fn>("eglTerminate");
    return fn ? fn(toRealDisplay(dpy)) : kEglFalse;
}

EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint* attrib_list, EGLConfig* configs,
                           EGLint config_size, EGLint* num_config) {
    using Fn = EGLBoolean (*)(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*);
    static Fn fn = resolveSymbol<Fn>("eglChooseConfig");
    return fn ? fn(toRealDisplay(dpy), attrib_list, configs, config_size, num_config) : kEglFalse;
}

EGLBoolean eglGetConfigs(EGLDisplay dpy, EGLConfig* configs, EGLint config_size,
                         EGLint* num_config) {
    using Fn = EGLBoolean (*)(EGLDisplay, EGLConfig*, EGLint, EGLint*);
    static Fn fn = resolveSymbol<Fn>("eglGetConfigs");
    return fn ? fn(toRealDisplay(dpy), configs, config_size, num_config) : kEglFalse;
}

EGLBoolean eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint* value) {
    using Fn = EGLBoolean (*)(EGLDisplay, EGLConfig, EGLint, EGLint*);
    static Fn fn = resolveSymbol<Fn>("eglGetConfigAttrib");
    return fn ? fn(toRealDisplay(dpy), config, attribute, value) : kEglFalse;
}

const char* eglQueryString(EGLDisplay dpy, EGLint name) {
    using Fn = const char* (*)(EGLDisplay, EGLint);
    static Fn fn = resolveSymbol<Fn>("eglQueryString");
    return fn ? fn(toRealDisplay(dpy), name) : nullptr;
}

EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context,
                            const EGLint* attrib_list) {
    using Fn = EGLContext (*)(EGLDisplay, EGLConfig, EGLContext, const EGLint*);
    static Fn fn = resolveSymbol<Fn>("eglCreateContext");
    return fn ? fn(toRealDisplay(dpy), config, share_context, attrib_list) : nullptr;
}

EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx) {
    using Fn = EGLBoolean (*)(EGLDisplay, EGLContext);
    static Fn fn = resolveSymbol<Fn>("eglDestroyContext");
    return fn ? fn(toRealDisplay(dpy), ctx) : kEglFalse;
}

EGLBoolean eglQueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint* value) {
    using Fn = EGLBoolean (*)(EGLDisplay, EGLContext, EGLint, EGLint*);
    static Fn fn = resolveSymbol<Fn>("eglQueryContext");
    return fn ? fn(toRealDisplay(dpy), ctx, attribute, value) : kEglFalse;
}

EGLSurface eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint* attrib_list) {
    using Fn = EGLSurface (*)(EGLDisplay, EGLConfig, const EGLint*);
    static Fn fn = resolveSymbol<Fn>("eglCreatePbufferSurface");
    return fn ? fn(toRealDisplay(dpy), config, attrib_list) : nullptr;
}

EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win,
                                  const EGLint* attrib_list) {
    using Fn = EGLSurface (*)(EGLDisplay, EGLConfig, EGLNativeWindowType, const EGLint*);
    static Fn fn = resolveSymbol<Fn>("eglCreateWindowSurface");
    return fn ? fn(toRealDisplay(dpy), config, win, attrib_list) : nullptr;
}

EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface) {
    using Fn = EGLBoolean (*)(EGLDisplay, EGLSurface);
    static Fn fn = resolveSymbol<Fn>("eglDestroySurface");
    return fn ? fn(toRealDisplay(dpy), surface) : kEglFalse;
}

EGLBoolean eglQuerySurface(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint* value) {
    using Fn = EGLBoolean (*)(EGLDisplay, EGLSurface, EGLint, EGLint*);
    static Fn fn = resolveSymbol<Fn>("eglQuerySurface");
    return fn ? fn(toRealDisplay(dpy), surface, attribute, value) : kEglFalse;
}

EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) {
    using Fn = EGLBoolean (*)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
    static Fn fn = resolveSymbol<Fn>("eglMakeCurrent");
    return fn ? fn(toRealDisplay(dpy), draw, read, ctx) : kEglFalse;
}

EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    using Fn = EGLBoolean (*)(EGLDisplay, EGLSurface);
    static Fn fn = resolveSymbol<Fn>("eglSwapBuffers");
    return fn ? fn(toRealDisplay(dpy), surface) : kEglFalse;
}

EGLImageKHR eglCreateImageKHR(EGLDisplay dpy, EGLContext ctx, EGLenum target,
                              EGLClientBuffer buffer, const EGLint* attrib_list) {
    using Fn = EGLImageKHR (*)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint*);
    static Fn fn = resolveEglExt<Fn>("eglCreateImageKHR");
    return fn ? fn(toRealDisplay(dpy), ctx, target, buffer, attrib_list) : nullptr;
}

EGLBoolean eglDestroyImageKHR(EGLDisplay dpy, EGLImageKHR image) {
    using Fn = EGLBoolean (*)(EGLDisplay, EGLImageKHR);
    static Fn fn = resolveEglExt<Fn>("eglDestroyImageKHR");
    return fn ? fn(toRealDisplay(dpy), image) : kEglFalse;
}

EGLImage eglCreateImage(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer,
                        const EGLAttrib* attrib_list) {
    using Fn = EGLImage (*)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLAttrib*);
    static Fn fn = resolveEglExt<Fn>("eglCreateImage");
    return fn ? fn(toRealDisplay(dpy), ctx, target, buffer, attrib_list) : nullptr;
}

EGLBoolean eglDestroyImage(EGLDisplay dpy, EGLImage image) {
    using Fn = EGLBoolean (*)(EGLDisplay, EGLImage);
    static Fn fn = resolveEglExt<Fn>("eglDestroyImage");
    return fn ? fn(toRealDisplay(dpy), image) : kEglFalse;
}

EGLSyncKHR eglCreateSyncKHR(EGLDisplay dpy, EGLenum type, const EGLint* attrib_list) {
    using Fn = EGLSyncKHR (*)(EGLDisplay, EGLenum, const EGLint*);
    static Fn fn = resolveEglExt<Fn>("eglCreateSyncKHR");
    return fn ? fn(toRealDisplay(dpy), type, attrib_list) : nullptr;
}

EGLBoolean eglDestroySyncKHR(EGLDisplay dpy, EGLSyncKHR sync) {
    using Fn = EGLBoolean (*)(EGLDisplay, EGLSyncKHR);
    static Fn fn = resolveEglExt<Fn>("eglDestroySyncKHR");
    return fn ? fn(toRealDisplay(dpy), sync) : kEglFalse;
}

EGLint eglClientWaitSyncKHR(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout) {
    using Fn = EGLint (*)(EGLDisplay, EGLSyncKHR, EGLint, EGLTimeKHR);
    static Fn fn = resolveEglExt<Fn>("eglClientWaitSyncKHR");
    return fn ? fn(toRealDisplay(dpy), sync, flags, timeout) : 0;
}

EGLint eglWaitSyncKHR(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags) {
    using Fn = EGLint (*)(EGLDisplay, EGLSyncKHR, EGLint);
    static Fn fn = resolveEglExt<Fn>("eglWaitSyncKHR");
    return fn ? fn(toRealDisplay(dpy), sync, flags) : 0;
}

EGLint eglDupNativeFenceFDANDROID(EGLDisplay dpy, EGLSyncKHR sync) {
    using Fn = EGLint (*)(EGLDisplay, EGLSyncKHR);
    static Fn fn = resolveEglExt<Fn>("eglDupNativeFenceFDANDROID");
    return fn ? fn(toRealDisplay(dpy), sync) : -1;
}

__eglMustCastToProperFunctionPointerType eglGetProcAddress(const char* procname) {
    if (procname) {
        if (strcmp(procname, "eglCreateImageKHR") == 0) {
            return reinterpret_cast<__eglMustCastToProperFunctionPointerType>(eglCreateImageKHR);
        }
        if (strcmp(procname, "eglDestroyImageKHR") == 0) {
            return reinterpret_cast<__eglMustCastToProperFunctionPointerType>(eglDestroyImageKHR);
        }
        if (strcmp(procname, "eglCreateImage") == 0) {
            return reinterpret_cast<__eglMustCastToProperFunctionPointerType>(eglCreateImage);
        }
        if (strcmp(procname, "eglDestroyImage") == 0) {
            return reinterpret_cast<__eglMustCastToProperFunctionPointerType>(eglDestroyImage);
        }
        if (strcmp(procname, "eglCreateSyncKHR") == 0) {
            return reinterpret_cast<__eglMustCastToProperFunctionPointerType>(eglCreateSyncKHR);
        }
        if (strcmp(procname, "eglDestroySyncKHR") == 0) {
            return reinterpret_cast<__eglMustCastToProperFunctionPointerType>(eglDestroySyncKHR);
        }
        if (strcmp(procname, "eglClientWaitSyncKHR") == 0) {
            return reinterpret_cast<__eglMustCastToProperFunctionPointerType>(eglClientWaitSyncKHR);
        }
        if (strcmp(procname, "eglWaitSyncKHR") == 0) {
            return reinterpret_cast<__eglMustCastToProperFunctionPointerType>(eglWaitSyncKHR);
        }
        if (strcmp(procname, "eglDupNativeFenceFDANDROID") == 0) {
            return reinterpret_cast<__eglMustCastToProperFunctionPointerType>(
                    eglDupNativeFenceFDANDROID);
        }
        if (strcmp(procname, "eglGetPlatformDisplayEXT") == 0) {
            return reinterpret_cast<__eglMustCastToProperFunctionPointerType>(
                    eglGetPlatformDisplayEXT);
        }
    }
    return realGetProcAddress(procname);
}

}  // extern "C"
