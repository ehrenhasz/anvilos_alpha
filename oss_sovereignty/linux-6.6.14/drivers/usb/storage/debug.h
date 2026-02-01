 
 

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <linux/kernel.h>

#ifdef CONFIG_USB_STORAGE_DEBUG
void usb_stor_show_command(const struct us_data *us, struct scsi_cmnd *srb);
void usb_stor_show_sense(const struct us_data *us, unsigned char key,
			 unsigned char asc, unsigned char ascq);
__printf(2, 3) void usb_stor_dbg(const struct us_data *us,
				 const char *fmt, ...);

#define US_DEBUG(x)		x
#else
__printf(2, 3)
static inline void _usb_stor_dbg(const struct us_data *us,
				 const char *fmt, ...)
{
}
#define usb_stor_dbg(us, fmt, ...)				\
	do { if (0) _usb_stor_dbg(us, fmt, ##__VA_ARGS__); } while (0)
#define US_DEBUG(x)
#endif

#endif
