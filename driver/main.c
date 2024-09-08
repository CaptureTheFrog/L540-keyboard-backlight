#include <linux/module.h>
#include <linux/usb.h>
#include <linux/leds.h>

#include "../kbd_bl.h"

#define LED_NAME        "platform::kbd_backlight"

static struct usb_device_id your_device_table[] = {
        { USB_DEVICE(KBD_BL_USB_VENDOR_ID, KBD_BL_USB_PRODUCT_ID) },
        { } // Terminating entry
};
MODULE_DEVICE_TABLE(usb, your_device_table);

struct kbdbl_led {
    struct usb_device* udev;
    struct led_classdev led_cdev;
    // other members...
};

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
    struct urb *urb;
    struct usb_ctrlrequest *setup_data;
    struct kbdbl_led *priv_data = container_of(led_cdev, struct kbdbl_led, led_cdev);

    struct usb_device* udev = priv_data->udev;
    urb = usb_alloc_urb(0, GFP_KERNEL);
    if(urb == NULL){
        dev_err(led_cdev->dev, "%s - failed to allocate memory for urb\n", __func__);
        return -1;
    }

    // Allocate memory for the setup data using the provided struct usb_ctrlrequest
    setup_data = kmalloc(sizeof(struct usb_ctrlrequest), GFP_KERNEL);
    if (setup_data == NULL) {
        dev_err(led_cdev->dev, "%s - failed to allocate memory for setup data\n", __func__);
        usb_free_urb(urb);
        return -1;
    }

    // Fill in the setup data structure
    setup_data->bRequestType = USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_IN;
    setup_data->bRequest = KBD_BL_REQUEST_GET_TARGET_BRIGHTNESS;  // Your specific control request
    setup_data->wValue = 0;  // Value parameter
    setup_data->wIndex = 0;  // Index parameter
    setup_data->wLength = 1; // Length parameter (adjust as needed)

    unsigned char* rcv_buf =  kmalloc(1, GFP_KERNEL);

    usb_fill_control_urb(urb,
                         udev,
                         usb_rcvctrlpipe(udev, 0),
                         (unsigned char*) setup_data,
                         rcv_buf,
                         1,
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

    return 0;
}

struct led_classdev led_cdev_template = {
    .name           = LED_NAME,
    .brightness_set = your_led_brightness_set,
    .brightness_get = your_led_brightness_get,
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

    ret = led_classdev_register(&interface->dev, &priv_data->led_cdev);
    if (ret) {
        dev_err(&interface->dev, "Failed to register LED classdev: %d\n", ret);
        kfree(priv_data);
        return -ENODEV;
    }

    usb_set_intfdata(interface, priv_data);

    int retval = device_create_file(priv_data->led_cdev.dev, &dev_attr_flags);
    if (retval) {
        dev_err(&interface->dev, "failed to create sysfs entry\n");
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