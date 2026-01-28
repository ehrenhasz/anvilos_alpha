#ifndef CYPRESS_FIRMWARE_H
#define CYPRESS_FIRMWARE_H
#define CYPRESS_AN2135  0
#define CYPRESS_AN2235  1
#define CYPRESS_FX2     2
struct hexline {
	u8 len;
	u32 addr;
	u8 type;
	u8 data[255];
	u8 chk;
};
int cypress_load_firmware(struct usb_device *, const struct firmware *, int);
#endif
