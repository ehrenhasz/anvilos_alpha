#ifndef _LINUX_NTFS_SYSCTL_H
#define _LINUX_NTFS_SYSCTL_H
#if defined(DEBUG) && defined(CONFIG_SYSCTL)
extern int ntfs_sysctl(int add);
#else
static inline int ntfs_sysctl(int add)
{
	return 0;
}
#endif  
#endif  
