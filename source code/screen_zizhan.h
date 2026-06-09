/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SCREEN_ZIZHAN_H
#define _SCREEN_ZIZHAN_H

typedef struct screen_zizhan {
    int top;
    int left;
    int height;
    int width;
    int zorder;
    char * event_buffer;
    int buffer_size;
    //int buffer_index;
}screen_t;

#endif /* _SCREEN_ZIZHAN_H */