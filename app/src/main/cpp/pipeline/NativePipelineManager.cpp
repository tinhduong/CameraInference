#include "NativePipelineManager.h"
#include "../inference/MockInferenceEngine.h"
#include "../encoder/MockEncoder.h"
#include <android/log.h>
#include <chrono>

#define LOG_TAG "NativePipelineManager"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

NativePipelineManager::NativePipelineManager() {
    stats_.clear();
}

NativePipelineManager::~NativePipelineManager() {
    release();
}

bool NativePipelineManager::init(const NativePipelineConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;

    frameQueue_ = std::make_unique<FrameQueue>(config_.queueCapacity);

    inferenceEngine_ = std::make_unique<MockInferenceEngine>();
    if (!inferenceEngine_->init()) {
        LOGE("Failed to initialize inference engine");
        return false;
    }
    inferenceEngine_->setLatencyDelayMs(config_.mockInferenceDelayMs);

    frameEncoder_ = std::make_unique<MockEncoder>();
    if (!frameEncoder_->init()) {
        LOGE("Failed to initialize frame encoder");
        return false;
    }
    frameEncoder_->setLatencyDelayMs(config_.mockEncodeDelayMs);

    LOGI("NativePipelineManager initialized");
    return true;
}

void NativePipelineManager::release() {
    stop();
    
    std::lock_guard<std::mutex> lock(configMutex_);
    if (inferenceEngine_) {
        inferenceEngine_->release();
        inferenceEngine_.reset();
    }
    if (frameEncoder_) {
        frameEncoder_->release();
        frameEncoder_.reset();
    }
    frameQueue_.reset();
    LOGI("NativePipelineManager released");
}

bool NativePipelineManager::start() {
    if (isRunning_.load()) {
        LOGI("Pipeline already running");
        return true;
    }

    isRunning_ = true;
    stats_.clear();
    
    auto now = std::chrono::steady_clock::now();
    lastIncomingTime_ = now;
    lastProcessedTime_ = now;
    incomingFrameCount_ = 0;
    processedFrameCount_ = 0;

    std::lock_guard<std::mutex> lock(configMutex_);
    if (config_.mode == PipelineMode::ASYNC) {
        pipelineThread_ = std::thread(&NativePipelineManager::threadLoop, this);
    } else {
        // Sync mode: initialize EGL on the caller thread
        if (!eglCore_.init()) {
            LOGE("Failed to initialize EGL in sync mode");
            isRunning_ = false;
            return false;
        }
        eglSurface_ = eglCore_.createPbufferSurface(1, 1);
        if (!eglCore_.makeCurrent(eglSurface_)) {
            LOGE("Failed to bind EGL context in sync mode");
            eglCore_.destroySurface(eglSurface_);
            eglCore_.release();
            isRunning_ = false;
            return false;
        }
        if (!converter_.init()) {
            LOGE("Failed to init GPU converter in sync mode");
            eglCore_.makeUncurrent();
            eglCore_.destroySurface(eglSurface_);
            eglCore_.release();
            isRunning_ = false;
            return false;
        }
        if (!textureManager_.init()) {
            LOGE("Failed to init texture manager in sync mode");
            converter_.release();
            eglCore_.makeUncurrent();
            eglCore_.destroySurface(eglSurface_);
            eglCore_.release();
            isRunning_ = false;
            return false;
        }
        isEglInitialized_ = true;
        LOGI("EGL initialized in sync mode");
    }

    return true;
}

void NativePipelineManager::stop() {
    if (!isRunning_.load()) return;

    isRunning_ = false;

    if (frameQueue_) {
        frameQueue_->shutdown();
    }

    if (pipelineThread_.joinable()) {
        pipelineThread_.join();
    }

    // If EGL is initialized (which occurs in the SYNC caller thread)
    if (isEglInitialized_.load()) {
        eglCore_.makeCurrent(eglSurface_);
        textureManager_.release();
        converter_.release();
        eglCore_.makeUncurrent();
        if (eglSurface_ != EGL_NO_SURFACE) {
            eglCore_.destroySurface(eglSurface_);
            eglSurface_ = EGL_NO_SURFACE;
        }
        eglCore_.release();
        isEglInitialized_ = false;
        LOGI("EGL released in sync mode");
    }
}

void NativePipelineManager::updateConfig(const NativePipelineConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;

    if (frameQueue_) {
        frameQueue_->setCapacity(config_.queueCapacity);
    }
    if (inferenceEngine_) {
        inferenceEngine_->setLatencyDelayMs(config_.mockInferenceDelayMs);
    }
    if (frameEncoder_) {
        frameEncoder_->setLatencyDelayMs(config_.mockEncodeDelayMs);
    }
    LOGI("Pipeline configuration updated at runtime");
}

bool NativePipelineManager::processFrameSync(
    uint64_t frameId, uint64_t timestampNs,
    int width, int height, int rotationDegrees, int format,
    const uint8_t* yData, int yRowStride, int yPixelStride,
    const uint8_t* uData, int uRowStride, int uPixelStride,
    const uint8_t* vData, int vRowStride, int vPixelStride,
    FramePacket& outResult
) {
    if (!isRunning_.load() || !isEglInitialized_.load()) {
        return false;
    }

    // Measure incoming frame frequency
    auto now = std::chrono::steady_clock::now();
    incomingFrameCount_++;
    double elapsedSec = std::chrono::duration<double>(now - lastIncomingTime_).count();
    if (elapsedSec >= 1.0) {
        stats_.incomingFps = static_cast<uint32_t>(incomingFrameCount_ / elapsedSec);
        incomingFrameCount_ = 0;
        lastIncomingTime_ = now;
    }

    auto packet = std::make_unique<FramePacket>(frameId, timestampNs, width, height, rotationDegrees, format);
    
    // Copy plane buffers locally for processing
    packet->yPlane = {static_cast<uint32_t>(yRowStride), static_cast<uint32_t>(yPixelStride), static_cast<uint32_t>(height * yRowStride)};
    packet->yBuffer.assign(yData, yData + packet->yPlane.length);

    packet->uPlane = {static_cast<uint32_t>(uRowStride), static_cast<uint32_t>(uPixelStride), static_cast<uint32_t>((height / 2) * uRowStride)};
    packet->uBuffer.assign(uData, uData + packet->uPlane.length);

    packet->vPlane = {static_cast<uint32_t>(vRowStride), static_cast<uint32_t>(vPixelStride), static_cast<uint32_t>((height / 2) * vRowStride)};
    packet->vBuffer.assign(vData, vData + packet->vPlane.length);

    processFrameInternal(packet.get());

    // Update processing stats
    processedFrameCount_++;
    double procElapsedSec = std::chrono::duration<double>(now - lastProcessedTime_).count();
    if (procElapsedSec >= 1.0) {
        stats_.processedFps = static_cast<uint32_t>(processedFrameCount_ / procElapsedSec);
        processedFrameCount_ = 0;
        lastProcessedTime_ = now;
    }

    // Copy result out
    outResult = *packet;
    return true;
}

bool NativePipelineManager::enqueueFrameAsync(
    uint64_t frameId, uint64_t timestampNs,
    int width, int height, int rotationDegrees, int format,
    const uint8_t* yData, int yRowStride, int yPixelStride,
    const uint8_t* uData, int uRowStride, int uPixelStride,
    const uint8_t* vData, int vRowStride, int vPixelStride
) {
    if (!isRunning_.load() || !frameQueue_) {
        return false;
    }

    // Ingest metrics
    auto now = std::chrono::steady_clock::now();
    incomingFrameCount_++;
    double elapsedSec = std::chrono::duration<double>(now - lastIncomingTime_).count();
    if (elapsedSec >= 1.0) {
        stats_.incomingFps = static_cast<uint32_t>(incomingFrameCount_ / elapsedSec);
        incomingFrameCount_ = 0;
        lastIncomingTime_ = now;
    }

    auto packet = std::make_unique<FramePacket>(frameId, timestampNs, width, height, rotationDegrees, format);
    
    // Copy plane bytes into vectors
    packet->yPlane = {static_cast<uint32_t>(yRowStride), static_cast<uint32_t>(yPixelStride), static_cast<uint32_t>(height * yRowStride)};
    packet->yBuffer.assign(yData, yData + packet->yPlane.length);

    packet->uPlane = {static_cast<uint32_t>(uRowStride), static_cast<uint32_t>(uPixelStride), static_cast<uint32_t>((height / 2) * uRowStride)};
    packet->uBuffer.assign(uData, uData + packet->uPlane.length);

    packet->vPlane = {static_cast<uint32_t>(vRowStride), static_cast<uint32_t>(vPixelStride), static_cast<uint32_t>((height / 2) * vRowStride)};
    packet->vBuffer.assign(vData, vData + packet->vPlane.length);

    uint32_t droppedCount = 0;
    
    std::lock_guard<std::mutex> lock(configMutex_);
    bool success = frameQueue_->enqueue(std::move(packet), config_.dropPolicy, droppedCount);
    
    if (droppedCount > 0) {
        stats_.droppedFrames += droppedCount;
    }
    
    stats_.queueDepth = static_cast<uint32_t>(frameQueue_->size());
    return success;
}

PipelineStats NativePipelineManager::getStats() const {
    return stats_;
}

void NativePipelineManager::clearStats() {
    stats_.clear();
}

bool NativePipelineManager::getLatestResult(FramePacket& outPacket) {
    std::lock_guard<std::mutex> lock(latestResultMutex_);
    if (!latestResultPacket_) return false;
    outPacket = *latestResultPacket_;
    return true;
}

void NativePipelineManager::threadLoop() {
    // Thread-bound EGL Context Creation
    if (!eglCore_.init()) {
        LOGE("Failed to initialize EGL in worker thread");
        isRunning_ = false;
        return;
    }

    eglSurface_ = eglCore_.createPbufferSurface(1, 1);
    if (!eglCore_.makeCurrent(eglSurface_)) {
        LOGE("Failed to bind EGL context in worker thread");
        eglCore_.destroySurface(eglSurface_);
        eglCore_.release();
        isRunning_ = false;
        return;
    }

    if (!converter_.init()) {
        LOGE("Failed to init GPU converter in worker thread");
        eglCore_.makeUncurrent();
        eglCore_.destroySurface(eglSurface_);
        eglCore_.release();
        isRunning_ = false;
        return;
    }

    if (!textureManager_.init()) {
        LOGE("Failed to init texture manager in worker thread");
        converter_.release();
        eglCore_.makeUncurrent();
        eglCore_.destroySurface(eglSurface_);
        eglCore_.release();
        isRunning_ = false;
        return;
    }

    LOGI("EGL initialized successfully on background render thread");

    while (isRunning_.load()) {
        auto packet = frameQueue_->dequeue();
        if (!packet) {
            // Queue shut down
            break;
        }

        stats_.queueDepth = static_cast<uint32_t>(frameQueue_->size());

        // Process frame on this GL thread
        processFrameInternal(packet.get());

        // Measure processed FPS
        auto now = std::chrono::steady_clock::now();
        processedFrameCount_++;
        double elapsedSec = std::chrono::duration<double>(now - lastProcessedTime_).count();
        if (elapsedSec >= 1.0) {
            stats_.processedFps = static_cast<uint32_t>(processedFrameCount_ / elapsedSec);
            processedFrameCount_ = 0;
            lastProcessedTime_ = now;
        }

        // Cache the result for UI polling
        {
            std::lock_guard<std::mutex> lock(latestResultMutex_);
            latestResultPacket_ = std::make_unique<FramePacket>(*packet);
        }
    }

    // Release context on worker thread teardown
    eglCore_.makeCurrent(eglSurface_);
    textureManager_.release();
    converter_.release();
    eglCore_.makeUncurrent();
    if (eglSurface_ != EGL_NO_SURFACE) {
        eglCore_.destroySurface(eglSurface_);
        eglSurface_ = EGL_NO_SURFACE;
    }
    eglCore_.release();
    LOGI("EGL context released on worker thread shutdown");
}

void NativePipelineManager::processFrameInternal(FramePacket* packet) {
    if (!packet) return;

    // 1. GPU YUV->RGB Conversion (OpenGL ES)
    if (config_.gpuConversionEnabled) {
        auto tStart = std::chrono::steady_clock::now();

        // Bind offscreen frame target sized matching the processing input
        textureManager_.resizeFbo(packet->width, packet->height);

        // Execute shader conversion
        converter_.convert(
            packet->width, packet->height,
            packet->yBuffer.data(), packet->yPlane.rowStride, packet->yPlane.pixelStride,
            packet->uBuffer.data(), packet->uPlane.rowStride, packet->uPlane.pixelStride,
            packet->vBuffer.data(), packet->vPlane.rowStride, packet->vPlane.pixelStride,
            textureManager_.getFboId(), packet->width, packet->height
        );

        // Store GPU texture handle references
        packet->gpuTextureId = textureManager_.getRgbaTextureId();

        auto tEnd = std::chrono::steady_clock::now();
        packet->timeGpuConversionMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    }

    // 2. Readback (if needed for downstream CPU consumers)
    if (config_.rgbMode == RgbOutputMode::CPU_RGBA_FROM_GPU && config_.gpuConversionEnabled) {
        auto tStart = std::chrono::steady_clock::now();
        textureManager_.readPixels(packet->cpuRgbBuffer);
        auto tEnd = std::chrono::steady_clock::now();
        packet->timeGpuReadbackMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    }

    // 3. AI Inference (Mock or Real)
    if (config_.aiEnabled && inferenceEngine_) {
        inferenceEngine_->runInference(packet);
    }

    // 4. Encode (Mock or Real)
    if (config_.encodeEnabled && frameEncoder_) {
        frameEncoder_->encodeFrame(packet);
    }

    // 5. Compute End-to-End Latency
    auto tNow = std::chrono::steady_clock::now();
    packet->timeEndToEndMs = std::chrono::duration<double, std::milli>(tNow - packet->startTime).count();

    // Accumulate rolling averages in stats
    updateTimingAverages(packet);
}

void NativePipelineManager::updateTimingAverages(const FramePacket* packet) {
    float alpha = 0.1f; // Exponential moving average weight

    if (config_.gpuConversionEnabled) {
        stats_.avgGpuConversionMs = stats_.avgGpuConversionMs * (1.0f - alpha) + static_cast<float>(packet->timeGpuConversionMs) * alpha;
    }
    if (config_.rgbMode == RgbOutputMode::CPU_RGBA_FROM_GPU && config_.gpuConversionEnabled) {
        stats_.avgGpuReadbackMs = stats_.avgGpuReadbackMs * (1.0f - alpha) + static_cast<float>(packet->timeGpuReadbackMs) * alpha;
    }
    if (config_.aiEnabled) {
        stats_.avgInferenceMs = stats_.avgInferenceMs * (1.0f - alpha) + static_cast<float>(packet->timeInferenceMs) * alpha;
    }
    if (config_.encodeEnabled) {
        stats_.avgEncodeMs = stats_.avgEncodeMs * (1.0f - alpha) + static_cast<float>(packet->timeEncodeMs) * alpha;
    }
    stats_.avgEndToEndLatencyMs = stats_.avgEndToEndLatencyMs * (1.0f - alpha) + static_cast<float>(packet->timeEndToEndMs) * alpha;
}
