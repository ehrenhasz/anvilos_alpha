#ifndef __MPC86XX_H__
#define __MPC86XX_H__
extern void mpc86xx_smp_init(void);
extern void mpc86xx_init_irq(void);
extern long mpc86xx_time_init(void);
extern int mpc86xx_common_publish_devices(void);
#endif	 
