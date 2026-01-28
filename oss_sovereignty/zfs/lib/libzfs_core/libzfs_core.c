#include <libzfs_core.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#ifdef ZFS_DEBUG
#include <stdio.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <libzutil.h>
#include <sys/nvpair.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/zfs_ioctl.h>
#if __FreeBSD__
#define	BIG_PIPE_SIZE (64 * 1024)  
#endif
static int g_fd = -1;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_refcount;
#ifdef ZFS_DEBUG
static zfs_ioc_t fail_ioc_cmd = ZFS_IOC_LAST;
static zfs_errno_t fail_ioc_err;
static void
libzfs_core_debug_ioc(void)
{
	if (fail_ioc_cmd == ZFS_IOC_LAST) {
		char *ioc_test = getenv("ZFS_IOC_TEST");
		unsigned int ioc_num = 0, ioc_err = 0;
		if (ioc_test != NULL &&
		    sscanf(ioc_test, "%i:%i", &ioc_num, &ioc_err) == 2 &&
		    ioc_num < ZFS_IOC_LAST)  {
			fail_ioc_cmd = ioc_num;
			fail_ioc_err = ioc_err;
		}
	}
}
#endif
int
libzfs_core_init(void)
{
	(void) pthread_mutex_lock(&g_lock);
	if (g_refcount == 0) {
		g_fd = open(ZFS_DEV, O_RDWR|O_CLOEXEC);
		if (g_fd < 0) {
			(void) pthread_mutex_unlock(&g_lock);
			return (errno);
		}
	}
	g_refcount++;
#ifdef ZFS_DEBUG
	libzfs_core_debug_ioc();
#endif
	(void) pthread_mutex_unlock(&g_lock);
	return (0);
}
void
libzfs_core_fini(void)
{
	(void) pthread_mutex_lock(&g_lock);
	ASSERT3S(g_refcount, >, 0);
	g_refcount--;
	if (g_refcount == 0 && g_fd != -1) {
		(void) close(g_fd);
		g_fd = -1;
	}
	(void) pthread_mutex_unlock(&g_lock);
}
static int
lzc_ioctl(zfs_ioc_t ioc, const char *name,
    nvlist_t *source, nvlist_t **resultp)
{
	zfs_cmd_t zc = {"\0"};
	int error = 0;
	char *packed = NULL;
	size_t size = 0;
	ASSERT3S(g_refcount, >, 0);
	VERIFY3S(g_fd, !=, -1);
#ifdef ZFS_DEBUG
	if (ioc == fail_ioc_cmd)
		return (fail_ioc_err);
#endif
	if (name != NULL)
		(void) strlcpy(zc.zc_name, name, sizeof (zc.zc_name));
	if (source != NULL) {
		packed = fnvlist_pack(source, &size);
		zc.zc_nvlist_src = (uint64_t)(uintptr_t)packed;
		zc.zc_nvlist_src_size = size;
	}
	if (resultp != NULL) {
		*resultp = NULL;
		if (ioc == ZFS_IOC_CHANNEL_PROGRAM) {
			zc.zc_nvlist_dst_size = fnvlist_lookup_uint64(source,
			    ZCP_ARG_MEMLIMIT);
		} else {
			zc.zc_nvlist_dst_size = MAX(size * 2, 128 * 1024);
		}
		zc.zc_nvlist_dst = (uint64_t)(uintptr_t)
		    malloc(zc.zc_nvlist_dst_size);
		if (zc.zc_nvlist_dst == (uint64_t)0) {
			error = ENOMEM;
			goto out;
		}
	}
	while (lzc_ioctl_fd(g_fd, ioc, &zc) != 0) {
		if (errno == ENOMEM && resultp != NULL &&
		    ioc != ZFS_IOC_CHANNEL_PROGRAM) {
			free((void *)(uintptr_t)zc.zc_nvlist_dst);
			zc.zc_nvlist_dst_size *= 2;
			zc.zc_nvlist_dst = (uint64_t)(uintptr_t)
			    malloc(zc.zc_nvlist_dst_size);
			if (zc.zc_nvlist_dst == (uint64_t)0) {
				error = ENOMEM;
				goto out;
			}
		} else {
			error = errno;
			break;
		}
	}
	if (zc.zc_nvlist_dst_filled && resultp != NULL) {
		*resultp = fnvlist_unpack((void *)(uintptr_t)zc.zc_nvlist_dst,
		    zc.zc_nvlist_dst_size);
	}
out:
	if (packed != NULL)
		fnvlist_pack_free(packed, size);
	free((void *)(uintptr_t)zc.zc_nvlist_dst);
	return (error);
}
int
lzc_scrub(zfs_ioc_t ioc, const char *name,
    nvlist_t *source, nvlist_t **resultp)
{
	return (lzc_ioctl(ioc, name, source, resultp));
}
int
lzc_create(const char *fsname, enum lzc_dataset_type type, nvlist_t *props,
    uint8_t *wkeydata, uint_t wkeylen)
{
	int error;
	nvlist_t *hidden_args = NULL;
	nvlist_t *args = fnvlist_alloc();
	fnvlist_add_int32(args, "type", (dmu_objset_type_t)type);
	if (props != NULL)
		fnvlist_add_nvlist(args, "props", props);
	if (wkeydata != NULL) {
		hidden_args = fnvlist_alloc();
		fnvlist_add_uint8_array(hidden_args, "wkeydata", wkeydata,
		    wkeylen);
		fnvlist_add_nvlist(args, ZPOOL_HIDDEN_ARGS, hidden_args);
	}
	error = lzc_ioctl(ZFS_IOC_CREATE, fsname, args, NULL);
	nvlist_free(hidden_args);
	nvlist_free(args);
	return (error);
}
int
lzc_clone(const char *fsname, const char *origin, nvlist_t *props)
{
	int error;
	nvlist_t *hidden_args = NULL;
	nvlist_t *args = fnvlist_alloc();
	fnvlist_add_string(args, "origin", origin);
	if (props != NULL)
		fnvlist_add_nvlist(args, "props", props);
	error = lzc_ioctl(ZFS_IOC_CLONE, fsname, args, NULL);
	nvlist_free(hidden_args);
	nvlist_free(args);
	return (error);
}
int
lzc_promote(const char *fsname, char *snapnamebuf, int snapnamelen)
{
	zfs_cmd_t zc = {"\0"};
	ASSERT3S(g_refcount, >, 0);
	VERIFY3S(g_fd, !=, -1);
	(void) strlcpy(zc.zc_name, fsname, sizeof (zc.zc_name));
	if (lzc_ioctl_fd(g_fd, ZFS_IOC_PROMOTE, &zc) != 0) {
		int error = errno;
		if (error == EEXIST && snapnamebuf != NULL)
			(void) strlcpy(snapnamebuf, zc.zc_string, snapnamelen);
		return (error);
	}
	return (0);
}
int
lzc_rename(const char *source, const char *target)
{
	zfs_cmd_t zc = {"\0"};
	int error;
	ASSERT3S(g_refcount, >, 0);
	VERIFY3S(g_fd, !=, -1);
	(void) strlcpy(zc.zc_name, source, sizeof (zc.zc_name));
	(void) strlcpy(zc.zc_value, target, sizeof (zc.zc_value));
	error = lzc_ioctl_fd(g_fd, ZFS_IOC_RENAME, &zc);
	if (error != 0)
		error = errno;
	return (error);
}
int
lzc_destroy(const char *fsname)
{
	int error;
	nvlist_t *args = fnvlist_alloc();
	error = lzc_ioctl(ZFS_IOC_DESTROY, fsname, args, NULL);
	nvlist_free(args);
	return (error);
}
int
lzc_snapshot(nvlist_t *snaps, nvlist_t *props, nvlist_t **errlist)
{
	nvpair_t *elem;
	nvlist_t *args;
	int error;
	char pool[ZFS_MAX_DATASET_NAME_LEN];
	*errlist = NULL;
	elem = nvlist_next_nvpair(snaps, NULL);
	if (elem == NULL)
		return (0);
	(void) strlcpy(pool, nvpair_name(elem), sizeof (pool));
	pool[strcspn(pool, "/@")] = '\0';
	args = fnvlist_alloc();
	fnvlist_add_nvlist(args, "snaps", snaps);
	if (props != NULL)
		fnvlist_add_nvlist(args, "props", props);
	error = lzc_ioctl(ZFS_IOC_SNAPSHOT, pool, args, errlist);
	nvlist_free(args);
	return (error);
}
int
lzc_destroy_snaps(nvlist_t *snaps, boolean_t defer, nvlist_t **errlist)
{
	nvpair_t *elem;
	nvlist_t *args;
	int error;
	char pool[ZFS_MAX_DATASET_NAME_LEN];
	elem = nvlist_next_nvpair(snaps, NULL);
	if (elem == NULL)
		return (0);
	(void) strlcpy(pool, nvpair_name(elem), sizeof (pool));
	pool[strcspn(pool, "/@")] = '\0';
	args = fnvlist_alloc();
	fnvlist_add_nvlist(args, "snaps", snaps);
	if (defer)
		fnvlist_add_boolean(args, "defer");
	error = lzc_ioctl(ZFS_IOC_DESTROY_SNAPS, pool, args, errlist);
	nvlist_free(args);
	return (error);
}
int
lzc_snaprange_space(const char *firstsnap, const char *lastsnap,
    uint64_t *usedp)
{
	nvlist_t *args;
	nvlist_t *result;
	int err;
	char fs[ZFS_MAX_DATASET_NAME_LEN];
	char *atp;
	(void) strlcpy(fs, firstsnap, sizeof (fs));
	atp = strchr(fs, '@');
	if (atp == NULL)
		return (EINVAL);
	*atp = '\0';
	args = fnvlist_alloc();
	fnvlist_add_string(args, "firstsnap", firstsnap);
	err = lzc_ioctl(ZFS_IOC_SPACE_SNAPS, lastsnap, args, &result);
	nvlist_free(args);
	if (err == 0)
		*usedp = fnvlist_lookup_uint64(result, "used");
	fnvlist_free(result);
	return (err);
}
boolean_t
lzc_exists(const char *dataset)
{
	zfs_cmd_t zc = {"\0"};
	ASSERT3S(g_refcount, >, 0);
	VERIFY3S(g_fd, !=, -1);
	(void) strlcpy(zc.zc_name, dataset, sizeof (zc.zc_name));
	return (lzc_ioctl_fd(g_fd, ZFS_IOC_OBJSET_STATS, &zc) == 0);
}
int
lzc_sync(const char *pool_name, nvlist_t *innvl, nvlist_t **outnvl)
{
	(void) outnvl;
	return (lzc_ioctl(ZFS_IOC_POOL_SYNC, pool_name, innvl, NULL));
}
int
lzc_hold(nvlist_t *holds, int cleanup_fd, nvlist_t **errlist)
{
	char pool[ZFS_MAX_DATASET_NAME_LEN];
	nvlist_t *args;
	nvpair_t *elem;
	int error;
	elem = nvlist_next_nvpair(holds, NULL);
	if (elem == NULL)
		return (0);
	(void) strlcpy(pool, nvpair_name(elem), sizeof (pool));
	pool[strcspn(pool, "/@")] = '\0';
	args = fnvlist_alloc();
	fnvlist_add_nvlist(args, "holds", holds);
	if (cleanup_fd != -1)
		fnvlist_add_int32(args, "cleanup_fd", cleanup_fd);
	error = lzc_ioctl(ZFS_IOC_HOLD, pool, args, errlist);
	nvlist_free(args);
	return (error);
}
int
lzc_release(nvlist_t *holds, nvlist_t **errlist)
{
	char pool[ZFS_MAX_DATASET_NAME_LEN];
	nvpair_t *elem;
	elem = nvlist_next_nvpair(holds, NULL);
	if (elem == NULL)
		return (0);
	(void) strlcpy(pool, nvpair_name(elem), sizeof (pool));
	pool[strcspn(pool, "/@")] = '\0';
	return (lzc_ioctl(ZFS_IOC_RELEASE, pool, holds, errlist));
}
int
lzc_get_holds(const char *snapname, nvlist_t **holdsp)
{
	return (lzc_ioctl(ZFS_IOC_GET_HOLDS, snapname, NULL, holdsp));
}
static unsigned int
max_pipe_buffer(int infd)
{
#if __linux__
	static unsigned int max;
	if (max == 0) {
		max = 1048576;  
		FILE *procf = fopen("/proc/sys/fs/pipe-max-size", "re");
		if (procf != NULL) {
			if (fscanf(procf, "%u", &max) <= 0) {
			}
			fclose(procf);
		}
	}
	unsigned int cur = fcntl(infd, F_GETPIPE_SZ);
	if (getenv("ZFS_SET_PIPE_MAX") == NULL)
		return (cur);
	if (cur < max && fcntl(infd, F_SETPIPE_SZ, max) != -1)
		cur = max;
	return (cur);
#else
	(void) infd;
	return (BIG_PIPE_SIZE);
#endif
}
#if __linux__
struct send_worker_ctx {
	int from;	 
	int to;		 
};
static void *
send_worker(void *arg)
{
	struct send_worker_ctx *ctx = arg;
	unsigned int bufsiz = max_pipe_buffer(ctx->from);
	ssize_t rd;
	for (;;) {
		rd = splice(ctx->from, NULL, ctx->to, NULL, bufsiz,
		    SPLICE_F_MOVE | SPLICE_F_MORE);
		if ((rd == -1 && errno != EINTR) || rd == 0)
			break;
	}
	int err = (rd == -1) ? errno : 0;
	close(ctx->from);
	return ((void *)(uintptr_t)err);
}
#endif
int
lzc_send_wrapper(int (*func)(int, void *), int orig_fd, void *data)
{
#if __linux__
	struct stat sb;
	if (orig_fd != -1 && fstat(orig_fd, &sb) == -1)
		return (errno);
	if (orig_fd == -1 || S_ISFIFO(sb.st_mode)) {
		if (orig_fd != -1)
			(void) max_pipe_buffer(orig_fd);
		return (func(orig_fd, data));
	}
	if ((fcntl(orig_fd, F_GETFL) & O_ACCMODE) == O_RDONLY)
		return (errno = EBADF);
	int rw[2];
	if (pipe2(rw, O_CLOEXEC) == -1)
		return (errno);
	int err;
	pthread_t send_thread;
	struct send_worker_ctx ctx = {.from = rw[0], .to = orig_fd};
	if ((err = pthread_create(&send_thread, NULL, send_worker, &ctx))
	    != 0) {
		close(rw[0]);
		close(rw[1]);
		return (errno = err);
	}
	err = func(rw[1], data);
	void *send_err;
	close(rw[1]);
	pthread_join(send_thread, &send_err);
	if (err == 0 && send_err != 0)
		errno = err = (uintptr_t)send_err;
	return (err);
#else
	return (func(orig_fd, data));
#endif
}
int
lzc_send(const char *snapname, const char *from, int fd,
    enum lzc_send_flags flags)
{
	return (lzc_send_resume_redacted(snapname, from, fd, flags, 0, 0,
	    NULL));
}
int
lzc_send_redacted(const char *snapname, const char *from, int fd,
    enum lzc_send_flags flags, const char *redactbook)
{
	return (lzc_send_resume_redacted(snapname, from, fd, flags, 0, 0,
	    redactbook));
}
int
lzc_send_resume(const char *snapname, const char *from, int fd,
    enum lzc_send_flags flags, uint64_t resumeobj, uint64_t resumeoff)
{
	return (lzc_send_resume_redacted(snapname, from, fd, flags, resumeobj,
	    resumeoff, NULL));
}
static int
lzc_send_resume_redacted_cb_impl(const char *snapname, const char *from, int fd,
    enum lzc_send_flags flags, uint64_t resumeobj, uint64_t resumeoff,
    const char *redactbook)
{
	nvlist_t *args;
	int err;
	args = fnvlist_alloc();
	fnvlist_add_int32(args, "fd", fd);
	if (from != NULL)
		fnvlist_add_string(args, "fromsnap", from);
	if (flags & LZC_SEND_FLAG_LARGE_BLOCK)
		fnvlist_add_boolean(args, "largeblockok");
	if (flags & LZC_SEND_FLAG_EMBED_DATA)
		fnvlist_add_boolean(args, "embedok");
	if (flags & LZC_SEND_FLAG_COMPRESS)
		fnvlist_add_boolean(args, "compressok");
	if (flags & LZC_SEND_FLAG_RAW)
		fnvlist_add_boolean(args, "rawok");
	if (flags & LZC_SEND_FLAG_SAVED)
		fnvlist_add_boolean(args, "savedok");
	if (resumeobj != 0 || resumeoff != 0) {
		fnvlist_add_uint64(args, "resume_object", resumeobj);
		fnvlist_add_uint64(args, "resume_offset", resumeoff);
	}
	if (redactbook != NULL)
		fnvlist_add_string(args, "redactbook", redactbook);
	err = lzc_ioctl(ZFS_IOC_SEND_NEW, snapname, args, NULL);
	nvlist_free(args);
	return (err);
}
struct lzc_send_resume_redacted {
	const char *snapname;
	const char *from;
	enum lzc_send_flags flags;
	uint64_t resumeobj;
	uint64_t resumeoff;
	const char *redactbook;
};
static int
lzc_send_resume_redacted_cb(int fd, void *arg)
{
	struct lzc_send_resume_redacted *zsrr = arg;
	return (lzc_send_resume_redacted_cb_impl(zsrr->snapname, zsrr->from,
	    fd, zsrr->flags, zsrr->resumeobj, zsrr->resumeoff,
	    zsrr->redactbook));
}
int
lzc_send_resume_redacted(const char *snapname, const char *from, int fd,
    enum lzc_send_flags flags, uint64_t resumeobj, uint64_t resumeoff,
    const char *redactbook)
{
	struct lzc_send_resume_redacted zsrr = {
		.snapname = snapname,
		.from = from,
		.flags = flags,
		.resumeobj = resumeobj,
		.resumeoff = resumeoff,
		.redactbook = redactbook,
	};
	return (lzc_send_wrapper(lzc_send_resume_redacted_cb, fd, &zsrr));
}
static int
lzc_send_space_resume_redacted_cb_impl(const char *snapname, const char *from,
    enum lzc_send_flags flags, uint64_t resumeobj, uint64_t resumeoff,
    uint64_t resume_bytes, const char *redactbook, int fd, uint64_t *spacep)
{
	nvlist_t *args;
	nvlist_t *result;
	int err;
	args = fnvlist_alloc();
	if (from != NULL)
		fnvlist_add_string(args, "from", from);
	if (flags & LZC_SEND_FLAG_LARGE_BLOCK)
		fnvlist_add_boolean(args, "largeblockok");
	if (flags & LZC_SEND_FLAG_EMBED_DATA)
		fnvlist_add_boolean(args, "embedok");
	if (flags & LZC_SEND_FLAG_COMPRESS)
		fnvlist_add_boolean(args, "compressok");
	if (flags & LZC_SEND_FLAG_RAW)
		fnvlist_add_boolean(args, "rawok");
	if (resumeobj != 0 || resumeoff != 0) {
		fnvlist_add_uint64(args, "resume_object", resumeobj);
		fnvlist_add_uint64(args, "resume_offset", resumeoff);
		fnvlist_add_uint64(args, "bytes", resume_bytes);
	}
	if (redactbook != NULL)
		fnvlist_add_string(args, "redactbook", redactbook);
	if (fd != -1)
		fnvlist_add_int32(args, "fd", fd);
	err = lzc_ioctl(ZFS_IOC_SEND_SPACE, snapname, args, &result);
	nvlist_free(args);
	if (err == 0)
		*spacep = fnvlist_lookup_uint64(result, "space");
	nvlist_free(result);
	return (err);
}
struct lzc_send_space_resume_redacted {
	const char *snapname;
	const char *from;
	enum lzc_send_flags flags;
	uint64_t resumeobj;
	uint64_t resumeoff;
	uint64_t resume_bytes;
	const char *redactbook;
	uint64_t *spacep;
};
static int
lzc_send_space_resume_redacted_cb(int fd, void *arg)
{
	struct lzc_send_space_resume_redacted *zssrr = arg;
	return (lzc_send_space_resume_redacted_cb_impl(zssrr->snapname,
	    zssrr->from, zssrr->flags, zssrr->resumeobj, zssrr->resumeoff,
	    zssrr->resume_bytes, zssrr->redactbook, fd, zssrr->spacep));
}
int
lzc_send_space_resume_redacted(const char *snapname, const char *from,
    enum lzc_send_flags flags, uint64_t resumeobj, uint64_t resumeoff,
    uint64_t resume_bytes, const char *redactbook, int fd, uint64_t *spacep)
{
	struct lzc_send_space_resume_redacted zssrr = {
		.snapname = snapname,
		.from = from,
		.flags = flags,
		.resumeobj = resumeobj,
		.resumeoff = resumeoff,
		.resume_bytes = resume_bytes,
		.redactbook = redactbook,
		.spacep = spacep,
	};
	return (lzc_send_wrapper(lzc_send_space_resume_redacted_cb,
	    fd, &zssrr));
}
int
lzc_send_space(const char *snapname, const char *from,
    enum lzc_send_flags flags, uint64_t *spacep)
{
	return (lzc_send_space_resume_redacted(snapname, from, flags, 0, 0, 0,
	    NULL, -1, spacep));
}
static int
recv_read(int fd, void *buf, int ilen)
{
	char *cp = buf;
	int rv;
	int len = ilen;
	do {
		rv = read(fd, cp, len);
		cp += rv;
		len -= rv;
	} while (rv > 0);
	if (rv < 0 || len != 0)
		return (EIO);
	return (0);
}
static int
recv_impl(const char *snapname, nvlist_t *recvdprops, nvlist_t *localprops,
    uint8_t *wkeydata, uint_t wkeylen, const char *origin, boolean_t force,
    boolean_t heal, boolean_t resumable, boolean_t raw, int input_fd,
    const dmu_replay_record_t *begin_record, uint64_t *read_bytes,
    uint64_t *errflags, nvlist_t **errors)
{
	dmu_replay_record_t drr;
	char fsname[MAXPATHLEN];
	char *atp;
	int error;
	boolean_t payload = B_FALSE;
	ASSERT3S(g_refcount, >, 0);
	VERIFY3S(g_fd, !=, -1);
	(void) strlcpy(fsname, snapname, sizeof (fsname));
	atp = strchr(fsname, '@');
	if (atp == NULL)
		return (EINVAL);
	*atp = '\0';
	if (!lzc_exists(fsname)) {
		char *slashp = strrchr(fsname, '/');
		if (slashp == NULL)
			return (ENOENT);
		*slashp = '\0';
	}
	struct stat sb;
	if (fstat(input_fd, &sb) == -1)
		return (errno);
	if (S_ISFIFO(sb.st_mode))
		(void) max_pipe_buffer(input_fd);
	if (begin_record == NULL) {
		error = recv_read(input_fd, &drr, sizeof (drr));
		if (error != 0)
			return (error);
	} else {
		drr = *begin_record;
		payload = (begin_record->drr_payloadlen != 0);
	}
	if (resumable || heal || raw || wkeydata != NULL || payload) {
		nvlist_t *outnvl = NULL;
		nvlist_t *innvl = fnvlist_alloc();
		fnvlist_add_string(innvl, "snapname", snapname);
		if (recvdprops != NULL)
			fnvlist_add_nvlist(innvl, "props", recvdprops);
		if (localprops != NULL)
			fnvlist_add_nvlist(innvl, "localprops", localprops);
		if (wkeydata != NULL) {
			nvlist_t *hidden_args = fnvlist_alloc();
			fnvlist_add_uint8_array(hidden_args, "wkeydata",
			    wkeydata, wkeylen);
			fnvlist_add_nvlist(innvl, ZPOOL_HIDDEN_ARGS,
			    hidden_args);
			nvlist_free(hidden_args);
		}
		if (origin != NULL && strlen(origin))
			fnvlist_add_string(innvl, "origin", origin);
		fnvlist_add_byte_array(innvl, "begin_record",
		    (uchar_t *)&drr, sizeof (drr));
		fnvlist_add_int32(innvl, "input_fd", input_fd);
		if (force)
			fnvlist_add_boolean(innvl, "force");
		if (resumable)
			fnvlist_add_boolean(innvl, "resumable");
		if (heal)
			fnvlist_add_boolean(innvl, "heal");
		error = lzc_ioctl(ZFS_IOC_RECV_NEW, fsname, innvl, &outnvl);
		if (error == 0 && read_bytes != NULL)
			error = nvlist_lookup_uint64(outnvl, "read_bytes",
			    read_bytes);
		if (error == 0 && errflags != NULL)
			error = nvlist_lookup_uint64(outnvl, "error_flags",
			    errflags);
		if (error == 0 && errors != NULL) {
			nvlist_t *nvl;
			error = nvlist_lookup_nvlist(outnvl, "errors", &nvl);
			if (error == 0)
				*errors = fnvlist_dup(nvl);
		}
		fnvlist_free(innvl);
		fnvlist_free(outnvl);
	} else {
		zfs_cmd_t zc = {"\0"};
		char *rp_packed = NULL;
		char *lp_packed = NULL;
		size_t size;
		ASSERT3S(g_refcount, >, 0);
		(void) strlcpy(zc.zc_name, fsname, sizeof (zc.zc_name));
		(void) strlcpy(zc.zc_value, snapname, sizeof (zc.zc_value));
		if (recvdprops != NULL) {
			rp_packed = fnvlist_pack(recvdprops, &size);
			zc.zc_nvlist_src = (uint64_t)(uintptr_t)rp_packed;
			zc.zc_nvlist_src_size = size;
		}
		if (localprops != NULL) {
			lp_packed = fnvlist_pack(localprops, &size);
			zc.zc_nvlist_conf = (uint64_t)(uintptr_t)lp_packed;
			zc.zc_nvlist_conf_size = size;
		}
		if (origin != NULL)
			(void) strlcpy(zc.zc_string, origin,
			    sizeof (zc.zc_string));
		ASSERT3S(drr.drr_type, ==, DRR_BEGIN);
		zc.zc_begin_record = drr.drr_u.drr_begin;
		zc.zc_guid = force;
		zc.zc_cookie = input_fd;
		zc.zc_cleanup_fd = -1;
		zc.zc_action_handle = 0;
		zc.zc_nvlist_dst_size = 128 * 1024;
		zc.zc_nvlist_dst = (uint64_t)(uintptr_t)
		    malloc(zc.zc_nvlist_dst_size);
		error = lzc_ioctl_fd(g_fd, ZFS_IOC_RECV, &zc);
		if (error != 0) {
			error = errno;
		} else {
			if (read_bytes != NULL)
				*read_bytes = zc.zc_cookie;
			if (errflags != NULL)
				*errflags = zc.zc_obj;
			if (errors != NULL)
				VERIFY0(nvlist_unpack(
				    (void *)(uintptr_t)zc.zc_nvlist_dst,
				    zc.zc_nvlist_dst_size, errors, KM_SLEEP));
		}
		if (rp_packed != NULL)
			fnvlist_pack_free(rp_packed, size);
		if (lp_packed != NULL)
			fnvlist_pack_free(lp_packed, size);
		free((void *)(uintptr_t)zc.zc_nvlist_dst);
	}
	return (error);
}
int
lzc_receive(const char *snapname, nvlist_t *props, const char *origin,
    boolean_t force, boolean_t raw, int fd)
{
	return (recv_impl(snapname, props, NULL, NULL, 0, origin, force,
	    B_FALSE, B_FALSE, raw, fd, NULL, NULL, NULL, NULL));
}
int
lzc_receive_resumable(const char *snapname, nvlist_t *props, const char *origin,
    boolean_t force, boolean_t raw, int fd)
{
	return (recv_impl(snapname, props, NULL, NULL, 0, origin, force,
	    B_FALSE, B_TRUE, raw, fd, NULL, NULL, NULL, NULL));
}
int
lzc_receive_with_header(const char *snapname, nvlist_t *props,
    const char *origin, boolean_t force, boolean_t resumable, boolean_t raw,
    int fd, const dmu_replay_record_t *begin_record)
{
	if (begin_record == NULL)
		return (EINVAL);
	return (recv_impl(snapname, props, NULL, NULL, 0, origin, force,
	    B_FALSE, resumable, raw, fd, begin_record, NULL, NULL, NULL));
}
int
lzc_receive_one(const char *snapname, nvlist_t *props,
    const char *origin, boolean_t force, boolean_t resumable, boolean_t raw,
    int input_fd, const dmu_replay_record_t *begin_record, int cleanup_fd,
    uint64_t *read_bytes, uint64_t *errflags, uint64_t *action_handle,
    nvlist_t **errors)
{
	(void) action_handle, (void) cleanup_fd;
	return (recv_impl(snapname, props, NULL, NULL, 0, origin, force,
	    B_FALSE, resumable, raw, input_fd, begin_record,
	    read_bytes, errflags, errors));
}
int
lzc_receive_with_cmdprops(const char *snapname, nvlist_t *props,
    nvlist_t *cmdprops, uint8_t *wkeydata, uint_t wkeylen, const char *origin,
    boolean_t force, boolean_t resumable, boolean_t raw, int input_fd,
    const dmu_replay_record_t *begin_record, int cleanup_fd,
    uint64_t *read_bytes, uint64_t *errflags, uint64_t *action_handle,
    nvlist_t **errors)
{
	(void) action_handle, (void) cleanup_fd;
	return (recv_impl(snapname, props, cmdprops, wkeydata, wkeylen, origin,
	    force, B_FALSE, resumable, raw, input_fd, begin_record,
	    read_bytes, errflags, errors));
}
int lzc_receive_with_heal(const char *snapname, nvlist_t *props,
    nvlist_t *cmdprops, uint8_t *wkeydata, uint_t wkeylen, const char *origin,
    boolean_t force, boolean_t heal, boolean_t resumable, boolean_t raw,
    int input_fd, const dmu_replay_record_t *begin_record, int cleanup_fd,
    uint64_t *read_bytes, uint64_t *errflags, uint64_t *action_handle,
    nvlist_t **errors)
{
	(void) action_handle, (void) cleanup_fd;
	return (recv_impl(snapname, props, cmdprops, wkeydata, wkeylen, origin,
	    force, heal, resumable, raw, input_fd, begin_record,
	    read_bytes, errflags, errors));
}
int
lzc_rollback(const char *fsname, char *snapnamebuf, int snapnamelen)
{
	nvlist_t *args;
	nvlist_t *result;
	int err;
	args = fnvlist_alloc();
	err = lzc_ioctl(ZFS_IOC_ROLLBACK, fsname, args, &result);
	nvlist_free(args);
	if (err == 0 && snapnamebuf != NULL) {
		const char *snapname = fnvlist_lookup_string(result, "target");
		(void) strlcpy(snapnamebuf, snapname, snapnamelen);
	}
	nvlist_free(result);
	return (err);
}
int
lzc_rollback_to(const char *fsname, const char *snapname)
{
	nvlist_t *args;
	nvlist_t *result;
	int err;
	args = fnvlist_alloc();
	fnvlist_add_string(args, "target", snapname);
	err = lzc_ioctl(ZFS_IOC_ROLLBACK, fsname, args, &result);
	nvlist_free(args);
	nvlist_free(result);
	return (err);
}
int
lzc_bookmark(nvlist_t *bookmarks, nvlist_t **errlist)
{
	nvpair_t *elem;
	int error;
	char pool[ZFS_MAX_DATASET_NAME_LEN];
	elem = nvlist_next_nvpair(bookmarks, NULL);
	if (elem == NULL)
		return (0);
	(void) strlcpy(pool, nvpair_name(elem), sizeof (pool));
	pool[strcspn(pool, "/#")] = '\0';
	error = lzc_ioctl(ZFS_IOC_BOOKMARK, pool, bookmarks, errlist);
	return (error);
}
int
lzc_get_bookmarks(const char *fsname, nvlist_t *props, nvlist_t **bmarks)
{
	return (lzc_ioctl(ZFS_IOC_GET_BOOKMARKS, fsname, props, bmarks));
}
int
lzc_get_bookmark_props(const char *bookmark, nvlist_t **props)
{
	int error;
	nvlist_t *innvl = fnvlist_alloc();
	error = lzc_ioctl(ZFS_IOC_GET_BOOKMARK_PROPS, bookmark, innvl, props);
	fnvlist_free(innvl);
	return (error);
}
int
lzc_destroy_bookmarks(nvlist_t *bmarks, nvlist_t **errlist)
{
	nvpair_t *elem;
	int error;
	char pool[ZFS_MAX_DATASET_NAME_LEN];
	elem = nvlist_next_nvpair(bmarks, NULL);
	if (elem == NULL)
		return (0);
	(void) strlcpy(pool, nvpair_name(elem), sizeof (pool));
	pool[strcspn(pool, "/#")] = '\0';
	error = lzc_ioctl(ZFS_IOC_DESTROY_BOOKMARKS, pool, bmarks, errlist);
	return (error);
}
static int
lzc_channel_program_impl(const char *pool, const char *program, boolean_t sync,
    uint64_t instrlimit, uint64_t memlimit, nvlist_t *argnvl, nvlist_t **outnvl)
{
	int error;
	nvlist_t *args;
	args = fnvlist_alloc();
	fnvlist_add_string(args, ZCP_ARG_PROGRAM, program);
	fnvlist_add_nvlist(args, ZCP_ARG_ARGLIST, argnvl);
	fnvlist_add_boolean_value(args, ZCP_ARG_SYNC, sync);
	fnvlist_add_uint64(args, ZCP_ARG_INSTRLIMIT, instrlimit);
	fnvlist_add_uint64(args, ZCP_ARG_MEMLIMIT, memlimit);
	error = lzc_ioctl(ZFS_IOC_CHANNEL_PROGRAM, pool, args, outnvl);
	fnvlist_free(args);
	return (error);
}
int
lzc_channel_program(const char *pool, const char *program, uint64_t instrlimit,
    uint64_t memlimit, nvlist_t *argnvl, nvlist_t **outnvl)
{
	return (lzc_channel_program_impl(pool, program, B_TRUE, instrlimit,
	    memlimit, argnvl, outnvl));
}
int
lzc_pool_checkpoint(const char *pool)
{
	int error;
	nvlist_t *result = NULL;
	nvlist_t *args = fnvlist_alloc();
	error = lzc_ioctl(ZFS_IOC_POOL_CHECKPOINT, pool, args, &result);
	fnvlist_free(args);
	fnvlist_free(result);
	return (error);
}
int
lzc_pool_checkpoint_discard(const char *pool)
{
	int error;
	nvlist_t *result = NULL;
	nvlist_t *args = fnvlist_alloc();
	error = lzc_ioctl(ZFS_IOC_POOL_DISCARD_CHECKPOINT, pool, args, &result);
	fnvlist_free(args);
	fnvlist_free(result);
	return (error);
}
int
lzc_channel_program_nosync(const char *pool, const char *program,
    uint64_t timeout, uint64_t memlimit, nvlist_t *argnvl, nvlist_t **outnvl)
{
	return (lzc_channel_program_impl(pool, program, B_FALSE, timeout,
	    memlimit, argnvl, outnvl));
}
int
lzc_get_vdev_prop(const char *poolname, nvlist_t *innvl, nvlist_t **outnvl)
{
	return (lzc_ioctl(ZFS_IOC_VDEV_GET_PROPS, poolname, innvl, outnvl));
}
int
lzc_set_vdev_prop(const char *poolname, nvlist_t *innvl, nvlist_t **outnvl)
{
	return (lzc_ioctl(ZFS_IOC_VDEV_SET_PROPS, poolname, innvl, outnvl));
}
int
lzc_load_key(const char *fsname, boolean_t noop, uint8_t *wkeydata,
    uint_t wkeylen)
{
	int error;
	nvlist_t *ioc_args;
	nvlist_t *hidden_args;
	if (wkeydata == NULL)
		return (EINVAL);
	ioc_args = fnvlist_alloc();
	hidden_args = fnvlist_alloc();
	fnvlist_add_uint8_array(hidden_args, "wkeydata", wkeydata, wkeylen);
	fnvlist_add_nvlist(ioc_args, ZPOOL_HIDDEN_ARGS, hidden_args);
	if (noop)
		fnvlist_add_boolean(ioc_args, "noop");
	error = lzc_ioctl(ZFS_IOC_LOAD_KEY, fsname, ioc_args, NULL);
	nvlist_free(hidden_args);
	nvlist_free(ioc_args);
	return (error);
}
int
lzc_unload_key(const char *fsname)
{
	return (lzc_ioctl(ZFS_IOC_UNLOAD_KEY, fsname, NULL, NULL));
}
int
lzc_change_key(const char *fsname, uint64_t crypt_cmd, nvlist_t *props,
    uint8_t *wkeydata, uint_t wkeylen)
{
	int error;
	nvlist_t *ioc_args = fnvlist_alloc();
	nvlist_t *hidden_args = NULL;
	fnvlist_add_uint64(ioc_args, "crypt_cmd", crypt_cmd);
	if (wkeydata != NULL) {
		hidden_args = fnvlist_alloc();
		fnvlist_add_uint8_array(hidden_args, "wkeydata", wkeydata,
		    wkeylen);
		fnvlist_add_nvlist(ioc_args, ZPOOL_HIDDEN_ARGS, hidden_args);
	}
	if (props != NULL)
		fnvlist_add_nvlist(ioc_args, "props", props);
	error = lzc_ioctl(ZFS_IOC_CHANGE_KEY, fsname, ioc_args, NULL);
	nvlist_free(hidden_args);
	nvlist_free(ioc_args);
	return (error);
}
int
lzc_reopen(const char *pool_name, boolean_t scrub_restart)
{
	nvlist_t *args = fnvlist_alloc();
	int error;
	fnvlist_add_boolean_value(args, "scrub_restart", scrub_restart);
	error = lzc_ioctl(ZFS_IOC_POOL_REOPEN, pool_name, args, NULL);
	nvlist_free(args);
	return (error);
}
int
lzc_initialize(const char *poolname, pool_initialize_func_t cmd_type,
    nvlist_t *vdevs, nvlist_t **errlist)
{
	int error;
	nvlist_t *args = fnvlist_alloc();
	fnvlist_add_uint64(args, ZPOOL_INITIALIZE_COMMAND, (uint64_t)cmd_type);
	fnvlist_add_nvlist(args, ZPOOL_INITIALIZE_VDEVS, vdevs);
	error = lzc_ioctl(ZFS_IOC_POOL_INITIALIZE, poolname, args, errlist);
	fnvlist_free(args);
	return (error);
}
int
lzc_trim(const char *poolname, pool_trim_func_t cmd_type, uint64_t rate,
    boolean_t secure, nvlist_t *vdevs, nvlist_t **errlist)
{
	int error;
	nvlist_t *args = fnvlist_alloc();
	fnvlist_add_uint64(args, ZPOOL_TRIM_COMMAND, (uint64_t)cmd_type);
	fnvlist_add_nvlist(args, ZPOOL_TRIM_VDEVS, vdevs);
	fnvlist_add_uint64(args, ZPOOL_TRIM_RATE, rate);
	fnvlist_add_boolean_value(args, ZPOOL_TRIM_SECURE, secure);
	error = lzc_ioctl(ZFS_IOC_POOL_TRIM, poolname, args, errlist);
	fnvlist_free(args);
	return (error);
}
int
lzc_redact(const char *snapshot, const char *bookname, nvlist_t *snapnv)
{
	nvlist_t *args = fnvlist_alloc();
	fnvlist_add_string(args, "bookname", bookname);
	fnvlist_add_nvlist(args, "snapnv", snapnv);
	int error = lzc_ioctl(ZFS_IOC_REDACT, snapshot, args, NULL);
	fnvlist_free(args);
	return (error);
}
static int
wait_common(const char *pool, zpool_wait_activity_t activity, boolean_t use_tag,
    uint64_t tag, boolean_t *waited)
{
	nvlist_t *args = fnvlist_alloc();
	nvlist_t *result = NULL;
	fnvlist_add_int32(args, ZPOOL_WAIT_ACTIVITY, activity);
	if (use_tag)
		fnvlist_add_uint64(args, ZPOOL_WAIT_TAG, tag);
	int error = lzc_ioctl(ZFS_IOC_WAIT, pool, args, &result);
	if (error == 0 && waited != NULL)
		*waited = fnvlist_lookup_boolean_value(result,
		    ZPOOL_WAIT_WAITED);
	fnvlist_free(args);
	fnvlist_free(result);
	return (error);
}
int
lzc_wait(const char *pool, zpool_wait_activity_t activity, boolean_t *waited)
{
	return (wait_common(pool, activity, B_FALSE, 0, waited));
}
int
lzc_wait_tag(const char *pool, zpool_wait_activity_t activity, uint64_t tag,
    boolean_t *waited)
{
	return (wait_common(pool, activity, B_TRUE, tag, waited));
}
int
lzc_wait_fs(const char *fs, zfs_wait_activity_t activity, boolean_t *waited)
{
	nvlist_t *args = fnvlist_alloc();
	nvlist_t *result = NULL;
	fnvlist_add_int32(args, ZFS_WAIT_ACTIVITY, activity);
	int error = lzc_ioctl(ZFS_IOC_WAIT_FS, fs, args, &result);
	if (error == 0 && waited != NULL)
		*waited = fnvlist_lookup_boolean_value(result,
		    ZFS_WAIT_WAITED);
	fnvlist_free(args);
	fnvlist_free(result);
	return (error);
}
int
lzc_set_bootenv(const char *pool, const nvlist_t *env)
{
	return (lzc_ioctl(ZFS_IOC_SET_BOOTENV, pool, (nvlist_t *)env, NULL));
}
int
lzc_get_bootenv(const char *pool, nvlist_t **outnvl)
{
	return (lzc_ioctl(ZFS_IOC_GET_BOOTENV, pool, NULL, outnvl));
}
