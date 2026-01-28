#ifndef __DRIVERS_USB_CHIPIDEA_OTG_FSM_H
#define __DRIVERS_USB_CHIPIDEA_OTG_FSM_H
#include <linux/usb/otg-fsm.h>
#define TA_WAIT_VRISE        (100)	 
#define TA_WAIT_VFALL        (1000)	 
#define TA_WAIT_BCON         (10000)	 
#define TA_AIDL_BDIS         (5000)	 
#define TA_BIDL_ADIS         (500)	 
#define TB_DATA_PLS          (10)	 
#define TB_SRP_FAIL          (6000)	 
#define TB_ASE0_BRST         (155)	 
#define TB_SE0_SRP           (1000)	 
#define TB_SSEND_SRP         (1500)	 
#define TB_AIDL_BDIS         (20)	 
#if IS_ENABLED(CONFIG_USB_OTG_FSM)
int ci_hdrc_otg_fsm_init(struct ci_hdrc *ci);
int ci_otg_fsm_work(struct ci_hdrc *ci);
irqreturn_t ci_otg_fsm_irq(struct ci_hdrc *ci);
void ci_hdrc_otg_fsm_start(struct ci_hdrc *ci);
void ci_hdrc_otg_fsm_remove(struct ci_hdrc *ci);
#else
static inline int ci_hdrc_otg_fsm_init(struct ci_hdrc *ci)
{
	return 0;
}
static inline int ci_otg_fsm_work(struct ci_hdrc *ci)
{
	return -ENXIO;
}
static inline irqreturn_t ci_otg_fsm_irq(struct ci_hdrc *ci)
{
	return IRQ_NONE;
}
static inline void ci_hdrc_otg_fsm_start(struct ci_hdrc *ci)
{
}
static inline void ci_hdrc_otg_fsm_remove(struct ci_hdrc *ci)
{
}
#endif
#endif  
