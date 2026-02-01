 
#include <linux/ioctl.h>

 
struct amdgpu_debugfs_regs2_iocdata {
	__u32 use_srbm, use_grbm, pg_lock;
	struct {
		__u32 se, sh, instance;
	} grbm;
	struct {
		__u32 me, pipe, queue, vmid;
	} srbm;
};

struct amdgpu_debugfs_regs2_iocdata_v2 {
	__u32 use_srbm, use_grbm, pg_lock;
	struct {
		__u32 se, sh, instance;
	} grbm;
	struct {
		__u32 me, pipe, queue, vmid;
	} srbm;
	u32 xcc_id;
};

struct amdgpu_debugfs_gprwave_iocdata {
	u32 gpr_or_wave, se, sh, cu, wave, simd, xcc_id;
	struct {
		u32 thread, vpgr_or_sgpr;
	} gpr;
};

 
struct amdgpu_debugfs_regs2_data {
	struct amdgpu_device *adev;
	struct mutex lock;
	struct amdgpu_debugfs_regs2_iocdata_v2 id;
};

struct amdgpu_debugfs_gprwave_data {
	struct amdgpu_device *adev;
	struct mutex lock;
	struct amdgpu_debugfs_gprwave_iocdata id;
};

enum AMDGPU_DEBUGFS_REGS2_CMDS {
	AMDGPU_DEBUGFS_REGS2_CMD_SET_STATE=0,
	AMDGPU_DEBUGFS_REGS2_CMD_SET_STATE_V2,
};

enum AMDGPU_DEBUGFS_GPRWAVE_CMDS {
	AMDGPU_DEBUGFS_GPRWAVE_CMD_SET_STATE=0,
};


#define AMDGPU_DEBUGFS_REGS2_IOC_SET_STATE _IOWR(0x20, AMDGPU_DEBUGFS_REGS2_CMD_SET_STATE, struct amdgpu_debugfs_regs2_iocdata)
#define AMDGPU_DEBUGFS_REGS2_IOC_SET_STATE_V2 _IOWR(0x20, AMDGPU_DEBUGFS_REGS2_CMD_SET_STATE_V2, struct amdgpu_debugfs_regs2_iocdata_v2)


#define AMDGPU_DEBUGFS_GPRWAVE_IOC_SET_STATE _IOWR(0x20, AMDGPU_DEBUGFS_GPRWAVE_CMD_SET_STATE, struct amdgpu_debugfs_gprwave_iocdata)
