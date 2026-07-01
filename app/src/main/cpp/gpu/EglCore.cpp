#include "EglCore.h"
#include <android/log.h>

#define LOG_TAG "EglCore"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

EglCore::EglCore() {}

EglCore::~EglCore() {
    release();
}

bool EglCore::init(EGLContext sharedContext) {
    if (eglDisplay_ != EGL_NO_DISPLAY) {
        LOGI("EGL already initialized");
        return true;
    }

    eglDisplay_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay_ == EGL_NO_DISPLAY) {
        LOGE("unable to get EGL display");
        return false;
    }

    EGLint major, minor;
    if (!eglInitialize(eglDisplay_, &major, &minor)) {
        LOGE("unable to initialize EGL");
        eglDisplay_ = EGL_NO_DISPLAY;
        return false;
    }

    EGLint attribList[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_NONE
    };

    EGLint numConfigs;
    if (!eglChooseConfig(eglDisplay_, attribList, &eglConfig_, 1, &numConfigs) || numConfigs < 1) {
        LOGE("unable to choose config");
        release();
        return false;
    }

    EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    eglContext_ = eglCreateContext(eglDisplay_, eglConfig_, sharedContext, contextAttribs);
    if (eglContext_ == EGL_NO_CONTEXT) {
        LOGE("unable to create EGL context");
        release();
        return false;
    }

    LOGI("EGL initialized successfully. Version: %d.%d", major, minor);
    return true;
}

void EglCore::release() {
    if (eglDisplay_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(eglDisplay_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (eglContext_ != EGL_NO_CONTEXT) {
            eglDestroyContext(eglDisplay_, eglContext_);
            eglContext_ = EGL_NO_CONTEXT;
        }
        eglTerminate(eglDisplay_);
        eglDisplay_ = EGL_NO_DISPLAY;
    }
    eglConfig_ = nullptr;
}

EGLSurface EglCore::createPbufferSurface(int width, int height) {
    EGLint attribList[] = {
        EGL_WIDTH, width,
        EGL_HEIGHT, height,
        EGL_NONE
    };
    EGLSurface surface = eglCreatePbufferSurface(eglDisplay_, eglConfig_, attribList);
    if (surface == EGL_NO_SURFACE) {
        LOGE("unable to create pbuffer surface");
    }
    return surface;
}

void EglCore::destroySurface(EGLSurface surface) {
    if (surface != EGL_NO_SURFACE) {
        eglDestroySurface(eglDisplay_, surface);
    }
}

bool EglCore::makeCurrent(EGLSurface surface) {
    if (eglDisplay_ == EGL_NO_DISPLAY) {
        LOGE("Display is EGL_NO_DISPLAY");
        return false;
    }
    if (!eglMakeCurrent(eglDisplay_, surface, surface, eglContext_)) {
        LOGE("eglMakeCurrent failed");
        return false;
    }
    return true;
}

bool EglCore::makeUncurrent() {
    if (eglDisplay_ == EGL_NO_DISPLAY) return false;
    return eglMakeCurrent(eglDisplay_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}
