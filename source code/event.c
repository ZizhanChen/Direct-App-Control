#include <linux/types.h>
#include <linux/list.h>
#include <linux/compat.h>
#include "event.h"
#include <linux/slab.h>


event* dequeue_event(data_queue* queue) {
    char * buff;
    buff = kzalloc(sizeof(event), GFP_KERNEL);
    dequeue_data(queue, buff, sizeof(event));
    return (event *) buff;
}
