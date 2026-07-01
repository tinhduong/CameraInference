#include <jni.h>
#include "pipeline/NativePipelineManager.h"
#include <mutex>
#include <vector>
#include <android/log.h>

#define LOG_TAG "native-lib"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static NativePipelineManager* gPipelineManager = nullptr;
static std::mutex gManagerMutex;

// Helper to parse Kotlin NativeConfig class into C++ struct
NativePipelineConfig parseConfig(JNIEnv* env, jobject jConfig) {
    jclass clazz = env->GetObjectClass(jConfig);
    
    jfieldID fMode = env->GetFieldID(clazz, "mode", "I");
    jfieldID fRgbMode = env->GetFieldID(clazz, "rgbMode", "I");
    jfieldID fAiEnabled = env->GetFieldID(clazz, "aiEnabled", "Z");
    jfieldID fEncodeEnabled = env->GetFieldID(clazz, "encodeEnabled", "Z");
    jfieldID fGpuConversionEnabled = env->GetFieldID(clazz, "gpuConversionEnabled", "Z");
    jfieldID fQueueCapacity = env->GetFieldID(clazz, "queueCapacity", "I");
    jfieldID fDropPolicy = env->GetFieldID(clazz, "dropPolicy", "I");
    jfieldID fMockInference = env->GetFieldID(clazz, "mockInferenceDelayMs", "I");
    jfieldID fMockEncode = env->GetFieldID(clazz, "mockEncodeDelayMs", "I");

    NativePipelineConfig config;
    config.mode = static_cast<PipelineMode>(env->GetIntField(jConfig, fMode));
    config.rgbMode = static_cast<RgbOutputMode>(env->GetIntField(jConfig, fRgbMode));
    config.aiEnabled = env->GetBooleanField(jConfig, fAiEnabled);
    config.encodeEnabled = env->GetBooleanField(jConfig, fEncodeEnabled);
    config.gpuConversionEnabled = env->GetBooleanField(jConfig, fGpuConversionEnabled);
    config.queueCapacity = static_cast<size_t>(env->GetIntField(jConfig, fQueueCapacity));
    config.dropPolicy = static_cast<FrameDropPolicy>(env->GetIntField(jConfig, fDropPolicy));
    config.mockInferenceDelayMs = env->GetIntField(jConfig, fMockInference);
    config.mockEncodeDelayMs = env->GetIntField(jConfig, fMockEncode);

    return config;
}

// Helper to create Kotlin InferenceResult from C++ FramePacket
jobject buildInferenceResult(JNIEnv* env, const FramePacket& packet) {
    jclass listClass = env->FindClass("java/util/ArrayList");
    jmethodID listConstructor = env->GetMethodID(listClass, "<init>", "()V");
    jmethodID listAdd = env->GetMethodID(listClass, "add", "(Ljava/lang/Object;)Z");
    jobject listObj = env->NewObject(listClass, listConstructor);

    jclass detClass = env->FindClass("com/example/airealtime/core/nativebridge/model/DetectionItem");
    jmethodID detConstructor = env->GetMethodID(detClass, "<init>", "(FFFFFILjava/lang/String;)V");

    for (const auto& det : packet.detections) {
        jstring labelStr = env->NewStringUTF(det.label.c_str());
        jobject detObj = env->NewObject(detClass, detConstructor,
            det.xMin, det.yMin, det.xMax, det.yMax,
            det.confidence, det.labelId, labelStr
        );
        env->CallBooleanMethod(listObj, listAdd, detObj);
        env->DeleteLocalRef(labelStr);
        env->DeleteLocalRef(detObj);
    }

    jclass resultClass = env->FindClass("com/example/airealtime/core/nativebridge/model/InferenceResult");
    jmethodID resultConstructor = env->GetMethodID(resultClass, "<init>", "(JJIIIFLjava/util/List;)V");
    
    jobject resultObj = env->NewObject(resultClass, resultConstructor,
        static_cast<jlong>(packet.frameId),
        static_cast<jlong>(packet.timestampNs),
        packet.width,
        packet.height,
        packet.rotationDegrees,
        static_cast<jfloat>(packet.timeEndToEndMs),
        listObj
    );

    return resultObj;
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_example_airealtime_core_nativebridge_NativeBridge_initNativePipeline(
    JNIEnv* env, jobject thiz, jobject jConfig) {
    std::lock_guard<std::mutex> lock(gManagerMutex);
    if (!gPipelineManager) {
        gPipelineManager = new NativePipelineManager();
    }
    NativePipelineConfig config = parseConfig(env, jConfig);
    return gPipelineManager->init(config) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_example_airealtime_core_nativebridge_NativeBridge_startNativePipeline(
    JNIEnv* env, jobject thiz) {
    std::lock_guard<std::mutex> lock(gManagerMutex);
    if (gPipelineManager) {
        return gPipelineManager->start() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_example_airealtime_core_nativebridge_NativeBridge_stopNativePipeline(
    JNIEnv* env, jobject thiz) {
    std::lock_guard<std::mutex> lock(gManagerMutex);
    if (gPipelineManager) {
        gPipelineManager->stop();
    }
}

JNIEXPORT void JNICALL
Java_com_example_airealtime_core_nativebridge_NativeBridge_releaseNativePipeline(
    JNIEnv* env, jobject thiz) {
    std::lock_guard<std::mutex> lock(gManagerMutex);
    if (gPipelineManager) {
        gPipelineManager->release();
        delete gPipelineManager;
        gPipelineManager = nullptr;
    }
}

JNIEXPORT void JNICALL
Java_com_example_airealtime_core_nativebridge_NativeBridge_updateNativeConfig(
    JNIEnv* env, jobject thiz, jobject jConfig) {
    std::lock_guard<std::mutex> lock(gManagerMutex);
    if (gPipelineManager) {
        NativePipelineConfig config = parseConfig(env, jConfig);
        gPipelineManager->updateConfig(config);
    }
}

JNIEXPORT jobject JNICALL
Java_com_example_airealtime_core_nativebridge_NativeBridge_processFrameSync(
    JNIEnv* env, jobject thiz, jlong frameId, jlong timestampNs, jint width, jint height,
    jint rotationDegrees, jint format, jobject yBuf, jint yRowStride, jint yPixelStride,
    jobject uBuf, jint uRowStride, jint uPixelStride, jobject vBuf, jint vRowStride, jint vPixelStride) {
    
    std::lock_guard<std::mutex> lock(gManagerMutex);
    if (!gPipelineManager) return nullptr;

    const uint8_t* yData = static_cast<const uint8_t*>(env->GetDirectBufferAddress(yBuf));
    const uint8_t* uData = static_cast<const uint8_t*>(env->GetDirectBufferAddress(uBuf));
    const uint8_t* vData = static_cast<const uint8_t*>(env->GetDirectBufferAddress(vBuf));

    if (!yData || !uData || !vData) {
        LOGE("Failed to get direct buffer addresses from JNI");
        return nullptr;
    }

    FramePacket result(0, 0, 0, 0, 0, 0);
    bool success = gPipelineManager->processFrameSync(
        frameId, timestampNs, width, height, rotationDegrees, format,
        yData, yRowStride, yPixelStride,
        uData, uRowStride, uPixelStride,
        vData, vRowStride, vPixelStride,
        result
    );

    if (!success) return nullptr;

    return buildInferenceResult(env, result);
}

JNIEXPORT jboolean JNICALL
Java_com_example_airealtime_core_nativebridge_NativeBridge_enqueueFrameAsync(
    JNIEnv* env, jobject thiz, jlong frameId, jlong timestampNs, jint width, jint height,
    jint rotationDegrees, jint format, jobject yBuf, jint yRowStride, jint yPixelStride,
    jobject uBuf, jint uRowStride, jint uPixelStride, jobject vBuf, jint vRowStride, jint vPixelStride) {
    
    std::lock_guard<std::mutex> lock(gManagerMutex);
    if (!gPipelineManager) return JNI_FALSE;

    const uint8_t* yData = static_cast<const uint8_t*>(env->GetDirectBufferAddress(yBuf));
    const uint8_t* uData = static_cast<const uint8_t*>(env->GetDirectBufferAddress(uBuf));
    const uint8_t* vData = static_cast<const uint8_t*>(env->GetDirectBufferAddress(vBuf));

    if (!yData || !uData || !vData) {
        LOGE("Failed to get direct buffer address from JNI async");
        return JNI_FALSE;
    }

    bool success = gPipelineManager->enqueueFrameAsync(
        frameId, timestampNs, width, height, rotationDegrees, format,
        yData, yRowStride, yPixelStride,
        uData, uRowStride, uPixelStride,
        vData, vRowStride, vPixelStride
    );

    return success ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobject JNICALL
Java_com_example_airealtime_core_nativebridge_NativeBridge_getLatestResult(
    JNIEnv* env, jobject thiz) {
    std::lock_guard<std::mutex> lock(gManagerMutex);
    if (!gPipelineManager) return nullptr;

    FramePacket packet(0, 0, 0, 0, 0, 0);
    if (gPipelineManager->getLatestResult(packet)) {
        return buildInferenceResult(env, packet);
    }
    return nullptr;
}

JNIEXPORT jobject JNICALL
Java_com_example_airealtime_core_nativebridge_NativeBridge_getNativeStats(
    JNIEnv* env, jobject thiz) {
    std::lock_guard<std::mutex> lock(gManagerMutex);
    if (!gPipelineManager) return nullptr;

    PipelineStats stats = gPipelineManager->getStats();

    jclass statsClass = env->FindClass("com/example/airealtime/core/nativebridge/model/NativeStats");
    jmethodID statsConstructor = env->GetMethodID(statsClass, "<init>", "(IIIIFFFFF)V");
    
    return env->NewObject(statsClass, statsConstructor,
        static_cast<jint>(stats.incomingFps.load()),
        static_cast<jint>(stats.processedFps.load()),
        static_cast<jint>(stats.droppedFrames.load()),
        static_cast<jint>(stats.queueDepth.load()),
        static_cast<jfloat>(stats.avgInferenceMs.load()),
        static_cast<jfloat>(stats.avgEncodeMs.load()),
        static_cast<jfloat>(stats.avgGpuConversionMs.load()),
        static_cast<jfloat>(stats.avgGpuReadbackMs.load()),
        static_cast<jfloat>(stats.avgEndToEndLatencyMs.load())
    );
}

JNIEXPORT void JNICALL
Java_com_example_airealtime_core_nativebridge_NativeBridge_clearNativeStats(
    JNIEnv* env, jobject thiz) {
    std::lock_guard<std::mutex> lock(gManagerMutex);
    if (gPipelineManager) {
        gPipelineManager->clearStats();
    }
}

}
