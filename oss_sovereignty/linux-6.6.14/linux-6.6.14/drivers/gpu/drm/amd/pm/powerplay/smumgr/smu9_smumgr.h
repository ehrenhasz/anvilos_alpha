#ifndef _SMU9_SMUMANAGER_H_
#define _SMU9_SMUMANAGER_H_
bool smu9_is_smc_ram_running(struct pp_hwmgr *hwmgr);
int smu9_send_msg_to_smc(struct pp_hwmgr *hwmgr, uint16_t msg);
int smu9_send_msg_to_smc_with_parameter(struct pp_hwmgr *hwmgr,
					uint16_t msg, uint32_t parameter);
uint32_t smu9_get_argument(struct pp_hwmgr *hwmgr);
#endif
