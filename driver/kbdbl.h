//
// Created by eddie on 13/09/24.
//

#ifndef DRIVER_KBDBL_H
#define DRIVER_KBDBL_H

#include <linux/usb.h>
#include <linux/leds.h>
#include <linux/acpi.h>

struct kbdbl_led {
    struct usb_device* udev;
    struct led_classdev led_cdev;
    struct acpi_device* active_lid;
    // other members...
};

#endif //DRIVER_KBDBL_H
