
 

#include <linux/slab.h>
#include <linux/namei.h>
#include <linux/ctype.h>
#include <linux/fs_context.h>

#include <linux/sunrpc/svcsock.h>
#include <linux/lockd/lockd.h>
#include <linux/sunrpc/addr.h>
#include <linux/sunrpc/gss_api.h>
#include <linux/sunrpc/rpc_pipe_fs.h>
#include <linux/module.h>
#include <linux/fsnotify.h>

#include "idmap.h"
#include "nfsd.h"
#include "cache.h"
#include "state.h"
#include "netns.h"
#include "pnfs.h"
#include "filecache.h"
#include "trace.h"

 
enum {
	NFSD_Root = 1,
	NFSD_List,
	NFSD_Export_Stats,
	NFSD_Export_features,
	NFSD_Fh,
	NFSD_FO_UnlockIP,
	NFSD_FO_UnlockFS,
	NFSD_Threads,
	NFSD_Pool_Threads,
	NFSD_Pool_Stats,
	NFSD_Reply_Cache_Stats,
	NFSD_Versions,
	NFSD_Ports,
	NFSD_MaxBlkSize,
	NFSD_MaxConnections,
	NFSD_Filecache,
	 
#ifdef CONFIG_NFSD_V4
	NFSD_Leasetime,
	NFSD_Gracetime,
	NFSD_RecoveryDir,
	NFSD_V4EndGrace,
#endif
	NFSD_MaxReserved
};

 
static ssize_t write_filehandle(struct file *file, char *buf, size_t size);
static ssize_t write_unlock_ip(struct file *file, char *buf, size_t size);
static ssize_t write_unlock_fs(struct file *file, char *buf, size_t size);
static ssize_t write_threads(struct file *file, char *buf, size_t size);
static ssize_t write_pool_threads(struct file *file, char *buf, size_t size);
static ssize_t write_versions(struct file *file, char *buf, size_t size);
static ssize_t write_ports(struct file *file, char *buf, size_t size);
static ssize_t write_maxblksize(struct file *file, char *buf, size_t size);
static ssize_t write_maxconn(struct file *file, char *buf, size_t size);
#ifdef CONFIG_NFSD_V4
static ssize_t write_leasetime(struct file *file, char *buf, size_t size);
static ssize_t write_gracetime(struct file *file, char *buf, size_t size);
static ssize_t write_recoverydir(struct file *file, char *buf, size_t size);
static ssize_t write_v4_end_grace(struct file *file, char *buf, size_t size);
#endif

static ssize_t (*const write_op[])(struct file *, char *, size_t) = {
	[NFSD_Fh] = write_filehandle,
	[NFSD_FO_UnlockIP] = write_unlock_ip,
	[NFSD_FO_UnlockFS] = write_unlock_fs,
	[NFSD_Threads] = write_threads,
	[NFSD_Pool_Threads] = write_pool_threads,
	[NFSD_Versions] = write_versions,
	[NFSD_Ports] = write_ports,
	[NFSD_MaxBlkSize] = write_maxblksize,
	[NFSD_MaxConnections] = write_maxconn,
#ifdef CONFIG_NFSD_V4
	[NFSD_Leasetime] = write_leasetime,
	[NFSD_Gracetime] = write_gracetime,
	[NFSD_RecoveryDir] = write_recoverydir,
	[NFSD_V4EndGrace] = write_v4_end_grace,
#endif
};

static ssize_t nfsctl_transaction_write(struct file *file, const char __user *buf, size_t size, loff_t *pos)
{
	ino_t ino =  file_inode(file)->i_ino;
	char *data;
	ssize_t rv;

	if (ino >= ARRAY_SIZE(write_op) || !write_op[ino])
		return -EINVAL;

	data = simple_transaction_get(file, buf, size);
	if (IS_ERR(data))
		return PTR_ERR(data);

	rv = write_op[ino](file, data, size);
	if (rv < 0)
		return rv;

	simple_transaction_set(file, rv);
	return size;
}

static ssize_t nfsctl_transaction_read(struct file *file, char __user *buf, size_t size, loff_t *pos)
{
	if (! file->private_data) {
		 
		ssize_t rv = nfsctl_transaction_write(file, buf, 0, pos);
		if (rv < 0)
			return rv;
	}
	return simple_transaction_read(file, buf, size, pos);
}

static const struct file_operations transaction_ops = {
	.write		= nfsctl_transaction_write,
	.read		= nfsctl_transaction_read,
	.release	= simple_transaction_release,
	.llseek		= default_llseek,
};

static int exports_net_open(struct net *net, struct file *file)
{
	int err;
	struct seq_file *seq;
	struct nfsd_net *nn = net_generic(net, nfsd_net_id);

	err = seq_open(file, &nfs_exports_op);
	if (err)
		return err;

	seq = file->private_data;
	seq->private = nn->svc_export_cache;
	return 0;
}

static int exports_nfsd_open(struct inode *inode, struct file *file)
{
	return exports_net_open(inode->i_sb->s_fs_info, file);
}

static const struct file_operations exports_nfsd_operations = {
	.open		= exports_nfsd_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int export_features_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%x 0x%x\n", NFSEXP_ALLFLAGS, NFSEXP_SECINFO_FLAGS);
	return 0;
}

DEFINE_SHOW_ATTRIBUTE(export_features);

static const struct file_operations pool_stats_operations = {
	.open		= nfsd_pool_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= nfsd_pool_stats_release,
};

DEFINE_SHOW_ATTRIBUTE(nfsd_reply_cache_stats);

DEFINE_SHOW_ATTRIBUTE(nfsd_file_cache_stats);

 
 

static inline struct net *netns(struct file *file)
{
	return file_inode(file)->i_sb->s_fs_info;
}

 
static ssize_t write_unlock_ip(struct file *file, char *buf, size_t size)
{
	struct sockaddr_storage address;
	struct sockaddr *sap = (struct sockaddr *)&address;
	size_t salen = sizeof(address);
	char *fo_path;
	struct net *net = netns(file);

	 
	if (size == 0)
		return -EINVAL;

	if (buf[size-1] != '\n')
		return -EINVAL;

	fo_path = buf;
	if (qword_get(&buf, fo_path, size) < 0)
		return -EINVAL;

	if (rpc_pton(net, fo_path, size, sap, salen) == 0)
		return -EINVAL;

	trace_nfsd_ctl_unlock_ip(net, buf);
	return nlmsvc_unlock_all_by_ip(sap);
}

 
static ssize_t write_unlock_fs(struct file *file, char *buf, size_t size)
{
	struct path path;
	char *fo_path;
	int error;

	 
	if (size == 0)
		return -EINVAL;

	if (buf[size-1] != '\n')
		return -EINVAL;

	fo_path = buf;
	if (qword_get(&buf, fo_path, size) < 0)
		return -EINVAL;
	trace_nfsd_ctl_unlock_fs(netns(file), fo_path);
	error = kern_path(fo_path, 0, &path);
	if (error)
		return error;

	 
	error = nlmsvc_unlock_all_by_sb(path.dentry->d_sb);

	path_put(&path);
	return error;
}

 
static ssize_t write_filehandle(struct file *file, char *buf, size_t size)
{
	char *dname, *path;
	int maxsize;
	char *mesg = buf;
	int len;
	struct auth_domain *dom;
	struct knfsd_fh fh;

	if (size == 0)
		return -EINVAL;

	if (buf[size-1] != '\n')
		return -EINVAL;
	buf[size-1] = 0;

	dname = mesg;
	len = qword_get(&mesg, dname, size);
	if (len <= 0)
		return -EINVAL;

	path = dname+len+1;
	len = qword_get(&mesg, path, size);
	if (len <= 0)
		return -EINVAL;

	len = get_int(&mesg, &maxsize);
	if (len)
		return len;

	if (maxsize < NFS_FHSIZE)
		return -EINVAL;
	maxsize = min(maxsize, NFS3_FHSIZE);

	if (qword_get(&mesg, mesg, size) > 0)
		return -EINVAL;

	trace_nfsd_ctl_filehandle(netns(file), dname, path, maxsize);

	 
	dom = unix_domain_find(dname);
	if (!dom)
		return -ENOMEM;

	len = exp_rootfh(netns(file), dom, path, &fh, maxsize);
	auth_domain_put(dom);
	if (len)
		return len;

	mesg = buf;
	len = SIMPLE_TRANSACTION_LIMIT;
	qword_addhex(&mesg, &len, fh.fh_raw, fh.fh_size);
	mesg[-1] = '\n';
	return mesg - buf;
}

 
static ssize_t write_threads(struct file *file, char *buf, size_t size)
{
	char *mesg = buf;
	int rv;
	struct net *net = netns(file);

	if (size > 0) {
		int newthreads;
		rv = get_int(&mesg, &newthreads);
		if (rv)
			return rv;
		if (newthreads < 0)
			return -EINVAL;
		trace_nfsd_ctl_threads(net, newthreads);
		rv = nfsd_svc(newthreads, net, file->f_cred);
		if (rv < 0)
			return rv;
	} else
		rv = nfsd_nrthreads(net);

	return scnprintf(buf, SIMPLE_TRANSACTION_LIMIT, "%d\n", rv);
}

 
static ssize_t write_pool_threads(struct file *file, char *buf, size_t size)
{
	 
	char *mesg = buf;
	int i;
	int rv;
	int len;
	int npools;
	int *nthreads;
	struct net *net = netns(file);

	mutex_lock(&nfsd_mutex);
	npools = nfsd_nrpools(net);
	if (npools == 0) {
		 
		mutex_unlock(&nfsd_mutex);
		strcpy(buf, "0\n");
		return strlen(buf);
	}

	nthreads = kcalloc(npools, sizeof(int), GFP_KERNEL);
	rv = -ENOMEM;
	if (nthreads == NULL)
		goto out_free;

	if (size > 0) {
		for (i = 0; i < npools; i++) {
			rv = get_int(&mesg, &nthreads[i]);
			if (rv == -ENOENT)
				break;		 
			if (rv)
				goto out_free;	 
			rv = -EINVAL;
			if (nthreads[i] < 0)
				goto out_free;
			trace_nfsd_ctl_pool_threads(net, i, nthreads[i]);
		}
		rv = nfsd_set_nrthreads(i, nthreads, net);
		if (rv)
			goto out_free;
	}

	rv = nfsd_get_nrthreads(npools, nthreads, net);
	if (rv)
		goto out_free;

	mesg = buf;
	size = SIMPLE_TRANSACTION_LIMIT;
	for (i = 0; i < npools && size > 0; i++) {
		snprintf(mesg, size, "%d%c", nthreads[i], (i == npools-1 ? '\n' : ' '));
		len = strlen(mesg);
		size -= len;
		mesg += len;
	}
	rv = mesg - buf;
out_free:
	kfree(nthreads);
	mutex_unlock(&nfsd_mutex);
	return rv;
}

static ssize_t
nfsd_print_version_support(struct nfsd_net *nn, char *buf, int remaining,
		const char *sep, unsigned vers, int minor)
{
	const char *format = minor < 0 ? "%s%c%u" : "%s%c%u.%u";
	bool supported = !!nfsd_vers(nn, vers, NFSD_TEST);

	if (vers == 4 && minor >= 0 &&
	    !nfsd_minorversion(nn, minor, NFSD_TEST))
		supported = false;
	if (minor == 0 && supported)
		 
		return 0;
	return snprintf(buf, remaining, format, sep,
			supported ? '+' : '-', vers, minor);
}

static ssize_t __write_versions(struct file *file, char *buf, size_t size)
{
	char *mesg = buf;
	char *vers, *minorp, sign;
	int len, num, remaining;
	ssize_t tlen = 0;
	char *sep;
	struct nfsd_net *nn = net_generic(netns(file), nfsd_net_id);

	if (size > 0) {
		if (nn->nfsd_serv)
			 
			return -EBUSY;
		if (buf[size-1] != '\n')
			return -EINVAL;
		buf[size-1] = 0;
		trace_nfsd_ctl_version(netns(file), buf);

		vers = mesg;
		len = qword_get(&mesg, vers, size);
		if (len <= 0) return -EINVAL;
		do {
			enum vers_op cmd;
			unsigned minor;
			sign = *vers;
			if (sign == '+' || sign == '-')
				num = simple_strtol((vers+1), &minorp, 0);
			else
				num = simple_strtol(vers, &minorp, 0);
			if (*minorp == '.') {
				if (num != 4)
					return -EINVAL;
				if (kstrtouint(minorp+1, 0, &minor) < 0)
					return -EINVAL;
			}

			cmd = sign == '-' ? NFSD_CLEAR : NFSD_SET;
			switch(num) {
#ifdef CONFIG_NFSD_V2
			case 2:
#endif
			case 3:
				nfsd_vers(nn, num, cmd);
				break;
			case 4:
				if (*minorp == '.') {
					if (nfsd_minorversion(nn, minor, cmd) < 0)
						return -EINVAL;
				} else if ((cmd == NFSD_SET) != nfsd_vers(nn, num, NFSD_TEST)) {
					 
					minor = 0;
					while (nfsd_minorversion(nn, minor, cmd) >= 0)
						minor++;
				}
				break;
			default:
				 
				if (cmd == NFSD_SET)
					return -EINVAL;
			}
			vers += len + 1;
		} while ((len = qword_get(&mesg, vers, size)) > 0);
		 
		nfsd_reset_versions(nn);
	}

	 
	sep = "";
	remaining = SIMPLE_TRANSACTION_LIMIT;
	for (num=2 ; num <= 4 ; num++) {
		int minor;
		if (!nfsd_vers(nn, num, NFSD_AVAIL))
			continue;

		minor = -1;
		do {
			len = nfsd_print_version_support(nn, buf, remaining,
					sep, num, minor);
			if (len >= remaining)
				goto out;
			remaining -= len;
			buf += len;
			tlen += len;
			minor++;
			if (len)
				sep = " ";
		} while (num == 4 && minor <= NFSD_SUPPORTED_MINOR_VERSION);
	}
out:
	len = snprintf(buf, remaining, "\n");
	if (len >= remaining)
		return -EINVAL;
	return tlen + len;
}

 
static ssize_t write_versions(struct file *file, char *buf, size_t size)
{
	ssize_t rv;

	mutex_lock(&nfsd_mutex);
	rv = __write_versions(file, buf, size);
	mutex_unlock(&nfsd_mutex);
	return rv;
}

 
static ssize_t __write_ports_names(char *buf, struct net *net)
{
	struct nfsd_net *nn = net_generic(net, nfsd_net_id);

	if (nn->nfsd_serv == NULL)
		return 0;
	return svc_xprt_names(nn->nfsd_serv, buf, SIMPLE_TRANSACTION_LIMIT);
}

 
static ssize_t __write_ports_addfd(char *buf, struct net *net, const struct cred *cred)
{
	char *mesg = buf;
	int fd, err;
	struct nfsd_net *nn = net_generic(net, nfsd_net_id);
	struct svc_serv *serv;

	err = get_int(&mesg, &fd);
	if (err != 0 || fd < 0)
		return -EINVAL;
	trace_nfsd_ctl_ports_addfd(net, fd);

	err = nfsd_create_serv(net);
	if (err != 0)
		return err;

	serv = nn->nfsd_serv;
	err = svc_addsock(serv, net, fd, buf, SIMPLE_TRANSACTION_LIMIT, cred);

	if (err < 0 && !serv->sv_nrthreads && !nn->keep_active)
		nfsd_last_thread(net);
	else if (err >= 0 && !serv->sv_nrthreads && !xchg(&nn->keep_active, 1))
		svc_get(serv);

	svc_put(serv);
	return err;
}

 
static ssize_t __write_ports_addxprt(char *buf, struct net *net, const struct cred *cred)
{
	char transport[16];
	struct svc_xprt *xprt;
	int port, err;
	struct nfsd_net *nn = net_generic(net, nfsd_net_id);
	struct svc_serv *serv;

	if (sscanf(buf, "%15s %5u", transport, &port) != 2)
		return -EINVAL;

	if (port < 1 || port > USHRT_MAX)
		return -EINVAL;
	trace_nfsd_ctl_ports_addxprt(net, transport, port);

	err = nfsd_create_serv(net);
	if (err != 0)
		return err;

	serv = nn->nfsd_serv;
	err = svc_xprt_create(serv, transport, net,
			      PF_INET, port, SVC_SOCK_ANONYMOUS, cred);
	if (err < 0)
		goto out_err;

	err = svc_xprt_create(serv, transport, net,
			      PF_INET6, port, SVC_SOCK_ANONYMOUS, cred);
	if (err < 0 && err != -EAFNOSUPPORT)
		goto out_close;

	if (!serv->sv_nrthreads && !xchg(&nn->keep_active, 1))
		svc_get(serv);

	svc_put(serv);
	return 0;
out_close:
	xprt = svc_find_xprt(serv, transport, net, PF_INET, port);
	if (xprt != NULL) {
		svc_xprt_close(xprt);
		svc_xprt_put(xprt);
	}
out_err:
	if (!serv->sv_nrthreads && !nn->keep_active)
		nfsd_last_thread(net);

	svc_put(serv);
	return err;
}

static ssize_t __write_ports(struct file *file, char *buf, size_t size,
			     struct net *net)
{
	if (size == 0)
		return __write_ports_names(buf, net);

	if (isdigit(buf[0]))
		return __write_ports_addfd(buf, net, file->f_cred);

	if (isalpha(buf[0]))
		return __write_ports_addxprt(buf, net, file->f_cred);

	return -EINVAL;
}

 
static ssize_t write_ports(struct file *file, char *buf, size_t size)
{
	ssize_t rv;

	mutex_lock(&nfsd_mutex);
	rv = __write_ports(file, buf, size, netns(file));
	mutex_unlock(&nfsd_mutex);
	return rv;
}


int nfsd_max_blksize;

 
static ssize_t write_maxblksize(struct file *file, char *buf, size_t size)
{
	char *mesg = buf;
	struct nfsd_net *nn = net_generic(netns(file), nfsd_net_id);

	if (size > 0) {
		int bsize;
		int rv = get_int(&mesg, &bsize);
		if (rv)
			return rv;
		trace_nfsd_ctl_maxblksize(netns(file), bsize);

		 
		bsize = max_t(int, bsize, 1024);
		bsize = min_t(int, bsize, NFSSVC_MAXBLKSIZE);
		bsize &= ~(1024-1);
		mutex_lock(&nfsd_mutex);
		if (nn->nfsd_serv) {
			mutex_unlock(&nfsd_mutex);
			return -EBUSY;
		}
		nfsd_max_blksize = bsize;
		mutex_unlock(&nfsd_mutex);
	}

	return scnprintf(buf, SIMPLE_TRANSACTION_LIMIT, "%d\n",
							nfsd_max_blksize);
}

 
static ssize_t write_maxconn(struct file *file, char *buf, size_t size)
{
	char *mesg = buf;
	struct nfsd_net *nn = net_generic(netns(file), nfsd_net_id);
	unsigned int maxconn = nn->max_connections;

	if (size > 0) {
		int rv = get_uint(&mesg, &maxconn);

		if (rv)
			return rv;
		trace_nfsd_ctl_maxconn(netns(file), maxconn);
		nn->max_connections = maxconn;
	}

	return scnprintf(buf, SIMPLE_TRANSACTION_LIMIT, "%u\n", maxconn);
}

#ifdef CONFIG_NFSD_V4
static ssize_t __nfsd4_write_time(struct file *file, char *buf, size_t size,
				  time64_t *time, struct nfsd_net *nn)
{
	struct dentry *dentry = file_dentry(file);
	char *mesg = buf;
	int rv, i;

	if (size > 0) {
		if (nn->nfsd_serv)
			return -EBUSY;
		rv = get_int(&mesg, &i);
		if (rv)
			return rv;
		trace_nfsd_ctl_time(netns(file), dentry->d_name.name,
				    dentry->d_name.len, i);

		 
		if (i < 10 || i > 3600)
			return -EINVAL;
		*time = i;
	}

	return scnprintf(buf, SIMPLE_TRANSACTION_LIMIT, "%lld\n", *time);
}

static ssize_t nfsd4_write_time(struct file *file, char *buf, size_t size,
				time64_t *time, struct nfsd_net *nn)
{
	ssize_t rv;

	mutex_lock(&nfsd_mutex);
	rv = __nfsd4_write_time(file, buf, size, time, nn);
	mutex_unlock(&nfsd_mutex);
	return rv;
}

 
static ssize_t write_leasetime(struct file *file, char *buf, size_t size)
{
	struct nfsd_net *nn = net_generic(netns(file), nfsd_net_id);
	return nfsd4_write_time(file, buf, size, &nn->nfsd4_lease, nn);
}

 
static ssize_t write_gracetime(struct file *file, char *buf, size_t size)
{
	struct nfsd_net *nn = net_generic(netns(file), nfsd_net_id);
	return nfsd4_write_time(file, buf, size, &nn->nfsd4_grace, nn);
}

static ssize_t __write_recoverydir(struct file *file, char *buf, size_t size,
				   struct nfsd_net *nn)
{
	char *mesg = buf;
	char *recdir;
	int len, status;

	if (size > 0) {
		if (nn->nfsd_serv)
			return -EBUSY;
		if (size > PATH_MAX || buf[size-1] != '\n')
			return -EINVAL;
		buf[size-1] = 0;

		recdir = mesg;
		len = qword_get(&mesg, recdir, size);
		if (len <= 0)
			return -EINVAL;
		trace_nfsd_ctl_recoverydir(netns(file), recdir);

		status = nfs4_reset_recoverydir(recdir);
		if (status)
			return status;
	}

	return scnprintf(buf, SIMPLE_TRANSACTION_LIMIT, "%s\n",
							nfs4_recoverydir());
}

 
static ssize_t write_recoverydir(struct file *file, char *buf, size_t size)
{
	ssize_t rv;
	struct nfsd_net *nn = net_generic(netns(file), nfsd_net_id);

	mutex_lock(&nfsd_mutex);
	rv = __write_recoverydir(file, buf, size, nn);
	mutex_unlock(&nfsd_mutex);
	return rv;
}

 
static ssize_t write_v4_end_grace(struct file *file, char *buf, size_t size)
{
	struct nfsd_net *nn = net_generic(netns(file), nfsd_net_id);

	if (size > 0) {
		switch(buf[0]) {
		case 'Y':
		case 'y':
		case '1':
			if (!nn->nfsd_serv)
				return -EBUSY;
			trace_nfsd_end_grace(netns(file));
			nfsd4_end_grace(nn);
			break;
		default:
			return -EINVAL;
		}
	}

	return scnprintf(buf, SIMPLE_TRANSACTION_LIMIT, "%c\n",
			 nn->grace_ended ? 'Y' : 'N');
}

#endif

 
 

 
static struct inode *nfsd_get_inode(struct super_block *sb, umode_t mode)
{
	struct inode *inode = new_inode(sb);
	if (!inode)
		return NULL;
	 
	inode->i_ino = iunique(sb, NFSD_MaxReserved);
	inode->i_mode = mode;
	inode->i_atime = inode->i_mtime = inode_set_ctime_current(inode);
	switch (mode & S_IFMT) {
	case S_IFDIR:
		inode->i_fop = &simple_dir_operations;
		inode->i_op = &simple_dir_inode_operations;
		inc_nlink(inode);
		break;
	case S_IFLNK:
		inode->i_op = &simple_symlink_inode_operations;
		break;
	default:
		break;
	}
	return inode;
}

static int __nfsd_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode, struct nfsdfs_client *ncl)
{
	struct inode *inode;

	inode = nfsd_get_inode(dir->i_sb, mode);
	if (!inode)
		return -ENOMEM;
	if (ncl) {
		inode->i_private = ncl;
		kref_get(&ncl->cl_ref);
	}
	d_add(dentry, inode);
	inc_nlink(dir);
	fsnotify_mkdir(dir, dentry);
	return 0;
}

static struct dentry *nfsd_mkdir(struct dentry *parent, struct nfsdfs_client *ncl, char *name)
{
	struct inode *dir = parent->d_inode;
	struct dentry *dentry;
	int ret = -ENOMEM;

	inode_lock(dir);
	dentry = d_alloc_name(parent, name);
	if (!dentry)
		goto out_err;
	ret = __nfsd_mkdir(d_inode(parent), dentry, S_IFDIR | 0600, ncl);
	if (ret)
		goto out_err;
out:
	inode_unlock(dir);
	return dentry;
out_err:
	dput(dentry);
	dentry = ERR_PTR(ret);
	goto out;
}

#if IS_ENABLED(CONFIG_SUNRPC_GSS)
static int __nfsd_symlink(struct inode *dir, struct dentry *dentry,
			  umode_t mode, const char *content)
{
	struct inode *inode;

	inode = nfsd_get_inode(dir->i_sb, mode);
	if (!inode)
		return -ENOMEM;

	inode->i_link = (char *)content;
	inode->i_size = strlen(content);

	d_add(dentry, inode);
	inc_nlink(dir);
	fsnotify_create(dir, dentry);
	return 0;
}

 
static void _nfsd_symlink(struct dentry *parent, const char *name,
			  const char *content)
{
	struct inode *dir = parent->d_inode;
	struct dentry *dentry;
	int ret;

	inode_lock(dir);
	dentry = d_alloc_name(parent, name);
	if (!dentry)
		goto out;
	ret = __nfsd_symlink(d_inode(parent), dentry, S_IFLNK | 0777, content);
	if (ret)
		dput(dentry);
out:
	inode_unlock(dir);
}
#else
static inline void _nfsd_symlink(struct dentry *parent, const char *name,
				 const char *content)
{
}

#endif

static void clear_ncl(struct inode *inode)
{
	struct nfsdfs_client *ncl = inode->i_private;

	inode->i_private = NULL;
	kref_put(&ncl->cl_ref, ncl->cl_release);
}

static struct nfsdfs_client *__get_nfsdfs_client(struct inode *inode)
{
	struct nfsdfs_client *nc = inode->i_private;

	if (nc)
		kref_get(&nc->cl_ref);
	return nc;
}

struct nfsdfs_client *get_nfsdfs_client(struct inode *inode)
{
	struct nfsdfs_client *nc;

	inode_lock_shared(inode);
	nc = __get_nfsdfs_client(inode);
	inode_unlock_shared(inode);
	return nc;
}
 
static void nfsdfs_remove_file(struct inode *dir, struct dentry *dentry)
{
	int ret;

	clear_ncl(d_inode(dentry));
	dget(dentry);
	ret = simple_unlink(dir, dentry);
	d_drop(dentry);
	fsnotify_unlink(dir, dentry);
	dput(dentry);
	WARN_ON_ONCE(ret);
}

static void nfsdfs_remove_files(struct dentry *root)
{
	struct dentry *dentry, *tmp;

	list_for_each_entry_safe(dentry, tmp, &root->d_subdirs, d_child) {
		if (!simple_positive(dentry)) {
			WARN_ON_ONCE(1);  
			continue;
		}
		nfsdfs_remove_file(d_inode(root), dentry);
	}
}

 
static  int nfsdfs_create_files(struct dentry *root,
				const struct tree_descr *files,
				struct dentry **fdentries)
{
	struct inode *dir = d_inode(root);
	struct inode *inode;
	struct dentry *dentry;
	int i;

	inode_lock(dir);
	for (i = 0; files->name && files->name[0]; i++, files++) {
		dentry = d_alloc_name(root, files->name);
		if (!dentry)
			goto out;
		inode = nfsd_get_inode(d_inode(root)->i_sb,
					S_IFREG | files->mode);
		if (!inode) {
			dput(dentry);
			goto out;
		}
		inode->i_fop = files->ops;
		inode->i_private = __get_nfsdfs_client(dir);
		d_add(dentry, inode);
		fsnotify_create(dir, dentry);
		if (fdentries)
			fdentries[i] = dentry;
	}
	inode_unlock(dir);
	return 0;
out:
	nfsdfs_remove_files(root);
	inode_unlock(dir);
	return -ENOMEM;
}

 
struct dentry *nfsd_client_mkdir(struct nfsd_net *nn,
				 struct nfsdfs_client *ncl, u32 id,
				 const struct tree_descr *files,
				 struct dentry **fdentries)
{
	struct dentry *dentry;
	char name[11];
	int ret;

	sprintf(name, "%u", id);

	dentry = nfsd_mkdir(nn->nfsd_client_dir, ncl, name);
	if (IS_ERR(dentry))  
		return NULL;
	ret = nfsdfs_create_files(dentry, files, fdentries);
	if (ret) {
		nfsd_client_rmdir(dentry);
		return NULL;
	}
	return dentry;
}

 
void nfsd_client_rmdir(struct dentry *dentry)
{
	struct inode *dir = d_inode(dentry->d_parent);
	struct inode *inode = d_inode(dentry);
	int ret;

	inode_lock(dir);
	nfsdfs_remove_files(dentry);
	clear_ncl(inode);
	dget(dentry);
	ret = simple_rmdir(dir, dentry);
	WARN_ON_ONCE(ret);
	d_drop(dentry);
	fsnotify_rmdir(dir, dentry);
	dput(dentry);
	inode_unlock(dir);
}

static int nfsd_fill_super(struct super_block *sb, struct fs_context *fc)
{
	struct nfsd_net *nn = net_generic(current->nsproxy->net_ns,
							nfsd_net_id);
	struct dentry *dentry;
	int ret;

	static const struct tree_descr nfsd_files[] = {
		[NFSD_List] = {"exports", &exports_nfsd_operations, S_IRUGO},
		 
		[NFSD_Export_Stats] = {"export_stats", &exports_nfsd_operations, S_IRUGO},
		[NFSD_Export_features] = {"export_features",
					&export_features_fops, S_IRUGO},
		[NFSD_FO_UnlockIP] = {"unlock_ip",
					&transaction_ops, S_IWUSR|S_IRUSR},
		[NFSD_FO_UnlockFS] = {"unlock_filesystem",
					&transaction_ops, S_IWUSR|S_IRUSR},
		[NFSD_Fh] = {"filehandle", &transaction_ops, S_IWUSR|S_IRUSR},
		[NFSD_Threads] = {"threads", &transaction_ops, S_IWUSR|S_IRUSR},
		[NFSD_Pool_Threads] = {"pool_threads", &transaction_ops, S_IWUSR|S_IRUSR},
		[NFSD_Pool_Stats] = {"pool_stats", &pool_stats_operations, S_IRUGO},
		[NFSD_Reply_Cache_Stats] = {"reply_cache_stats",
					&nfsd_reply_cache_stats_fops, S_IRUGO},
		[NFSD_Versions] = {"versions", &transaction_ops, S_IWUSR|S_IRUSR},
		[NFSD_Ports] = {"portlist", &transaction_ops, S_IWUSR|S_IRUGO},
		[NFSD_MaxBlkSize] = {"max_block_size", &transaction_ops, S_IWUSR|S_IRUGO},
		[NFSD_MaxConnections] = {"max_connections", &transaction_ops, S_IWUSR|S_IRUGO},
		[NFSD_Filecache] = {"filecache", &nfsd_file_cache_stats_fops, S_IRUGO},
#ifdef CONFIG_NFSD_V4
		[NFSD_Leasetime] = {"nfsv4leasetime", &transaction_ops, S_IWUSR|S_IRUSR},
		[NFSD_Gracetime] = {"nfsv4gracetime", &transaction_ops, S_IWUSR|S_IRUSR},
		[NFSD_RecoveryDir] = {"nfsv4recoverydir", &transaction_ops, S_IWUSR|S_IRUSR},
		[NFSD_V4EndGrace] = {"v4_end_grace", &transaction_ops, S_IWUSR|S_IRUGO},
#endif
		  {""}
	};

	ret = simple_fill_super(sb, 0x6e667364, nfsd_files);
	if (ret)
		return ret;
	_nfsd_symlink(sb->s_root, "supported_krb5_enctypes",
		      "/proc/net/rpc/gss_krb5_enctypes");
	dentry = nfsd_mkdir(sb->s_root, NULL, "clients");
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);
	nn->nfsd_client_dir = dentry;
	return 0;
}

static int nfsd_fs_get_tree(struct fs_context *fc)
{
	return get_tree_keyed(fc, nfsd_fill_super, get_net(fc->net_ns));
}

static void nfsd_fs_free_fc(struct fs_context *fc)
{
	if (fc->s_fs_info)
		put_net(fc->s_fs_info);
}

static const struct fs_context_operations nfsd_fs_context_ops = {
	.free		= nfsd_fs_free_fc,
	.get_tree	= nfsd_fs_get_tree,
};

static int nfsd_init_fs_context(struct fs_context *fc)
{
	put_user_ns(fc->user_ns);
	fc->user_ns = get_user_ns(fc->net_ns->user_ns);
	fc->ops = &nfsd_fs_context_ops;
	return 0;
}

static void nfsd_umount(struct super_block *sb)
{
	struct net *net = sb->s_fs_info;

	nfsd_shutdown_threads(net);

	kill_litter_super(sb);
	put_net(net);
}

static struct file_system_type nfsd_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "nfsd",
	.init_fs_context = nfsd_init_fs_context,
	.kill_sb	= nfsd_umount,
};
MODULE_ALIAS_FS("nfsd");

#ifdef CONFIG_PROC_FS

static int exports_proc_open(struct inode *inode, struct file *file)
{
	return exports_net_open(current->nsproxy->net_ns, file);
}

static const struct proc_ops exports_proc_ops = {
	.proc_open	= exports_proc_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= seq_release,
};

static int create_proc_exports_entry(void)
{
	struct proc_dir_entry *entry;

	entry = proc_mkdir("fs/nfs", NULL);
	if (!entry)
		return -ENOMEM;
	entry = proc_create("exports", 0, entry, &exports_proc_ops);
	if (!entry) {
		remove_proc_entry("fs/nfs", NULL);
		return -ENOMEM;
	}
	return 0;
}
#else  
static int create_proc_exports_entry(void)
{
	return 0;
}
#endif

unsigned int nfsd_net_id;

 
static __net_init int nfsd_net_init(struct net *net)
{
	int retval;
	struct nfsd_net *nn = net_generic(net, nfsd_net_id);

	retval = nfsd_export_init(net);
	if (retval)
		goto out_export_error;
	retval = nfsd_idmap_init(net);
	if (retval)
		goto out_idmap_error;
	retval = nfsd_net_reply_cache_init(nn);
	if (retval)
		goto out_repcache_error;
	nn->nfsd_versions = NULL;
	nn->nfsd4_minorversions = NULL;
	nfsd4_init_leases_net(nn);
	get_random_bytes(&nn->siphash_key, sizeof(nn->siphash_key));
	seqlock_init(&nn->writeverf_lock);

	return 0;

out_repcache_error:
	nfsd_idmap_shutdown(net);
out_idmap_error:
	nfsd_export_shutdown(net);
out_export_error:
	return retval;
}

 
static __net_exit void nfsd_net_exit(struct net *net)
{
	struct nfsd_net *nn = net_generic(net, nfsd_net_id);

	nfsd_net_reply_cache_destroy(nn);
	nfsd_idmap_shutdown(net);
	nfsd_export_shutdown(net);
	nfsd_netns_free_versions(nn);
}

static struct pernet_operations nfsd_net_ops = {
	.init = nfsd_net_init,
	.exit = nfsd_net_exit,
	.id   = &nfsd_net_id,
	.size = sizeof(struct nfsd_net),
};

static int __init init_nfsd(void)
{
	int retval;

	retval = nfsd4_init_slabs();
	if (retval)
		return retval;
	retval = nfsd4_init_pnfs();
	if (retval)
		goto out_free_slabs;
	retval = nfsd_stat_init();	 
	if (retval)
		goto out_free_pnfs;
	retval = nfsd_drc_slab_create();
	if (retval)
		goto out_free_stat;
	nfsd_lockd_init();	 
	retval = create_proc_exports_entry();
	if (retval)
		goto out_free_lockd;
	retval = register_pernet_subsys(&nfsd_net_ops);
	if (retval < 0)
		goto out_free_exports;
	retval = register_cld_notifier();
	if (retval)
		goto out_free_subsys;
	retval = nfsd4_create_laundry_wq();
	if (retval)
		goto out_free_cld;
	retval = register_filesystem(&nfsd_fs_type);
	if (retval)
		goto out_free_all;
	return 0;
out_free_all:
	nfsd4_destroy_laundry_wq();
out_free_cld:
	unregister_cld_notifier();
out_free_subsys:
	unregister_pernet_subsys(&nfsd_net_ops);
out_free_exports:
	remove_proc_entry("fs/nfs/exports", NULL);
	remove_proc_entry("fs/nfs", NULL);
out_free_lockd:
	nfsd_lockd_shutdown();
	nfsd_drc_slab_free();
out_free_stat:
	nfsd_stat_shutdown();
out_free_pnfs:
	nfsd4_exit_pnfs();
out_free_slabs:
	nfsd4_free_slabs();
	return retval;
}

static void __exit exit_nfsd(void)
{
	unregister_filesystem(&nfsd_fs_type);
	nfsd4_destroy_laundry_wq();
	unregister_cld_notifier();
	unregister_pernet_subsys(&nfsd_net_ops);
	nfsd_drc_slab_free();
	remove_proc_entry("fs/nfs/exports", NULL);
	remove_proc_entry("fs/nfs", NULL);
	nfsd_stat_shutdown();
	nfsd_lockd_shutdown();
	nfsd4_free_slabs();
	nfsd4_exit_pnfs();
}

MODULE_AUTHOR("Olaf Kirch <okir@monad.swb.de>");
MODULE_DESCRIPTION("In-kernel NFS server");
MODULE_LICENSE("GPL");
module_init(init_nfsd)
module_exit(exit_nfsd)
