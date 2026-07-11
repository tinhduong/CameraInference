#include "EGLManager.h"
#include <android/log.h>

#define TAG "EGLManager"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

EGLManager::EGLManager() :
    eglDisplay(EGL_NO_DISPLAY),
    eglConfig(nullptr),
    eglContext(EGL_NO_CONTEXT),
    previewSurface(EGL_NO_SURFACE),
    encoderSurface(EGL_NO_SURFACE),
    dummySurface(EGL_NO_SURFACE),
    previewWindow(nullptr),
    encoderWindow(nullptr),
    eglPresentationTimeANDROID(nullptr) {}

EGLManager::~EGLManager() {
    releaseEGL();
}

bool EGLManager::initEGL() {
    eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay failed");
        return false;
    }

    EGLint major, minor;
    if (!eglInitialize(eglDisplay, &major, &minor)) {
        LOGE("eglInitialize failed");
        return false;
    }
    LOGD("EGL Initialized Version: %d.%d", major, minor);

    const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 0,
        EGL_NONE
    };

    EGLint numConfigs;
    if (!eglChooseConfig(eglDisplay, configAttribs, &eglConfig, 1, &numConfigs) || numConfigs <= 0) {
        LOGE("eglChooseConfig failed");
        return false;
    }

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    eglContext = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttribs);
    if (eglContext == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed");
        return false;
    }

    // Create 1x1 dummy pbuffer surface to allow makeCurrent even without standard window surfaces
    const EGLint pbufferAttribs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };
    dummySurface = eglCreatePbufferSurface(eglDisplay, eglConfig, pbufferAttribs);
    if (dummySurface == EGL_NO_SURFACE) {
        LOGE("Failed to create dummy pbuffer surface");
        return false;
    }

    // Query extension function for presentation timestamps (critical for video encoding sync)
    eglPresentationTimeANDROID = (PFNEGLPRESENTATIONTIMEANDROIDPROC)
        eglGetProcAddress("eglPresentationTimeANDROID");
    if (!eglPresentationTimeANDROID) {
        LOGE("eglPresentationTimeANDROID extension not supported");
    }

    // Bind dummy surface as start-state
    if (!makeCurrent(dummySurface)) {
        LOGE("Failed to make dummy surface current");
        return false;
    }

    return true;
}

void EGLManager::releaseEGL() {
    destroyPreviewSurface();
    destroyEncoderSurface();

    if (eglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        
        if (dummySurface != EGL_NO_SURFACE) {
            eglDestroySurface(eglDisplay, dummySurface);
            dummySurface = EGL_NO_SURFACE;
        }
        if (eglContext != EGL_NO_CONTEXT) {
            eglDestroyContext(eglDisplay, eglContext);
            eglContext = EGL_NO_CONTEXT;
        }
        eglTerminate(eglDisplay);
        eglDisplay = EGL_NO_DISPLAY;
    }
}

bool EGLManager::createPreviewSurface(ANativeWindow* window) {
    destroyPreviewSurface();

    if (window == nullptr) {
        LOGE("preview window is null");
        return false;
    }

    previewWindow = window;
    ANativeWindow_acquire(previewWindow);

    previewSurface = eglCreateWindowSurface(eglDisplay, eglConfig, previewWindow, nullptr);
    if (previewSurface == EGL_NO_SURFACE) {
        LOGE("Failed to create preview EGLSurface: %d", eglGetError());
        return false;
    }

    LOGD("Preview EGLSurface created successfully");
    return true;
}

void EGLManager::destroyPreviewSurface() {
    if (eglDisplay != EGL_NO_DISPLAY && previewSurface != EGL_NO_SURFACE) {
        eglDestroySurface(eglDisplay, previewSurface);
        previewSurface = EGL_NO_SURFACE;
    }
    if (previewWindow != nullptr) {
        ANativeWindow_release(previewWindow);
        previewWindow = nullptr;
    }
}

bool EGLManager::createEncoderSurface(ANativeWindow* window) {
    destroyEncoderSurface();

    if (window == nullptr) {
        LOGE("encoder window is null");
        return false;
    }

    encoderWindow = window;
    ANativeWindow_acquire(encoderWindow);

    // Ensure format matches RGB/RGBA for EGL Window compatibility
    ANativeWindow_setBuffersGeometry(encoderWindow, 0, 0, WINDOW_FORMAT_RGBA_8888);

    encoderSurface = eglCreateWindowSurface(eglDisplay, eglConfig, encoderWindow, nullptr);
    if (encoderSurface == EGL_NO_SURFACE) {
        LOGE("Failed to create encoder EGLSurface: %d", eglGetError());
        return false;
    }

    LOGD("Encoder EGLSurface created successfully");
    return true;
}

void EGLManager::destroyEncoderSurface() {
    if (eglDisplay != EGL_NO_DISPLAY && encoderSurface != EGL_NO_SURFACE) {
        eglDestroySurface(eglDisplay, encoderSurface);
        encoderSurface = EGL_NO_SURFACE;
    }
    if (encoderWindow != nullptr) {
        ANativeWindow_release(encoderWindow);
        encoderWindow = nullptr;
    }
}

bool EGLManager::makeCurrent(EGLSurface surface) {
    if (eglDisplay == EGL_NO_DISPLAY) {
        return false;
    }
    EGLSurface surf = (surface == EGL_NO_SURFACE) ? dummySurface : surface;
    if (!eglMakeCurrent(eglDisplay, surf, surf, eglContext)) {
        LOGE("eglMakeCurrent failed: %d", eglGetError());
        return false;
    }
    return true;
}

void EGLManager::swapBuffers(EGLSurface surface) {
    if (eglDisplay != EGL_NO_DISPLAY && surface != EGL_NO_SURFACE) {
        eglSwapBuffers(eglDisplay, surface);
    }
}

void EGLManager::setPresentationTime(EGLSurface surface, int64_t nsecs) {
    if (eglPresentationTimeANDROID && eglDisplay != EGL_NO_DISPLAY && surface != EGL_NO_SURFACE) {
        eglPresentationTimeANDROID(eglDisplay, surface, nsecs);
    }
}
