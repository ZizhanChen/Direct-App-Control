#define LOG_TAG "InputEventThread"

#include "InputEventThread.h"

#include <inputzizhan/InputReaderZizhan.h>
#include <log/log.h>

namespace zizhan {

InputEventThread::InputEventThread(InputReader* reader, const std::string& name)
      : android::Thread(false /* canCallJava */),
        mReader(reader),
        mName(name),
        mReady(false) {
    if (mReader == nullptr) {
        ALOGE("%s: InputReader is null!", mName.c_str());
    }
}

InputEventThread::~InputEventThread() {
    // 析构时确保线程退出，避免 use-after-free
    requestExitAndWait();
}

void InputEventThread::requestExit() {
    // 先设置 Thread 的退出标志
    android::Thread::requestExit();
    // 尝试唤醒可能阻塞在 fetchEvents 里的线程
    wake();
}

void InputEventThread::requestExitAndWait() {
    android::Thread::requestExitAndWait();
}

void InputEventThread::wake() {
    // 如果 LibZizhan::fetchEvents() 阻塞在 syscall 上，且内核不支持被打断，
    // 则需要在内核里提供 wake 机制，例如：
    //   mLibZizhan->wake();
    // 现在保持空实现，依赖 fetchEvents 自身超时或返回。
}

void InputEventThread::waitReady() {
    android::Mutex::Autolock _l(mLock);
    while (!mReady) {
        mCondition.wait(mLock);
    }
}

bool InputEventThread::threadLoop() {
    if (mReader == nullptr) {
        ALOGE("%s: exiting because InputReader is null", mName.c_str());
        return false;
    }

    {
        // 通知外部：线程已就绪
        android::Mutex::Autolock _l(mLock);
        mReady = true;
        mCondition.broadcast();
    }

    ALOGI("%s: entering input event loop", mName.c_str());

    // 仿 ActivityThread::loop()：持续循环，直到 exitPending() 为 true
    while (!exitPending()) {
        // loopOnceZizhan 内部是一次 fetch + process 的完整循环
        mReader->loopOnceZizhan();
    }

    ALOGI("%s: exiting input event loop", mName.c_str());
    return false; // 返回 false 表示线程结束
}

} // namespace zizhan
