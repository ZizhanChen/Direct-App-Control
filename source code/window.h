#ifndef __WINDOW_H__
#define __WINDOW_H__

#include <linux/types.h>
#include <linux/list.h>
#include "event.h"

typedef struct window
{
	pid_t pid;
    int vsync_tick;
    int left;
    int top;
    int height;  
	int width;
    int zorder;
    char * event_buffer; 
    size_t event_buffer_size;  // 缓冲区总大小
    size_t event_buffer_index; // 当前写入位置索引
    //struct mm_struct *mm;     // 关联的内存结构
    struct list_head node;
}window;

void init_window_list(void);
void add_window(pid_t pid, int left, int top, int height, int width, int zorder, char * event_buffer,  int buffer_size);
void del_window(pid_t pid);
window* get_window(pid_t pid);
void set_window(pid_t pid, int left, int top, int height, int width, int zorder, char * event_buffer);
window* get_first_window(void);
window* get_next_window(window* win);
struct list_head* get_window_list(void);
size_t fetch_events(pid_t pid, char * event_buffer);


#endif
