#ifndef __ARCHE_PLATFORM_H
#define __ARCHE_PLATFORM_H
enum arche_platform_state {
	ARCHE_PLATFORM_STATE_OFF,
	ARCHE_PLATFORM_STATE_ACTIVE,
	ARCHE_PLATFORM_STATE_STANDBY,
	ARCHE_PLATFORM_STATE_FW_FLASHING,
};
int __init arche_apb_init(void);
void __exit arche_apb_exit(void);
int apb_ctrl_coldboot(struct device *dev);
int apb_ctrl_fw_flashing(struct device *dev);
int apb_ctrl_standby_boot(struct device *dev);
void apb_ctrl_poweroff(struct device *dev);
#endif	 
