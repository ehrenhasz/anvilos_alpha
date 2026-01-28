#ifndef _SYS_MNTTAB_H
#define	_SYS_MNTTAB_H
#include <stdio.h>
#include <sys/types.h>
#ifdef MNTTAB
#undef MNTTAB
#endif  
#include <paths.h>
#include <sys/mount.h>
#define	MNTTAB		_PATH_DEVZERO
#define	MS_NOMNTTAB		0x0
#define	MS_RDONLY		0x1
#define	umount2(p, f)	unmount(p, f)
#define	MNT_LINE_MAX	4108
#define	MNT_TOOLONG	1	 
#define	MNT_TOOMANY	2	 
#define	MNT_TOOFEW	3	 
struct mnttab {
	char *mnt_special;
	char *mnt_mountp;
	char *mnt_fstype;
	char *mnt_mntopts;
};
struct extmnttab {
	char *mnt_special;
	char *mnt_mountp;
	char *mnt_fstype;
	char *mnt_mntopts;
	uint_t mnt_major;
	uint_t mnt_minor;
};
struct stat64;
struct statfs;
extern int getmntany(FILE *fp, struct mnttab *mp, struct mnttab *mpref);
extern int _sol_getmntent(FILE *fp, struct mnttab *mp);
extern int getextmntent(const char *path, struct extmnttab *entry,
    struct stat64 *statbuf);
extern void statfs2mnttab(struct statfs *sfs, struct mnttab *mp);
extern char *hasmntopt(struct mnttab *mnt, const char *opt);
extern int getmntent(FILE *fp, struct mnttab *mp);
#endif
