 

 
#ifndef _LIBSPL_LIBSHARE_IMPL_H
#define	_LIBSPL_LIBSHARE_IMPL_H

typedef const struct sa_share_impl {
	const char *sa_zfsname;
	const char *sa_mountpoint;
	const char *sa_shareopts;
} *sa_share_impl_t;

typedef struct {
	int (*const enable_share)(sa_share_impl_t share);
	int (*const disable_share)(sa_share_impl_t share);
	boolean_t (*const is_shared)(sa_share_impl_t share);
	int (*const validate_shareopts)(const char *shareopts);
	int (*const commit_shares)(void);
	void (*const truncate_shares)(void);
} sa_fstype_t;

extern const sa_fstype_t libshare_nfs_type, libshare_smb_type;

#endif  
