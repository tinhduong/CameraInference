#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include "EGLManager.h"
#include "PBOManager.h"
#include "AITask.h"
#include "Renderer.h"

#define TAG "native-lib"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// Root orchestrator grouping the GPU render loop, double PBO managers, and AI worker thread
struct Engine {
    EGLManager egl;
    PBOManager pbo;
    AITask ai;
    Renderer renderer;
    int aiWidth;
    int aiHeight;

    Engine(int aiW, int aiH) : aiWidth(aiW), aiHeight(aiH) {
        renderer.init(&egl, &pbo, &ai);
    }
};

// Global environment cache
static JavaVM* gJavaVM = nullptr;
static jclass gBridgeClassGlobal = nullptr;
static jmethodID gCallbackMethodId = nullptr;

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    gJavaVM = vm;
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        LOGE("GetEnv failed in JNI_OnLoad");
        return JNI_ERR;
    }

    jclass localClass = env->FindClass("com/example/camerapipe/JniBridge");
    if (!localClass) {
        LOGE("Failed to find JniBridge Kotlin class");
        return JNI_ERR;
    }

    // Cache the class global reference (avoids ClassLoader failure on background threads)
    gBridgeClassGlobal = reinterpret_cast<jclass>(env->NewGlobalRef(localClass));
    env->DeleteLocalRef(localClass);

    gCallbackMethodId = env->GetStaticMethodID(
        gBridgeClassGlobal, "triggerAiCallback", "(Ljava/lang/String;FJ)V");
    if (!gCallbackMethodId) {
        LOGE("Failed to find triggerAiCallback static method ID");
        return JNI_ERR;
    }

    LOGD("native-lib JNI_OnLoad configured successfully");
    return JNI_VERSION_1_6;
}

JNIEXPORT jlong JNICALL
Java_com_example_camerapipe_JniBridge_nativeInit(JNIEnv* env, jobject thiz, jint aiWidth, jint aiHeight) {
    Engine* engine = new Engine(aiWidth, aiHeight);
    LOGD("nativeInit: Engine instance created.");
    return reinterpret_cast<jlong>(engine);
}

JNIEXPORT void JNICALL
Java_com_example_camerapipe_JniBridge_nativeRelease(JNIEnv* env, jobject thiz, jlong enginePtr) {
    Engine* engine = reinterpret_cast<Engine*>(enginePtr);
    delete engine;
    LOGD("nativeRelease: Engine instance released.");
}

JNIEXPORT void JNICALL
Java_com_example_camerapipe_JniBridge_nativeOnSurfaceCreated(JNIEnv* env, jobject thiz, jlong enginePtr, jobject surface) {
    Engine* engine = reinterpret_cast<Engine*>(enginePtr);
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (window) {
        engine->renderer.setPreviewWindow(window);
    } else {
        LOGE("nativeOnSurfaceCreated: Could not construct ANativeWindow from Java Surface");
    }
}

JNIEXPORT void JNICALL
Java_com_example_camerapipe_JniBridge_nativeOnSurfaceDestroyed(JNIEnv* env, jobject thiz, jlong enginePtr) {
    Engine* engine = reinterpret_cast<Engine*>(enginePtr);
    engine->renderer.setPreviewWindow(nullptr);
}

JNIEXPORT void JNICALL
Java_com_example_camerapipe_JniBridge_nativeSetEncoderSurface(JNIEnv* env, jobject thiz, jlong enginePtr, jobject surface, jint width, jint height) {
    Engine* engine = reinterpret_cast<Engine*>(enginePtr);
    if (surface) {
        ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
        if (window) {
            engine->renderer.setEncoderWindow(window, width, height);
        } else {
            LOGE("nativeSetEncoderSurface: Could not construct ANativeWindow");
        }
    } else {
        engine->renderer.setEncoderWindow(nullptr, 0, 0);
    }
}

JNIEXPORT jint JNICALL
Java_com_example_camerapipe_JniBridge_nativeStartPipeline(JNIEnv* env, jobject thiz, jlong enginePtr, jobject surfaceTexture) {
    Engine* engine = reinterpret_cast<Engine*>(enginePtr);
    
    // 1. Start AI background processor thread
    engine->ai.start(gJavaVM, gBridgeClassGlobal, gCallbackMethodId, engine->aiWidth, engine->aiHeight);
    
    // 2. Start the primary Render Loop thread (which generates and returns the GL Texture ID)
    int textureId = engine->renderer.start(gJavaVM, surfaceTexture, engine->aiWidth, engine->aiHeight);
    
    return textureId;
}

JNIEXPORT void JNICALL
Java_com_example_camerapipe_JniBridge_nativeStopPipeline(JNIEnv* env, jobject thiz, jlong enginePtr) {
    Engine* engine = reinterpret_cast<Engine*>(enginePtr);
    engine->renderer.stop();
    engine->ai.stop();
}

JNIEXPORT void JNICALL
Java_com_example_camerapipe_JniBridge_nativeOnFrameAvailable(JNIEnv* env, jobject thiz, jlong enginePtr) {
    Engine* engine = reinterpret_cast<Engine*>(enginePtr);
    engine->renderer.onFrameAvailable();
}

JNIEXPORT void JNICALL
Java_com_example_camerapipe_JniBridge_nativeSetCameraResolution(JNIEnv* env, jobject thiz, jlong enginePtr, jint width, jint height) {
    Engine* engine = reinterpret_cast<Engine*>(enginePtr);
    engine->renderer.setCameraResolution(width, height);
}

} // extern "C"
