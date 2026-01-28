#ifndef _SPL_FILE_H
#define	_SPL_FILE_H
#define	FIGNORECASE		0x00080000
#define	FKIOCTL			0x80000000
#define	ED_CASE_CONFLICT	0x10
#ifdef HAVE_INODE_LOCK_SHARED
#define	spl_inode_lock(ip)		inode_lock(ip)
#define	spl_inode_unlock(ip)		inode_unlock(ip)
#define	spl_inode_lock_shared(ip)	inode_lock_shared(ip)
#define	spl_inode_unlock_shared(ip)	inode_unlock_shared(ip)
#define	spl_inode_trylock(ip)		inode_trylock(ip)
#define	spl_inode_trylock_shared(ip)	inode_trylock_shared(ip)
#define	spl_inode_is_locked(ip)		inode_is_locked(ip)
#define	spl_inode_lock_nested(ip, s)	inode_lock_nested(ip, s)
#else
#define	spl_inode_lock(ip)		mutex_lock(&(ip)->i_mutex)
#define	spl_inode_unlock(ip)		mutex_unlock(&(ip)->i_mutex)
#define	spl_inode_lock_shared(ip)	mutex_lock(&(ip)->i_mutex)
#define	spl_inode_unlock_shared(ip)	mutex_unlock(&(ip)->i_mutex)
#define	spl_inode_trylock(ip)		mutex_trylock(&(ip)->i_mutex)
#define	spl_inode_trylock_shared(ip)	mutex_trylock(&(ip)->i_mutex)
#define	spl_inode_is_locked(ip)		mutex_is_locked(&(ip)->i_mutex)
#define	spl_inode_lock_nested(ip, s)	mutex_lock_nested(&(ip)->i_mutex, s)
#endif
#endif  
