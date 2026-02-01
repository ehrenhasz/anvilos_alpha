 
#ifndef __NVIF_IF0001_H__
#define __NVIF_IF0001_H__

#define NVIF_CONTROL_PSTATE_INFO                                           0x00
#define NVIF_CONTROL_PSTATE_ATTR                                           0x01
#define NVIF_CONTROL_PSTATE_USER                                           0x02

struct nvif_control_pstate_info_v0 {
	__u8  version;
	__u8  count;  
#define NVIF_CONTROL_PSTATE_INFO_V0_USTATE_DISABLE                         (-1)
#define NVIF_CONTROL_PSTATE_INFO_V0_USTATE_PERFMON                         (-2)
	__s8  ustate_ac;  
	__s8  ustate_dc;  
	__s8  pwrsrc;  
#define NVIF_CONTROL_PSTATE_INFO_V0_PSTATE_UNKNOWN                         (-1)
#define NVIF_CONTROL_PSTATE_INFO_V0_PSTATE_PERFMON                         (-2)
	__s8  pstate;  
	__u8  pad06[2];
};

struct nvif_control_pstate_attr_v0 {
	__u8  version;
#define NVIF_CONTROL_PSTATE_ATTR_V0_STATE_CURRENT                          (-1)
	__s8  state;  
	__u8  index;  
	__u8  pad03[5];
	__u32 min;
	__u32 max;
	char  name[32];
	char  unit[16];
};

struct nvif_control_pstate_user_v0 {
	__u8  version;
#define NVIF_CONTROL_PSTATE_USER_V0_STATE_UNKNOWN                          (-1)
#define NVIF_CONTROL_PSTATE_USER_V0_STATE_PERFMON                          (-2)
	__s8  ustate;  
	__s8  pwrsrc;  
	__u8  pad03[5];
};
#endif
