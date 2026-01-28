#ifndef _OPENSOLARIS_SYS_VNODE_H_
#define	_OPENSOLARIS_SYS_VNODE_H_
struct vnode;
struct vattr;
struct xucred;
typedef struct flock	flock64_t;
typedef	struct vnode	vnode_t;
typedef	struct vattr	vattr_t;
#if __FreeBSD_version < 1400093
typedef enum vtype vtype_t;
#else
#define	vtype_t __enum_uint8(vtype)
#endif
#include <sys/types.h>
#include <sys/queue.h>
#include_next <sys/sdt.h>
#include <sys/namei.h>
enum symfollow { NO_FOLLOW = NOFOLLOW };
#define	NOCRED	((struct ucred *)0)	 
#define	F_FREESP	11 	 
#include <sys/proc.h>
#include <sys/vnode_impl.h>
#ifndef IN_BASE
#include_next <sys/vnode.h>
#endif
#include <sys/ccompat.h>
#include <sys/mount.h>
#include <sys/cred.h>
#include <sys/fcntl.h>
#include <sys/refcount.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/syscallsubr.h>
#include <sys/vm.h>
#include <vm/vm_object.h>
typedef	struct vop_vector	vnodeops_t;
#define	VOP_FID		VOP_VPTOFH
#define	vop_fid		vop_vptofh
#define	vop_fid_args	vop_vptofh_args
#define	a_fid		a_fhp
#define	rootvfs		(rootvnode == NULL ? NULL : rootvnode->v_mount)
#ifndef IN_BASE
static __inline int
vn_is_readonly(vnode_t *vp)
{
	return (vp->v_mount->mnt_flag & MNT_RDONLY);
}
#endif
#define	vn_vfswlock(vp)		(0)
#define	vn_vfsunlock(vp)	do { } while (0)
#define	vn_ismntpt(vp)	   \
	((vp)->v_type == VDIR && (vp)->v_mountedhere != NULL)
#define	vn_mountedvfs(vp)	((vp)->v_mountedhere)
#define	vn_has_cached_data(vp)	\
	((vp)->v_object != NULL && \
	(vp)->v_object->resident_page_count > 0)
#ifndef IN_BASE
static __inline void
vn_flush_cached_data(vnode_t *vp, boolean_t sync)
{
#if __FreeBSD_version > 1300054
	if (vm_object_mightbedirty(vp->v_object)) {
#else
	if (vp->v_object->flags & OBJ_MIGHTBEDIRTY) {
#endif
		int flags = sync ? OBJPC_SYNC : 0;
		vn_lock(vp, LK_SHARED | LK_RETRY);
		zfs_vmobject_wlock(vp->v_object);
		vm_object_page_clean(vp->v_object, 0, 0, flags);
		zfs_vmobject_wunlock(vp->v_object);
		VOP_UNLOCK1(vp);
	}
}
#endif
#define	vn_exists(vp)		do { } while (0)
#define	vn_invalid(vp)		do { } while (0)
#define	vn_free(vp)		do { } while (0)
#define	vn_matchops(vp, vops)	((vp)->v_op == &(vops))
#define	VN_HOLD(v)	vref(v)
#define	VN_RELE(v)	vrele(v)
#define	VN_URELE(v)	vput(v)
#define	vnevent_create(vp, ct)			do { } while (0)
#define	vnevent_link(vp, ct)			do { } while (0)
#define	vnevent_remove(vp, dvp, name, ct)	do { } while (0)
#define	vnevent_rmdir(vp, dvp, name, ct)	do { } while (0)
#define	vnevent_rename_src(vp, dvp, name, ct)	do { } while (0)
#define	vnevent_rename_dest(vp, dvp, name, ct)	do { } while (0)
#define	vnevent_rename_dest_dir(vp, ct)		do { } while (0)
#define	specvp(vp, rdev, type, cr)	(VN_HOLD(vp), (vp))
#define	MANDLOCK(vp, mode)	(0)
#define	va_mask		va_spare
#define	va_nodeid	va_fileid
#define	va_nblocks	va_bytes
#define	va_blksize	va_blocksize
#define	MAXOFFSET_T	OFF_MAX
#define	FIGNORECASE	0x00
#undef AT_UID
#undef AT_GID
#define	AT_MODE		0x00002
#define	AT_UID		0x00004
#define	AT_GID		0x00008
#define	AT_FSID		0x00010
#define	AT_NODEID	0x00020
#define	AT_NLINK	0x00040
#define	AT_SIZE		0x00080
#define	AT_ATIME	0x00100
#define	AT_MTIME	0x00200
#define	AT_CTIME	0x00400
#define	AT_RDEV		0x00800
#define	AT_BLKSIZE	0x01000
#define	AT_NBLOCKS	0x02000
#define	AT_SEQ		0x08000
#define	AT_XVATTR	0x10000
#define	AT_ALL		(AT_MODE|AT_UID|AT_GID|AT_FSID|AT_NODEID|\
			AT_NLINK|AT_SIZE|AT_ATIME|AT_MTIME|AT_CTIME|\
			AT_RDEV|AT_BLKSIZE|AT_NBLOCKS|AT_SEQ)
#define	AT_STAT		(AT_MODE|AT_UID|AT_GID|AT_FSID|AT_NODEID|AT_NLINK|\
			AT_SIZE|AT_ATIME|AT_MTIME|AT_CTIME|AT_RDEV)
#define	AT_TIMES	(AT_ATIME|AT_MTIME|AT_CTIME)
#define	AT_NOSET	(AT_NLINK|AT_RDEV|AT_FSID|AT_NODEID|\
			AT_BLKSIZE|AT_NBLOCKS|AT_SEQ)
#ifndef IN_BASE
static __inline void
vattr_init_mask(vattr_t *vap)
{
	vap->va_mask = 0;
	if (vap->va_uid != (uid_t)VNOVAL)
		vap->va_mask |= AT_UID;
	if (vap->va_gid != (gid_t)VNOVAL)
		vap->va_mask |= AT_GID;
	if (vap->va_size != (u_quad_t)VNOVAL)
		vap->va_mask |= AT_SIZE;
	if (vap->va_atime.tv_sec != VNOVAL)
		vap->va_mask |= AT_ATIME;
	if (vap->va_mtime.tv_sec != VNOVAL)
		vap->va_mask |= AT_MTIME;
	if (vap->va_mode != (uint16_t)VNOVAL)
		vap->va_mask |= AT_MODE;
	if (vap->va_flags != VNOVAL)
		vap->va_mask |= AT_XVATTR;
}
#endif
#define		RLIM64_INFINITY 0
#include <sys/vfs.h>
#endif	 
