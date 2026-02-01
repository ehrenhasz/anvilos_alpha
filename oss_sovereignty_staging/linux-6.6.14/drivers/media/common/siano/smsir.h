 
 

#ifndef __SMS_IR_H__
#define __SMS_IR_H__

#include <linux/input.h>
#include <media/rc-core.h>

struct smscore_device_t;

struct ir_t {
	struct rc_dev *dev;
	char name[40];
	char phys[32];

	char *rc_codes;

	u32 timeout;
	u32 controller;
};

#ifdef CONFIG_SMS_SIANO_RC
int sms_ir_init(struct smscore_device_t *coredev);
void sms_ir_exit(struct smscore_device_t *coredev);
void sms_ir_event(struct smscore_device_t *coredev,
			const char *buf, int len);
#else
inline static int sms_ir_init(struct smscore_device_t *coredev) {
	return 0;
}
inline static void sms_ir_exit(struct smscore_device_t *coredev) {};
inline static void sms_ir_event(struct smscore_device_t *coredev,
			const char *buf, int len) {};
#endif

#endif  

