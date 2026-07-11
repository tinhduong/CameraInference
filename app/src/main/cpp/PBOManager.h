#ifndef PBO_MANAGER_H
#define PBO_MANAGER_H

#include <GLES3/gl3.h>
#include <vector>
#include <cstdint>
#include <cstddef>

class PBOManager {
public:
    PBOManager();
    ~PBOManager();

    void init(int w, int h);
    void release();

    // Asynchronously reads back RGBA pixels from FBO and copies them as RGB888 into a CPU buffer.
    // Swaps PBOs and returns pointer to RGB888 array when data is available (typically from frame N-1).
    uint8_t* readbackFrameAsync(GLuint fboId);

private:
    int width;
    int height;
    size_t rgbaSize; // width * height * 4
    size_t rgbSize;  // width * height * 3
    GLuint pboIds[2];
    int frameCount;
    std::vector<uint8_t> localCpuBuffer;
    bool pboInitialized;
};

#endif // PBO_MANAGER_H
