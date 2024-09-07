#ifndef __kbd_bl_h
#define __kbd_bl_h

#include "kbd_bl_descriptor.h"

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#define KBD_BL_REQUEST_GET_TARGET_BRIGHTNESS	0x0
#define KBD_BL_REQUEST_SET_TARGET_BRIGHTNESS	0x1
#define KBD_BL_REQUEST_GET_ACTUAL_BRIGHTNESS	0x2
#define KBD_BL_REQUEST_GET_STATE_DATA			0x3
#define KBD_BL_REQUEST_SET_STATE_DATA			0x4
#define KBD_BL_REQUEST_MAX						KBD_BL_REQUEST_SET_STATE_DATA

#define KBD_BL_FLAGS_NONE			0
#define KBD_BL_FLAGS_HARDWARE_BLINK	(1<<0)
#define KBD_BL_FLAGS_SUSPEND		(1<<1)
#define KBD_BL_FLAGS_FADE_ENABLED	(1<<2)
#define KBD_BL_FLAGS_FADE_SINE		(1<<3)

typedef struct {
	uint8_t flags;
    uint8_t target_brightness;
    uint8_t low_blink_target_brightness;
	uint32_t delay_on;
    uint32_t delay_off;
    uint32_t fade_time;

} __attribute__((packed)) status_t;

typedef struct{
    status_t external_status;
} internal_status_t;

#endif