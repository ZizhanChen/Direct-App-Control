// SPDX-License-Identifier: GPL-2.0-only
/*
 * 32bit compatibility wrappers for the input subsystem.
 *
 * Very heavily based on evdev.c - Copyright (c) 1999-2002 Vojtech Pavlik
 */

#include <linux/export.h>
#include <linux/uaccess.h>
#include "input-compat.h"

#include <linux/time.h>
#include <linux/timekeeping.h>
#include <linux/printk.h>

//typedef ssize_t (*zizhan_write_t)(const char* data, size_t length);
//typedef ssize_t (*zizhan_dump_t)(size_t length);

//static zizhan_write_t zizhan_write_handler;
//static zizhan_dump_t zizhan_dump_handler;

//void register_zizhan_write_handler(zizhan_write_t handler) {
//	zizhan_write_handler = handler;
//}

//void register_zizhan_dump_handler(zizhan_dump_t handler) {
//	zizhan_dump_handler = handler;
//}

//EXPORT_SYMBOL(register_zizhan_write_handler);
//EXPORT_SYMBOL(register_zizhan_dump_handler);

ssize_t zizhan_dump (size_t length);
ssize_t zizhan_write (const char* data, size_t length);
void zizhan_dispatch_once(void);

#ifdef CONFIG_COMPAT

//extern ssize_t zizhan_write (const char* data, size_t length);
//extern ssize_t zizhan_dump (size_t length);


int input_event_from_user(const char __user *buffer,
			  struct input_event *event)
{
	if (in_compat_syscall() && !COMPAT_USE_64BIT_TIME) {
		struct input_event_compat compat_event;

		if (copy_from_user(&compat_event, buffer,
				   sizeof(struct input_event_compat)))
			return -EFAULT;

		event->input_event_sec = compat_event.sec;
		event->input_event_usec = compat_event.usec;
		event->type = compat_event.type;
		event->code = compat_event.code;
		event->value = compat_event.value;

	} else {
		if (copy_from_user(event, buffer, sizeof(struct input_event)))
			return -EFAULT;
	}

	return 0;
}

int input_event_to_user(char __user *buffer,
			const struct input_event *event)
{
	if (in_compat_syscall() && !COMPAT_USE_64BIT_TIME) {
		struct input_event_compat compat_event;
		char event_string[sizeof(struct input_event_compat)];

		compat_event.sec = event->input_event_sec;
		compat_event.usec = event->input_event_usec;
		compat_event.type = event->type;
		compat_event.code = event->code;
		compat_event.value = event->value;

		//zizhan
		printk(KERN_INFO pr_fmt("[zizhan] Compact Event. Type: %d, Code: %d, Value: %d\n"), compat_event.type, compat_event.code, compat_event.value);
		//if (zizhan_write_handler && zizhan_dump_handler){
			//memcpy(event_string, &compat_event, sizeof(struct input_event_compat));
			//zizhan_write(event_string, sizeof(struct input_event_compat));
			//zizhan_dump(sizeof(struct input_event_compat));
		//}
		memcpy(event_string, &compat_event, sizeof(struct input_event_compat));
		zizhan_write(event_string, sizeof(struct input_event_compat));
		zizhan_dispatch_once();

		if (copy_to_user(buffer, &compat_event,
				 sizeof(struct input_event_compat)))
			return -EFAULT;

	} else {
		//zizhan
		printk(KERN_INFO pr_fmt("[zizhan] Event. Type: %d, Code: %d, Value: %d\n"), event->type, event->code, event->value);
		char event_string[sizeof(struct input_event)];
		//if (zizhan_write_handler && zizhan_dump_handler){
			//memcpy(event_string, event, sizeof(struct input_event));
			//zizhan_write(event_string, sizeof(struct input_event));
			//zizhan_dump(sizeof(struct input_event));
		//}

		memcpy(event_string, event, sizeof(struct input_event));
		zizhan_write(event_string, sizeof(struct input_event));
		zizhan_dispatch_once();
		if (copy_to_user(buffer, event, sizeof(struct input_event)))
			return -EFAULT;
	}

	return 0;
}

int input_ff_effect_from_user(const char __user *buffer, size_t size,
			      struct ff_effect *effect)
{
	if (in_compat_syscall()) {
		struct ff_effect_compat *compat_effect;

		if (size != sizeof(struct ff_effect_compat))
			return -EINVAL;

		/*
		 * It so happens that the pointer which needs to be changed
		 * is the last field in the structure, so we can retrieve the
		 * whole thing and replace just the pointer.
		 */
		compat_effect = (struct ff_effect_compat *)effect;

		if (copy_from_user(compat_effect, buffer,
				   sizeof(struct ff_effect_compat)))
			return -EFAULT;

		if (compat_effect->type == FF_PERIODIC &&
		    compat_effect->u.periodic.waveform == FF_CUSTOM)
			effect->u.periodic.custom_data =
				compat_ptr(compat_effect->u.periodic.custom_data);
	} else {
		if (size != sizeof(struct ff_effect))
			return -EINVAL;

		if (copy_from_user(effect, buffer, sizeof(struct ff_effect)))
			return -EFAULT;
	}

	return 0;
}

#else

int input_event_from_user(const char __user *buffer,
			 struct input_event *event)
{
	if (copy_from_user(event, buffer, sizeof(struct input_event)))
		return -EFAULT;

	return 0;
}

int input_event_to_user(char __user *buffer,
			const struct input_event *event)
{
	if (copy_to_user(buffer, event, sizeof(struct input_event)))
		return -EFAULT;

	return 0;
}

int input_ff_effect_from_user(const char __user *buffer, size_t size,
			      struct ff_effect *effect)
{
	if (size != sizeof(struct ff_effect))
		return -EINVAL;

	if (copy_from_user(effect, buffer, sizeof(struct ff_effect)))
		return -EFAULT;

	return 0;
}

#endif /* CONFIG_COMPAT */

EXPORT_SYMBOL_GPL(input_event_from_user);
EXPORT_SYMBOL_GPL(input_event_to_user);
EXPORT_SYMBOL_GPL(input_ff_effect_from_user);
