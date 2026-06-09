#ifndef __DATA_QUEUE_H__
#define __DATA_QUEUE_H__

#include <linux/types.h>

typedef struct data_queue
{
	int buf_len;
	char* buf;
	int head;  // idle buffer head
	int tail;  // data buffer head
	bool full;
}data_queue;

typedef struct queue
{
	struct data_queue common;
	int (*enqueue)(struct data_queue*,const char* , int , int /* ms */);
	int (*dequeue)(struct data_queue*, char __user *, int );
	bool (*exist_data)(struct data_queue*);
    int (*dump)(struct data_queue* queue, int size);
}queue_t;

int enqueue_data(struct data_queue* queue, const char* data, int size, int timeout/* ms */);
int dequeue_data(struct data_queue* queue, char __user *buff, int size);
bool check_exist_data(struct data_queue* queue);
int dump_data(struct data_queue* queue, int size);

#define DECLARE_DATA_QUEUE(name, size) \
	static char buf_##name[size] = "\0"; \
	static struct queue name = { \
		.common = { \
			.buf_len = size, \
			.buf = buf_##name, \
			.head = 0, \
			.tail = 0, \
			.full = false, \
		}, \
		.enqueue = enqueue_data, \
		.dequeue = dequeue_data, \
        .dump = dump_data,\
		.exist_data = check_exist_data, \
	};


#endif