#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <chrono>

struct PlaneMetadata {
    uint32_t rowStride;
    uint32_t pixelStride;
    uint32_t length;
};

struct Detection {
    float xMin;
    float yMin;
    float xMax;
    float yMax;
    float confidence;
    int labelId;
    std::string label;
};

struct FramePacket {
    uint64_t frameId;
    uint64_t timestampNs;
    int width;
    int height;
    int rotationDegrees;
    int format; // YUV_420_888

    // Timing metrics in milliseconds
    double timeGpuConversionMs = 0.0;
    double timeInferenceMs = 0.0;
    double timeEncodeMs = 0.0;
    double timeGpuReadbackMs = 0.0;
    double timeEndToEndMs = 0.0;

    std::chrono::steady_clock::time_point startTime;

    // Buffer data copied for async pipeline
    std::vector<uint8_t> yBuffer;
    std::vector<uint8_t> uBuffer;
    std::vector<uint8_t> vBuffer;

    PlaneMetadata yPlane;
    PlaneMetadata uPlane;
    PlaneMetadata vPlane;

    // GPU-backed representations
    uint32_t gpuTextureId = 0;
    std::vector<uint8_t> cpuRgbBuffer; // CPU-visible RGBA buffer read back from GPU

    // Inference outputs
    std::vector<Detection> detections;

    FramePacket(uint64_t fid, uint64_t ts, int w, int h, int rot, int fmt);
    ~FramePacket();
};
