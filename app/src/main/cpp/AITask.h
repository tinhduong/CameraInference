#ifndef AI_TASK_H
#define AI_TASK_H

#include <jni.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <string>

class AITask {
public:
    AITask();
    ~AITask();

    void start(JavaVM* vm, jclass bridgeClassGlobal, jmethodID callbackMethodId, int w, int h);
    void stop();
    void resize(int w, int h);

    // Submits an RGB_888 frame. Performs frame-dropping if the AI thread is busy.
    void submitFrame(const uint8_t* rgbBuffer);

private:
    void threadLoop();
    void runDummyInference(const uint8_t* rgbData, int w, int h, std::string& outLabel, float& outScore);

    JavaVM* javaVM;
    jclass jniBridgeClass;
    jmethodID callbackMethod;
    std::mutex processingMutex;

    int width;
    int height;
    size_t rgbSize;

    std::thread workerThread;
    std::mutex threadMutex;
    std::condition_variable threadCv;
    std::atomic<bool> isRunning;

    std::vector<uint8_t> sharedRgbBuffer;
    bool hasNewFrame;

    std::vector<uint8_t> threadLocalBuffer;
};

#endif // AI_TASK_H
