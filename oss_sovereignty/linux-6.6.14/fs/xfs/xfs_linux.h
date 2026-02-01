
 
#ifndef __XFS_LINUX__
#define __XFS_LINUX__

#include <linux/types.h>
#include <linux/uuid.h>

 

typedef __s64			xfs_off_t;	 
typedef unsigned long long	xfs_ino_t;	 
typedef __s64			xfs_daddr_t;	 
typedef __u32			xfs_dev_t;
typedef __u32			xfs_nlink_t;

#include "xfs_types.h"

#include "kmem.h"
#include "mrlock.h"

#include <linux/semaphore.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/slab.h>
#include <linux/crc32c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/file.h>
#include <linux/filelock.h>
#include <linux/swap.h>
#include <linux/errno.h>
#include <linux/sched/signal.h>
#include <linux/bitops.h>
#include <linux/major.h>
#include <linux/pagemap.h>
#include <linux/vfs.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/sort.h>
#include <linux/cpu.h>
#include <linux/notifier.h>
#include <linux/delay.h>
#include <linux/log2.h>
#include <linux/spinlock.h>
#include <linux/random.h>
#include <linux/ctype.h>
#include <linux/writeback.h>
#include <linux/capability.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/list_sort.h>
#include <linux/ratelimit.h>
#include <linux/rhashtable.h>
#include <linux/xattr.h>
#include <linux/mnt_idmapping.h>
#include <linux/debugfs.h>

#include <asm/page.h>
#include <asm/div64.h>
#include <asm/param.h>
#include <linux/uaccess.h>
#include <asm/byteorder.h>
#include <asm/unaligned.h>

#include "xfs_fs.h"
#include "xfs_stats.h"
#include "xfs_sysctl.h"
#include "xfs_iops.h"
#include "xfs_aops.h"
#include "xfs_super.h"
#include "xfs_cksum.h"
#include "xfs_buf.h"
#include "xfs_message.h"
#include "xfs_drain.h"

#ifdef __BIG_ENDIAN
#define XFS_NATIVE_HOST 1
#else
#undef XFS_NATIVE_HOST
#endif

#define irix_sgid_inherit	xfs_params.sgid_inherit.val
#define irix_symlink_mode	xfs_params.symlink_mode.val
#define xfs_panic_mask		xfs_params.panic_mask.val
#define xfs_error_level		xfs_params.error_level.val
#define xfs_syncd_centisecs	xfs_params.syncd_timer.val
#define xfs_stats_clear		xfs_params.stats_clear.val
#define xfs_inherit_sync	xfs_params.inherit_sync.val
#define xfs_inherit_nodump	xfs_params.inherit_nodump.val
#define xfs_inherit_noatime	xfs_params.inherit_noatim.val
#define xfs_inherit_nosymlinks	xfs_params.inherit_nosym.val
#define xfs_rotorstep		xfs_params.rotorstep.val
#define xfs_inherit_nodefrag	xfs_params.inherit_nodfrg.val
#define xfs_fstrm_centisecs	xfs_params.fstrm_timer.val
#define xfs_blockgc_secs	xfs_params.blockgc_timer.val

#define current_cpu()		(raw_smp_processor_id())
#define current_set_flags_nested(sp, f)		\
		(*(sp) = current->flags, current->flags |= (f))
#define current_restore_flags_nested(sp, f)	\
		(current->flags = ((current->flags & ~(f)) | (*(sp) & (f))))

#define NBBY		8		 

 
#define	BLKDEV_IOSHIFT		PAGE_SHIFT
#define	BLKDEV_IOSIZE		(1<<BLKDEV_IOSHIFT)
 
#define	BLKDEV_BB		BTOBB(BLKDEV_IOSIZE)

#define ENOATTR		ENODATA		 
#define EWRONGFS	EINVAL		 
#define EFSCORRUPTED	EUCLEAN		 
#define EFSBADCRC	EBADMSG		 

#define __return_address __builtin_return_address(0)

 
#define __this_address	({ __label__ __here; __here: barrier(); &&__here; })

#define XFS_PROJID_DEFAULT	0

#define howmany(x, y)	(((x)+((y)-1))/(y))

static inline void delay(long ticks)
{
	schedule_timeout_uninterruptible(ticks);
}

 
struct xfs_kobj {
	struct kobject		kobject;
	struct completion	complete;
};

struct xstats {
	struct xfsstats __percpu	*xs_stats;
	struct xfs_kobj			xs_kobj;
};

extern struct xstats xfsstats;

static inline dev_t xfs_to_linux_dev_t(xfs_dev_t dev)
{
	return MKDEV(sysv_major(dev) & 0x1ff, sysv_minor(dev));
}

static inline xfs_dev_t linux_to_xfs_dev_t(dev_t dev)
{
	return sysv_encode_dev(dev);
}

 
#define xfs_sort(a,n,s,fn)	sort(a,n,s,fn,NULL)
#define xfs_stack_trace()	dump_stack()

static inline uint64_t rounddown_64(uint64_t x, uint32_t y)
{
	do_div(x, y);
	return x * y;
}

static inline uint64_t roundup_64(uint64_t x, uint32_t y)
{
	x += y - 1;
	do_div(x, y);
	return x * y;
}

static inline uint64_t howmany_64(uint64_t x, uint32_t y)
{
	x += y - 1;
	do_div(x, y);
	return x;
}

int xfs_rw_bdev(struct block_device *bdev, sector_t sector, unsigned int count,
		char *data, enum req_op op);

#define ASSERT_ALWAYS(expr)	\
	(likely(expr) ? (void)0 : assfail(NULL, #expr, __FILE__, __LINE__))

#ifdef DEBUG
#define ASSERT(expr)	\
	(likely(expr) ? (void)0 : assfail(NULL, #expr, __FILE__, __LINE__))

#else	 

#ifdef XFS_WARN

#define ASSERT(expr)	\
	(likely(expr) ? (void)0 : asswarn(NULL, #expr, __FILE__, __LINE__))

#else	 

#define ASSERT(expr)		((void)0)

#endif  
#endif  

#define XFS_IS_CORRUPT(mp, expr)	\
	(unlikely(expr) ? xfs_corruption_error(#expr, XFS_ERRLEVEL_LOW, (mp), \
					       NULL, 0, __FILE__, __LINE__, \
					       __this_address), \
			  true : false)

#define STATIC static noinline

#ifdef CONFIG_XFS_RT

 
#define XFS_IS_REALTIME_INODE(ip)			\
	(((ip)->i_diflags & XFS_DIFLAG_REALTIME) &&	\
	 (ip)->i_mount->m_rtdev_targp)
#define XFS_IS_REALTIME_MOUNT(mp) ((mp)->m_rtdev_targp ? 1 : 0)
#else
#define XFS_IS_REALTIME_INODE(ip) (0)
#define XFS_IS_REALTIME_MOUNT(mp) (0)
#endif

 
#ifdef DEBUG
# define PTR_FMT "%px"
#else
# define PTR_FMT "%p"
#endif

#endif  
