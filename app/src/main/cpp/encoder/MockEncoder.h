#pragma once
#include "IFrameEncoder.h"
#include <atomic>

class MockEncoder : public IFrameEncoder {
public:
    MockEncoder();
    ~MockEncoder() override;

    bool init() override;
    void release() override;
    void setLatencyDelayMs(int delayMs) override;
    bool encodeFrame(FramePacket* packet) override;

private:
    std::atomic<int> latencyDelayMs_{5}; // 5ms default delay
};
