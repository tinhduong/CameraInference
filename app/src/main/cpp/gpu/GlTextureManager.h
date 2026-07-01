#pragma once
#include <GLES3/gl3.h>
#include <vector>

class GlTextureManager {
public:
    GlTextureManager();
    ~GlTextureManager();

    bool init();
    void release();

    bool resizeFbo(int width, int height);

    GLuint getFboId() const { return fboId_; }
    GLuint getRgbaTextureId() const { return rgbaTextureId_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

    // Read back pixel data from FBO to CPU-visible RGBA buffer.
    bool readPixels(std::vector<uint8_t>& outBuffer);

private:
    GLuint fboId_ = 0;
    GLuint rgbaTextureId_ = 0;
    int width_ = 0;
    int height_ = 0;
};
