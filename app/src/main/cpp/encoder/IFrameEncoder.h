#pragma once
#include "../pipeline/FramePacket.h"

class IFrameEncoder {
public:
    virtual ~IFrameEncoder() = default;
    virtual bool init() = 0;
    virtual void release() = 0;
    virtual void setLatencyDelayMs(int delayMs) = 0;
    
    // Simulates or writes frame encoding
    virtual bool encodeFrame(FramePacket* packet) = 0;
};
