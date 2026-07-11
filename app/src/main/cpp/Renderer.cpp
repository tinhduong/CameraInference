#include "Renderer.h"
#include <android/log.h>
#include <cstdlib>

#define TAG "Renderer"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static const char* VERTEX_SHADER =
    "#version 300 es\n"
    "in vec4 aPosition;\n"
    "in vec2 aTexCoord;\n"
    "out vec2 vTexCoord;\n"
    "uniform mat4 uSTMatrix;\n"
    "void main() {\n"
    "    gl_Position = aPosition;\n"
    "    vTexCoord = (uSTMatrix * vec4(aTexCoord, 0.0, 1.0)).xy;\n"
    "}\n";

static const char* FRAGMENT_SHADER =
    "#version 300 es\n"
    "#extension GL_OES_EGL_image_external_essl3 : require\n"
    "precision mediump float;\n"
    "in vec2 vTexCoord;\n"
    "out vec4 fragColor;\n"
    "uniform samplerExternalOES sTexture;\n"
    "void main() {\n"
    "    fragColor = texture(sTexture, vTexCoord);\n"
    "}\n";

static GLuint loadShader(GLenum shaderType, const char* pSource) {
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, nullptr);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char* buf = (char*) malloc(infoLen);
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, nullptr, buf);
                    LOGE("Could not compile shader %d:\n%s\n", shaderType, buf);
                    free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
        }
    }
    return shader;
}

static GLuint createProgram(const char* pVertexSource, const char* pFragmentSource) {
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) return 0;
    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) {
        glDeleteShader(vertexShader);
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        glAttachShader(program, pixelShader);
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = (char*) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, nullptr, buf);
                    LOGE("Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    glDeleteShader(vertexShader);
    glDeleteShader(pixelShader);
    return program;
}

Renderer::Renderer() :
    eglManager(nullptr),
    pboManager(nullptr),
    aiTask(nullptr),
    isRunning(false),
    frameAvailable(false),
    pendingPreviewUpdate(false),
    newPreviewWindow(nullptr),
    pendingEncoderUpdate(false),
    newEncoderWindow(nullptr),
    pendingEncoderWidth(0),
    pendingEncoderHeight(0),
    oesTextureId(0),
    fboId(0),
    aiTextureId(0),
    programId(0),
    vaoId(0),
    positionLoc(-1),
    texCoordLoc(-1),
    matrixLoc(-1),
    previewWidth(0),
    previewHeight(0),
    encoderWidth(0),
    encoderHeight(0),
    aiWidth(0),
    aiHeight(0),
    cameraWidth(0),
    cameraHeight(0),
    javaVM(nullptr),
    surfaceTextureRef(nullptr),
    matrixArrayRef(nullptr),
    updateTexImageMethod(nullptr),
    getTimestampMethod(nullptr),
    getTransformMatrixMethod(nullptr) {
    vboId[0] = 0;
    vboId[1] = 0;
}

Renderer::~Renderer() {
    stop();
}

void Renderer::init(EGLManager* egl, PBOManager* pbo, AITask* ai) {
    eglManager = egl;
    pboManager = pbo;
    aiTask = ai;
}

int Renderer::start(JavaVM* vm, jobject stObj, int aiW, int aiH) {
    javaVM = vm;
    aiWidth = aiW;
    aiHeight = aiH;

    JNIEnv* env = nullptr;
    bool isAttached = false;
    jint res = javaVM->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (res == JNI_EDETACHED) {
        if (javaVM->AttachCurrentThread(&env, nullptr) == JNI_OK) {
            isAttached = true;
        } else {
            LOGE("Failed to attach thread in Renderer::start");
            return 0;
        }
    } else if (res != JNI_OK) {
        LOGE("Failed to get JNI Env in Renderer::start");
        return 0;
    }

    surfaceTextureRef = env->NewGlobalRef(stObj);

    jfloatArray localMat = env->NewFloatArray(16);
    matrixArrayRef = (jfloatArray) env->NewGlobalRef(localMat);
    env->DeleteLocalRef(localMat);

    jclass stClass = env->GetObjectClass(surfaceTextureRef);
    updateTexImageMethod = env->GetMethodID(stClass, "updateTexImage", "()V");
    getTimestampMethod = env->GetMethodID(stClass, "getTimestamp", "()J");
    getTransformMatrixMethod = env->GetMethodID(stClass, "getTransformMatrix", "([F)V");

    if (isAttached) {
        javaVM->DetachCurrentThread();
    }

    isRunning = true;
    frameAvailable = false;
    oesTextureId = 0;

    renderThread = std::thread(&Renderer::threadLoop, this);

    // Wait block until OpenGL initialization completes on the Render Thread
    std::unique_lock<std::mutex> lock(renderMutex);
    renderCv.wait(lock, [this]() { return oesTextureId != 0 || !isRunning; });

    return oesTextureId;
}

void Renderer::stop() {
    if (isRunning) {
        isRunning = false;
        renderCv.notify_all();
        if (renderThread.joinable()) {
            renderThread.join();
        }
    }
}

void Renderer::onFrameAvailable() {
    {
        std::lock_guard<std::mutex> lock(renderMutex);
        frameAvailable = true;
    }
    renderCv.notify_all();
}

void Renderer::setPreviewWindow(ANativeWindow* window) {
    {
        std::lock_guard<std::mutex> lock(renderMutex);
        newPreviewWindow = window;
        pendingPreviewUpdate = true;
    }
    renderCv.notify_all();
}

void Renderer::setEncoderWindow(ANativeWindow* window, int w, int h) {
    {
        std::lock_guard<std::mutex> lock(renderMutex);
        newEncoderWindow = window;
        pendingEncoderWidth = w;
        pendingEncoderHeight = h;
        pendingEncoderUpdate = true;
    }
    renderCv.notify_all();
}

void Renderer::setCameraResolution(int w, int h) {
    std::lock_guard<std::mutex> lock(renderMutex);
    cameraWidth = w;
    cameraHeight = h;
    LOGD("Camera resolution set to: %dx%d", w, h);
}

void Renderer::threadLoop() {
    // 1. Initialize EGL Display, Config, Context, and dummy surface
    if (!eglManager->initEGL()) {
        LOGE("Could not initialize EGL on Render Thread");
        isRunning = false;
        renderCv.notify_all();
        return;
    }

    // 2. Prepare OpenGL shaders, geometry, textures and FBO
    if (!setupGL()) {
        LOGE("Failed to build OpenGL layout");
        isRunning = false;
        renderCv.notify_all();
        return;
    }

    // Notify the start() method that GL setup is complete and texture ID is ready
    {
        std::lock_guard<std::mutex> lock(renderMutex);
        renderCv.notify_all();
    }

    JNIEnv* env = nullptr;
    javaVM->AttachCurrentThread(&env, nullptr);

    // Attach SurfaceTexture to EGL thread's GL context
    jclass stClass = env->GetObjectClass(surfaceTextureRef);
    jmethodID attachToGLContextMethod = env->GetMethodID(stClass, "attachToGLContext", "(I)V");
    env->CallVoidMethod(surfaceTextureRef, attachToGLContextMethod, oesTextureId);
    env->DeleteLocalRef(stClass);

    while (isRunning) {
        std::unique_lock<std::mutex> lock(renderMutex);
        renderCv.wait(lock, [this]() {
            return !isRunning || frameAvailable || pendingPreviewUpdate || pendingEncoderUpdate;
        });

        if (!isRunning) {
            break;
        }

        // Apply EGL preview surface modifications
        if (pendingPreviewUpdate) {
            eglManager->makeCurrent(EGL_NO_SURFACE);
            if (newPreviewWindow) {
                eglManager->createPreviewSurface(newPreviewWindow);
            } else {
                eglManager->destroyPreviewSurface();
            }
            pendingPreviewUpdate = false;
        }

        // Apply EGL encoder surface modifications
        if (pendingEncoderUpdate) {
            eglManager->makeCurrent(EGL_NO_SURFACE);
            if (newEncoderWindow) {
                eglManager->createEncoderSurface(newEncoderWindow);
                encoderWidth = pendingEncoderWidth;
                encoderHeight = pendingEncoderHeight;
            } else {
                eglManager->destroyEncoderSurface();
                encoderWidth = 0;
                encoderHeight = 0;
            }
            pendingEncoderUpdate = false;
        }

        if (frameAvailable) {
            drawFrame(env);
            frameAvailable = false;
        }
    }

    teardownGL();
    eglManager->releaseEGL();

    // Release JNI Global Refs to avoid memory leaks
    env->DeleteGlobalRef(surfaceTextureRef);
    env->DeleteGlobalRef(matrixArrayRef);

    javaVM->DetachCurrentThread();
}

bool Renderer::setupGL() {
    programId = createProgram(VERTEX_SHADER, FRAGMENT_SHADER);
    if (!programId) {
        LOGE("Shader program creation failed");
        return false;
    }

    positionLoc = glGetAttribLocation(programId, "aPosition");
    texCoordLoc = glGetAttribLocation(programId, "aTexCoord");
    matrixLoc = glGetUniformLocation(programId, "uSTMatrix");

    // Geometry data
    const GLfloat POSITIONS[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 0.0f, 1.0f
    };

    const GLfloat TEX_COORDS[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f
    };

    glGenVertexArrays(1, &vaoId);
    glGenBuffers(2, vboId);

    glBindVertexArray(vaoId);

    // Positions VBO
    glBindBuffer(GL_ARRAY_BUFFER, vboId[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(POSITIONS), POSITIONS, GL_STATIC_DRAW);
    glVertexAttribPointer(positionLoc, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(positionLoc);

    // TexCoords VBO
    glBindBuffer(GL_ARRAY_BUFFER, vboId[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(TEX_COORDS), TEX_COORDS, GL_STATIC_DRAW);
    glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(texCoordLoc);

    glBindVertexArray(0);

    // Create camera OES texture ID
    glGenTextures(1, &oesTextureId);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, oesTextureId);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

    // Create FBO texture for AI Model downscaled output (Sink 3)
    glGenTextures(1, &aiTextureId);
    glBindTexture(GL_TEXTURE_2D, aiTextureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, aiWidth, aiHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Bind texture to FBO
    glGenFramebuffers(1, &fboId);
    glBindFramebuffer(GL_FRAMEBUFFER, fboId);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, aiTextureId, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("AI Downscale Framebuffer creation incomplete: %d", status);
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Initialise double PBO buffer readback pipeline
    pboManager->init(aiWidth, aiHeight);

    return true;
}

void Renderer::teardownGL() {
    pboManager->release();

    if (fboId != 0) {
        glDeleteFramebuffers(1, &fboId);
        fboId = 0;
    }
    if (aiTextureId != 0) {
        glDeleteTextures(1, &aiTextureId);
        aiTextureId = 0;
    }
    if (oesTextureId != 0) {
        glDeleteTextures(1, &oesTextureId);
        oesTextureId = 0;
    }
    if (vaoId != 0) {
        glDeleteVertexArrays(1, &vaoId);
        vaoId = 0;
    }
    if (vboId[0] != 0) {
        glDeleteBuffers(2, vboId);
        vboId[0] = 0;
        vboId[1] = 0;
    }
    if (programId != 0) {
        glDeleteProgram(programId);
        programId = 0;
    }
}

void Renderer::drawFrame(JNIEnv* env) {
    // 1. Fetch camera frame onto OES texture
    env->CallVoidMethod(surfaceTextureRef, updateTexImageMethod);
    jlong frameTimestamp = env->CallLongMethod(surfaceTextureRef, getTimestampMethod);
    
    // 2. Fetch transform matrix and update vertex shader
    env->CallVoidMethod(surfaceTextureRef, getTransformMatrixMethod, matrixArrayRef);
    float matrix[16];
    env->GetFloatArrayRegion(matrixArrayRef, 0, 16, matrix);

    // Setup uniform/program states
    glUseProgram(programId);
    glUniformMatrix4fv(matrixLoc, 1, GL_FALSE, matrix);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, oesTextureId);

    glBindVertexArray(vaoId);

    // --- Sink 1: UI Preview rendering ---
    EGLSurface previewSurf = eglManager->getPreviewSurface();
    if (previewSurf != EGL_NO_SURFACE) {
        eglManager->makeCurrent(previewSurf);
        
        // Dynamically request viewport sizes to handle orientation/UI changes gracefully
        EGLint w = 0, h = 0;
        eglQuerySurface(eglManager->getDisplay(), previewSurf, EGL_WIDTH, &w);
        eglQuerySurface(eglManager->getDisplay(), previewSurf, EGL_HEIGHT, &h);
        previewWidth = w;
        previewHeight = h;

        int camW = cameraWidth > 0 ? cameraWidth : 720;
        int camH = cameraHeight > 0 ? cameraHeight : 1280;

        float aspectRatioCamera = (float)camH / (float)camW;
        float aspectRatioViewport = (float)previewWidth / (float)previewHeight;

        int renderX = 0;
        int renderY = 0;
        int renderW = previewWidth;
        int renderH = previewHeight;

        if (aspectRatioViewport < aspectRatioCamera) {
            renderW = (int)(previewHeight * aspectRatioCamera);
            renderX = (previewWidth - renderW) / 2;
        } else {
            renderH = (int)(previewWidth / aspectRatioCamera);
            renderY = (previewHeight - renderH) / 2;
        }

        glViewport(renderX, renderY, renderW, renderH);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        eglManager->swapBuffers(previewSurf);
    }

    // --- Sink 2: Hardware Video Encoder rendering ---
    EGLSurface encoderSurf = eglManager->getEncoderSurface();
    if (encoderSurf != EGL_NO_SURFACE) {
        eglManager->makeCurrent(encoderSurf);
        glViewport(0, 0, encoderWidth, encoderHeight);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        
        // Apply camera timestamp before swap to lock video sync rate correctly
        eglManager->setPresentationTime(encoderSurf, frameTimestamp);
        eglManager->swapBuffers(encoderSurf);
    }

    // --- Sink 3: AI Inference FBO offscreen rendering ---
    // Make dummy surface current to satisfy thread render state, draw to FBO
    eglManager->makeCurrent(eglManager->getDummySurface());

    // Dynamically resize AI frame buffer to match active camera resolution (e.g., 1080P/720P)
    int targetW = cameraWidth > 0 ? cameraWidth : aiWidth;
    int targetH = cameraHeight > 0 ? cameraHeight : aiHeight;
    if (aiWidth != targetW || aiHeight != targetH) {
        LOGD("AI frame size change detected. Recreating FBO/PBO from %dx%d to %dx%d", 
             aiWidth, aiHeight, targetW, targetH);
        aiWidth = targetW;
        aiHeight = targetH;
        
        // Re-initialize AI task thread-local buffers
        aiTask->resize(aiWidth, aiHeight);
        
        // Recreate FBO 2D texture
        glDeleteTextures(1, &aiTextureId);
        glGenTextures(1, &aiTextureId);
        glBindTexture(GL_TEXTURE_2D, aiTextureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, aiWidth, aiHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        
        glBindFramebuffer(GL_FRAMEBUFFER, fboId);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, aiTextureId, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        // Re-initialize PBOManager for the new dimensions
        pboManager->init(aiWidth, aiHeight);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fboId);
    glViewport(0, 0, aiWidth, aiHeight);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Perform asynchronous PBO double-buffered readback
    uint8_t* aiRgbBuffer = pboManager->readbackFrameAsync(fboId);
    if (aiRgbBuffer != nullptr) {
        // Forward RGB data to AI thread
        aiTask->submitFrame(aiRgbBuffer);
    }

    // Cleanup bindings
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
    glUseProgram(0);
}
