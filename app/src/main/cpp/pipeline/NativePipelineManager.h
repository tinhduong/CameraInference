#pragma once
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include "FrameQueue.h"
#include "PipelineStats.h"
#include "../gpu/EglCore.h"
#include "../gpu/YuvToRgbConverter.h"
#include "../gpu/GlTextureManager.h"
#include "../inference/IInferenceEngine.h"
#include "../encoder/IFrameEncoder.h"

enum class PipelineMode {
    SYNC = 0,
    ASYNC = 1
};

enum class RgbOutputMode {
    GPU_TEXTURE = 0,
    CPU_RGBA_FROM_GPU = 1
};

struct NativePipelineConfig {
    PipelineMode mode = PipelineMode::ASYNC;
    RgbOutputMode rgbMode = RgbOutputMode::GPU_TEXTURE;
    bool aiEnabled = true;
    bool encodeEnabled = true;
    bool gpuConversionEnabled = true;
    size_t queueCapacity = 3;
    FrameDropPolicy dropPolicy = FrameDropPolicy::DROP_OLDEST;
    int mockInferenceDelayMs = 10;
    int mockEncodeDelayMs = 5;
};

class NativePipelineManager {
public:
    NativePipelineManager();
    ~NativePipelineManager();

    bool init(const NativePipelineConfig& config);
    void release();

    bool start();
    void stop();

    void updateConfig(const NativePipelineConfig& config);

    // Ingest frame synchronously
    bool processFrameSync(
        uint64_t frameId, uint64_t timestampNs,
        int width, int height, int rotationDegrees, int format,
        const uint8_t* yData, int yRowStride, int yPixelStride,
        const uint8_t* uData, int uRowStride, int uPixelStride,
        const uint8_t* vData, int vRowStride, int vPixelStride,
        FramePacket& outResult
    );

    // Ingest frame asynchronously (enqueues copy)
    bool enqueueFrameAsync(
        uint64_t frameId, uint64_t timestampNs,
        int width, int height, int rotationDegrees, int format,
        const uint8_t* yData, int yRowStride, int yPixelStride,
        const uint8_t* uData, int uRowStride, int uPixelStride,
        const uint8_t* vData, int vRowStride, int vPixelStride
    );

    PipelineStats getStats() const;
    void clearStats();
    
    // Gets the latest processed frame result for polling
    bool getLatestResult(FramePacket& outPacket);

private:
    void threadLoop();
    void processFrameInternal(FramePacket* packet);
    void updateTimingAverages(const FramePacket* packet);

    NativePipelineConfig config_;
    std::atomic<bool> isRunning_{false};
    std::atomic<bool> isEglInitialized_{false};
    
    std::mutex configMutex_;
    PipelineStats stats_;
    std::atomic<uint64_t> frameCounter_{0};
    
    std::mutex latestResultMutex_;
    std::unique_ptr<FramePacket> latestResultPacket_;

    std::unique_ptr<FrameQueue> frameQueue_;
    std::thread pipelineThread_;

    // GPU Context
    EglCore eglCore_;
    EGLSurface eglSurface_ = EGL_NO_SURFACE;
    YuvToRgbConverter converter_;
    GlTextureManager textureManager_;

    // Inference & Encoder engines
    std::unique_ptr<IInferenceEngine> inferenceEngine_;
    std::unique_ptr<IFrameEncoder> frameEncoder_;

    // Timing tracking
    std::chrono::steady_clock::time_point lastIncomingTime_;
    std::chrono::steady_clock::time_point lastProcessedTime_;
    uint32_t incomingFrameCount_ = 0;
    uint32_t processedFrameCount_ = 0;
};
