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

#ifndef _LIBINPUTZIZHAN_LIBZIZHAN_H
#define _LIBINPUTZIZHAN_LIBZIZHAN_H

#include <input/Input.h>
#include <input/InputDevice.h>
#include <input/InputEventLabels.h>
#include <utils/Errors.h>
//#include <utils/PropertyMap.h>

namespace zizhan {

//typedef struct compat_event {
//	__u32 sec;
//	__u32 usec;
//	__u16 type;
//	__u16 code;
//	__s32 value;
//}event;


typedef struct event {
#if (__BITS_PER_LONG != 32 || !defined(__USE_TIME_BITS64)) && !defined(__KERNEL__)
	struct timeval time;
#define input_event_sec time.tv_sec
#define input_event_usec time.tv_usec
#else
	__kernel_ulong_t __sec;
#if defined(__sparc__) && defined(__arch64__)
	unsigned int __usec;
	unsigned int __pad;
#else
	__kernel_ulong_t __usec;
#endif
#define input_event_sec  __sec
#define input_event_usec __usec
#endif
	__u16 type;
	__u16 code;
	__s32 value;
} event;
    
class LibZizhan {
public:
    LibZizhan();
    ~LibZizhan();
public:
    int32_t fetchEvents(event * readBuffer);
    int32_t addWindow(int left, int top, int height, int width, int zorder);
    int32_t delWindow();
    void processRawEvents(event *readBuffer, RawEvent *mEventBufferZizhan, int eventCount);
    nsecs_t processEventTimestamp(const event& event);
    int32_t getDeviceIdLocked();
    int32_t printZizhan();
};
} // namespace zizhan

#endif // _LIBINPUTZIZHAN_LIBZIZHAN_H
