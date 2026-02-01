
 
#include "xfs.h"
#include "xfs_error.h"

 
xfs_param_t xfs_params = {
			   
	.sgid_inherit	= {	0,		0,		1	},
	.symlink_mode	= {	0,		0,		1	},
	.panic_mask	= {	0,		0,		XFS_PTAG_MASK},
	.error_level	= {	0,		3,		11	},
	.syncd_timer	= {	1*100,		30*100,		7200*100},
	.stats_clear	= {	0,		0,		1	},
	.inherit_sync	= {	0,		1,		1	},
	.inherit_nodump	= {	0,		1,		1	},
	.inherit_noatim = {	0,		1,		1	},
	.xfs_buf_timer	= {	100/2,		1*100,		30*100	},
	.xfs_buf_age	= {	1*100,		15*100,		7200*100},
	.inherit_nosym	= {	0,		0,		1	},
	.rotorstep	= {	1,		1,		255	},
	.inherit_nodfrg	= {	0,		1,		1	},
	.fstrm_timer	= {	1,		30*100,		3600*100},
	.blockgc_timer	= {	1,		300,		3600*24},
};

struct xfs_globals xfs_globals = {
	.log_recovery_delay	=	0,	 
	.mount_delay		=	0,	 
#ifdef XFS_ASSERT_FATAL
	.bug_on_assert		=	true,	 
#else
	.bug_on_assert		=	false,	 
#endif
#ifdef DEBUG
	.pwork_threads		=	-1,	 
	.larp			=	false,	 
#endif
};
