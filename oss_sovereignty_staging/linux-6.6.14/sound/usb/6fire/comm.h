 
 
#ifndef USB6FIRE_COMM_H
#define USB6FIRE_COMM_H

#include "common.h"

enum  
{
	COMM_RECEIVER_BUFSIZE = 64,
};

struct comm_runtime {
	struct sfire_chip *chip;

	struct urb receiver;
	u8 *receiver_buffer;

	u8 serial;  

	void (*init_urb)(struct comm_runtime *rt, struct urb *urb, u8 *buffer,
			void *context, void(*handler)(struct urb *urb));
	 
	int (*write8)(struct comm_runtime *rt, u8 request, u8 reg, u8 value);
	int (*write16)(struct comm_runtime *rt, u8 request, u8 reg,
			u8 vh, u8 vl);
};

int usb6fire_comm_init(struct sfire_chip *chip);
void usb6fire_comm_abort(struct sfire_chip *chip);
void usb6fire_comm_destroy(struct sfire_chip *chip);
#endif  

