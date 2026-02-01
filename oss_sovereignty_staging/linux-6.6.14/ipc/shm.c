
 

#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/hugetlb.h>
#include <linux/shm.h>
#include <linux/init.h>
#include <linux/file.h>
#include <linux/mman.h>
#include <linux/shmem_fs.h>
#include <linux/security.h>
#include <linux/syscalls.h>
#include <linux/audit.h>
#include <linux/capability.h>
#include <linux/ptrace.h>
#include <linux/seq_file.h>
#include <linux/rwsem.h>
#include <linux/nsproxy.h>
#include <linux/mount.h>
#include <linux/ipc_namespace.h>
#include <linux/rhashtable.h>

#include <linux/uaccess.h>

#include "util.h"

struct shmid_kernel  
{
	struct kern_ipc_perm	shm_perm;
	struct file		*shm_file;
	unsigned long		shm_nattch;
	unsigned long		shm_segsz;
	time64_t		shm_atim;
	time64_t		shm_dtim;
	time64_t		shm_ctim;
	struct pid		*shm_cprid;
	struct pid		*shm_lprid;
	struct ucounts		*mlock_ucounts;

	 
	struct task_struct	*shm_creator;

	 
	struct list_head	shm_clist;
	struct ipc_namespace	*ns;
} __randomize_layout;

 
#define SHM_DEST	01000	 
#define SHM_LOCKED	02000    

struct shm_file_data {
	int id;
	struct ipc_namespace *ns;
	struct file *file;
	const struct vm_operations_struct *vm_ops;
};

#define shm_file_data(file) (*((struct shm_file_data **)&(file)->private_data))

static const struct file_operations shm_file_operations;
static const struct vm_operations_struct shm_vm_ops;

#define shm_ids(ns)	((ns)->ids[IPC_SHM_IDS])

#define shm_unlock(shp)			\
	ipc_unlock(&(shp)->shm_perm)

static int newseg(struct ipc_namespace *, struct ipc_params *);
static void shm_open(struct vm_area_struct *vma);
static void shm_close(struct vm_area_struct *vma);
static void shm_destroy(struct ipc_namespace *ns, struct shmid_kernel *shp);
#ifdef CONFIG_PROC_FS
static int sysvipc_shm_proc_show(struct seq_file *s, void *it);
#endif

void shm_init_ns(struct ipc_namespace *ns)
{
	ns->shm_ctlmax = SHMMAX;
	ns->shm_ctlall = SHMALL;
	ns->shm_ctlmni = SHMMNI;
	ns->shm_rmid_forced = 0;
	ns->shm_tot = 0;
	ipc_init_ids(&shm_ids(ns));
}

 
static void do_shm_rmid(struct ipc_namespace *ns, struct kern_ipc_perm *ipcp)
{
	struct shmid_kernel *shp;

	shp = container_of(ipcp, struct shmid_kernel, shm_perm);
	WARN_ON(ns != shp->ns);

	if (shp->shm_nattch) {
		shp->shm_perm.mode |= SHM_DEST;
		 
		ipc_set_key_private(&shm_ids(ns), &shp->shm_perm);
		shm_unlock(shp);
	} else
		shm_destroy(ns, shp);
}

#ifdef CONFIG_IPC_NS
void shm_exit_ns(struct ipc_namespace *ns)
{
	free_ipcs(ns, &shm_ids(ns), do_shm_rmid);
	idr_destroy(&ns->ids[IPC_SHM_IDS].ipcs_idr);
	rhashtable_destroy(&ns->ids[IPC_SHM_IDS].key_ht);
}
#endif

static int __init ipc_ns_init(void)
{
	shm_init_ns(&init_ipc_ns);
	return 0;
}

pure_initcall(ipc_ns_init);

void __init shm_init(void)
{
	ipc_init_proc_interface("sysvipc/shm",
#if BITS_PER_LONG <= 32
				"       key      shmid perms       size  cpid  lpid nattch   uid   gid  cuid  cgid      atime      dtime      ctime        rss       swap\n",
#else
				"       key      shmid perms                  size  cpid  lpid nattch   uid   gid  cuid  cgid      atime      dtime      ctime                   rss                  swap\n",
#endif
				IPC_SHM_IDS, sysvipc_shm_proc_show);
}

static inline struct shmid_kernel *shm_obtain_object(struct ipc_namespace *ns, int id)
{
	struct kern_ipc_perm *ipcp = ipc_obtain_object_idr(&shm_ids(ns), id);

	if (IS_ERR(ipcp))
		return ERR_CAST(ipcp);

	return container_of(ipcp, struct shmid_kernel, shm_perm);
}

static inline struct shmid_kernel *shm_obtain_object_check(struct ipc_namespace *ns, int id)
{
	struct kern_ipc_perm *ipcp = ipc_obtain_object_check(&shm_ids(ns), id);

	if (IS_ERR(ipcp))
		return ERR_CAST(ipcp);

	return container_of(ipcp, struct shmid_kernel, shm_perm);
}

 
static inline struct shmid_kernel *shm_lock(struct ipc_namespace *ns, int id)
{
	struct kern_ipc_perm *ipcp;

	rcu_read_lock();
	ipcp = ipc_obtain_object_idr(&shm_ids(ns), id);
	if (IS_ERR(ipcp))
		goto err;

	ipc_lock_object(ipcp);
	 
	if (ipc_valid_object(ipcp)) {
		 
		return container_of(ipcp, struct shmid_kernel, shm_perm);
	}

	ipc_unlock_object(ipcp);
	ipcp = ERR_PTR(-EIDRM);
err:
	rcu_read_unlock();
	 
	return ERR_CAST(ipcp);
}

static inline void shm_lock_by_ptr(struct shmid_kernel *ipcp)
{
	rcu_read_lock();
	ipc_lock_object(&ipcp->shm_perm);
}

static void shm_rcu_free(struct rcu_head *head)
{
	struct kern_ipc_perm *ptr = container_of(head, struct kern_ipc_perm,
							rcu);
	struct shmid_kernel *shp = container_of(ptr, struct shmid_kernel,
							shm_perm);
	security_shm_free(&shp->shm_perm);
	kfree(shp);
}

 
static inline void shm_clist_rm(struct shmid_kernel *shp)
{
	struct task_struct *creator;

	 
	rcu_read_lock();

	 
	if (!list_empty(&shp->shm_clist)) {
		 
		creator = shp->shm_creator;

		task_lock(creator);
		 
		list_del_init(&shp->shm_clist);
		task_unlock(creator);
	}
	rcu_read_unlock();
}

static inline void shm_rmid(struct shmid_kernel *s)
{
	shm_clist_rm(s);
	ipc_rmid(&shm_ids(s->ns), &s->shm_perm);
}


static int __shm_open(struct shm_file_data *sfd)
{
	struct shmid_kernel *shp;

	shp = shm_lock(sfd->ns, sfd->id);

	if (IS_ERR(shp))
		return PTR_ERR(shp);

	if (shp->shm_file != sfd->file) {
		 
		shm_unlock(shp);
		return -EINVAL;
	}

	shp->shm_atim = ktime_get_real_seconds();
	ipc_update_pid(&shp->shm_lprid, task_tgid(current));
	shp->shm_nattch++;
	shm_unlock(shp);
	return 0;
}

 
static void shm_open(struct vm_area_struct *vma)
{
	struct file *file = vma->vm_file;
	struct shm_file_data *sfd = shm_file_data(file);
	int err;

	 
	if (sfd->vm_ops->open)
		sfd->vm_ops->open(vma);

	err = __shm_open(sfd);
	 
	WARN_ON_ONCE(err);
}

 
static void shm_destroy(struct ipc_namespace *ns, struct shmid_kernel *shp)
{
	struct file *shm_file;

	shm_file = shp->shm_file;
	shp->shm_file = NULL;
	ns->shm_tot -= (shp->shm_segsz + PAGE_SIZE - 1) >> PAGE_SHIFT;
	shm_rmid(shp);
	shm_unlock(shp);
	if (!is_file_hugepages(shm_file))
		shmem_lock(shm_file, 0, shp->mlock_ucounts);
	fput(shm_file);
	ipc_update_pid(&shp->shm_cprid, NULL);
	ipc_update_pid(&shp->shm_lprid, NULL);
	ipc_rcu_putref(&shp->shm_perm, shm_rcu_free);
}

 
static bool shm_may_destroy(struct shmid_kernel *shp)
{
	return (shp->shm_nattch == 0) &&
	       (shp->ns->shm_rmid_forced ||
		(shp->shm_perm.mode & SHM_DEST));
}

 
static void __shm_close(struct shm_file_data *sfd)
{
	struct shmid_kernel *shp;
	struct ipc_namespace *ns = sfd->ns;

	down_write(&shm_ids(ns).rwsem);
	 
	shp = shm_lock(ns, sfd->id);

	 
	if (WARN_ON_ONCE(IS_ERR(shp)))
		goto done;  

	ipc_update_pid(&shp->shm_lprid, task_tgid(current));
	shp->shm_dtim = ktime_get_real_seconds();
	shp->shm_nattch--;
	if (shm_may_destroy(shp))
		shm_destroy(ns, shp);
	else
		shm_unlock(shp);
done:
	up_write(&shm_ids(ns).rwsem);
}

static void shm_close(struct vm_area_struct *vma)
{
	struct file *file = vma->vm_file;
	struct shm_file_data *sfd = shm_file_data(file);

	 
	if (sfd->vm_ops->close)
		sfd->vm_ops->close(vma);

	__shm_close(sfd);
}

 
static int shm_try_destroy_orphaned(int id, void *p, void *data)
{
	struct ipc_namespace *ns = data;
	struct kern_ipc_perm *ipcp = p;
	struct shmid_kernel *shp = container_of(ipcp, struct shmid_kernel, shm_perm);

	 
	if (!list_empty(&shp->shm_clist))
		return 0;

	if (shm_may_destroy(shp)) {
		shm_lock_by_ptr(shp);
		shm_destroy(ns, shp);
	}
	return 0;
}

void shm_destroy_orphaned(struct ipc_namespace *ns)
{
	down_write(&shm_ids(ns).rwsem);
	if (shm_ids(ns).in_use)
		idr_for_each(&shm_ids(ns).ipcs_idr, &shm_try_destroy_orphaned, ns);
	up_write(&shm_ids(ns).rwsem);
}

 
void exit_shm(struct task_struct *task)
{
	for (;;) {
		struct shmid_kernel *shp;
		struct ipc_namespace *ns;

		task_lock(task);

		if (list_empty(&task->sysvshm.shm_clist)) {
			task_unlock(task);
			break;
		}

		shp = list_first_entry(&task->sysvshm.shm_clist, struct shmid_kernel,
				shm_clist);

		 
		ns = shp->ns;

		 
		if (!ns->shm_rmid_forced)
			goto unlink_continue;

		 
		ns = get_ipc_ns_not_zero(ns);
		if (!ns) {
unlink_continue:
			list_del_init(&shp->shm_clist);
			task_unlock(task);
			continue;
		}

		 
		WARN_ON(!ipc_rcu_getref(&shp->shm_perm));

		 
		list_del_init(&shp->shm_clist);

		task_unlock(task);

		 
		down_write(&shm_ids(ns).rwsem);
		shm_lock_by_ptr(shp);
		 
		ipc_rcu_putref(&shp->shm_perm, shm_rcu_free);

		if (ipc_valid_object(&shp->shm_perm)) {
			if (shm_may_destroy(shp))
				shm_destroy(ns, shp);
			else
				shm_unlock(shp);
		} else {
			 
			shm_unlock(shp);
		}

		up_write(&shm_ids(ns).rwsem);
		put_ipc_ns(ns);  
	}
}

static vm_fault_t shm_fault(struct vm_fault *vmf)
{
	struct file *file = vmf->vma->vm_file;
	struct shm_file_data *sfd = shm_file_data(file);

	return sfd->vm_ops->fault(vmf);
}

static int shm_may_split(struct vm_area_struct *vma, unsigned long addr)
{
	struct file *file = vma->vm_file;
	struct shm_file_data *sfd = shm_file_data(file);

	if (sfd->vm_ops->may_split)
		return sfd->vm_ops->may_split(vma, addr);

	return 0;
}

static unsigned long shm_pagesize(struct vm_area_struct *vma)
{
	struct file *file = vma->vm_file;
	struct shm_file_data *sfd = shm_file_data(file);

	if (sfd->vm_ops->pagesize)
		return sfd->vm_ops->pagesize(vma);

	return PAGE_SIZE;
}

#ifdef CONFIG_NUMA
static int shm_set_policy(struct vm_area_struct *vma, struct mempolicy *new)
{
	struct file *file = vma->vm_file;
	struct shm_file_data *sfd = shm_file_data(file);
	int err = 0;

	if (sfd->vm_ops->set_policy)
		err = sfd->vm_ops->set_policy(vma, new);
	return err;
}

static struct mempolicy *shm_get_policy(struct vm_area_struct *vma,
					unsigned long addr)
{
	struct file *file = vma->vm_file;
	struct shm_file_data *sfd = shm_file_data(file);
	struct mempolicy *pol = NULL;

	if (sfd->vm_ops->get_policy)
		pol = sfd->vm_ops->get_policy(vma, addr);
	else if (vma->vm_policy)
		pol = vma->vm_policy;

	return pol;
}
#endif

static int shm_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct shm_file_data *sfd = shm_file_data(file);
	int ret;

	 
	ret = __shm_open(sfd);
	if (ret)
		return ret;

	ret = call_mmap(sfd->file, vma);
	if (ret) {
		__shm_close(sfd);
		return ret;
	}
	sfd->vm_ops = vma->vm_ops;
#ifdef CONFIG_MMU
	WARN_ON(!sfd->vm_ops->fault);
#endif
	vma->vm_ops = &shm_vm_ops;
	return 0;
}

static int shm_release(struct inode *ino, struct file *file)
{
	struct shm_file_data *sfd = shm_file_data(file);

	put_ipc_ns(sfd->ns);
	fput(sfd->file);
	shm_file_data(file) = NULL;
	kfree(sfd);
	return 0;
}

static int shm_fsync(struct file *file, loff_t start, loff_t end, int datasync)
{
	struct shm_file_data *sfd = shm_file_data(file);

	if (!sfd->file->f_op->fsync)
		return -EINVAL;
	return sfd->file->f_op->fsync(sfd->file, start, end, datasync);
}

static long shm_fallocate(struct file *file, int mode, loff_t offset,
			  loff_t len)
{
	struct shm_file_data *sfd = shm_file_data(file);

	if (!sfd->file->f_op->fallocate)
		return -EOPNOTSUPP;
	return sfd->file->f_op->fallocate(file, mode, offset, len);
}

static unsigned long shm_get_unmapped_area(struct file *file,
	unsigned long addr, unsigned long len, unsigned long pgoff,
	unsigned long flags)
{
	struct shm_file_data *sfd = shm_file_data(file);

	return sfd->file->f_op->get_unmapped_area(sfd->file, addr, len,
						pgoff, flags);
}

static const struct file_operations shm_file_operations = {
	.mmap		= shm_mmap,
	.fsync		= shm_fsync,
	.release	= shm_release,
	.get_unmapped_area	= shm_get_unmapped_area,
	.llseek		= noop_llseek,
	.fallocate	= shm_fallocate,
};

 
static const struct file_operations shm_file_operations_huge = {
	.mmap		= shm_mmap,
	.fsync		= shm_fsync,
	.release	= shm_release,
	.get_unmapped_area	= shm_get_unmapped_area,
	.llseek		= noop_llseek,
	.fallocate	= shm_fallocate,
};

bool is_file_shm_hugepages(struct file *file)
{
	return file->f_op == &shm_file_operations_huge;
}

static const struct vm_operations_struct shm_vm_ops = {
	.open	= shm_open,	 
	.close	= shm_close,	 
	.fault	= shm_fault,
	.may_split = shm_may_split,
	.pagesize = shm_pagesize,
#if defined(CONFIG_NUMA)
	.set_policy = shm_set_policy,
	.get_policy = shm_get_policy,
#endif
};

 
static int newseg(struct ipc_namespace *ns, struct ipc_params *params)
{
	key_t key = params->key;
	int shmflg = params->flg;
	size_t size = params->u.size;
	int error;
	struct shmid_kernel *shp;
	size_t numpages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	struct file *file;
	char name[13];
	vm_flags_t acctflag = 0;

	if (size < SHMMIN || size > ns->shm_ctlmax)
		return -EINVAL;

	if (numpages << PAGE_SHIFT < size)
		return -ENOSPC;

	if (ns->shm_tot + numpages < ns->shm_tot ||
			ns->shm_tot + numpages > ns->shm_ctlall)
		return -ENOSPC;

	shp = kmalloc(sizeof(*shp), GFP_KERNEL_ACCOUNT);
	if (unlikely(!shp))
		return -ENOMEM;

	shp->shm_perm.key = key;
	shp->shm_perm.mode = (shmflg & S_IRWXUGO);
	shp->mlock_ucounts = NULL;

	shp->shm_perm.security = NULL;
	error = security_shm_alloc(&shp->shm_perm);
	if (error) {
		kfree(shp);
		return error;
	}

	sprintf(name, "SYSV%08x", key);
	if (shmflg & SHM_HUGETLB) {
		struct hstate *hs;
		size_t hugesize;

		hs = hstate_sizelog((shmflg >> SHM_HUGE_SHIFT) & SHM_HUGE_MASK);
		if (!hs) {
			error = -EINVAL;
			goto no_file;
		}
		hugesize = ALIGN(size, huge_page_size(hs));

		 
		if (shmflg & SHM_NORESERVE)
			acctflag = VM_NORESERVE;
		file = hugetlb_file_setup(name, hugesize, acctflag,
				HUGETLB_SHMFS_INODE, (shmflg >> SHM_HUGE_SHIFT) & SHM_HUGE_MASK);
	} else {
		 
		if  ((shmflg & SHM_NORESERVE) &&
				sysctl_overcommit_memory != OVERCOMMIT_NEVER)
			acctflag = VM_NORESERVE;
		file = shmem_kernel_file_setup(name, size, acctflag);
	}
	error = PTR_ERR(file);
	if (IS_ERR(file))
		goto no_file;

	shp->shm_cprid = get_pid(task_tgid(current));
	shp->shm_lprid = NULL;
	shp->shm_atim = shp->shm_dtim = 0;
	shp->shm_ctim = ktime_get_real_seconds();
	shp->shm_segsz = size;
	shp->shm_nattch = 0;
	shp->shm_file = file;
	shp->shm_creator = current;

	 
	error = ipc_addid(&shm_ids(ns), &shp->shm_perm, ns->shm_ctlmni);
	if (error < 0)
		goto no_id;

	shp->ns = ns;

	task_lock(current);
	list_add(&shp->shm_clist, &current->sysvshm.shm_clist);
	task_unlock(current);

	 
	file_inode(file)->i_ino = shp->shm_perm.id;

	ns->shm_tot += numpages;
	error = shp->shm_perm.id;

	ipc_unlock_object(&shp->shm_perm);
	rcu_read_unlock();
	return error;

no_id:
	ipc_update_pid(&shp->shm_cprid, NULL);
	ipc_update_pid(&shp->shm_lprid, NULL);
	fput(file);
	ipc_rcu_putref(&shp->shm_perm, shm_rcu_free);
	return error;
no_file:
	call_rcu(&shp->shm_perm.rcu, shm_rcu_free);
	return error;
}

 
static int shm_more_checks(struct kern_ipc_perm *ipcp, struct ipc_params *params)
{
	struct shmid_kernel *shp;

	shp = container_of(ipcp, struct shmid_kernel, shm_perm);
	if (shp->shm_segsz < params->u.size)
		return -EINVAL;

	return 0;
}

long ksys_shmget(key_t key, size_t size, int shmflg)
{
	struct ipc_namespace *ns;
	static const struct ipc_ops shm_ops = {
		.getnew = newseg,
		.associate = security_shm_associate,
		.more_checks = shm_more_checks,
	};
	struct ipc_params shm_params;

	ns = current->nsproxy->ipc_ns;

	shm_params.key = key;
	shm_params.flg = shmflg;
	shm_params.u.size = size;

	return ipcget(ns, &shm_ids(ns), &shm_ops, &shm_params);
}

SYSCALL_DEFINE3(shmget, key_t, key, size_t, size, int, shmflg)
{
	return ksys_shmget(key, size, shmflg);
}

static inline unsigned long copy_shmid_to_user(void __user *buf, struct shmid64_ds *in, int version)
{
	switch (version) {
	case IPC_64:
		return copy_to_user(buf, in, sizeof(*in));
	case IPC_OLD:
	    {
		struct shmid_ds out;

		memset(&out, 0, sizeof(out));
		ipc64_perm_to_ipc_perm(&in->shm_perm, &out.shm_perm);
		out.shm_segsz	= in->shm_segsz;
		out.shm_atime	= in->shm_atime;
		out.shm_dtime	= in->shm_dtime;
		out.shm_ctime	= in->shm_ctime;
		out.shm_cpid	= in->shm_cpid;
		out.shm_lpid	= in->shm_lpid;
		out.shm_nattch	= in->shm_nattch;

		return copy_to_user(buf, &out, sizeof(out));
	    }
	default:
		return -EINVAL;
	}
}

static inline unsigned long
copy_shmid_from_user(struct shmid64_ds *out, void __user *buf, int version)
{
	switch (version) {
	case IPC_64:
		if (copy_from_user(out, buf, sizeof(*out)))
			return -EFAULT;
		return 0;
	case IPC_OLD:
	    {
		struct shmid_ds tbuf_old;

		if (copy_from_user(&tbuf_old, buf, sizeof(tbuf_old)))
			return -EFAULT;

		out->shm_perm.uid	= tbuf_old.shm_perm.uid;
		out->shm_perm.gid	= tbuf_old.shm_perm.gid;
		out->shm_perm.mode	= tbuf_old.shm_perm.mode;

		return 0;
	    }
	default:
		return -EINVAL;
	}
}

static inline unsigned long copy_shminfo_to_user(void __user *buf, struct shminfo64 *in, int version)
{
	switch (version) {
	case IPC_64:
		return copy_to_user(buf, in, sizeof(*in));
	case IPC_OLD:
	    {
		struct shminfo out;

		if (in->shmmax > INT_MAX)
			out.shmmax = INT_MAX;
		else
			out.shmmax = (int)in->shmmax;

		out.shmmin	= in->shmmin;
		out.shmmni	= in->shmmni;
		out.shmseg	= in->shmseg;
		out.shmall	= in->shmall;

		return copy_to_user(buf, &out, sizeof(out));
	    }
	default:
		return -EINVAL;
	}
}

 
static void shm_add_rss_swap(struct shmid_kernel *shp,
	unsigned long *rss_add, unsigned long *swp_add)
{
	struct inode *inode;

	inode = file_inode(shp->shm_file);

	if (is_file_hugepages(shp->shm_file)) {
		struct address_space *mapping = inode->i_mapping;
		struct hstate *h = hstate_file(shp->shm_file);
		*rss_add += pages_per_huge_page(h) * mapping->nrpages;
	} else {
#ifdef CONFIG_SHMEM
		struct shmem_inode_info *info = SHMEM_I(inode);

		spin_lock_irq(&info->lock);
		*rss_add += inode->i_mapping->nrpages;
		*swp_add += info->swapped;
		spin_unlock_irq(&info->lock);
#else
		*rss_add += inode->i_mapping->nrpages;
#endif
	}
}

 
static void shm_get_stat(struct ipc_namespace *ns, unsigned long *rss,
		unsigned long *swp)
{
	int next_id;
	int total, in_use;

	*rss = 0;
	*swp = 0;

	in_use = shm_ids(ns).in_use;

	for (total = 0, next_id = 0; total < in_use; next_id++) {
		struct kern_ipc_perm *ipc;
		struct shmid_kernel *shp;

		ipc = idr_find(&shm_ids(ns).ipcs_idr, next_id);
		if (ipc == NULL)
			continue;
		shp = container_of(ipc, struct shmid_kernel, shm_perm);

		shm_add_rss_swap(shp, rss, swp);

		total++;
	}
}

 
static int shmctl_down(struct ipc_namespace *ns, int shmid, int cmd,
		       struct shmid64_ds *shmid64)
{
	struct kern_ipc_perm *ipcp;
	struct shmid_kernel *shp;
	int err;

	down_write(&shm_ids(ns).rwsem);
	rcu_read_lock();

	ipcp = ipcctl_obtain_check(ns, &shm_ids(ns), shmid, cmd,
				      &shmid64->shm_perm, 0);
	if (IS_ERR(ipcp)) {
		err = PTR_ERR(ipcp);
		goto out_unlock1;
	}

	shp = container_of(ipcp, struct shmid_kernel, shm_perm);

	err = security_shm_shmctl(&shp->shm_perm, cmd);
	if (err)
		goto out_unlock1;

	switch (cmd) {
	case IPC_RMID:
		ipc_lock_object(&shp->shm_perm);
		 
		do_shm_rmid(ns, ipcp);
		goto out_up;
	case IPC_SET:
		ipc_lock_object(&shp->shm_perm);
		err = ipc_update_perm(&shmid64->shm_perm, ipcp);
		if (err)
			goto out_unlock0;
		shp->shm_ctim = ktime_get_real_seconds();
		break;
	default:
		err = -EINVAL;
		goto out_unlock1;
	}

out_unlock0:
	ipc_unlock_object(&shp->shm_perm);
out_unlock1:
	rcu_read_unlock();
out_up:
	up_write(&shm_ids(ns).rwsem);
	return err;
}

static int shmctl_ipc_info(struct ipc_namespace *ns,
			   struct shminfo64 *shminfo)
{
	int err = security_shm_shmctl(NULL, IPC_INFO);
	if (!err) {
		memset(shminfo, 0, sizeof(*shminfo));
		shminfo->shmmni = shminfo->shmseg = ns->shm_ctlmni;
		shminfo->shmmax = ns->shm_ctlmax;
		shminfo->shmall = ns->shm_ctlall;
		shminfo->shmmin = SHMMIN;
		down_read(&shm_ids(ns).rwsem);
		err = ipc_get_maxidx(&shm_ids(ns));
		up_read(&shm_ids(ns).rwsem);
		if (err < 0)
			err = 0;
	}
	return err;
}

static int shmctl_shm_info(struct ipc_namespace *ns,
			   struct shm_info *shm_info)
{
	int err = security_shm_shmctl(NULL, SHM_INFO);
	if (!err) {
		memset(shm_info, 0, sizeof(*shm_info));
		down_read(&shm_ids(ns).rwsem);
		shm_info->used_ids = shm_ids(ns).in_use;
		shm_get_stat(ns, &shm_info->shm_rss, &shm_info->shm_swp);
		shm_info->shm_tot = ns->shm_tot;
		shm_info->swap_attempts = 0;
		shm_info->swap_successes = 0;
		err = ipc_get_maxidx(&shm_ids(ns));
		up_read(&shm_ids(ns).rwsem);
		if (err < 0)
			err = 0;
	}
	return err;
}

static int shmctl_stat(struct ipc_namespace *ns, int shmid,
			int cmd, struct shmid64_ds *tbuf)
{
	struct shmid_kernel *shp;
	int err;

	memset(tbuf, 0, sizeof(*tbuf));

	rcu_read_lock();
	if (cmd == SHM_STAT || cmd == SHM_STAT_ANY) {
		shp = shm_obtain_object(ns, shmid);
		if (IS_ERR(shp)) {
			err = PTR_ERR(shp);
			goto out_unlock;
		}
	} else {  
		shp = shm_obtain_object_check(ns, shmid);
		if (IS_ERR(shp)) {
			err = PTR_ERR(shp);
			goto out_unlock;
		}
	}

	 
	if (cmd == SHM_STAT_ANY)
		audit_ipc_obj(&shp->shm_perm);
	else {
		err = -EACCES;
		if (ipcperms(ns, &shp->shm_perm, S_IRUGO))
			goto out_unlock;
	}

	err = security_shm_shmctl(&shp->shm_perm, cmd);
	if (err)
		goto out_unlock;

	ipc_lock_object(&shp->shm_perm);

	if (!ipc_valid_object(&shp->shm_perm)) {
		ipc_unlock_object(&shp->shm_perm);
		err = -EIDRM;
		goto out_unlock;
	}

	kernel_to_ipc64_perm(&shp->shm_perm, &tbuf->shm_perm);
	tbuf->shm_segsz	= shp->shm_segsz;
	tbuf->shm_atime	= shp->shm_atim;
	tbuf->shm_dtime	= shp->shm_dtim;
	tbuf->shm_ctime	= shp->shm_ctim;
#ifndef CONFIG_64BIT
	tbuf->shm_atime_high = shp->shm_atim >> 32;
	tbuf->shm_dtime_high = shp->shm_dtim >> 32;
	tbuf->shm_ctime_high = shp->shm_ctim >> 32;
#endif
	tbuf->shm_cpid	= pid_vnr(shp->shm_cprid);
	tbuf->shm_lpid	= pid_vnr(shp->shm_lprid);
	tbuf->shm_nattch = shp->shm_nattch;

	if (cmd == IPC_STAT) {
		 
		err = 0;
	} else {
		 
		err = shp->shm_perm.id;
	}

	ipc_unlock_object(&shp->shm_perm);
out_unlock:
	rcu_read_unlock();
	return err;
}

static int shmctl_do_lock(struct ipc_namespace *ns, int shmid, int cmd)
{
	struct shmid_kernel *shp;
	struct file *shm_file;
	int err;

	rcu_read_lock();
	shp = shm_obtain_object_check(ns, shmid);
	if (IS_ERR(shp)) {
		err = PTR_ERR(shp);
		goto out_unlock1;
	}

	audit_ipc_obj(&(shp->shm_perm));
	err = security_shm_shmctl(&shp->shm_perm, cmd);
	if (err)
		goto out_unlock1;

	ipc_lock_object(&shp->shm_perm);

	 
	if (!ipc_valid_object(&shp->shm_perm)) {
		err = -EIDRM;
		goto out_unlock0;
	}

	if (!ns_capable(ns->user_ns, CAP_IPC_LOCK)) {
		kuid_t euid = current_euid();

		if (!uid_eq(euid, shp->shm_perm.uid) &&
		    !uid_eq(euid, shp->shm_perm.cuid)) {
			err = -EPERM;
			goto out_unlock0;
		}
		if (cmd == SHM_LOCK && !rlimit(RLIMIT_MEMLOCK)) {
			err = -EPERM;
			goto out_unlock0;
		}
	}

	shm_file = shp->shm_file;
	if (is_file_hugepages(shm_file))
		goto out_unlock0;

	if (cmd == SHM_LOCK) {
		struct ucounts *ucounts = current_ucounts();

		err = shmem_lock(shm_file, 1, ucounts);
		if (!err && !(shp->shm_perm.mode & SHM_LOCKED)) {
			shp->shm_perm.mode |= SHM_LOCKED;
			shp->mlock_ucounts = ucounts;
		}
		goto out_unlock0;
	}

	 
	if (!(shp->shm_perm.mode & SHM_LOCKED))
		goto out_unlock0;
	shmem_lock(shm_file, 0, shp->mlock_ucounts);
	shp->shm_perm.mode &= ~SHM_LOCKED;
	shp->mlock_ucounts = NULL;
	get_file(shm_file);
	ipc_unlock_object(&shp->shm_perm);
	rcu_read_unlock();
	shmem_unlock_mapping(shm_file->f_mapping);

	fput(shm_file);
	return err;

out_unlock0:
	ipc_unlock_object(&shp->shm_perm);
out_unlock1:
	rcu_read_unlock();
	return err;
}

static long ksys_shmctl(int shmid, int cmd, struct shmid_ds __user *buf, int version)
{
	int err;
	struct ipc_namespace *ns;
	struct shmid64_ds sem64;

	if (cmd < 0 || shmid < 0)
		return -EINVAL;

	ns = current->nsproxy->ipc_ns;

	switch (cmd) {
	case IPC_INFO: {
		struct shminfo64 shminfo;
		err = shmctl_ipc_info(ns, &shminfo);
		if (err < 0)
			return err;
		if (copy_shminfo_to_user(buf, &shminfo, version))
			err = -EFAULT;
		return err;
	}
	case SHM_INFO: {
		struct shm_info shm_info;
		err = shmctl_shm_info(ns, &shm_info);
		if (err < 0)
			return err;
		if (copy_to_user(buf, &shm_info, sizeof(shm_info)))
			err = -EFAULT;
		return err;
	}
	case SHM_STAT:
	case SHM_STAT_ANY:
	case IPC_STAT: {
		err = shmctl_stat(ns, shmid, cmd, &sem64);
		if (err < 0)
			return err;
		if (copy_shmid_to_user(buf, &sem64, version))
			err = -EFAULT;
		return err;
	}
	case IPC_SET:
		if (copy_shmid_from_user(&sem64, buf, version))
			return -EFAULT;
		fallthrough;
	case IPC_RMID:
		return shmctl_down(ns, shmid, cmd, &sem64);
	case SHM_LOCK:
	case SHM_UNLOCK:
		return shmctl_do_lock(ns, shmid, cmd);
	default:
		return -EINVAL;
	}
}

SYSCALL_DEFINE3(shmctl, int, shmid, int, cmd, struct shmid_ds __user *, buf)
{
	return ksys_shmctl(shmid, cmd, buf, IPC_64);
}

#ifdef CONFIG_ARCH_WANT_IPC_PARSE_VERSION
long ksys_old_shmctl(int shmid, int cmd, struct shmid_ds __user *buf)
{
	int version = ipc_parse_version(&cmd);

	return ksys_shmctl(shmid, cmd, buf, version);
}

SYSCALL_DEFINE3(old_shmctl, int, shmid, int, cmd, struct shmid_ds __user *, buf)
{
	return ksys_old_shmctl(shmid, cmd, buf);
}
#endif

#ifdef CONFIG_COMPAT

struct compat_shmid_ds {
	struct compat_ipc_perm shm_perm;
	int shm_segsz;
	old_time32_t shm_atime;
	old_time32_t shm_dtime;
	old_time32_t shm_ctime;
	compat_ipc_pid_t shm_cpid;
	compat_ipc_pid_t shm_lpid;
	unsigned short shm_nattch;
	unsigned short shm_unused;
	compat_uptr_t shm_unused2;
	compat_uptr_t shm_unused3;
};

struct compat_shminfo64 {
	compat_ulong_t shmmax;
	compat_ulong_t shmmin;
	compat_ulong_t shmmni;
	compat_ulong_t shmseg;
	compat_ulong_t shmall;
	compat_ulong_t __unused1;
	compat_ulong_t __unused2;
	compat_ulong_t __unused3;
	compat_ulong_t __unused4;
};

struct compat_shm_info {
	compat_int_t used_ids;
	compat_ulong_t shm_tot, shm_rss, shm_swp;
	compat_ulong_t swap_attempts, swap_successes;
};

static int copy_compat_shminfo_to_user(void __user *buf, struct shminfo64 *in,
					int version)
{
	if (in->shmmax > INT_MAX)
		in->shmmax = INT_MAX;
	if (version == IPC_64) {
		struct compat_shminfo64 info;
		memset(&info, 0, sizeof(info));
		info.shmmax = in->shmmax;
		info.shmmin = in->shmmin;
		info.shmmni = in->shmmni;
		info.shmseg = in->shmseg;
		info.shmall = in->shmall;
		return copy_to_user(buf, &info, sizeof(info));
	} else {
		struct shminfo info;
		memset(&info, 0, sizeof(info));
		info.shmmax = in->shmmax;
		info.shmmin = in->shmmin;
		info.shmmni = in->shmmni;
		info.shmseg = in->shmseg;
		info.shmall = in->shmall;
		return copy_to_user(buf, &info, sizeof(info));
	}
}

static int put_compat_shm_info(struct shm_info *ip,
				struct compat_shm_info __user *uip)
{
	struct compat_shm_info info;

	memset(&info, 0, sizeof(info));
	info.used_ids = ip->used_ids;
	info.shm_tot = ip->shm_tot;
	info.shm_rss = ip->shm_rss;
	info.shm_swp = ip->shm_swp;
	info.swap_attempts = ip->swap_attempts;
	info.swap_successes = ip->swap_successes;
	return copy_to_user(uip, &info, sizeof(info));
}

static int copy_compat_shmid_to_user(void __user *buf, struct shmid64_ds *in,
					int version)
{
	if (version == IPC_64) {
		struct compat_shmid64_ds v;
		memset(&v, 0, sizeof(v));
		to_compat_ipc64_perm(&v.shm_perm, &in->shm_perm);
		v.shm_atime	 = lower_32_bits(in->shm_atime);
		v.shm_atime_high = upper_32_bits(in->shm_atime);
		v.shm_dtime	 = lower_32_bits(in->shm_dtime);
		v.shm_dtime_high = upper_32_bits(in->shm_dtime);
		v.shm_ctime	 = lower_32_bits(in->shm_ctime);
		v.shm_ctime_high = upper_32_bits(in->shm_ctime);
		v.shm_segsz = in->shm_segsz;
		v.shm_nattch = in->shm_nattch;
		v.shm_cpid = in->shm_cpid;
		v.shm_lpid = in->shm_lpid;
		return copy_to_user(buf, &v, sizeof(v));
	} else {
		struct compat_shmid_ds v;
		memset(&v, 0, sizeof(v));
		to_compat_ipc_perm(&v.shm_perm, &in->shm_perm);
		v.shm_perm.key = in->shm_perm.key;
		v.shm_atime = in->shm_atime;
		v.shm_dtime = in->shm_dtime;
		v.shm_ctime = in->shm_ctime;
		v.shm_segsz = in->shm_segsz;
		v.shm_nattch = in->shm_nattch;
		v.shm_cpid = in->shm_cpid;
		v.shm_lpid = in->shm_lpid;
		return copy_to_user(buf, &v, sizeof(v));
	}
}

static int copy_compat_shmid_from_user(struct shmid64_ds *out, void __user *buf,
					int version)
{
	memset(out, 0, sizeof(*out));
	if (version == IPC_64) {
		struct compat_shmid64_ds __user *p = buf;
		return get_compat_ipc64_perm(&out->shm_perm, &p->shm_perm);
	} else {
		struct compat_shmid_ds __user *p = buf;
		return get_compat_ipc_perm(&out->shm_perm, &p->shm_perm);
	}
}

static long compat_ksys_shmctl(int shmid, int cmd, void __user *uptr, int version)
{
	struct ipc_namespace *ns;
	struct shmid64_ds sem64;
	int err;

	ns = current->nsproxy->ipc_ns;

	if (cmd < 0 || shmid < 0)
		return -EINVAL;

	switch (cmd) {
	case IPC_INFO: {
		struct shminfo64 shminfo;
		err = shmctl_ipc_info(ns, &shminfo);
		if (err < 0)
			return err;
		if (copy_compat_shminfo_to_user(uptr, &shminfo, version))
			err = -EFAULT;
		return err;
	}
	case SHM_INFO: {
		struct shm_info shm_info;
		err = shmctl_shm_info(ns, &shm_info);
		if (err < 0)
			return err;
		if (put_compat_shm_info(&shm_info, uptr))
			err = -EFAULT;
		return err;
	}
	case IPC_STAT:
	case SHM_STAT_ANY:
	case SHM_STAT:
		err = shmctl_stat(ns, shmid, cmd, &sem64);
		if (err < 0)
			return err;
		if (copy_compat_shmid_to_user(uptr, &sem64, version))
			err = -EFAULT;
		return err;

	case IPC_SET:
		if (copy_compat_shmid_from_user(&sem64, uptr, version))
			return -EFAULT;
		fallthrough;
	case IPC_RMID:
		return shmctl_down(ns, shmid, cmd, &sem64);
	case SHM_LOCK:
	case SHM_UNLOCK:
		return shmctl_do_lock(ns, shmid, cmd);
	default:
		return -EINVAL;
	}
	return err;
}

COMPAT_SYSCALL_DEFINE3(shmctl, int, shmid, int, cmd, void __user *, uptr)
{
	return compat_ksys_shmctl(shmid, cmd, uptr, IPC_64);
}

#ifdef CONFIG_ARCH_WANT_COMPAT_IPC_PARSE_VERSION
long compat_ksys_old_shmctl(int shmid, int cmd, void __user *uptr)
{
	int version = compat_ipc_parse_version(&cmd);

	return compat_ksys_shmctl(shmid, cmd, uptr, version);
}

COMPAT_SYSCALL_DEFINE3(old_shmctl, int, shmid, int, cmd, void __user *, uptr)
{
	return compat_ksys_old_shmctl(shmid, cmd, uptr);
}
#endif
#endif

 
long do_shmat(int shmid, char __user *shmaddr, int shmflg,
	      ulong *raddr, unsigned long shmlba)
{
	struct shmid_kernel *shp;
	unsigned long addr = (unsigned long)shmaddr;
	unsigned long size;
	struct file *file, *base;
	int    err;
	unsigned long flags = MAP_SHARED;
	unsigned long prot;
	int acc_mode;
	struct ipc_namespace *ns;
	struct shm_file_data *sfd;
	int f_flags;
	unsigned long populate = 0;

	err = -EINVAL;
	if (shmid < 0)
		goto out;

	if (addr) {
		if (addr & (shmlba - 1)) {
			if (shmflg & SHM_RND) {
				addr &= ~(shmlba - 1);   

				 
				if (!addr && (shmflg & SHM_REMAP))
					goto out;
			} else
#ifndef __ARCH_FORCE_SHMLBA
				if (addr & ~PAGE_MASK)
#endif
					goto out;
		}

		flags |= MAP_FIXED;
	} else if ((shmflg & SHM_REMAP))
		goto out;

	if (shmflg & SHM_RDONLY) {
		prot = PROT_READ;
		acc_mode = S_IRUGO;
		f_flags = O_RDONLY;
	} else {
		prot = PROT_READ | PROT_WRITE;
		acc_mode = S_IRUGO | S_IWUGO;
		f_flags = O_RDWR;
	}
	if (shmflg & SHM_EXEC) {
		prot |= PROT_EXEC;
		acc_mode |= S_IXUGO;
	}

	 
	ns = current->nsproxy->ipc_ns;
	rcu_read_lock();
	shp = shm_obtain_object_check(ns, shmid);
	if (IS_ERR(shp)) {
		err = PTR_ERR(shp);
		goto out_unlock;
	}

	err = -EACCES;
	if (ipcperms(ns, &shp->shm_perm, acc_mode))
		goto out_unlock;

	err = security_shm_shmat(&shp->shm_perm, shmaddr, shmflg);
	if (err)
		goto out_unlock;

	ipc_lock_object(&shp->shm_perm);

	 
	if (!ipc_valid_object(&shp->shm_perm)) {
		ipc_unlock_object(&shp->shm_perm);
		err = -EIDRM;
		goto out_unlock;
	}

	 
	base = get_file(shp->shm_file);
	shp->shm_nattch++;
	size = i_size_read(file_inode(base));
	ipc_unlock_object(&shp->shm_perm);
	rcu_read_unlock();

	err = -ENOMEM;
	sfd = kzalloc(sizeof(*sfd), GFP_KERNEL);
	if (!sfd) {
		fput(base);
		goto out_nattch;
	}

	file = alloc_file_clone(base, f_flags,
			  is_file_hugepages(base) ?
				&shm_file_operations_huge :
				&shm_file_operations);
	err = PTR_ERR(file);
	if (IS_ERR(file)) {
		kfree(sfd);
		fput(base);
		goto out_nattch;
	}

	sfd->id = shp->shm_perm.id;
	sfd->ns = get_ipc_ns(ns);
	sfd->file = base;
	sfd->vm_ops = NULL;
	file->private_data = sfd;

	err = security_mmap_file(file, prot, flags);
	if (err)
		goto out_fput;

	if (mmap_write_lock_killable(current->mm)) {
		err = -EINTR;
		goto out_fput;
	}

	if (addr && !(shmflg & SHM_REMAP)) {
		err = -EINVAL;
		if (addr + size < addr)
			goto invalid;

		if (find_vma_intersection(current->mm, addr, addr + size))
			goto invalid;
	}

	addr = do_mmap(file, addr, size, prot, flags, 0, 0, &populate, NULL);
	*raddr = addr;
	err = 0;
	if (IS_ERR_VALUE(addr))
		err = (long)addr;
invalid:
	mmap_write_unlock(current->mm);
	if (populate)
		mm_populate(addr, populate);

out_fput:
	fput(file);

out_nattch:
	down_write(&shm_ids(ns).rwsem);
	shp = shm_lock(ns, shmid);
	shp->shm_nattch--;

	if (shm_may_destroy(shp))
		shm_destroy(ns, shp);
	else
		shm_unlock(shp);
	up_write(&shm_ids(ns).rwsem);
	return err;

out_unlock:
	rcu_read_unlock();
out:
	return err;
}

SYSCALL_DEFINE3(shmat, int, shmid, char __user *, shmaddr, int, shmflg)
{
	unsigned long ret;
	long err;

	err = do_shmat(shmid, shmaddr, shmflg, &ret, SHMLBA);
	if (err)
		return err;
	force_successful_syscall_return();
	return (long)ret;
}

#ifdef CONFIG_COMPAT

#ifndef COMPAT_SHMLBA
#define COMPAT_SHMLBA	SHMLBA
#endif

COMPAT_SYSCALL_DEFINE3(shmat, int, shmid, compat_uptr_t, shmaddr, int, shmflg)
{
	unsigned long ret;
	long err;

	err = do_shmat(shmid, compat_ptr(shmaddr), shmflg, &ret, COMPAT_SHMLBA);
	if (err)
		return err;
	force_successful_syscall_return();
	return (long)ret;
}
#endif

 
long ksys_shmdt(char __user *shmaddr)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	unsigned long addr = (unsigned long)shmaddr;
	int retval = -EINVAL;
#ifdef CONFIG_MMU
	loff_t size = 0;
	struct file *file;
	VMA_ITERATOR(vmi, mm, addr);
#endif

	if (addr & ~PAGE_MASK)
		return retval;

	if (mmap_write_lock_killable(mm))
		return -EINTR;

	 
	 

#ifdef CONFIG_MMU
	for_each_vma(vmi, vma) {
		 
		if ((vma->vm_ops == &shm_vm_ops) &&
			(vma->vm_start - addr)/PAGE_SIZE == vma->vm_pgoff) {

			 
			file = vma->vm_file;
			size = i_size_read(file_inode(vma->vm_file));
			do_vma_munmap(&vmi, vma, vma->vm_start, vma->vm_end,
				      NULL, false);
			 
			retval = 0;
			vma = vma_next(&vmi);
			break;
		}
	}

	 
	size = PAGE_ALIGN(size);
	while (vma && (loff_t)(vma->vm_end - addr) <= size) {
		 
		if ((vma->vm_ops == &shm_vm_ops) &&
		    ((vma->vm_start - addr)/PAGE_SIZE == vma->vm_pgoff) &&
		    (vma->vm_file == file)) {
			do_vma_munmap(&vmi, vma, vma->vm_start, vma->vm_end,
				      NULL, false);
		}

		vma = vma_next(&vmi);
	}

#else	 
	vma = vma_lookup(mm, addr);
	 
	if (vma && vma->vm_start == addr && vma->vm_ops == &shm_vm_ops) {
		do_munmap(mm, vma->vm_start, vma->vm_end - vma->vm_start, NULL);
		retval = 0;
	}

#endif

	mmap_write_unlock(mm);
	return retval;
}

SYSCALL_DEFINE1(shmdt, char __user *, shmaddr)
{
	return ksys_shmdt(shmaddr);
}

#ifdef CONFIG_PROC_FS
static int sysvipc_shm_proc_show(struct seq_file *s, void *it)
{
	struct pid_namespace *pid_ns = ipc_seq_pid_ns(s);
	struct user_namespace *user_ns = seq_user_ns(s);
	struct kern_ipc_perm *ipcp = it;
	struct shmid_kernel *shp;
	unsigned long rss = 0, swp = 0;

	shp = container_of(ipcp, struct shmid_kernel, shm_perm);
	shm_add_rss_swap(shp, &rss, &swp);

#if BITS_PER_LONG <= 32
#define SIZE_SPEC "%10lu"
#else
#define SIZE_SPEC "%21lu"
#endif

	seq_printf(s,
		   "%10d %10d  %4o " SIZE_SPEC " %5u %5u  "
		   "%5lu %5u %5u %5u %5u %10llu %10llu %10llu "
		   SIZE_SPEC " " SIZE_SPEC "\n",
		   shp->shm_perm.key,
		   shp->shm_perm.id,
		   shp->shm_perm.mode,
		   shp->shm_segsz,
		   pid_nr_ns(shp->shm_cprid, pid_ns),
		   pid_nr_ns(shp->shm_lprid, pid_ns),
		   shp->shm_nattch,
		   from_kuid_munged(user_ns, shp->shm_perm.uid),
		   from_kgid_munged(user_ns, shp->shm_perm.gid),
		   from_kuid_munged(user_ns, shp->shm_perm.cuid),
		   from_kgid_munged(user_ns, shp->shm_perm.cgid),
		   shp->shm_atim,
		   shp->shm_dtim,
		   shp->shm_ctim,
		   rss * PAGE_SIZE,
		   swp * PAGE_SIZE);

	return 0;
}
#endif
