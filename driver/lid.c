//
// Created by eddie on 13/09/24.
//

#include "kbdbl.h"
#include "lid.h"
#include <linux/acpi.h>

ssize_t lid_show(struct device *dev, struct device_attribute *attr, char *buf) {
    struct led_classdev* led_cdev = dev_get_drvdata(dev);
    struct kbdbl_led* priv_data = container_of(led_cdev, struct kbdbl_led, led_cdev);

    struct lid_show_callback_data data;
    data.priv_data = priv_data;
    data.buf = buf;
    data.len = 0;
    data.len += scnprintf(data.buf + data.len, PAGE_SIZE - data.len, priv_data->active_lid == NULL ? "[none]" : "none");

    acpi_status lid_callback(acpi_handle handle, u32 lvl, void *context, void **rv) {
        struct acpi_device* device = acpi_fetch_acpi_dev(handle);
        struct lid_show_callback_data* data = (struct lid_show_callback_data*)context;
        data->len += scnprintf(data->buf + data->len, PAGE_SIZE - data->len,
                               device == data->priv_data->active_lid ? " [%s]" : " %s",
                               acpi_device_bid(device));
        return AE_OK;
    }

    // Walk ACPI namespace to find LID devices
    acpi_get_devices("PNP0C0D", lid_callback, &data, NULL);

    data.len += scnprintf(data.buf + data.len, PAGE_SIZE - data.len, "\n");
    return data.len;
}

ssize_t lid_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    struct led_classdev* led_cdev = dev_get_drvdata(dev);
    struct kbdbl_led* priv_data = container_of(led_cdev, struct kbdbl_led, led_cdev);

    if(sysfs_streq(buf, "none")) {
        priv_data->active_lid = NULL;
        return count;
    }

    struct lid_store_callback_data data;
    data.priv_data = priv_data;
    data.buf = buf;
    data.device_found = false;

    acpi_status lid_callback(acpi_handle handle, u32 lvl, void *context, void **rv) {
        struct acpi_device* device = acpi_fetch_acpi_dev(handle);
        struct lid_store_callback_data* data = (struct lid_store_callback_data*)context;
        if(sysfs_streq(data->buf, acpi_device_bid(device))){
            data->priv_data->active_lid = device;
            data->device_found = true;
            return AE_CTRL_TERMINATE;
        }
        return AE_OK;
    }

    // Walk ACPI namespace to find LID devices
    acpi_get_devices("PNP0C0D", lid_callback, &data, NULL);

    if(!data.device_found){
        return -EINVAL;
    }

    return count;
}