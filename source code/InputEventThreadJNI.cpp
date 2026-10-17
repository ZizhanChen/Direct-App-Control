#define LOG_TAG "InputEventThreadJNI"
#include <jni.h>
#include <log/log.h>
#include <mutex>
#include <memory>
#include "InputEventThread.h"

static std::unique_ptr<zizhan::InputEventThread> gInputEventThread;
static std::mutex gLock;

static void startLocked() {
    if (gInputEventThread) return;
    gInputEventThread = std::make_unique<zizhan::InputEventThread>("AppInputEventThread");
    android::status_t err = gInputEventThread->run("AppInputEventThread");
    if (err != android::OK) {
        ALOGE("Failed to start AppInputEventThread, err=%d", err);
        gInputEventThread.reset();
    } else {
        ALOGI("AppInputEventThread started");
    }
}

static void stopLocked() {
    if (!gInputEventThread) return;
    gInputEventThread->requestExitAndWait();
    gInputEventThread.reset();
    ALOGI("AppInputEventThread stopped");
}

extern "C" JNIEXPORT void JNICALL
Java_android_app_ActivityThread_startAppInputEventThread(JNIEnv*, jclass) {
    std::scoped_lock _l(gLock);
    startLocked();
}

extern "C" JNIEXPORT void JNICALL
Java_android_app_ActivityThread_stopAppInputEventThread(JNIEnv*, jclass) {
    std::scoped_lock _l(gLock);
    stopLocked();
}
