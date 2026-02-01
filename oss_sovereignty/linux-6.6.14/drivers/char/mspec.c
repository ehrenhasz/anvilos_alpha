
 

 

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/numa.h>
#include <linux/refcount.h>
#include <asm/page.h>
#include <linux/atomic.h>
#include <asm/tlbflush.h>
#include <asm/uncached.h>


#define CACHED_ID	"Cached,"
#define UNCACHED_ID	"Uncached"
#define REVISION	"4.0"
#define MSPEC_BASENAME	"mspec"

 
enum mspec_page_type {
	MSPEC_CACHED = 2,
	MSPEC_UNCACHED
};

 
struct vma_data {
	refcount_t refcnt;	 
	spinlock_t lock;	 
	int count;		 
	enum mspec_page_type type;  
	unsigned long vm_start;	 
	unsigned long vm_end;	 
	unsigned long maddr[];	 
};

 
static void
mspec_open(struct vm_area_struct *vma)
{
	struct vma_data *vdata;

	vdata = vma->vm_private_data;
	refcount_inc(&vdata->refcnt);
}

 
static void
mspec_close(struct vm_area_struct *vma)
{
	struct vma_data *vdata;
	int index, last_index;
	unsigned long my_page;

	vdata = vma->vm_private_data;

	if (!refcount_dec_and_test(&vdata->refcnt))
		return;

	last_index = (vdata->vm_end - vdata->vm_start) >> PAGE_SHIFT;
	for (index = 0; index < last_index; index++) {
		if (vdata->maddr[index] == 0)
			continue;
		 
		my_page = vdata->maddr[index];
		vdata->maddr[index] = 0;
		memset((char *)my_page, 0, PAGE_SIZE);
		uncached_free_page(my_page, 1);
	}

	kvfree(vdata);
}

 
static vm_fault_t
mspec_fault(struct vm_fault *vmf)
{
	unsigned long paddr, maddr;
	unsigned long pfn;
	pgoff_t index = vmf->pgoff;
	struct vma_data *vdata = vmf->vma->vm_private_data;

	maddr = (volatile unsigned long) vdata->maddr[index];
	if (maddr == 0) {
		maddr = uncached_alloc_page(numa_node_id(), 1);
		if (maddr == 0)
			return VM_FAULT_OOM;

		spin_lock(&vdata->lock);
		if (vdata->maddr[index] == 0) {
			vdata->count++;
			vdata->maddr[index] = maddr;
		} else {
			uncached_free_page(maddr, 1);
			maddr = vdata->maddr[index];
		}
		spin_unlock(&vdata->lock);
	}

	paddr = maddr & ~__IA64_UNCACHED_OFFSET;
	pfn = paddr >> PAGE_SHIFT;

	return vmf_insert_pfn(vmf->vma, vmf->address, pfn);
}

static const struct vm_operations_struct mspec_vm_ops = {
	.open = mspec_open,
	.close = mspec_close,
	.fault = mspec_fault,
};

 
static int
mspec_mmap(struct file *file, struct vm_area_struct *vma,
					enum mspec_page_type type)
{
	struct vma_data *vdata;
	int pages, vdata_size;

	if (vma->vm_pgoff != 0)
		return -EINVAL;

	if ((vma->vm_flags & VM_SHARED) == 0)
		return -EINVAL;

	if ((vma->vm_flags & VM_WRITE) == 0)
		return -EPERM;

	pages = vma_pages(vma);
	vdata_size = sizeof(struct vma_data) + pages * sizeof(long);
	vdata = kvzalloc(vdata_size, GFP_KERNEL);
	if (!vdata)
		return -ENOMEM;

	vdata->vm_start = vma->vm_start;
	vdata->vm_end = vma->vm_end;
	vdata->type = type;
	spin_lock_init(&vdata->lock);
	refcount_set(&vdata->refcnt, 1);
	vma->vm_private_data = vdata;

	vm_flags_set(vma, VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP);
	if (vdata->type == MSPEC_UNCACHED)
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_ops = &mspec_vm_ops;

	return 0;
}

static int
cached_mmap(struct file *file, struct vm_area_struct *vma)
{
	return mspec_mmap(file, vma, MSPEC_CACHED);
}

static int
uncached_mmap(struct file *file, struct vm_area_struct *vma)
{
	return mspec_mmap(file, vma, MSPEC_UNCACHED);
}

static const struct file_operations cached_fops = {
	.owner = THIS_MODULE,
	.mmap = cached_mmap,
	.llseek = noop_llseek,
};

static struct miscdevice cached_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mspec_cached",
	.fops = &cached_fops
};

static const struct file_operations uncached_fops = {
	.owner = THIS_MODULE,
	.mmap = uncached_mmap,
	.llseek = noop_llseek,
};

static struct miscdevice uncached_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mspec_uncached",
	.fops = &uncached_fops
};

 
static int __init
mspec_init(void)
{
	int ret;

	ret = misc_register(&cached_miscdev);
	if (ret) {
		printk(KERN_ERR "%s: failed to register device %i\n",
		       CACHED_ID, ret);
		return ret;
	}
	ret = misc_register(&uncached_miscdev);
	if (ret) {
		printk(KERN_ERR "%s: failed to register device %i\n",
		       UNCACHED_ID, ret);
		misc_deregister(&cached_miscdev);
		return ret;
	}

	printk(KERN_INFO "%s %s initialized devices: %s %s\n",
	       MSPEC_BASENAME, REVISION, CACHED_ID, UNCACHED_ID);

	return 0;
}

static void __exit
mspec_exit(void)
{
	misc_deregister(&uncached_miscdev);
	misc_deregister(&cached_miscdev);
}

module_init(mspec_init);
module_exit(mspec_exit);

MODULE_AUTHOR("Silicon Graphics, Inc. <linux-altix@sgi.com>");
MODULE_DESCRIPTION("Driver for SGI SN special memory operations");
MODULE_LICENSE("GPL");
