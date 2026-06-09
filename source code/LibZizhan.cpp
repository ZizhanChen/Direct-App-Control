/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "LibZizhan"

#define __NR_add_window 451
#define __NR_fetch_events 452
#define __NR_del_window 453

#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include <utils/Errors.h>
#include <utils/Log.h>

#include <inputzizhan/InputReaderZizhan.h>
#include <inputzizhan/LibZizhan.h>

#include <sys/sysinfo.h>
#include <time.h>

namespace zizhan {

// --- LibZizhan ---

LibZizhan::LibZizhan() {
}

LibZizhan::~LibZizhan() {
}

void log_with_microseconds(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto us = std::chrono::time_point_cast<std::chrono::microseconds>(now);
    
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    struct tm time_info;
    localtime_r(&now_c, &time_info);
    
    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &time_info);
    
    auto microseconds = us.time_since_epoch().count() % 1000000;
    
    ALOGD("[%s.%06lld] %s", time_buf, microseconds, message.c_str());
}

int64_t getSystemUptimeMillis() {
    struct sysinfo info;
    sysinfo(&info);
    return info.uptime * 1000LL; // 秒转毫秒
}

void logAbsoluteTime(nsecs_t eventWhen) {
    // 获取系统启动时间点（日历时间）
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    int64_t realtime_now = now.tv_sec * 1000000000LL + now.tv_nsec;
    int64_t uptime_ns = getSystemUptimeMillis() * 1000000LL;
    int64_t boot_time_ns = realtime_now - uptime_ns;
    
    // 计算事件绝对时间
    int64_t event_absolute_ns = boot_time_ns + eventWhen;
    
    // 转换为 time_t 和微秒
    time_t sec = event_absolute_ns / 1000000000LL;
    long us = (event_absolute_ns % 1000000000LL) / 1000LL;
    
    // 格式化为本地时间
    struct tm tm_time;
    localtime_r(&sec, &tm_time);
    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &tm_time);
    
    ALOGD("[zizhan] The absolute time of event is : %s.%06ld", time_buf, us);
}

int32_t LibZizhan::printZizhan() {
    ALOGD("[zizhan] test\n");
    return 0;
}

int32_t LibZizhan::fetchEvents(event * readBuffer){
    int retval = syscall(__NR_fetch_events, readBuffer);
    event& iev = readBuffer[0];
    ALOGD("[zizhan] fetch events, ret val is [%d], fetched event is [type=%d, code=%d, value=%d]\n", retval, 
        iev.type, iev.code, iev.value);
    return retval;
}

int32_t LibZizhan::addWindow(int left, int top, int height, int width, int zorder){
    int ret = syscall(__NR_add_window, left, top, height, width, zorder);
    ALOGD("[zizhan] addWindow, ret val is [%d]", ret);
    return ret;
}

int32_t LibZizhan::delWindow(){
    int ret = syscall(__NR_del_window);
    ALOGD("[zizhan] delWindow, ret val is [%d]", ret);
    return ret;
}

nsecs_t LibZizhan::processEventTimestamp(const event& event) {
    const nsecs_t inputEventTime = 
        seconds_to_nanoseconds(event.input_event_sec) +
        microseconds_to_nanoseconds(event.input_event_usec);
    return inputEventTime;
}


// 修改后的处理函数
void LibZizhan::processRawEvents(event *readBuffer, RawEvent *mEventBufferZizhan, int eventCount) {
    ALOGD("[zizhan] Processing %d raw events", eventCount);
    
    for (int i = 0; i < eventCount; i++) {
        event& iev = readBuffer[i];
        RawEvent* event = &mEventBufferZizhan[i];
        
        event->deviceId = getDeviceIdLocked();
        event->when = processEventTimestamp(iev);
        event->type = iev.type;
        event->code = iev.code;
        event->value = iev.value;
        
        ALOGD("[zizhan] Processed event %d: [when=%ld, deviceId=%d, type=%d, code=%d, value=%d]",
              i, (long)event->when, event->deviceId, event->type, event->code, event->value);
        //log_with_microseconds("[zizhan] input event finish!!!!");
        //logAbsoluteTime(event->when);
    }
}

int32_t LibZizhan::getDeviceIdLocked() {
    int deviceId = 9;
    return deviceId;
}

//void LibZizhan::processEvents(const RawEventZizhan* rawEvents, size_t count) {
//    processEventsZizhan(rawEvents, count);
//}



} // namespace zizhan
