 
#include <linux/syscalls.h>
#include <linux/export.h>
#include <linux/uaccess.h>
#include <linux/fs_struct.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/prefetch.h>
#include "mount.h"
#include "internal.h"

struct prepend_buffer {
	char *buf;
	int len;
};
#define DECLARE_BUFFER(__name, __buf, __len) \
	struct prepend_buffer __name = {.buf = __buf + __len, .len = __len}

static char *extract_string(struct prepend_buffer *p)
{
	if (likely(p->len >= 0))
		return p->buf;
	return ERR_PTR(-ENAMETOOLONG);
}

static bool prepend_char(struct prepend_buffer *p, unsigned char c)
{
	if (likely(p->len > 0)) {
		p->len--;
		*--p->buf = c;
		return true;
	}
	p->len = -1;
	return false;
}

 
static bool prepend_copy(void *dst, const void *src, int len)
{
	if (unlikely(copy_from_kernel_nofault(dst, src, len))) {
		memset(dst, 'x', len);
		return false;
	}
	return true;
}

static bool prepend(struct prepend_buffer *p, const char *str, int namelen)
{
	 
	if (p->len < 0)
		return false;

	 
	if (p->len < namelen) {
		 
		str += namelen - p->len;
		p->buf -= p->len;
		prepend_copy(p->buf, str, p->len);
		p->len = -1;
		return false;
	}

	 
	p->len -= namelen;
	p->buf -= namelen;
	return prepend_copy(p->buf, str, namelen);
}

 
static bool prepend_name(struct prepend_buffer *p, const struct qstr *name)
{
	const char *dname = smp_load_acquire(&name->name);  
	u32 dlen = READ_ONCE(name->len);

	return prepend(p, dname, dlen) && prepend_char(p, '/');
}

static int __prepend_path(const struct dentry *dentry, const struct mount *mnt,
			  const struct path *root, struct prepend_buffer *p)
{
	while (dentry != root->dentry || &mnt->mnt != root->mnt) {
		const struct dentry *parent = READ_ONCE(dentry->d_parent);

		if (dentry == mnt->mnt.mnt_root) {
			struct mount *m = READ_ONCE(mnt->mnt_parent);
			struct mnt_namespace *mnt_ns;

			if (likely(mnt != m)) {
				dentry = READ_ONCE(mnt->mnt_mountpoint);
				mnt = m;
				continue;
			}
			 
			mnt_ns = READ_ONCE(mnt->mnt_ns);
			 
			if (!IS_ERR_OR_NULL(mnt_ns) && !is_anon_ns(mnt_ns))
				return 1;	 
			else
				return 2;	 
		}

		if (unlikely(dentry == parent))
			 
			return 3;

		prefetch(parent);
		if (!prepend_name(p, &dentry->d_name))
			break;
		dentry = parent;
	}
	return 0;
}

 
static int prepend_path(const struct path *path,
			const struct path *root,
			struct prepend_buffer *p)
{
	unsigned seq, m_seq = 0;
	struct prepend_buffer b;
	int error;

	rcu_read_lock();
restart_mnt:
	read_seqbegin_or_lock(&mount_lock, &m_seq);
	seq = 0;
	rcu_read_lock();
restart:
	b = *p;
	read_seqbegin_or_lock(&rename_lock, &seq);
	error = __prepend_path(path->dentry, real_mount(path->mnt), root, &b);
	if (!(seq & 1))
		rcu_read_unlock();
	if (need_seqretry(&rename_lock, seq)) {
		seq = 1;
		goto restart;
	}
	done_seqretry(&rename_lock, seq);

	if (!(m_seq & 1))
		rcu_read_unlock();
	if (need_seqretry(&mount_lock, m_seq)) {
		m_seq = 1;
		goto restart_mnt;
	}
	done_seqretry(&mount_lock, m_seq);

	if (unlikely(error == 3))
		b = *p;

	if (b.len == p->len)
		prepend_char(&b, '/');

	*p = b;
	return error;
}

 
char *__d_path(const struct path *path,
	       const struct path *root,
	       char *buf, int buflen)
{
	DECLARE_BUFFER(b, buf, buflen);

	prepend_char(&b, 0);
	if (unlikely(prepend_path(path, root, &b) > 0))
		return NULL;
	return extract_string(&b);
}

char *d_absolute_path(const struct path *path,
	       char *buf, int buflen)
{
	struct path root = {};
	DECLARE_BUFFER(b, buf, buflen);

	prepend_char(&b, 0);
	if (unlikely(prepend_path(path, &root, &b) > 1))
		return ERR_PTR(-EINVAL);
	return extract_string(&b);
}

static void get_fs_root_rcu(struct fs_struct *fs, struct path *root)
{
	unsigned seq;

	do {
		seq = read_seqcount_begin(&fs->seq);
		*root = fs->root;
	} while (read_seqcount_retry(&fs->seq, seq));
}

 
char *d_path(const struct path *path, char *buf, int buflen)
{
	DECLARE_BUFFER(b, buf, buflen);
	struct path root;

	 
	if (path->dentry->d_op && path->dentry->d_op->d_dname &&
	    (!IS_ROOT(path->dentry) || path->dentry != path->mnt->mnt_root))
		return path->dentry->d_op->d_dname(path->dentry, buf, buflen);

	rcu_read_lock();
	get_fs_root_rcu(current->fs, &root);
	if (unlikely(d_unlinked(path->dentry)))
		prepend(&b, " (deleted)", 11);
	else
		prepend_char(&b, 0);
	prepend_path(path, &root, &b);
	rcu_read_unlock();

	return extract_string(&b);
}
EXPORT_SYMBOL(d_path);

 
char *dynamic_dname(char *buffer, int buflen, const char *fmt, ...)
{
	va_list args;
	char temp[64];
	int sz;

	va_start(args, fmt);
	sz = vsnprintf(temp, sizeof(temp), fmt, args) + 1;
	va_end(args);

	if (sz > sizeof(temp) || sz > buflen)
		return ERR_PTR(-ENAMETOOLONG);

	buffer += buflen - sz;
	return memcpy(buffer, temp, sz);
}

char *simple_dname(struct dentry *dentry, char *buffer, int buflen)
{
	DECLARE_BUFFER(b, buffer, buflen);
	 
	prepend(&b, " (deleted)", 11);
	prepend(&b, dentry->d_name.name, dentry->d_name.len);
	prepend_char(&b, '/');
	return extract_string(&b);
}

 
static char *__dentry_path(const struct dentry *d, struct prepend_buffer *p)
{
	const struct dentry *dentry;
	struct prepend_buffer b;
	int seq = 0;

	rcu_read_lock();
restart:
	dentry = d;
	b = *p;
	read_seqbegin_or_lock(&rename_lock, &seq);
	while (!IS_ROOT(dentry)) {
		const struct dentry *parent = dentry->d_parent;

		prefetch(parent);
		if (!prepend_name(&b, &dentry->d_name))
			break;
		dentry = parent;
	}
	if (!(seq & 1))
		rcu_read_unlock();
	if (need_seqretry(&rename_lock, seq)) {
		seq = 1;
		goto restart;
	}
	done_seqretry(&rename_lock, seq);
	if (b.len == p->len)
		prepend_char(&b, '/');
	return extract_string(&b);
}

char *dentry_path_raw(const struct dentry *dentry, char *buf, int buflen)
{
	DECLARE_BUFFER(b, buf, buflen);

	prepend_char(&b, 0);
	return __dentry_path(dentry, &b);
}
EXPORT_SYMBOL(dentry_path_raw);

char *dentry_path(const struct dentry *dentry, char *buf, int buflen)
{
	DECLARE_BUFFER(b, buf, buflen);

	if (unlikely(d_unlinked(dentry)))
		prepend(&b, "//deleted", 10);
	else
		prepend_char(&b, 0);
	return __dentry_path(dentry, &b);
}

static void get_fs_root_and_pwd_rcu(struct fs_struct *fs, struct path *root,
				    struct path *pwd)
{
	unsigned seq;

	do {
		seq = read_seqcount_begin(&fs->seq);
		*root = fs->root;
		*pwd = fs->pwd;
	} while (read_seqcount_retry(&fs->seq, seq));
}

 
SYSCALL_DEFINE2(getcwd, char __user *, buf, unsigned long, size)
{
	int error;
	struct path pwd, root;
	char *page = __getname();

	if (!page)
		return -ENOMEM;

	rcu_read_lock();
	get_fs_root_and_pwd_rcu(current->fs, &root, &pwd);

	if (unlikely(d_unlinked(pwd.dentry))) {
		rcu_read_unlock();
		error = -ENOENT;
	} else {
		unsigned len;
		DECLARE_BUFFER(b, page, PATH_MAX);

		prepend_char(&b, 0);
		if (unlikely(prepend_path(&pwd, &root, &b) > 0))
			prepend(&b, "(unreachable)", 13);
		rcu_read_unlock();

		len = PATH_MAX - b.len;
		if (unlikely(len > PATH_MAX))
			error = -ENAMETOOLONG;
		else if (unlikely(len > size))
			error = -ERANGE;
		else if (copy_to_user(buf, b.buf, len))
			error = -EFAULT;
		else
			error = len;
	}
	__putname(page);
	return error;
}
