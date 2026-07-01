#include "MockInferenceEngine.h"
#include <thread>
#include <chrono>
#include <cmath>
#include <android/log.h>

#define LOG_TAG "MockInferenceEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

MockInferenceEngine::MockInferenceEngine() {}

MockInferenceEngine::~MockInferenceEngine() {
    release();
}

bool MockInferenceEngine::init() {
    LOGI("MockInferenceEngine initialized");
    return true;
}

void MockInferenceEngine::release() {
    LOGI("MockInferenceEngine released");
}

void MockInferenceEngine::setLatencyDelayMs(int delayMs) {
    latencyDelayMs_ = delayMs;
}

bool MockInferenceEngine::runInference(FramePacket* packet) {
    if (!packet) return false;

    auto tStart = std::chrono::steady_clock::now();

    int delay = latencyDelayMs_.load();
    if (delay > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }

    // Generate circling box based on timestamp
    double seconds = static_cast<double>(packet->timestampNs) / 1e9;
    float cx = 0.5f + 0.2f * static_cast<float>(std::cos(seconds));
    float cy = 0.5f + 0.2f * static_cast<float>(std::sin(seconds));
    float size = 0.12f;

    Detection det1;
    det1.xMin = cx - size;
    det1.yMin = cy - size;
    det1.xMax = cx + size;
    det1.yMax = cy + size;
    det1.confidence = 0.91f;
    det1.labelId = 0;
    det1.label = "Person";

    Detection det2;
    det2.xMin = 0.15f;
    det2.yMin = 0.15f;
    det2.xMax = 0.35f;
    det2.yMax = 0.35f;
    det2.confidence = 0.81f;
    det2.labelId = 1;
    det2.label = "Phone";

    packet->detections.push_back(det1);
    packet->detections.push_back(det2);

    auto tEnd = std::chrono::steady_clock::now();
    packet->timeInferenceMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    return true;
}
