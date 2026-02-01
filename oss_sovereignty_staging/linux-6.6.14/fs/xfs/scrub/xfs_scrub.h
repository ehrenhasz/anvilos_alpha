
 
#ifndef __XFS_SCRUB_H__
#define __XFS_SCRUB_H__

#ifndef CONFIG_XFS_ONLINE_SCRUB
# define xfs_scrub_metadata(file, sm)	(-ENOTTY)
#else
int xfs_scrub_metadata(struct file *file, struct xfs_scrub_metadata *sm);
#endif  

#endif	 
