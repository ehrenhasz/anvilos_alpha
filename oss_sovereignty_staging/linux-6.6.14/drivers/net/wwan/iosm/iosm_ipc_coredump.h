 

#ifndef _IOSM_IPC_COREDUMP_H_
#define _IOSM_IPC_COREDUMP_H_

#include "iosm_ipc_devlink.h"

 
#define MAX_CD_LIST_SIZE  0x1000

 
#define MAX_DATA_SIZE 0x00010000

 
#define MAX_SIZE_LEN 32

 
struct iosm_cd_list_entry {
	__le32 size;
	char filename[IOSM_MAX_FILENAME_LEN];
} __packed;

 
struct iosm_cd_list {
	__le32 num_entries;
	struct iosm_cd_list_entry entry[];
} __packed;

 
struct iosm_cd_table {
	__le32 version;
	struct iosm_cd_list list;
} __packed;

int ipc_coredump_collect(struct iosm_devlink *devlink, u8 **data, int entry,
			 u32 region_size);

int ipc_coredump_get_list(struct iosm_devlink *devlink, u16 cmd);

#endif  
