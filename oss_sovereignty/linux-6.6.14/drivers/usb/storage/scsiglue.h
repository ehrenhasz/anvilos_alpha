 
 

#ifndef _SCSIGLUE_H_
#define _SCSIGLUE_H_

extern void usb_stor_report_device_reset(struct us_data *us);
extern void usb_stor_report_bus_reset(struct us_data *us);
extern void usb_stor_host_template_init(struct scsi_host_template *sht,
					const char *name, struct module *owner);

extern unsigned char usb_stor_sense_invalidCDB[18];

#endif
