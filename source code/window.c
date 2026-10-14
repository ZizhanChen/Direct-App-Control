#include <linux/list.h>
#include "window.h"
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>

#define LOG_TAG	 "window:"
#define print_dbg(fmt, ...) printk(KERN_DEBUG LOG_TAG "%s:%d->" fmt "\n", \
					__func__, __LINE__, ##__VA_ARGS__)
#define print_err(fmt, ...) printk(KERN_ERR LOG_TAG "%s:%d->" fmt "\n", \
					__func__, __LINE__, ##__VA_ARGS__)
#define print_info(fmt, ...) printk(KERN_NOTICE LOG_TAG "%s:%d->" fmt "\n", \
					__func__, __LINE__, ##__VA_ARGS__)


//struct list_head window_list;
LIST_HEAD(window_list);

//DEFINE_SPINLOCK(window_list_lock);

//#define USE_LOCK 1
//#define lock_share()  do {if(USE_LOCK) spin_lock(&window_list_lock);} while(0)
//#define unlock_share()  do {if(USE_LOCK) spin_unlock(&window_list_lock);} while(0)

//void init_window_list(void)
//{
//    INIT_LIST_HEAD(&window_list);
//}
//EXPORT_SYMBOL(init_window_list);

void add_window(pid_t pid, int left, int top, int height, int width, int zorder,
                char * event_buffer, int buffer_size) 
{
    window * win;
    win = kzalloc(sizeof(window), GFP_KERNEL);
    win->pid = pid; 
    win->vsync_tick = 0;
    win->left = left;
    win->top = top;
    win->height = height;
    win->width = width;
    win->zorder = zorder;
    win->event_buffer = event_buffer;
    win->event_buffer_size = buffer_size;
    win->event_buffer_index = 0;

    INIT_LIST_HEAD(&win->fb_handler_list);

    list_add(&win->node, &window_list);
    print_dbg("[zizhan] ---------- window debug -----------");
    print_dbg("[zizhan] window pid = %d", win->pid);
    print_dbg("[zizhan] left = %d, top = %d, height = %d, width=%d",
              win->left, win->top, win->height, win->width);
    print_dbg("[zizhan] zorder = %d", win->zorder);
    print_dbg("[zizhan] ---------------------------------------");
}
//EXPORT_SYMBOL(add_window);

void del_window(pid_t pid)
{
    struct window *win, *tmp;
    
    list_for_each_entry_safe(win, tmp, &window_list, node) {
        if (win->pid == pid) {
            // 1. 从全局链表移除
            list_del(&win->node);
            
            // 2. 释放事件缓冲区
            if (win->event_buffer) {
                kfree(win->event_buffer);
                win->event_buffer = NULL;
            }

            // 3. 释放所有 fb_handler 节点
            fb_handler_node *fnode, *ftmp;
            list_for_each_entry_safe(fnode, ftmp, &win->fb_handler_list, node) {
                list_del(&fnode->node);
                kfree(fnode);
            }
            
            // 4. 释放窗口结构
            kfree(win);
            
            print_dbg("[zizhan] Deleted window for PID %d\n", pid);
        }
    }
}
//EXPORT_SYMBOL(del_window);

window* get_window(pid_t pid)
{
    window *win = NULL;
    //lock_share();
    list_for_each_entry(win, &window_list, node) {
        if (win->pid == pid){
            //unlock_share();
            return win;
        }
    }
    //unlock_share();
    return NULL;
}
//EXPORT_SYMBOL(get_window);

void set_window(pid_t pid, int left, int top, int height, int width, int zorder, char * event_buffer)
{
    window *win = NULL;
    //lock_share();
    list_for_each_entry(win, &window_list, node) {
        if (win->pid == pid){
            win->left = left;
            win->top = top;
            win->height = height;
            win->width = width;
            win->zorder = width;
            win->event_buffer = event_buffer;
        }
    }
    //unlock_share();
}
//EXPORT_SYMBOL(set_window);

window* get_first_window(void){
    window *win = NULL;
    win = list_first_entry_or_null(&window_list, struct window, node);
    return win;
}

window* get_next_window(window* win){
    return list_next_entry(win, node);
}

struct list_head* get_window_list(void){
    return &window_list;
}

void add_fb_handler(window *win, uint32_t* handler)
{
    if (!win) {
        print_err("add_fb_handler: win is NULL");
        return;
    }

    fb_handler_node *node = kzalloc(sizeof(fb_handler_node), GFP_KERNEL);
    if (!node) {
        print_err("add_fb_handler: kzalloc failed");
        return;
    }

    node->fb_handler = handler;
    list_add(&node->node, &win->fb_handler_list);
    print_dbg("[zizhan] add_fb_handler: win pid=%d, handler=%p", win->pid, handler);
}

void del_fb_handler(window *win, uint32_t* handler)
{
    if (!win) {
        print_err("del_fb_handler: win is NULL");
        return;
    }

    fb_handler_node *node, *tmp;
    list_for_each_entry_safe(node, tmp, &win->fb_handler_list, node) {
        if (node->fb_handler == handler) {
            list_del(&node->node);
            kfree(node);
            print_dbg("[zizhan] del_fb_handler: win pid=%d, handler=%p", win->pid, handler);
            break;  // 假设同一个 handler 只添加一次
        }
    }
}

//size_t fetch_events(pid_t pid, char * event_buffer){
//    window* win;
//    size_t event_size = sizeof(event);
//    size_t total_events;
//    size_t bytes_to_copy;
    
//   lock_share();
//    win = get_window(pid);
//    if (win == NULL) {
//        print_dbg("[zizhan] no such window with pid [%d]\n", pid);
//        unlock_share();
//        return 0;
//    }
    // 计算缓冲区中的事件总数
//    total_events = win->event_buffer_index / event_size;
    
//    if (total_events == 0) {
//        print_dbg("[zizhan] no events in buffer\n");
//        unlock_share();
//        return 0; // 缓冲区为空
//    }
    
    // 计算需要复制的总字节数
//    bytes_to_copy = total_events * event_size;

    // 复制所有事件
//    memcpy(event_buffer, win->event_buffer, bytes_to_copy);
    
//    print_dbg("[zizhan] fetched %zu events from buffer\n", total_events);
    
    // 重置缓冲区索引
//    win->event_buffer_index = 0;
//    unlock_share();
//    return total_events; // 返回获取的事件数量
//}

size_t fetch_events(pid_t pid, char *event_buffer) {
    //print_dbg("[zizhan] enter fetch events for pid [%d]\n", pid);
    window* win;
    size_t event_size = sizeof(event);
    size_t total_events;
    
    //lock_share();
    //win = get_window(pid);
    win = get_first_window();
    if (!win) {
        print_dbg("[zizhan] no window for pid [%d]\n", pid);
        //unlock_share();
        return 0;
    }

    //total_events = win->event_buffer_index / event_size;
    total_events = 1;
    if (total_events == 0) {
        print_dbg("[zizhan] no events for pid [%d]\n", pid);
        //unlock_share();
        return 0;
    }

    // 计算线性可复制的第一部分长度
    size_t first_chunk_size = win->event_buffer_size - (win->event_buffer_index % win->event_buffer_size);
    first_chunk_size = (first_chunk_size > total_events * event_size) ? 
                       total_events * event_size : first_chunk_size;

    // 复制第一部分（缓冲区末尾到结束）
    memcpy(event_buffer, win->event_buffer + win->event_buffer_index, first_chunk_size);
    
    // 复制第二部分（缓冲区开头剩余数据）
    if (first_chunk_size < total_events * event_size) {
        size_t second_chunk_size = total_events * event_size - first_chunk_size;
        memcpy(event_buffer + first_chunk_size, win->event_buffer, second_chunk_size);
    }

    win->event_buffer_index = 0; // 重置索引
    //unlock_share();
    return total_events;
}
