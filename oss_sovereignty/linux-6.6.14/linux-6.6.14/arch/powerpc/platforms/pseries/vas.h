#ifndef _VAS_H
#define _VAS_H
#include <asm/vas.h>
#include <linux/mutex.h>
#include <linux/stringify.h>
#define VAS_MOD_WIN_CLOSE	PPC_BIT(0)
#define VAS_MOD_WIN_JOBS_KILL	PPC_BIT(1)
#define VAS_MOD_WIN_DR		PPC_BIT(3)
#define VAS_MOD_WIN_PR		PPC_BIT(4)
#define VAS_MOD_WIN_SF		PPC_BIT(5)
#define VAS_MOD_WIN_TA		PPC_BIT(6)
#define VAS_MOD_WIN_FLAGS	(VAS_MOD_WIN_JOBS_KILL | VAS_MOD_WIN_DR | \
				VAS_MOD_WIN_PR | VAS_MOD_WIN_SF)
#define VAS_WIN_ACTIVE		0x0
#define VAS_WIN_CLOSED		0x1
#define VAS_WIN_INACTIVE	0x2	 
#define VAS_WIN_MOD_IN_PROCESS	0x3
#define VAS_COPY_PASTE_USER_MODE	0x00000001
#define VAS_COP_OP_USER_MODE		0x00000010
#define VAS_GZIP_QOS_CAPABILITIES	0x56516F73477A6970
#define VAS_GZIP_DEFAULT_CAPABILITIES	0x56446566477A6970
enum vas_migrate_action {
	VAS_SUSPEND,
	VAS_RESUME,
};
enum vas_cop_feat_type {
	VAS_GZIP_QOS_FEAT_TYPE,
	VAS_GZIP_DEF_FEAT_TYPE,
	VAS_MAX_FEAT_TYPE,
};
struct hv_vas_cop_feat_caps {
	__be64	descriptor;
	u8	win_type;		 
	u8	user_mode;
	__be16	max_lpar_creds;
	__be16	max_win_creds;
	union {
		__be16	reserved;
		__be16	def_lpar_creds;  
	};
	__be16	target_lpar_creds;
} __packed __aligned(0x1000);
struct vas_cop_feat_caps {
	u64		descriptor;
	u8		win_type;	 
	u8		user_mode;	 
	u16		max_lpar_creds;	 
	u16		max_win_creds;
	union {
		u16	reserved;	 
		u16	def_lpar_creds;  
	};
	atomic_t	nr_total_credits;	 
	atomic_t	nr_used_credits;	 
};
struct vas_caps {
	struct vas_cop_feat_caps caps;
	struct list_head list;	 
	int nr_open_wins_progress;	 
	int nr_close_wins;	 
	int nr_open_windows;	 
	u8 feat;		 
};
struct hv_vas_win_lpar {
	__be16	version;
	u8	win_type;
	u8	status;
	__be16	credits;	 
	__be16	reserved;
	__be32	pid;		 
	__be32	tid;		 
	__be64	win_addr;	 
	__be32	interrupt;	 
	__be32	fault;		 
	__be64	domain[6];
	__be64	win_util;	 
} __packed __aligned(0x1000);
struct pseries_vas_window {
	struct vas_window vas_win;
	u64 win_addr;		 
	u8 win_type;		 
	u32 complete_irq;	 
	u32 fault_irq;		 
	u64 domain[6];		 
	u64 util;
	u32 pid;		 
	struct list_head win_list;
	u64 flags;
	char *name;
	int fault_virq;
	atomic_t pending_faults;  
};
int sysfs_add_vas_caps(struct vas_cop_feat_caps *caps);
int vas_reconfig_capabilties(u8 type, int new_nr_creds);
int __init sysfs_pseries_vas_init(struct vas_all_caps *vas_caps);
#ifdef CONFIG_PPC_VAS
int vas_migration_handler(int action);
int pseries_vas_dlpar_cpu(void);
#else
static inline int vas_migration_handler(int action)
{
	return 0;
}
static inline int pseries_vas_dlpar_cpu(void)
{
	return 0;
}
#endif
#endif  
