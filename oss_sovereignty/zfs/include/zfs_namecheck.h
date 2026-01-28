#ifndef	_ZFS_NAMECHECK_H
#define	_ZFS_NAMECHECK_H extern __attribute__((visibility("default")))
#ifdef	__cplusplus
extern "C" {
#endif
typedef enum {
	NAME_ERR_LEADING_SLASH,		 
	NAME_ERR_EMPTY_COMPONENT,	 
	NAME_ERR_TRAILING_SLASH,	 
	NAME_ERR_INVALCHAR,		 
	NAME_ERR_MULTIPLE_DELIMITERS,	 
	NAME_ERR_NOLETTER,		 
	NAME_ERR_RESERVED,		 
	NAME_ERR_DISKLIKE,		 
	NAME_ERR_TOOLONG,		 
	NAME_ERR_SELF_REF,		 
	NAME_ERR_PARENT_REF,		 
	NAME_ERR_NO_AT,			 
	NAME_ERR_NO_POUND, 		 
} namecheck_err_t;
#define	ZFS_PERMSET_MAXLEN	64
_ZFS_NAMECHECK_H int zfs_max_dataset_nesting;
_ZFS_NAMECHECK_H int get_dataset_depth(const char *);
_ZFS_NAMECHECK_H int pool_namecheck(const char *, namecheck_err_t *, char *);
_ZFS_NAMECHECK_H int entity_namecheck(const char *, namecheck_err_t *, char *);
_ZFS_NAMECHECK_H int dataset_namecheck(const char *, namecheck_err_t *, char *);
_ZFS_NAMECHECK_H int snapshot_namecheck(const char *, namecheck_err_t *,
    char *);
_ZFS_NAMECHECK_H int bookmark_namecheck(const char *, namecheck_err_t *,
    char *);
_ZFS_NAMECHECK_H int dataset_nestcheck(const char *);
_ZFS_NAMECHECK_H int mountpoint_namecheck(const char *, namecheck_err_t *);
_ZFS_NAMECHECK_H int zfs_component_namecheck(const char *, namecheck_err_t *,
    char *);
_ZFS_NAMECHECK_H int permset_namecheck(const char *, namecheck_err_t *,
    char *);
#ifdef	__cplusplus
}
#endif
#endif	 
