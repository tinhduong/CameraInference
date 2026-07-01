#include "MockEncoder.h"
#include <thread>
#include <chrono>
#include <android/log.h>

#define LOG_TAG "MockEncoder"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

MockEncoder::MockEncoder() {}

MockEncoder::~MockEncoder() {
    release();
}

bool MockEncoder::init() {
    LOGI("MockEncoder initialized");
    return true;
}

void MockEncoder::release() {
    LOGI("MockEncoder released");
}

void MockEncoder::setLatencyDelayMs(int delayMs) {
    latencyDelayMs_ = delayMs;
}

bool MockEncoder::encodeFrame(FramePacket* packet) {
    if (!packet) return false;

    auto tStart = std::chrono::steady_clock::now();

    int delay = latencyDelayMs_.load();
    if (delay > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }

    auto tEnd = std::chrono::steady_clock::now();
    packet->timeEncodeMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    return true;
}
