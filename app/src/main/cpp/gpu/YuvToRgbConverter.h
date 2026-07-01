#pragma once
#include "GlShaderProgram.h"
#include <vector>

class YuvToRgbConverter {
public:
    YuvToRgbConverter();
    ~YuvToRgbConverter();

    bool init();
    void release();

    // Converts YUV planes on the GPU using shaders. 
    // Renders the output directly into the FBO.
    bool convert(
        int width, int height,
        const uint8_t* yData, int yRowStride, int yPixelStride,
        const uint8_t* uData, int uRowStride, int uPixelStride,
        const uint8_t* vData, int vRowStride, int vPixelStride,
        GLuint outputFbo, int outWidth, int outHeight
    );

private:
    void setupTextures(int width, int height);
    void uploadPlane(GLuint textureId, const uint8_t* data, int width, int height, int rowStride, int pixelStride, std::vector<uint8_t>& tempBuffer);

    GlShaderProgram program_;
    GLuint yTexture_ = 0;
    GLuint uTexture_ = 0;
    GLuint vTexture_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;

    int texWidth_ = 0;
    int texHeight_ = 0;

    // CPU buffers to align strides
    std::vector<uint8_t> uTemp_;
    std::vector<uint8_t> vTemp_;
};
