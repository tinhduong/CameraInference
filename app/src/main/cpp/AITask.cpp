#include "AITask.h"
#include <android/log.h>
#include <chrono>

#define TAG "AITask"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

AITask::AITask() :
    javaVM(nullptr),
    jniBridgeClass(nullptr),
    callbackMethod(nullptr),
    width(0),
    height(0),
    rgbSize(0),
    isRunning(false),
    hasNewFrame(false) {}

AITask::~AITask() {
    stop();
}

void AITask::start(JavaVM* vm, jclass bridgeClassGlobal, jmethodID callbackMethodId, int w, int h) {
    stop();

    javaVM = vm;
    jniBridgeClass = bridgeClassGlobal;
    callbackMethod = callbackMethodId;
    width = w;
    height = h;
    rgbSize = width * height * 3;

    sharedRgbBuffer.resize(rgbSize);
    threadLocalBuffer.resize(rgbSize);
    hasNewFrame = false;
    isRunning = true;

    workerThread = std::thread(&AITask::threadLoop, this);
    LOGD("AI Worker Thread started.");
}

void AITask::stop() {
    if (isRunning) {
        isRunning = false;
        threadCv.notify_one();
        if (workerThread.joinable()) {
            workerThread.join();
        }
        LOGD("AI Worker Thread stopped.");
    }
}

void AITask::resize(int w, int h) {
    std::lock_guard<std::mutex> procLock(processingMutex);
    std::lock_guard<std::mutex> lock(threadMutex);
    if (width == w && height == h) return;
    width = w;
    height = h;
    rgbSize = width * height * 3;
    sharedRgbBuffer.resize(rgbSize);
    threadLocalBuffer.resize(rgbSize);
    hasNewFrame = false;
    LOGD("AITask resized to: %dx%d", w, h);
}

void AITask::submitFrame(const uint8_t* rgbBuffer) {
    if (!isRunning || rgbBuffer == nullptr) return;

    {
        std::lock_guard<std::mutex> lock(threadMutex);
        std::memcpy(sharedRgbBuffer.data(), rgbBuffer, rgbSize);
        hasNewFrame = true;
    }
    threadCv.notify_one();
}

void AITask::threadLoop() {
    JNIEnv* env = nullptr;
    // Attach background thread to JVM so we can execute callbacks
    jint res = javaVM->AttachCurrentThread(&env, nullptr);
    if (res != JNI_OK || env == nullptr) {
        LOGE("Failed to attach AI worker thread to JVM");
        return;
    }

    while (isRunning) {
        std::unique_lock<std::mutex> lock(threadMutex);
        threadCv.wait(lock, [this]() { return !isRunning || hasNewFrame; });

        if (!isRunning) {
            break;
        }

        // Copy shared buffer to thread-local buffer inside the lock,
        // then unlock immediately to allow the GPU render thread to submit frames without blocking.
        std::memcpy(threadLocalBuffer.data(), sharedRgbBuffer.data(), rgbSize);
        
        int activeW = width;
        int activeH = height;
        
        hasNewFrame = false;
        lock.unlock();

        {
            std::lock_guard<std::mutex> procLock(processingMutex);
            if (!isRunning) break;
            
            auto startTime = std::chrono::high_resolution_clock::now();

            std::string label;
            float score = 0.0f;
            runDummyInference(threadLocalBuffer.data(), activeW, activeH, label, score);

            auto endTime = std::chrono::high_resolution_clock::now();
            auto latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

            // Send results back to Kotlin UI layer via JNI static call
            jstring jlabel = env->NewStringUTF(label.c_str());
            env->CallStaticVoidMethod(jniBridgeClass, callbackMethod, jlabel, (jfloat)score, (jlong)latencyMs);
            env->DeleteLocalRef(jlabel);
        }
    }

    javaVM->DetachCurrentThread();
}

void AITask::runDummyInference(const uint8_t* rgbData, int w, int h, std::string& outLabel, float& outScore) {
    // Simply simulate the partner's model execution time of 40ms
    std::this_thread::sleep_for(std::chrono::milliseconds(40));

    // Simple pseudo-random mock output generation
    static int toggleCounter = 0;
    toggleCounter = (toggleCounter + 1) % 3;
    if (toggleCounter == 0) {
        outLabel = "Mobile Device Interface";
        outScore = 0.91f;
    } else if (toggleCounter == 1) {
        outLabel = "Human Face Context";
        outScore = 0.85f;
    } else {
        outLabel = "Office Interior / Keyboard";
        outScore = 0.79f;
    }

    LOGD("runDummyInference simulated (%dx%d): 40ms", w, h);
}
