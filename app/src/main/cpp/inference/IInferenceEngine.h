#pragma once
#include "../pipeline/FramePacket.h"

class IInferenceEngine {
public:
    virtual ~IInferenceEngine() = default;
    virtual bool init() = 0;
    virtual void release() = 0;
    
    // Configure delay for simulation
    virtual void setLatencyDelayMs(int delayMs) = 0;

    // Execute model forward pass on frame
    virtual bool runInference(FramePacket* packet) = 0;
};
