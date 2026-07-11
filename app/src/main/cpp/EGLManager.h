#ifndef EGL_MANAGER_H
#define EGL_MANAGER_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <cstdint>

class EGLManager {
public:
    EGLManager();
    ~EGLManager();

    bool initEGL();
    void releaseEGL();

    bool createPreviewSurface(ANativeWindow* window);
    void destroyPreviewSurface();

    bool createEncoderSurface(ANativeWindow* window);
    void destroyEncoderSurface();

    bool makeCurrent(EGLSurface surface);
    void swapBuffers(EGLSurface surface);
    void setPresentationTime(EGLSurface surface, int64_t nsecs);

    EGLContext getContext() const { return eglContext; }
    EGLDisplay getDisplay() const { return eglDisplay; }
    EGLSurface getPreviewSurface() const { return previewSurface; }
    EGLSurface getEncoderSurface() const { return encoderSurface; }
    EGLSurface getDummySurface() const { return dummySurface; }

private:
    EGLDisplay eglDisplay;
    EGLConfig eglConfig;
    EGLContext eglContext;

    EGLSurface previewSurface;
    EGLSurface encoderSurface;
    EGLSurface dummySurface; // 1x1 fallback pbuffer surface

    ANativeWindow* previewWindow;
    ANativeWindow* encoderWindow;

    // EGL Extension function pointer for setting timestamps on encoder surface
    typedef EGLBoolean (EGLAPIENTRYP PFNEGLPRESENTATIONTIMEANDROIDPROC)(
        EGLDisplay dpy, EGLSurface surface, khronos_stime_nanoseconds_t time);
    PFNEGLPRESENTATIONTIMEANDROIDPROC eglPresentationTimeANDROID;
};

#endif // EGL_MANAGER_H
