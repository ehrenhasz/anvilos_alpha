#ifndef __WCMD_H__
#define __WCMD_H__
#include "device.h"
enum vnt_cmd {
	WLAN_CMD_INIT_MAC80211,
	WLAN_CMD_SETPOWER,
	WLAN_CMD_TBTT_WAKEUP,
	WLAN_CMD_BECON_SEND,
	WLAN_CMD_CHANGE_ANTENNA
};
#define CMD_Q_SIZE              32
enum vnt_cmd_state {
	WLAN_CMD_INIT_MAC80211_START,
	WLAN_CMD_SETPOWER_START,
	WLAN_CMD_TBTT_WAKEUP_START,
	WLAN_CMD_BECON_SEND_START,
	WLAN_CMD_CHANGE_ANTENNA_START,
	WLAN_CMD_IDLE
};
struct vnt_private;
void vnt_reset_command_timer(struct vnt_private *priv);
int vnt_schedule_command(struct vnt_private *priv, enum vnt_cmd);
void vnt_run_command(struct work_struct *work);
#endif  
