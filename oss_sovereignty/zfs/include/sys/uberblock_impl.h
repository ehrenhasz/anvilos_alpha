#ifndef _SYS_UBERBLOCK_IMPL_H
#define	_SYS_UBERBLOCK_IMPL_H
#include <sys/uberblock.h>
#ifdef	__cplusplus
extern "C" {
#endif
#define	UBERBLOCK_MAGIC		0x00bab10c		 
#define	UBERBLOCK_SHIFT		10			 
#define	MMP_MAGIC		0xa11cea11		 
#define	MMP_INTERVAL_VALID_BIT	0x01
#define	MMP_SEQ_VALID_BIT	0x02
#define	MMP_FAIL_INT_VALID_BIT	0x04
#define	MMP_VALID(ubp)		(ubp->ub_magic == UBERBLOCK_MAGIC && \
				    ubp->ub_mmp_magic == MMP_MAGIC)
#define	MMP_INTERVAL_VALID(ubp)	(MMP_VALID(ubp) && (ubp->ub_mmp_config & \
				    MMP_INTERVAL_VALID_BIT))
#define	MMP_SEQ_VALID(ubp)	(MMP_VALID(ubp) && (ubp->ub_mmp_config & \
				    MMP_SEQ_VALID_BIT))
#define	MMP_FAIL_INT_VALID(ubp)	(MMP_VALID(ubp) && (ubp->ub_mmp_config & \
				    MMP_FAIL_INT_VALID_BIT))
#define	MMP_INTERVAL(ubp)	((ubp->ub_mmp_config & 0x00000000FFFFFF00) \
				    >> 8)
#define	MMP_SEQ(ubp)		((ubp->ub_mmp_config & 0x0000FFFF00000000) \
				    >> 32)
#define	MMP_FAIL_INT(ubp)	((ubp->ub_mmp_config & 0xFFFF000000000000) \
				    >> 48)
#define	MMP_INTERVAL_SET(write) \
	    (((uint64_t)(write & 0xFFFFFF) << 8) | MMP_INTERVAL_VALID_BIT)
#define	MMP_SEQ_SET(seq) \
	    (((uint64_t)(seq & 0xFFFF) << 32) | MMP_SEQ_VALID_BIT)
#define	MMP_FAIL_INT_SET(fail) \
	    (((uint64_t)(fail & 0xFFFF) << 48) | MMP_FAIL_INT_VALID_BIT)
struct uberblock {
	uint64_t	ub_magic;	 
	uint64_t	ub_version;	 
	uint64_t	ub_txg;		 
	uint64_t	ub_guid_sum;	 
	uint64_t	ub_timestamp;	 
	blkptr_t	ub_rootbp;	 
	uint64_t	ub_software_version;
	uint64_t	ub_mmp_magic;	 
	uint64_t	ub_mmp_delay;
	uint64_t	ub_mmp_config;
	uint64_t	ub_checkpoint_txg;
};
#ifdef	__cplusplus
}
#endif
#endif	 
