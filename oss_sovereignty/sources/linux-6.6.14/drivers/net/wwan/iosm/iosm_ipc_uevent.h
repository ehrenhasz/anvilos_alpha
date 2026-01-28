

#ifndef IOSM_IPC_UEVENT_H
#define IOSM_IPC_UEVENT_H


#define UEVENT_MDM_NOT_READY "MDM_NOT_READY"
#define UEVENT_ROM_READY "ROM_READY"
#define UEVENT_MDM_READY "MDM_READY"
#define UEVENT_CRASH "CRASH"
#define UEVENT_CD_READY "CD_READY"
#define UEVENT_CD_READY_LINK_DOWN "CD_READY_LINK_DOWN"
#define UEVENT_MDM_TIMEOUT "MDM_TIMEOUT"


#define MAX_UEVENT_LEN 64


struct ipc_uevent_info {
	struct device *dev;
	char uevent[MAX_UEVENT_LEN];
	struct work_struct work;
};


void ipc_uevent_send(struct device *dev, char *uevent);

#endif
