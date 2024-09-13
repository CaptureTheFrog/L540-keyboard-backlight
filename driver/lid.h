//
// Created by eddie on 13/09/24.
//

#ifndef DRIVER_LID_H
#define DRIVER_LID_H

#include <linux/device.h>

struct lid_show_callback_data {
    char* buf;
    ssize_t len;
    struct kbdbl_led* priv_data;
};

struct lid_store_callback_data {
    const char* buf;
    bool device_found;
    struct kbdbl_led* priv_data;
};

ssize_t lid_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
ssize_t lid_show(struct device *dev, struct device_attribute *attr, char *buf);
struct acpi_device* get_only_lid(void);

#endif //DRIVER_LID_H
