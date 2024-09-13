//
// Created by eddie on 13/09/24.
//

#ifndef DRIVER_BRIGHTNESS_H
#define DRIVER_BRIGHTNESS_H

#include <linux/leds.h>

void your_led_brightness_set(struct led_classdev *led_cdev, enum led_brightness brightness);
enum led_brightness your_led_brightness_get(struct led_classdev *led_cdev);

#endif //DRIVER_BRIGHTNESS_H
