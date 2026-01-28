#ifndef	_SYS_ZFS_IMPL_H
#define	_SYS_ZFS_IMPL_H
#ifdef	__cplusplus
extern "C" {
#endif
typedef struct
{
	const char *name;
	uint32_t (*getcnt)(void);
	uint32_t (*getid)(void);
	const char *(*getname)(void);
	void (*set_fastest)(uint32_t id);
	void (*setid)(uint32_t id);
	int (*setname)(const char *val);
} zfs_impl_t;
extern const zfs_impl_t *zfs_impl_get_ops(const char *algo);
extern const zfs_impl_t zfs_blake3_ops;
extern const zfs_impl_t zfs_sha256_ops;
extern const zfs_impl_t zfs_sha512_ops;
#ifdef	__cplusplus
}
#endif
#endif	 
