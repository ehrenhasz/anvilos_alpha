 
 

#ifndef __MESON_VDEC_ESPARSER_H_
#define __MESON_VDEC_ESPARSER_H_

#include <linux/platform_device.h>

#include "vdec.h"

int esparser_init(struct platform_device *pdev, struct amvdec_core *core);
int esparser_power_up(struct amvdec_session *sess);

 
int esparser_queue_eos(struct amvdec_core *core, const u8 *data, u32 len);

 
void esparser_queue_all_src(struct work_struct *work);

#define ESPARSER_MIN_PACKET_SIZE SZ_4K

#endif
