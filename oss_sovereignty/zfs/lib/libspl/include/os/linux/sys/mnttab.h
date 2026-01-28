#ifndef _SYS_MNTTAB_H
#define	_SYS_MNTTAB_H
#include <stdio.h>
#include <mntent.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef MNTTAB
#undef MNTTAB
#endif  
#define	MNTTAB		"/proc/self/mounts"
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
struct statfs;
extern int getmntany(FILE *fp, struct mnttab *mp, struct mnttab *mpref);
extern int _sol_getmntent(FILE *fp, struct mnttab *mp);
extern int getextmntent(const char *path, struct extmnttab *mp,
    struct stat64 *statbuf);
static inline char *_sol_hasmntopt(struct mnttab *mnt, const char *opt)
{
	struct mntent mnt_new;
	mnt_new.mnt_opts = mnt->mnt_mntopts;
	return (hasmntopt(&mnt_new, opt));
}
#define	hasmntopt	_sol_hasmntopt
#define	getmntent	_sol_getmntent
#endif
