#ifndef RENDERER_H
#define RENDERER_H

#include "EGLManager.h"
#include "PBOManager.h"
#include "AITask.h"
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <GLES2/gl2ext.h>
#include <jni.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

class Renderer {
public:
    Renderer();
    ~Renderer();

    void init(EGLManager* egl, PBOManager* pbo, AITask* ai);
    
    // Starts the C++ render thread loop and attaches Java SurfaceTexture
    int start(JavaVM* vm, jobject stObj, int aiW, int aiH);
    void stop();

    // Event signals triggered from JNI
    void onFrameAvailable();
    void setPreviewWindow(ANativeWindow* window);
    void setEncoderWindow(ANativeWindow* window, int w, int h);
    void setCameraResolution(int w, int h);

private:
    void threadLoop();
    bool setupGL();
    void teardownGL();
    void drawFrame(JNIEnv* env);

    // Engine subsystems pointers (owned by Engine)
    EGLManager* eglManager;
    PBOManager* pboManager;
    AITask* aiTask;

    // Render loop thread components
    std::thread renderThread;
    std::mutex renderMutex;
    std::condition_variable renderCv;
    std::atomic<bool> isRunning;
    std::atomic<bool> frameAvailable;

    // Surface update requests (processed exclusively on Render Thread to avoid driver races)
    std::atomic<bool> pendingPreviewUpdate;
    ANativeWindow* newPreviewWindow;

    std::atomic<bool> pendingEncoderUpdate;
    ANativeWindow* newEncoderWindow;
    int pendingEncoderWidth;
    int pendingEncoderHeight;

    // GL Objects
    GLuint oesTextureId;
    GLuint fboId;
    GLuint aiTextureId;
    GLuint programId;
    GLuint vaoId;
    GLuint vboId[2]; // Position, TexCoords

    // Shader Uniforms/Attribs Locations
    GLint positionLoc;
    GLint texCoordLoc;
    GLint matrixLoc;

    // Dimensions
    int previewWidth;
    int previewHeight;
    int encoderWidth;
    int encoderHeight;
    int aiWidth;
    int aiHeight;
    int cameraWidth;
    int cameraHeight;

    // JNI objects cached on startup
    JavaVM* javaVM;
    jobject surfaceTextureRef;
    jfloatArray matrixArrayRef;

    // Cached JNI method IDs
    jmethodID updateTexImageMethod;
    jmethodID getTimestampMethod;
    jmethodID getTransformMatrixMethod;
};

#endif // RENDERER_H
