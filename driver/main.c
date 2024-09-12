#include <linux/module.h>
#include <linux/usb.h>
#include <linux/leds.h>
#include <linux/acpi.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>

#include "../kbd_bl.h"

#define LED_NAME        "platform::kbd_backlight"

// TODO: use mutexes to protect LED access! see led_cdev.led_access mutex?

static struct usb_device_id your_device_table[] = {
        { USB_DEVICE(KBD_BL_USB_VENDOR_ID, KBD_BL_USB_PRODUCT_ID) },
        { } // Terminating entry
};
MODULE_DEVICE_TABLE(usb, your_device_table);

struct kbdbl_led {
    struct usb_device* udev;
    struct led_classdev led_cdev;
    acpi_bus_id active_lid;
    // other members...
};

#define ACPI_BUS_ID_MAX_LEN    (sizeof(acpi_bus_id) / sizeof(((acpi_bus_id*)0)[0]))

static void urb_cleanup_generic(struct urb *urb)
{
    kfree(urb->transfer_buffer);
    kfree(urb->setup_packet);
    usb_free_urb(urb);
}

static ssize_t flags_show(struct device *dev, struct device_attribute *attr, char *buf) {
    int retval;
    struct led_classdev* led_cdev = dev_get_drvdata(dev);
    struct kbdbl_led* priv_data = container_of(led_cdev, struct kbdbl_led, led_cdev);
    struct usb_device* udev = priv_data->udev;

    // needs to be DMA-safe so kmalloc it
    status_t* status = kmalloc(sizeof(status_t), GFP_KERNEL); // Buffer to receive the flag status from the device
    if (!status) {
        printk(KERN_ERR "%s - failed to allocate memory for status\n", __func__);
        return -ENOMEM;
    }

    retval = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
                             KBD_BL_REQUEST_GET_STATE_DATA, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_IN,
                             0, 0,
                             status, sizeof(status_t), 50);
    if (retval < 0) {
        printk(KERN_ERR "%s - failed to submit control msg, error %d\n", __func__, retval);
        kfree(status);
        return retval;
    }

    if (retval != sizeof(status_t)){
        printk(KERN_ERR "%s - device returned %d bytes but we expected %lu\n", __func__, retval, sizeof(status_t));
        kfree(status);
        return -EINVAL;
    }

    memcpy(buf, &status->flags, sizeof(status->flags));
    kfree(status);

    return sizeof(status->flags);
}

static ssize_t flags_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    int retval;
    struct led_classdev* led_cdev = dev_get_drvdata(dev);
    struct kbdbl_led* priv_data = container_of(led_cdev, struct kbdbl_led, led_cdev);
    struct usb_device* udev = priv_data->udev;

    if(count != sizeof(((status_t*)0)->flags)){
        return -EINVAL;
    }

    // needs to be DMA-safe so kmalloc it
    typeof(((status_t*)0)->flags)* flags = kmalloc(sizeof(((status_t*)0)->flags), GFP_KERNEL);
    if (!flags) {
        printk(KERN_ERR "%s - failed to allocate memory for falgs\n", __func__);
        return -ENOMEM;
    }

    memcpy(flags, buf, sizeof(((status_t*)0)->flags));

    retval = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
                             KBD_BL_REQUEST_SET_STATE_DATA, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_OUT,
                             0, offsetof(status_t, flags),
                             flags, sizeof(((status_t*)0)->flags), 5000);
    if (retval < 0) {
        printk(KERN_ERR "%s - failed to submit control msg, error %d\n", __func__, retval);
        kfree(flags);
        return retval;
    }

    if (retval != sizeof(((status_t*)0)->flags)){
        printk(KERN_ERR "%s - we wrote %d bytes but expected to write %lu\n", __func__, retval, sizeof(((status_t*)0)->flags));
        kfree(flags);
        return -EINVAL;
    }

    kfree(flags);
    return retval;
}

static DEVICE_ATTR(flags, 0664, flags_show, flags_store);


static ssize_t delay_on_show(struct device *dev, struct device_attribute *attr, char *buf) {
    int retval;
    struct led_classdev* led_cdev = dev_get_drvdata(dev);
    struct kbdbl_led* priv_data = container_of(led_cdev, struct kbdbl_led, led_cdev);
    struct usb_device* udev = priv_data->udev;

    // needs to be DMA-safe so kmalloc it
    status_t* status = kmalloc(sizeof(status_t), GFP_KERNEL); // Buffer to receive the flag status from the device
    if (!status) {
        printk(KERN_ERR "%s - failed to allocate memory for status\n", __func__);
        return -ENOMEM;
    }

    retval = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
                             KBD_BL_REQUEST_GET_STATE_DATA, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_IN,
                             0, 0,
                             status, sizeof(status_t), 50);
    if (retval < 0) {
        printk(KERN_ERR "%s - failed to submit control msg, error %d\n", __func__, retval);
        kfree(status);
        return retval;
    }

    if (retval != sizeof(status_t)){
        printk(KERN_ERR "%s - device returned %d bytes but we expected %lu\n", __func__, retval, sizeof(status_t));
        kfree(status);
        return -EINVAL;
    }

    retval = snprintf(buf, PAGE_SIZE, "%d\n", status->delay_on);
    kfree(status);

    return retval;
}

static ssize_t delay_on_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    int retval;
    struct led_classdev* led_cdev = dev_get_drvdata(dev);
    struct kbdbl_led* priv_data = container_of(led_cdev, struct kbdbl_led, led_cdev);
    struct usb_device* udev = priv_data->udev;
    
    // needs to be DMA-safe so kmalloc it
    typeof(((status_t*)0)->delay_on)* value = kmalloc(sizeof(((status_t*)0)->delay_on), GFP_KERNEL);
    if (!value) {
        printk(KERN_ERR "%s - failed to allocate memory for value\n", __func__);
        return -ENOMEM;
    }

    if(kstrtoul(buf, 10, (unsigned long*)value) != 0){
        printk(KERN_ERR "%s - failed to parse unsigned long\n", __func__);
        kfree(value);
        return -EINVAL;
    }

    retval = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
                             KBD_BL_REQUEST_SET_STATE_DATA, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_OUT,
                             0, offsetof(status_t, delay_on),
                             value, sizeof(((status_t*)0)->delay_on), 5000);
    if (retval < 0) {
        printk(KERN_ERR "%s - failed to submit control msg, error %d\n", __func__, retval);
        kfree(value);
        return retval;
    }

    if (retval != sizeof(((status_t*)0)->delay_on)){
        printk(KERN_ERR "%s - we wrote %d bytes but expected to write %lu\n", __func__, retval, sizeof(((status_t*)0)->delay_on));
        kfree(value);
        return -EINVAL;
    }

    kfree(value);
    return count;
}

static DEVICE_ATTR(delay_on, 0664, delay_on_show, delay_on_store);

static ssize_t delay_off_show(struct device *dev, struct device_attribute *attr, char *buf) {
    int retval;
    struct led_classdev* led_cdev = dev_get_drvdata(dev);
    struct kbdbl_led* priv_data = container_of(led_cdev, struct kbdbl_led, led_cdev);
    struct usb_device* udev = priv_data->udev;

    // needs to be DMA-safe so kmalloc it
    status_t* status = kmalloc(sizeof(status_t), GFP_KERNEL); // Buffer to receive the flag status from the device
    if (!status) {
        printk(KERN_ERR "%s - failed to allocate memory for status\n", __func__);
        return -ENOMEM;
    }

    retval = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
                             KBD_BL_REQUEST_GET_STATE_DATA, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_IN,
                             0, 0,
                             status, sizeof(status_t), 50);
    if (retval < 0) {
        printk(KERN_ERR "%s - failed to submit control msg, error %d\n", __func__, retval);
        kfree(status);
        return retval;
    }

    if (retval != sizeof(status_t)){
        printk(KERN_ERR "%s - device returned %d bytes but we expected %lu\n", __func__, retval, sizeof(status_t));
        kfree(status);
        return -EINVAL;
    }

    retval = snprintf(buf, PAGE_SIZE, "%d\n", status->delay_off);
    kfree(status);

    return retval;
}

static ssize_t delay_off_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    int retval;
    struct led_classdev* led_cdev = dev_get_drvdata(dev);
    struct kbdbl_led* priv_data = container_of(led_cdev, struct kbdbl_led, led_cdev);
    struct usb_device* udev = priv_data->udev;

    // needs to be DMA-safe so kmalloc it
    typeof(((status_t*)0)->delay_off)* value = kmalloc(sizeof(((status_t*)0)->delay_off), GFP_KERNEL);
    if (!value) {
        printk(KERN_ERR "%s - failed to allocate memory for value\n", __func__);
        return -ENOMEM;
    }

    if(kstrtoul(buf, 10, (unsigned long*)value) != 0){
        printk(KERN_ERR "%s - failed to parse unsigned long\n", __func__);
        kfree(value);
        return -EINVAL;
    }

    retval = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
                             KBD_BL_REQUEST_SET_STATE_DATA, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_OUT,
                             0, offsetof(status_t, delay_off),
                             value, sizeof(((status_t*)0)->delay_off), 5000);
    if (retval < 0) {
        printk(KERN_ERR "%s - failed to submit control msg, error %d\n", __func__, retval);
        kfree(value);
        return retval;
    }

    if (retval != sizeof(((status_t*)0)->delay_off)){
        printk(KERN_ERR "%s - we wrote %d bytes but expected to write %lu\n", __func__, retval, sizeof(((status_t*)0)->delay_off));
        kfree(value);
        return -EINVAL;
    }

    kfree(value);
    return count;
}

static DEVICE_ATTR(delay_off, 0664, delay_off_show, delay_off_store);

struct lid_callback_data {
    char *buf;
    ssize_t len;
    struct kbdbl_led* priv_data;
};

static ssize_t lid_show(struct device *dev, struct device_attribute *attr, char *buf) {
    struct led_classdev* led_cdev = dev_get_drvdata(dev);
    struct kbdbl_led* priv_data = container_of(led_cdev, struct kbdbl_led, led_cdev);

    struct lid_callback_data data;
    data.priv_data = priv_data;
    data.buf = buf;
    data.len = 0;
    data.len += scnprintf(data.buf + data.len, PAGE_SIZE - data.len, priv_data->active_lid[0] == '\x00' ? "[none]" : "none");

    acpi_status lid_callback(acpi_handle handle, u32 lvl, void *context, void **rv) {
        struct acpi_device* device = acpi_fetch_acpi_dev(handle);
        struct lid_callback_data* data = (struct lid_callback_data*)context;
        data->len += scnprintf(data->buf + data->len, PAGE_SIZE - data->len,
                               strncmp(acpi_device_bid(device), data->priv_data->active_lid, ACPI_BUS_ID_MAX_LEN) == 0 ?
                                    " [%s]" : " %s",
                               acpi_device_bid(device));
        return AE_OK;
    }

    // Walk ACPI namespace to find LID devices
    acpi_get_devices("PNP0C0D", lid_callback, &data, NULL);

    data.len += scnprintf(data.buf + data.len, PAGE_SIZE - data.len, "\n");
    return data.len;
}

static ssize_t lid_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    struct led_classdev* led_cdev = dev_get_drvdata(dev);
    struct kbdbl_led* priv_data = container_of(led_cdev, struct kbdbl_led, led_cdev);

    if(sysfs_streq(buf, "none")) {
        memset(priv_data->active_lid, 0, ACPI_BUS_ID_MAX_LEN);
        return count;
    }

    struct lid_callback_data data;
    data.priv_data = priv_data;
    data.buf = buf;
    data.len = 0;

    acpi_status lid_callback(acpi_handle handle, u32 lvl, void *context, void **rv) {
        struct acpi_device* device = acpi_fetch_acpi_dev(handle);
        struct lid_callback_data* data = (struct lid_callback_data*)context;
        if(sysfs_streq(data->buf, acpi_device_bid(device))){
            strncpy(data->priv_data->active_lid, acpi_device_bid(device), ACPI_BUS_ID_MAX_LEN);
            data->len = 1;
            return AE_CTRL_TERMINATE;
        }
        return AE_OK;
    }

    // Walk ACPI namespace to find LID devices
    acpi_get_devices("PNP0C0D", lid_callback, &data, NULL);

    if(data.len == 0){
        return -EINVAL;
    }

    return count;
}

static DEVICE_ATTR(lid, 0664, lid_show, lid_store);

static void your_led_brightness_set(struct led_classdev *led_cdev,
                                    enum led_brightness brightness)
{
    struct urb *urb;
	struct usb_ctrlrequest *setup_data;
    struct kbdbl_led *priv_data = container_of(led_cdev, struct kbdbl_led, led_cdev);

    struct usb_device* udev = priv_data->udev;
    urb = usb_alloc_urb(0, GFP_KERNEL);
    if(urb == NULL){
		dev_err(led_cdev->dev, "%s - failed to allocate memory for urb\n", __func__);
		return;
	}

	// Allocate memory for the setup data using the provided struct usb_ctrlrequest
	setup_data = kmalloc(sizeof(struct usb_ctrlrequest), GFP_KERNEL);
	if (setup_data == NULL) {
		dev_err(led_cdev->dev, "%s - failed to allocate memory for setup data\n", __func__);
		usb_free_urb(urb);
		return;
	}

	// Fill in the setup data structure
	setup_data->bRequestType = USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_OUT;
	setup_data->bRequest = KBD_BL_REQUEST_SET_TARGET_BRIGHTNESS;  // Your specific control request
	setup_data->wValue = cpu_to_le16(brightness);  // Value parameter
	setup_data->wIndex = 0;  // Index parameter
	setup_data->wLength = 0; // Length parameter (adjust as needed)

    usb_fill_control_urb(urb,
                         udev,
                         usb_sndctrlpipe(udev, 0),
						 (unsigned char*) setup_data,
                         NULL,
                         0,
						 urb_cleanup_generic,
						 NULL);

    /* send the data out the control port */
    int retval = usb_submit_urb(urb, GFP_KERNEL);
    if (retval) {
        dev_err(led_cdev->dev,
                "%s - failed submitting control urb, error %d\n",
                __func__, retval);
		usb_free_urb(urb);
		kfree(setup_data);
    }
}

static enum led_brightness your_led_brightness_get(struct led_classdev *led_cdev)
{
    int retval;
    struct kbdbl_led* priv_data = container_of(led_cdev, struct kbdbl_led, led_cdev);
    struct usb_device* udev = priv_data->udev;

    // needs to be DMA-safe so kmalloc it
    typeof(((status_t*)0)->target_brightness)* value = kmalloc(sizeof(((status_t*)0)->target_brightness), GFP_KERNEL);
    if (!value) {
        printk(KERN_ERR "%s - failed to allocate memory for value\n", __func__);
        return -ENOMEM;
    }

    retval = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
                             KBD_BL_REQUEST_GET_TARGET_BRIGHTNESS, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_IN,
                             0, 0,
                             value, sizeof(((status_t*)0)->target_brightness), 50);
    if (retval < 0) {
        printk(KERN_ERR "%s - failed to submit control msg, error %d\n", __func__, retval);
        kfree(value);
        return retval;
    }

    if (retval != sizeof(((status_t*)0)->target_brightness)){
        printk(KERN_ERR "%s - device returned %d bytes but we expected %lu\n", __func__, retval, sizeof(((status_t*)0)->target_brightness));
        kfree(value);
        return -EINVAL;
    }

    enum led_brightness brightness = (enum led_brightness)*value;
    kfree(value);

    return brightness;
}

struct led_classdev led_cdev_template = {
    .name           = LED_NAME,
    .brightness_set = your_led_brightness_set,
    .brightness_get = your_led_brightness_get,
    .max_brightness = BRIGHTNESS_T_MAX,
};

static int your_module_probe(struct usb_interface *interface,
                             const struct usb_device_id *id)
{
    int ret;

    struct usb_device *udev = interface_to_usbdev(interface);
    struct kbdbl_led *priv_data;

    // Allocate and initialize your private data structure
    priv_data = kmalloc(sizeof(struct kbdbl_led), GFP_KERNEL);
    if (!priv_data)
        return -ENOMEM;

    // Initialize the USB device pointer in your private data
    priv_data->udev = udev;
    memcpy(&priv_data->led_cdev, &led_cdev_template, sizeof(struct led_classdev));

    priv_data->led_cdev.brightness = LED_OFF;  // Initial brightness
    memset(priv_data->active_lid, 0, ACPI_BUS_ID_MAX_LEN);

    ret = led_classdev_register(&interface->dev, &priv_data->led_cdev);
    if (ret) {
        dev_err(&interface->dev, "Failed to register LED classdev: %d\n", ret);
        kfree(priv_data);
        return -ENODEV;
    }

    usb_set_intfdata(interface, priv_data);

    int retval = device_create_file(priv_data->led_cdev.dev, &dev_attr_flags);
    if (retval) {
        dev_err(&interface->dev, "failed to create flags sysfs entry\n");
        kfree(priv_data);
        return retval;
    }

    retval = device_create_file(priv_data->led_cdev.dev, &dev_attr_delay_on);
    if (retval) {
        dev_err(&interface->dev, "failed to create delay_on sysfs entry\n");
        kfree(priv_data);
        return retval;
    }

    retval = device_create_file(priv_data->led_cdev.dev, &dev_attr_delay_off);
    if (retval) {
        dev_err(&interface->dev, "failed to create delay_off sysfs entry\n");
        kfree(priv_data);
        return retval;
    }

    retval = device_create_file(priv_data->led_cdev.dev, &dev_attr_lid);
    if (retval) {
        dev_err(&interface->dev, "failed to create lid sysfs entry\n");
        kfree(priv_data);
        return retval;
    }

    return 0;
}

static void your_module_disconnect(struct usb_interface *interface)
{
    struct  kbdbl_led* priv_data = usb_get_intfdata(interface);
    if(!priv_data){
        printk("%s - Could not get USB interface data\n", __func__);
        return;
    }
    device_remove_file(&interface->dev, &dev_attr_flags);
    device_remove_file(&interface->dev, &dev_attr_delay_on);
    device_remove_file(&interface->dev, &dev_attr_delay_off);
    device_remove_file(&interface->dev, &dev_attr_lid);
    led_classdev_unregister(&priv_data->led_cdev);
}

static struct usb_driver your_usb_driver = {
        .name = "your_driver",
        .id_table = your_device_table,
        .probe = your_module_probe,
        .disconnect = your_module_disconnect,
};

module_usb_driver(your_usb_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Your LED Kernel Module");