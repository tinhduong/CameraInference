#include "PBOManager.h"
#include <android/log.h>
#include <cstring>
#include <algorithm>

#define TAG "PBOManager"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

PBOManager::PBOManager() :
    width(0),
    height(0),
    rgbaSize(0),
    rgbSize(0),
    frameCount(0),
    pboInitialized(false) {
    pboIds[0] = 0;
    pboIds[1] = 0;
}

PBOManager::~PBOManager() {
    release();
}

void PBOManager::init(int w, int h) {
    release();
    width = w;
    height = h;
    rgbaSize = width * height * 4;
    rgbSize = width * height * 3;

    localCpuBuffer.resize(rgbSize);

    glGenBuffers(2, pboIds);
    for (int i = 0; i < 2; i++) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pboIds[i]);
        glBufferData(GL_PIXEL_PACK_BUFFER, rgbaSize, nullptr, GL_DYNAMIC_READ);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    frameCount = 0;
    pboInitialized = true;
}

void PBOManager::release() {
    if (pboInitialized) {
        glDeleteBuffers(2, pboIds);
        pboIds[0] = 0;
        pboIds[1] = 0;
        pboInitialized = false;
    }
    localCpuBuffer.clear();
}

uint8_t* PBOManager::readbackFrameAsync(GLuint fboId) {
    if (!pboInitialized) {
        return nullptr;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fboId);

    // Swap buffers index
    int writeIndex = frameCount % 2;
    int readIndex = (frameCount + 1) % 2;

    // Step 1: Request OpenGL to write pixels into the current write PBO asynchronously.
    // Specifying nullptr as the data pointer forces OpenGL to treat it as an offset into the bound pack buffer.
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pboIds[writeIndex]);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    uint8_t* resultPtr = nullptr;

    // Step 2: Map the other PBO to retrieve the pixels from the previous frame.
    // Since this frame was queued for readback in the previous cycle, the GPU transfer is complete,
    // meaning mapping this buffer is highly likely to be non-blocking.
    if (frameCount > 0) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pboIds[readIndex]);
        uint8_t* gpuPtr = (uint8_t*)glMapBufferRange(
            GL_PIXEL_PACK_BUFFER, 0, rgbaSize, GL_MAP_READ_BIT);
        
        if (gpuPtr != nullptr) {
            auto tStart = std::chrono::high_resolution_clock::now();
            // Unpack RGBA to RGB888 (skip alpha channel) during CPU copy to internal buffer
            uint8_t* src = gpuPtr;
            uint8_t* dst = localCpuBuffer.data();
            int numPixels = width * height;
            for (int i = 0; i < numPixels; ++i) {
                dst[0] = src[0]; // R
                dst[1] = src[1]; // G
                dst[2] = src[2]; // B
                src += 4;
                dst += 3;
            }
            auto tEnd = std::chrono::high_resolution_clock::now();
            auto copyMs = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count();
            if (frameCount % 30 == 0) {
                __android_log_print(ANDROID_LOG_DEBUG, "PBOManager", 
                                    "PBO readback mapping & unpack (%dx%d): %lld ms", 
                                    width, height, copyMs);
            }
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            resultPtr = localCpuBuffer.data();
        } else {
            LOGE("Failed to map pixel buffer");
        }
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    frameCount++;
    return resultPtr;
}
