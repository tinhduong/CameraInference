#pragma once
#include <atomic>
#include <cstdint>

struct PipelineStats {
    std::atomic<uint32_t> incomingFps{0};
    std::atomic<uint32_t> processedFps{0};
    std::atomic<uint32_t> droppedFrames{0};
    std::atomic<uint32_t> queueDepth{0};
    std::atomic<float> avgInferenceMs{0.0f};
    std::atomic<float> avgEncodeMs{0.0f};
    std::atomic<float> avgGpuConversionMs{0.0f};
    std::atomic<float> avgGpuReadbackMs{0.0f};
    std::atomic<float> avgEndToEndLatencyMs{0.0f};
    std::atomic<uint32_t> photoSuccessCount{0};
    std::atomic<uint32_t> photoFailureCount{0};
    std::atomic<uint32_t> videoSuccessCount{0};
    std::atomic<uint32_t> videoFailureCount{0};

    PipelineStats() = default;

    // Custom copy constructor because std::atomic is not copy-constructible
    PipelineStats(const PipelineStats& other) {
        incomingFps = other.incomingFps.load();
        processedFps = other.processedFps.load();
        droppedFrames = other.droppedFrames.load();
        queueDepth = other.queueDepth.load();
        avgInferenceMs = other.avgInferenceMs.load();
        avgEncodeMs = other.avgEncodeMs.load();
        avgGpuConversionMs = other.avgGpuConversionMs.load();
        avgGpuReadbackMs = other.avgGpuReadbackMs.load();
        avgEndToEndLatencyMs = other.avgEndToEndLatencyMs.load();
        photoSuccessCount = other.photoSuccessCount.load();
        photoFailureCount = other.photoFailureCount.load();
        videoSuccessCount = other.videoSuccessCount.load();
        videoFailureCount = other.videoFailureCount.load();
    }

    // Custom copy assignment operator
    PipelineStats& operator=(const PipelineStats& other) {
        if (this != &other) {
            incomingFps = other.incomingFps.load();
            processedFps = other.processedFps.load();
            droppedFrames = other.droppedFrames.load();
            queueDepth = other.queueDepth.load();
            avgInferenceMs = other.avgInferenceMs.load();
            avgEncodeMs = other.avgEncodeMs.load();
            avgGpuConversionMs = other.avgGpuConversionMs.load();
            avgGpuReadbackMs = other.avgGpuReadbackMs.load();
            avgEndToEndLatencyMs = other.avgEndToEndLatencyMs.load();
            photoSuccessCount = other.photoSuccessCount.load();
            photoFailureCount = other.photoFailureCount.load();
            videoSuccessCount = other.videoSuccessCount.load();
            videoFailureCount = other.videoFailureCount.load();
        }
        return *this;
    }

    void clear() {
        incomingFps = 0;
        processedFps = 0;
        droppedFrames = 0;
        queueDepth = 0;
        avgInferenceMs = 0.0f;
        avgEncodeMs = 0.0f;
        avgGpuConversionMs = 0.0f;
        avgGpuReadbackMs = 0.0f;
        avgEndToEndLatencyMs = 0.0f;
        photoSuccessCount = 0;
        photoFailureCount = 0;
        videoSuccessCount = 0;
        videoFailureCount = 0;
    }
};
