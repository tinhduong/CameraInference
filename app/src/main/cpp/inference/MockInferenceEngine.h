#pragma once
#include "IInferenceEngine.h"
#include <atomic>

class MockInferenceEngine : public IInferenceEngine {
public:
    MockInferenceEngine();
    ~MockInferenceEngine() override;

    bool init() override;
    void release() override;
    void setLatencyDelayMs(int delayMs) override;
    bool runInference(FramePacket* packet) override;

private:
    std::atomic<int> latencyDelayMs_{10}; // 10ms default delay
};
