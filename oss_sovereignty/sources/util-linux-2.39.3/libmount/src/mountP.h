

#ifndef _LIBMOUNT_PRIVATE_H
#define _LIBMOUNT_PRIVATE_H

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>

#include "c.h"
#include "list.h"
#include "debug.h"
#include "buffer.h"
#include "libmount.h"


#define MNT_DEBUG_HELP		(1 << 0)
#define MNT_DEBUG_INIT		(1 << 1)
#define MNT_DEBUG_CACHE		(1 << 2)
#define MNT_DEBUG_OPTIONS	(1 << 3)
#define MNT_DEBUG_LOCKS		(1 << 4)
#define MNT_DEBUG_TAB		(1 << 5)
#define MNT_DEBUG_FS		(1 << 6)
#define MNT_DEBUG_UPDATE	(1 << 7)
#define MNT_DEBUG_UTILS		(1 << 8)
#define MNT_DEBUG_CXT		(1 << 9)
#define MNT_DEBUG_DIFF		(1 << 10)
#define MNT_DEBUG_MONITOR	(1 << 11)
#define MNT_DEBUG_BTRFS		(1 << 12)
#define MNT_DEBUG_LOOP		(1 << 13)
#define MNT_DEBUG_VERITY	(1 << 14)
#define MNT_DEBUG_HOOK		(1 << 15)
#define MNT_DEBUG_OPTLIST	(1 << 16)

#define MNT_DEBUG_ALL		0xFFFFFF

UL_DEBUG_DECLARE_MASK(libmount);
#define DBG(m, x)	__UL_DBG(libmount, MNT_DEBUG_, m, x)
#define ON_DBG(m, x)	__UL_DBG_CALL(libmount, MNT_DEBUG_, m, x)
#define DBG_FLUSH	__UL_DBG_FLUSH(libmount, MNT_DEBUG_)

#define UL_DEBUG_CURRENT_MASK	UL_DEBUG_MASK(libmount)
#include "debugobj.h"


#define LIBMOUNT_TEXTDOMAIN	PACKAGE
#define UL_TEXTDOMAIN_EXPLICIT	LIBMOUNT_TEXTDOMAIN
#include "nls.h"



#define MNT_MNTTABDIR_EXT	".fstab"


#define MNT_RUNTIME_TOPDIR	"/run"

#define MNT_PATH_UTAB		MNT_RUNTIME_TOPDIR "/mount/utab"

#define MNT_PATH_TMPTGT		MNT_RUNTIME_TOPDIR "/mount/tmptgt"

#define MNT_UTAB_HEADER	"# libmount utab file\n"

#ifdef TEST_PROGRAM
struct libmnt_test {
	const char	*name;
	int		(*body)(struct libmnt_test *ts, int argc, char *argv[]);
	const char	*usage;
};


extern int mnt_run_test(struct libmnt_test *tests, int argc, char *argv[]);
#endif


extern int mnt_valid_tagname(const char *tagname);

extern const char *mnt_statfs_get_fstype(struct statfs *vfs);
extern int is_file_empty(const char *name);

extern int mnt_is_readonly(const char *path)
			__attribute__((nonnull));

extern int mnt_parse_offset(const char *str, size_t len, uintmax_t *res);

extern int mnt_chdir_to_parent(const char *target, char **filename);

extern char *mnt_get_username(const uid_t uid);
extern int mnt_get_uid(const char *username, uid_t *uid);
extern int mnt_get_gid(const char *groupname, gid_t *gid);
extern int mnt_parse_uid(const char *user, size_t user_len, uid_t *gid);
extern int mnt_parse_gid(const char *group, size_t group_len, gid_t *gid);
extern int mnt_parse_mode(const char *mode, size_t mode_len, mode_t *gid);
extern int mnt_in_group(gid_t gid);

extern int mnt_open_uniq_filename(const char *filename, char **name);

extern int mnt_has_regular_utab(const char **utab, int *writable);
extern const char *mnt_get_utab_path(void);

extern int mnt_get_filesystems(char ***filesystems, const char *pattern);
extern void mnt_free_filesystems(char **filesystems);

extern char *mnt_get_kernel_cmdline_option(const char *name);

extern int mnt_safe_stat(const char *target, struct stat *st);
extern int mnt_safe_lstat(const char *target, struct stat *st);
extern int mnt_is_path(const char *target);

extern int mnt_tmptgt_unshare(int *old_ns_fd);
extern int mnt_tmptgt_cleanup(int old_ns_fd);


extern int is_mountinfo(struct libmnt_table *tb);
extern int mnt_table_set_parser_fltrcb(	struct libmnt_table *tb,
					int (*cb)(struct libmnt_fs *, void *),
					void *data);

extern int __mnt_table_parse_mountinfo(struct libmnt_table *tb,
					const char *filename,
					struct libmnt_table *u_tb);

extern struct libmnt_fs *mnt_table_get_fs_root(struct libmnt_table *tb,
					struct libmnt_fs *fs,
					unsigned long mountflags,
					char **fsroot);

extern int __mnt_table_is_fs_mounted(	struct libmnt_table *tb,
					struct libmnt_fs *fstab_fs,
					const char *tgt_prefix);

extern int mnt_table_enable_noautofs(struct libmnt_table *tb, int ignore);
extern int mnt_table_is_noautofs(struct libmnt_table *tb);


struct libmnt_iter {
        struct list_head        *p;		
        struct list_head        *head;		
	int			direction;	
};

#define IS_ITER_FORWARD(_i)	((_i)->direction == MNT_ITER_FORWARD)
#define IS_ITER_BACKWARD(_i)	((_i)->direction == MNT_ITER_BACKWARD)

#define MNT_ITER_INIT(itr, list) \
	do { \
		(itr)->p = IS_ITER_FORWARD(itr) ? \
				(list)->next : (list)->prev; \
		(itr)->head = (list); \
	} while(0)

#define MNT_ITER_GET_ENTRY(itr, restype, member) \
		list_entry((itr)->p, restype, member)

#define MNT_ITER_ITERATE(itr) \
	do { \
		(itr)->p = IS_ITER_FORWARD(itr) ? \
				(itr)->p->next : (itr)->p->prev; \
	} while(0)



struct libmnt_fs {
	struct list_head ents;
	struct libmnt_table *tab;

	int		refcount;	

	unsigned int	opts_age;	
	struct libmnt_optlist *optlist;

	int		id;		
	int		parent;		
	dev_t		devno;		

	char		*bindsrc;	

	char		*source;	
	char		*tagname;	
	char		*tagval;	

	char		*root;		
	char		*target;	
	char		*fstype;	

	char		*optstr;	
	char		*vfs_optstr;	
	char		*opt_fields;	
	char		*fs_optstr;	
	char		*user_optstr;	
	char		*attrs;		

	int		freq;		
	int		passno;		

	
	char		*swaptype;	
	off_t		size;		
	off_t		usedsize;	
	int		priority;	

	int		flags;		
	pid_t		tid;		

	char		*comment;	

	void		*userdata;	
};


#define MNT_FS_PSEUDO	(1 << 1) 
#define MNT_FS_NET	(1 << 2) 
#define MNT_FS_SWAP	(1 << 3) 
#define MNT_FS_KERNEL	(1 << 4) 
#define MNT_FS_MERGED	(1 << 5) 


struct libmnt_table {
	int		fmt;		
	int		nents;		
	int		refcount;	
	int		comms;		
	char		*comm_intro;	
	char		*comm_tail;	

	struct libmnt_cache *cache;		

        int		(*errcb)(struct libmnt_table *tb,
				 const char *filename, int line);

	int		(*fltrcb)(struct libmnt_fs *fs, void *data);
	void		*fltrcb_data;

	int		noautofs;	

	struct list_head	ents;	
	void		*userdata;
};

extern struct libmnt_table *__mnt_new_table_from_file(const char *filename, int fmt, int empty_for_enoent);


enum {
	MNT_FMT_GUESS,
	MNT_FMT_FSTAB,			
	MNT_FMT_MTAB = MNT_FMT_FSTAB,	
	MNT_FMT_MOUNTINFO,		
	MNT_FMT_UTAB,			
	MNT_FMT_SWAPS			
};


enum {
	MNT_STAGE_PREP_SOURCE = 1,	
	MNT_STAGE_PREP_TARGET,		
	MNT_STAGE_PREP_OPTIONS,		
	MNT_STAGE_PREP,			

	MNT_STAGE_MOUNT_PRE = 100,	
	MNT_STAGE_MOUNT,		
	MNT_STAGE_MOUNT_POST,		

	MNT_STAGE_POST = 200		
};

struct libmnt_hookset {
	const char *name;				

	int firststage;
	int (*firstcall)(struct libmnt_context *, const struct libmnt_hookset *, void *);

	int (*deinit)(struct libmnt_context *, const struct libmnt_hookset *);	
};


extern const struct libmnt_hookset hookset_mount_legacy;
extern const struct libmnt_hookset hookset_mount;
extern const struct libmnt_hookset hookset_mkdir;
extern const struct libmnt_hookset hookset_subdir;
extern const struct libmnt_hookset hookset_owner;
extern const struct libmnt_hookset hookset_idmap;
extern const struct libmnt_hookset hookset_loopdev;
#ifdef HAVE_CRYPTSETUP
extern const struct libmnt_hookset hookset_veritydev;
#endif
#ifdef HAVE_LIBSELINUX
extern const struct libmnt_hookset hookset_selinux;
#endif

extern int mnt_context_deinit_hooksets(struct libmnt_context *cxt);
extern const struct libmnt_hookset *mnt_context_get_hookset(struct libmnt_context *cxt, const char *name);

extern int mnt_context_set_hookset_data(struct libmnt_context *cxt,
			const struct libmnt_hookset *hs,
			void *data);

extern void *mnt_context_get_hookset_data(struct libmnt_context *cxt,
			const struct libmnt_hookset *hs);

extern int mnt_context_has_hook(struct libmnt_context *cxt,
                         const struct libmnt_hookset *hs,
                         int stage,
                         void *data);

extern int mnt_context_append_hook(struct libmnt_context *cxt,
			const struct libmnt_hookset *hs,
			int stage,
			void *data,
			int (*func)(struct libmnt_context *,
				const struct libmnt_hookset *,
				void *));
extern int mnt_context_insert_hook(struct libmnt_context *cxt,
			const char *after,
			const struct libmnt_hookset *hs,
			int stage,
			void *data,
			int (*func)(struct libmnt_context *,
				const struct libmnt_hookset *,
				void *));

extern int mnt_context_remove_hook(struct libmnt_context *cxt,
			const struct libmnt_hookset *hs,
			int stage,
			void **data);
extern int mnt_context_call_hooks(struct libmnt_context *cxt, int stage);


struct libmnt_ns {
	int fd;				
	struct libmnt_cache *cache;	
};


struct libmnt_context
{
	int	action;		
	int	restricted;	

	char	*fstype_pattern;	
	char	*optstr_pattern;	

	struct libmnt_fs *fs;		

	struct libmnt_table *fstab;	
	struct libmnt_table *mountinfo;	
	struct libmnt_table *utab;	

	int	(*table_errcb)(struct libmnt_table *tb,	
			 const char *filename, int line);

	int	(*table_fltrcb)(struct libmnt_fs *fs, void *data);	
	void	*table_fltrcb_data;

	char	*(*pwd_get_cb)(struct libmnt_context *);		
	void	(*pwd_release_cb)(struct libmnt_context *, char *);	

	int	optsmode;	

	const void	*mountdata;	

	struct libmnt_cache	*cache;		
	struct libmnt_lock	*lock;		
	struct libmnt_update	*update;	

	struct libmnt_optlist	*optlist;	
	struct libmnt_optlist	*optlist_saved;	

	const struct libmnt_optmap *map_linux;		
	const struct libmnt_optmap *map_userspace;	

	const char	*mountinfo_path; 

	const char	*utab_path; 
	int		utab_writable; 

	char		*tgt_prefix;	

	int	flags;		

	char	*helper;	
	int	helper_status;	
	int	helper_exec_status; 

	pid_t	*children;	
	int	nchildren;	
	pid_t	pid;		

	int	syscall_status;	
	const char *syscall_name;	

	struct libmnt_ns	ns_orig;	
	struct libmnt_ns	ns_tgt;		
	struct libmnt_ns	*ns_cur;	

	unsigned int	enabled_textdomain : 1;	
	unsigned int	noautofs : 1;		
	unsigned int	has_selinux_opt : 1;	
	unsigned int    force_clone : 1;	

	struct list_head	hooksets_datas;	
	struct list_head	hooksets_hooks;	
};


#define MNT_FL_NOMTAB		(1 << 1)
#define MNT_FL_FAKE		(1 << 2)
#define MNT_FL_SLOPPY		(1 << 3)
#define MNT_FL_VERBOSE		(1 << 4)
#define MNT_FL_NOHELPERS	(1 << 5)
#define MNT_FL_LOOPDEL		(1 << 6)
#define MNT_FL_LAZY		(1 << 7)
#define MNT_FL_FORCE		(1 << 8)
#define MNT_FL_NOCANONICALIZE	(1 << 9)
#define MNT_FL_RDONLY_UMOUNT	(1 << 11)	
#define MNT_FL_FORK		(1 << 12)
#define MNT_FL_NOSWAPMATCH	(1 << 13)
#define MNT_FL_RWONLY_MOUNT	(1 << 14)	
#define MNT_FL_ONLYONCE		(1 << 15)

#define MNT_FL_MOUNTDATA	(1 << 20)
#define MNT_FL_TAB_APPLIED	(1 << 21)	
#define MNT_FL_MOUNTFLAGS_MERGED (1 << 22)	
#define MNT_FL_SAVED_USER	(1 << 23)
#define MNT_FL_PREPARED		(1 << 24)
#define MNT_FL_HELPER		(1 << 25)	
#define MNT_FL_MOUNTOPTS_FIXED  (1 << 27)
#define MNT_FL_TABPATHS_CHECKED	(1 << 28)
#define MNT_FL_FORCED_RDONLY	(1 << 29)	
#define MNT_FL_VERITYDEV_READY	(1 << 30)	


#define MNT_FL_DEFAULT		0


#define MNT_BIND_SETTABLE	(MS_NOSUID|MS_NODEV|MS_NOEXEC|MS_NOATIME|MS_NODIRATIME|MS_RELATIME|MS_RDONLY|MS_NOSYMFOLLOW)

#define set_syscall_status(_cxt, _name, _x) __extension__ ({ \
		if (!(_x)) { \
			DBG(CXT, ul_debug("syscall '%s' [%m]", _name)); \
			(_cxt)->syscall_status = -errno; \
			(_cxt)->syscall_name = (_name); \
		} else { \
			DBG(CXT, ul_debug("syscall '%s' [success]", _name)); \
			(_cxt)->syscall_status = 0; \
		} \
	})

#define reset_syscall_status(_cxt)	__extension__ ({ \
		DBG(CXT, ul_debug("reset syscall status")); \
		(_cxt)->syscall_status = 0; \
		(_cxt)->syscall_name = NULL; \
	})


extern const struct libmnt_optmap *mnt_optmap_get_entry(
			     struct libmnt_optmap const **maps,
                             int nmaps,
			     const char *name,
                             size_t namelen,
			     const struct libmnt_optmap **mapent);


extern int mnt_optstr_remove_option_at(char **optstr, char *begin, char *end);

extern int mnt_buffer_append_option(struct ul_buffer *buf,
                        const char *name, size_t namesz,
                        const char *val, size_t valsz, int quoted);


struct libmnt_opt;
struct libmnt_optlist;

extern struct libmnt_optlist *mnt_new_optlist(void);
extern void mnt_ref_optlist(struct libmnt_optlist *ls);
extern void mnt_unref_optlist(struct libmnt_optlist *ls);
extern struct libmnt_optlist *mnt_copy_optlist(struct libmnt_optlist *ls);
extern int mnt_optlist_is_empty(struct libmnt_optlist *ls);
extern unsigned int mnt_optlist_get_age(struct libmnt_optlist *ls);
extern int mnt_optlist_register_map(struct libmnt_optlist *ls, const struct libmnt_optmap *map);
extern int mnt_optlist_remove_opt(struct libmnt_optlist *ls, struct libmnt_opt *opt);
extern int mnt_optlist_remove_named(struct libmnt_optlist *ls, const char *name,
                             const struct libmnt_optmap *map);
extern int mnt_optlist_remove_flags(struct libmnt_optlist *ls, unsigned long flags,
                        const struct libmnt_optmap *map);
extern int mnt_optlist_next_opt(struct libmnt_optlist *ls,
                        struct libmnt_iter *itr, struct libmnt_opt **opt);
extern struct libmnt_opt *mnt_optlist_get_opt(struct libmnt_optlist *ls,
                        unsigned long id, const struct libmnt_optmap *map);
extern struct libmnt_opt *mnt_optlist_get_named(struct libmnt_optlist *ls,
                          const char *name, const struct libmnt_optmap *map);
extern int mnt_optlist_set_optstr(struct libmnt_optlist *ls, const char *optstr,
                          const struct libmnt_optmap *map);
extern int mnt_optlist_append_optstr(struct libmnt_optlist *ls, const char *optstr,
                        const struct libmnt_optmap *map);
extern int mnt_optlist_prepend_optstr(struct libmnt_optlist *ls, const char *optstr,
                        const struct libmnt_optmap *map);
extern int mnt_optlist_append_flags(struct libmnt_optlist *ls, unsigned long flags,
                          const struct libmnt_optmap *map);
extern int mnt_optlist_set_flags(struct libmnt_optlist *ls, unsigned long flags,
                          const struct libmnt_optmap *map);
extern int mnt_optlist_insert_flags(struct libmnt_optlist *ls, unsigned long flags,
                        const struct libmnt_optmap *map,
                        unsigned long after,
                        const struct libmnt_optmap *after_map);

enum {
	
	MNT_OL_FLTR_DFLT = 0,
	
	MNT_OL_FLTR_HELPERS,
	
	MNT_OL_FLTR_MTAB,
	
	MNT_OL_FLTR_ALL,
	
	MNT_OL_FLTR_UNKNOWN,

	__MNT_OL_FLTR_COUNT	
};


extern int mnt_optlist_get_flags(struct libmnt_optlist *ls, unsigned long *flags,
                          const struct libmnt_optmap *map, unsigned int what);


#define MNT_OL_REC	1
#define MNT_OL_NOREC	2

extern int mnt_optlist_get_attrs(struct libmnt_optlist *ls, uint64_t *set, uint64_t *clr, int rec);

extern int mnt_optlist_get_optstr(struct libmnt_optlist *ol, const char **optstr,
                        const struct libmnt_optmap *map, unsigned int what);
extern int mnt_optlist_strdup_optstr(struct libmnt_optlist *ls, char **optstr,
                        const struct libmnt_optmap *map, unsigned int what);

extern int mnt_optlist_get_propagation(struct libmnt_optlist *ls);
extern int mnt_optlist_is_propagation_only(struct libmnt_optlist *ls);
extern int mnt_optlist_is_remount(struct libmnt_optlist *ls);
extern int mnt_optlist_is_recursive(struct libmnt_optlist *ls);
extern int mnt_optlist_is_bind(struct libmnt_optlist *ls);
extern int mnt_optlist_is_rbind(struct libmnt_optlist *ls);
extern int mnt_optlist_is_move(struct libmnt_optlist *ls);
extern int mnt_optlist_is_rdonly(struct libmnt_optlist *ls);
extern int mnt_optlist_is_silent(struct libmnt_optlist *ls);

extern int mnt_optlist_merge_opts(struct libmnt_optlist *ls);

extern int mnt_opt_has_value(struct libmnt_opt *opt);
extern const char *mnt_opt_get_value(struct libmnt_opt *opt);
extern const char *mnt_opt_get_name(struct libmnt_opt *opt);
extern const struct libmnt_optmap *mnt_opt_get_map(struct libmnt_opt *opt);
extern const struct libmnt_optmap *mnt_opt_get_mapent(struct libmnt_opt *opt);
extern int mnt_opt_set_external(struct libmnt_opt *opt, int enable);
extern int mnt_opt_set_value(struct libmnt_opt *opt, const char *str);
extern int mnt_opt_set_u64value(struct libmnt_opt *opt, uint64_t num);
extern int mnt_opt_set_quoted_value(struct libmnt_opt *opt, const char *str);
extern int mnt_opt_is_external(struct libmnt_opt *opt);


extern int mnt_fs_follow_optlist(struct libmnt_fs *fs, struct libmnt_optlist *ol);
extern struct libmnt_fs *mnt_copy_mtab_fs(struct libmnt_fs *fs);
extern int __mnt_fs_set_source_ptr(struct libmnt_fs *fs, char *source)
			__attribute__((nonnull(1)));
extern int __mnt_fs_set_fstype_ptr(struct libmnt_fs *fs, char *fstype)
			__attribute__((nonnull(1)));
extern int __mnt_fs_set_target_ptr(struct libmnt_fs *fs, char *tgt)
			__attribute__((nonnull(1)));


extern struct libmnt_context *mnt_copy_context(struct libmnt_context *o);
extern int mnt_context_utab_writable(struct libmnt_context *cxt);
extern const char *mnt_context_get_writable_tabpath(struct libmnt_context *cxt);

extern int mnt_context_get_mountinfo(struct libmnt_context *cxt, struct libmnt_table **tb);
extern int mnt_context_get_mountinfo_for_target(struct libmnt_context *cxt,
				    struct libmnt_table **mountinfo, const char *tgt);

extern int mnt_context_prepare_srcpath(struct libmnt_context *cxt);
extern int mnt_context_guess_srcpath_fstype(struct libmnt_context *cxt, char **type);
extern int mnt_context_guess_fstype(struct libmnt_context *cxt);
extern int mnt_context_prepare_helper(struct libmnt_context *cxt,
				      const char *name, const char *type);
extern int mnt_context_prepare_update(struct libmnt_context *cxt);
extern int mnt_context_merge_mflags(struct libmnt_context *cxt);
extern int mnt_context_update_tabs(struct libmnt_context *cxt);

extern int mnt_context_umount_setopt(struct libmnt_context *cxt, int c, char *arg);
extern int mnt_context_mount_setopt(struct libmnt_context *cxt, int c, char *arg);

extern int mnt_context_propagation_only(struct libmnt_context *cxt)
			__attribute__((nonnull));

extern int mnt_context_delete_loopdev(struct libmnt_context *cxt);

extern int mnt_fork_context(struct libmnt_context *cxt);

extern int mnt_context_set_tabfilter(struct libmnt_context *cxt,
				     int (*fltr)(struct libmnt_fs *, void *),
				     void *data);

extern int mnt_context_get_generic_excode(int rc, char *buf, size_t bufsz, const char *fmt, ...)
				__attribute__ ((__format__ (__printf__, 4, 5)));
extern int mnt_context_get_mount_excode(struct libmnt_context *cxt, int mntrc, char *buf, size_t bufsz);
extern int mnt_context_get_umount_excode(struct libmnt_context *cxt, int mntrc, char *buf, size_t bufsz);

extern int mnt_context_has_template(struct libmnt_context *cxt);
extern int mnt_context_apply_template(struct libmnt_context *cxt);
extern int mnt_context_save_template(struct libmnt_context *cxt);

extern int mnt_context_apply_fs(struct libmnt_context *cxt, struct libmnt_fs *fs);

extern struct libmnt_optlist *mnt_context_get_optlist(struct libmnt_context *cxt);


extern int mnt_update_set_filename(struct libmnt_update *upd, const char *filename);
extern int mnt_update_already_done(struct libmnt_update *upd,
				   struct libmnt_lock *lc);

#if __linux__

extern uint64_t btrfs_get_default_subvol_id(const char *path);
#endif

#ifdef USE_LIBMOUNT_MOUNTFD_SUPPORT

struct libmnt_sysapi {
	int	fd_fs;		
	int	fd_tree;	

	unsigned int is_new_fs : 1 ;	
};

static inline struct libmnt_sysapi *mnt_context_get_sysapi(struct libmnt_context *cxt)
{
	return mnt_context_get_hookset_data(cxt, &hookset_mount);
}
#endif

#endif 
