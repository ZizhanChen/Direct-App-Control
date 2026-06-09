#ifndef __EVENT_H__
#define __EVENT_H__

#include <linux/types.h>
#include <linux/list.h>
#include <linux/compat.h>
#include "data_queue.h"

//typedef struct event
//{
//	int x;
//    int y;
//}event;

//typedef struct event {
//	compat_ulong_t sec;
//	compat_ulong_t usec;
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

event* dequeue_event(data_queue* queue);

#endif