#ifndef __INPUT_EVENT_THREAD_H__
#define __INPUT_EVENT_THREAD_H__

#include <utils/Thread.h>
#include <utils/Mutex.h>
#include <utils/Condition.h>
#include <string>
#include <atomic>

namespace zizhan {

class InputReader;

class InputEventThread : public android::Thread {
public:
    InputEventThread(InputReader* reader, const std::string& name);
    ~InputEventThread() override;

    // 请求退出（异步），类似于 ActivityThread 的 scheduleExit
    void requestExit() override;

    // 请求退出并等待线程结束（同步）
    void requestExitAndWait() override;

    // 唤醒可能阻塞在 fetchEvents 中的线程
    void wake();

    // 等待线程进入主循环（就绪通知）
    void waitReady();

private:
    // android::Thread 的核心循环入口
    bool threadLoop() override;

    InputReader* mReader;
    std::string mName;

    android::Mutex mLock;
    android::Condition mCondition;
    std::atomic<bool> mReady;
};

} // namespace zizhan

#endif // __INPUT_EVENT_THREAD_H__
