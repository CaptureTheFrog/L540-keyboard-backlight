# Thinkpad L540 USB Keyboard Backlight Controller and Linux Driver

---

*This is an ongoing project - hardware is 100% complete and built, firmware/driver is about 80% complete but needs cleaning up!*

---

Natively, the Thinkpad L540 hardware does not support backlit keyboards. An online seller of backlit thinkpad keyboards that *physically fit* in the L540 (but don't work electrically) neglected to tell me this. Therefore, I decided to build my own ATTiny85-powered keyboard backlight controller. It connects to the laptop over an internal unused USB ribbon cable socket (intended for a fingerprint reader), and communicates with my Linux kernel driver which exposes it as an LED device under sysfs.

I'm currently using a Digispark board for firmware and driver testing. That's becaue the hardware is more or less the same as my controller board, and it helpfully has an on-board LED that I can PWM to simulate the keyboard backlight. It can also be plugged into any old USB port, which is much easier for testing than the semi-permenant deign of my PCB!

PCB design files, photos, and a full blog-style writeup will be uploaded at a later date once the project is complete and fitted into a laptop.
