#include "GlTextureManager.h"
#include <android/log.h>

#define LOG_TAG "GlTextureManager"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

GlTextureManager::GlTextureManager() {}

GlTextureManager::~GlTextureManager() {
    release();
}

bool GlTextureManager::init() {
    glGenFramebuffers(1, &fboId_);
    glGenTextures(1, &rgbaTextureId_);
    return true;
}

void GlTextureManager::release() {
    if (rgbaTextureId_) {
        glDeleteTextures(1, &rgbaTextureId_);
        rgbaTextureId_ = 0;
    }
    if (fboId_) {
        glDeleteFramebuffers(1, &fboId_);
        fboId_ = 0;
    }
    width_ = 0;
    height_ = 0;
}

bool GlTextureManager::resizeFbo(int width, int height) {
    if (width_ == width && height_ == height) return true;
    width_ = width;
    height_ = height;

    glBindTexture(GL_TEXTURE_2D, rgbaTextureId_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, fboId_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rgbaTextureId_, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("Framebuffer creation status is incomplete: %d", status);
        return false;
    }

    return true;
}

bool GlTextureManager::readPixels(std::vector<uint8_t>& outBuffer) {
    if (width_ <= 0 || height_ <= 0 || fboId_ == 0) return false;
    
    outBuffer.resize(width_ * height_ * 4); // RGBA is 4 bytes per pixel

    glBindFramebuffer(GL_FRAMEBUFFER, fboId_);
    glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, outBuffer.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}
