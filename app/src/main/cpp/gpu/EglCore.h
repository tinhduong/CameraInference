#pragma once
#include <EGL/egl.h>
#include <EGL/eglext.h>

class EglCore {
public:
    EglCore();
    ~EglCore();

    bool init(EGLContext sharedContext = EGL_NO_CONTEXT);
    void release();

    EGLContext getContext() const { return eglContext_; }
    EGLDisplay getDisplay() const { return eglDisplay_; }

    EGLSurface createPbufferSurface(int width, int height);
    void destroySurface(EGLSurface surface);

    bool makeCurrent(EGLSurface surface);
    bool makeUncurrent();

private:
    EGLDisplay eglDisplay_ = EGL_NO_DISPLAY;
    EGLContext eglContext_ = EGL_NO_CONTEXT;
    EGLConfig eglConfig_ = nullptr;
};
