//
// Created by eddie on 13/09/24.
//

#include "brightness.h"
#include "kbdbl.h"
#include "../kbd_bl.h"


static void urb_cleanup_generic(struct urb *urb)
{
    kfree(urb->transfer_buffer);
    kfree(urb->setup_packet);
    usb_free_urb(urb);
}

void your_led_brightness_set(struct led_classdev *led_cdev,
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

enum led_brightness your_led_brightness_get(struct led_classdev *led_cdev)
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