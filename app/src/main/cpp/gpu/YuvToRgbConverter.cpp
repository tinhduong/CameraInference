#include "YuvToRgbConverter.h"
#include <android/log.h>
#include <cstring>

#define LOG_TAG "YuvToRgbConverter"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static const char* VERTEX_SHADER_SRC = R"glsl(#version 300 es
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)glsl";

static const char* FRAGMENT_SHADER_SRC = R"glsl(#version 300 es
precision mediump float;
in vec2 vTexCoord;
out vec4 fragColor;
uniform sampler2D yTexture;
uniform sampler2D uTexture;
uniform sampler2D vTexture;

// BT.601 standard studio-to-RGB conversion matrix
const mat3 yuv2rgb = mat3(
    1.164,  1.164,  1.164,
    0.0,   -0.391,  2.018,
    1.596, -0.813,  0.0
);

void main() {
    float y = texture(yTexture, vTexCoord).r - 0.0627;
    float u = texture(uTexture, vTexCoord).r - 0.5020;
    float v = texture(vTexture, vTexCoord).r - 0.5020;
    vec3 rgb = yuv2rgb * vec3(y, u, v);
    fragColor = vec4(rgb, 1.0);
}
)glsl";

YuvToRgbConverter::YuvToRgbConverter() {}

YuvToRgbConverter::~YuvToRgbConverter() {
    release();
}

bool YuvToRgbConverter::init() {
    if (!program_.init(VERTEX_SHADER_SRC, FRAGMENT_SHADER_SRC)) {
        LOGE("Failed to compile YUV to RGB shader program");
        return false;
    }

    glGenTextures(1, &yTexture_);
    glGenTextures(1, &uTexture_);
    glGenTextures(1, &vTexture_);

    static const float vertices[] = {
        // Positions   // TexCoords (flipped vertically to match camera sensor orientation defaults)
        -1.0f,  1.0f,  0.0f, 0.0f,
        -1.0f, -1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 1.0f,

        -1.0f,  1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 0.0f
    };

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return true;
}

void YuvToRgbConverter::release() {
    program_.release();
    if (yTexture_) { glDeleteTextures(1, &yTexture_); yTexture_ = 0; }
    if (uTexture_) { glDeleteTextures(1, &uTexture_); uTexture_ = 0; }
    if (vTexture_) { glDeleteTextures(1, &vTexture_); vTexture_ = 0; }
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    texWidth_ = 0;
    texHeight_ = 0;
}

void YuvToRgbConverter::setupTextures(int width, int height) {
    if (texWidth_ == width && texHeight_ == height) return;
    texWidth_ = width;
    texHeight_ = height;

    GLuint textures[] = {yTexture_, uTexture_, vTexture_};
    int w[] = {width, width / 2, width / 2};
    int h[] = {height, height / 2, height / 2};

    for (int i = 0; i < 3; i++) {
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w[i], h[i], 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void YuvToRgbConverter::uploadPlane(GLuint textureId, const uint8_t* data, int width, int height, int rowStride, int pixelStride, std::vector<uint8_t>& tempBuffer) {
    glBindTexture(GL_TEXTURE_2D, textureId);
    if (pixelStride == 1 && rowStride == width) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, data);
    } else {
        tempBuffer.resize(width * height);
        uint8_t* dst = tempBuffer.data();
        for (int r = 0; r < height; ++r) {
            const uint8_t* srcRow = data + (r * rowStride);
            uint8_t* dstRow = dst + (r * width);
            if (pixelStride == 1) {
                std::memcpy(dstRow, srcRow, width);
            } else {
                for (int c = 0; c < width; ++c) {
                    dstRow[c] = srcRow[c * pixelStride];
                }
            }
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, tempBuffer.data());
    }
}

bool YuvToRgbConverter::convert(
    int width, int height,
    const uint8_t* yData, int yRowStride, int yPixelStride,
    const uint8_t* uData, int uRowStride, int uPixelStride,
    const uint8_t* vData, int vRowStride, int vPixelStride,
    GLuint outputFbo, int outWidth, int outHeight
) {
    if (!yTexture_ || !uTexture_ || !vTexture_) return false;

    setupTextures(width, height);

    std::vector<uint8_t> yTemp;
    uploadPlane(yTexture_, yData, width, height, yRowStride, yPixelStride, yTemp);
    uploadPlane(uTexture_, uData, width / 2, height / 2, uRowStride, uPixelStride, uTemp_);
    uploadPlane(vTexture_, vData, width / 2, height / 2, vRowStride, vPixelStride, vTemp_);

    glBindFramebuffer(GL_FRAMEBUFFER, outputFbo);
    glViewport(0, 0, outWidth, outHeight);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    program_.use();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, yTexture_);
    glUniform1i(program_.getUniformLocation("yTexture"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, uTexture_);
    glUniform1i(program_.getUniformLocation("uTexture"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, vTexture_);
    glUniform1i(program_.getUniformLocation("vTexture"), 2);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}
