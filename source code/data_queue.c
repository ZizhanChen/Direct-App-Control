#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#include <linux/slab.h>

#include "data_queue.h"

#define LOG_TAG	 "data_queue:"
#define print_dbg(fmt, ...) printk(KERN_DEBUG LOG_TAG "%s:%d->" fmt "\n", \
					__func__, __LINE__, ##__VA_ARGS__)
#define print_err(fmt, ...) printk(KERN_ERR LOG_TAG "%s:%d->" fmt "\n", \
					__func__, __LINE__, ##__VA_ARGS__)
#define print_info(fmt, ...) printk(KERN_NOTICE LOG_TAG "%s:%d->" fmt "\n", \
					__func__, __LINE__, ##__VA_ARGS__)

//DEFINE_SPINLOCK(data_queue_lock);

//#define USE_LOCK 0
//#define lock_share()  do {if(USE_LOCK) spin_lock(&data_queue_lock);} while(0)
//#define unlock_share()  do {if(USE_LOCK) spin_unlock(&data_queue_lock);} while(0)

static int get_idle(struct data_queue* queue)
{
	int idle = 0;
	//lock_share();
	//print_dbg("---------- data queue debug -----------");
	//print_dbg("buf_len = %d", queue->buf_len);
	//print_dbg("full = %d", queue->full);
	//print_dbg("head = %d", queue->head);
	//print_dbg("tail = %d", queue->tail);
	//print_dbg("---------------------------------------");
	if(!queue->full && queue->tail == queue->head) idle = queue->buf_len;
	if(queue->head > queue->tail) idle = queue->buf_len - (queue->head-queue->tail);
	if(queue->head < queue->tail) idle = queue->tail - queue->head;
	//unlock_share();
	return idle;
}

static void push_data(struct data_queue* queue, const char* data, int size)
{
	memcpy(&queue->buf[queue->head], data, size);
	//lock_share();
	queue->head = (queue->head + size)%queue->buf_len;
	if(queue->head == queue->tail) queue->full = true;
	//unlock_share();
}

static int pull_data(struct data_queue* queue, char * buff, int size)
{
	int buf_len = queue->buf_len;
	if(buf_len - queue->tail >= size) {
		memcpy((void*)buff, (const void*)&queue->buf[queue->tail], (unsigned long)size);
	} else {
		int first_half = buf_len-queue->tail;
		memcpy((void*)buff, (const void*)&queue->buf[queue->tail],(unsigned long)(first_half));
		memcpy((void*)(buff+first_half), (const void*)&queue->buf[0],(unsigned long)(size-first_half));
	}
	//lock_share();
	queue->tail = (queue->tail + size) % buf_len;
	if(queue->head != queue->tail) queue->full = false;
	//unlock_share();
	return size;
}

int dump_data(struct data_queue* queue, int size)
{
	char* buffer = kmalloc(size, GFP_KERNEL);
    int buf_len = queue->buf_len;
	if(buf_len - queue->tail >= size) {
        memcpy(buffer, &queue->buf[queue->tail], size);
	} else {
		int first_half = buf_len-queue->tail;
        memcpy(buffer, &queue->buf[queue->tail], size);
        memcpy(buffer+first_half, &queue->buf[0], size-first_half);
	}
	//lock_share();
    print_dbg("dump queue: [%s]", buffer);
    queue->tail = (queue->tail + size) % buf_len;
	if(queue->head != queue->tail) queue->full = false;
	//unlock_share();
	kfree(buffer);
	return size;
}
EXPORT_SYMBOL(dump_data);

int enqueue_data(struct data_queue* queue, const char* data, int size, int timeout/* ms */)
{
	int buf_len = queue->buf_len;
	int idle = 0;
	if(data == NULL || size > buf_len) {
		print_err("params are not valid.");
		return -1;
	}
	idle = get_idle(queue);

	if(idle >= size) {
		push_data(queue, data, size);
		return size;
	}

	while(timeout > 0) {
		msleep(10);
		timeout -= 10;
		if(get_idle(queue) > size) {
			push_data(queue, data, size);
			return size;
		}
	}
	return 0;
}
EXPORT_SYMBOL(enqueue_data);

int dequeue_data(struct data_queue* queue, char * buff, int size)
{
	int valid_size = 0;
	int idle = 0;
	int buf_len = queue->buf_len;
	if(buff == NULL || size <= 0) {
		print_err("params are not valid.");
		return -1;
	}

	if(get_idle(queue) == buf_len) {
		print_info("no data exist.");
		return 0;
	}

	idle = get_idle(queue);
	valid_size = buf_len - idle;
	if(size >= valid_size) return pull_data(queue, buff, valid_size);
	return pull_data(queue, buff, size);
}
EXPORT_SYMBOL(dequeue_data);

bool check_exist_data(struct data_queue* queue)
{
	return (queue->buf_len - get_idle(queue) > 0);
}
EXPORT_SYMBOL(check_exist_data);