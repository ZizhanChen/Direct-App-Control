#include <linux/list.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/kernel.h>  

#include "dispatcher.h"
#include "data_queue.h"
#include "window.h"
#include "event.h"

#include <linux/time.h>
#include <linux/timekeeping.h>
#include <linux/printk.h>

#define LOG_TAG	 "dispatcher:"
#define print_dbg(fmt, ...) printk(KERN_DEBUG LOG_TAG "%s:%d->" fmt "\n", \
					__func__, __LINE__, ##__VA_ARGS__)
#define print_err(fmt, ...) printk(KERN_ERR LOG_TAG "%s:%d->" fmt "\n", \
					__func__, __LINE__, ##__VA_ARGS__)
#define print_info(fmt, ...) printk(KERN_NOTICE LOG_TAG "%s:%d->" fmt "\n", \
					__func__, __LINE__, ##__VA_ARGS__)

static bool policy(event * e, window * win);
static int deliver(event * e, window * win);
void dispatch_once(data_queue* queue);

void dispatch_once(data_queue* queue)
{
    window* win;
	event* e;
	struct list_head* window_list;
	e = dequeue_event(queue);
    //print_dbg("[zizhan] dispatch_once is called with event [type = %d, code = %d, value = %d]", 
    //         e->type, e->code, e->value);
	window_list = get_window_list();
	list_for_each_entry(win, window_list, node){
		if (policy(e, win)) {
			deliver(e, win);
		}
	}
}

static bool policy(event * e, window * win){
	if((e->x > win->left) && (e->x < win->left + win->width) && (e->y > win->top) && (e->y < win->top + win->height)){
		return true;
	}
	return false;
}

//static int deliver(event * e, window * win)
//{
//	print_dbg("[zizhan] event [type = %d, code = %d, value = %d] delivered to window [%ld]", e->type, e->code, e->value, win->pid);
//	memcpy(win->event_buffer, e, sizeof(event));
//	print_dbg("[zizhan] the buffer content is [%s]", win->event_buffer);
//	return 0;
//}

static int deliver(event * e, window * win)
{
    // 计算事件大小
    size_t event_size = sizeof(event);
    
    // 检查缓冲区是否足够大
    if (win->event_buffer_size < event_size) {
        print_dbg("[zizhan] error: event buffer too small (%zu < %zu)", 
                 win->event_buffer_size, event_size);
        return -1;
    }
    
    // 计算写入位置
    char *write_ptr = win->event_buffer + win->event_buffer_index;
    
    // 如果剩余空间不足，回到缓冲区开头
    if (win->event_buffer_index + event_size > win->event_buffer_size) {
        win->event_buffer_index = 0;
        write_ptr = win->event_buffer;
    }
    
    // 复制事件到缓冲区
    memcpy(write_ptr, e, event_size);
    
    // 更新索引位置
    win->event_buffer_index += event_size;
    
    print_dbg("[zizhan] event [type = %d, code = %d, value = %d] delivered to window [%d] at index %zu", 
             e->type, e->code, e->value, win->pid, win->event_buffer_index - event_size);
    
    // 打印缓冲区内容（注意：二进制数据可能包含零终止符）
    //print_dbg("[zizhan] buffer head: [%.*s]", (int)event_size, write_ptr);
    
    return 0;
}

