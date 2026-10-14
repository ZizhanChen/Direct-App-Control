#ifndef __WINDOW_H__
#define __WINDOW_H__

#include <linux/types.h>
#include <linux/list.h>
#include "event.h"

typedef struct fb_handler_node {
    uint32_t* fb_handler;          
    struct list_head node;        
} fb_handler_node;

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
    size_t event_buffer_size;      
    size_t event_buffer_index;     
    struct list_head node;
    struct list_head fb_handler_list;
} window;

void init_window_list(void);
void add_window(pid_t pid, int left, int top, int height, int width, int zorder,
                char * event_buffer, int buffer_size);
void del_window(pid_t pid);
window* get_window(pid_t pid);
void set_window(pid_t pid, int left, int top, int height, int width, int zorder,
                char * event_buffer);
window* get_first_window(void);
window* get_next_window(window* win);
struct list_head* get_window_list(void);
size_t fetch_events(pid_t pid, char * event_buffer);

void add_fb_handler(window *win, uint32_t* handler);
void del_fb_handler(window *win, uint32_t* handler);

#endif
