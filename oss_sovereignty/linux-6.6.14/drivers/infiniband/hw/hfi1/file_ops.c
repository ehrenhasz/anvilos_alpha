
 

#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <linux/sched/mm.h>
#include <linux/bitmap.h>

#include <rdma/ib.h>

#include "hfi.h"
#include "pio.h"
#include "device.h"
#include "common.h"
#include "trace.h"
#include "mmu_rb.h"
#include "user_sdma.h"
#include "user_exp_rcv.h"
#include "aspm.h"

#undef pr_fmt
#define pr_fmt(fmt) DRIVER_NAME ": " fmt

#define SEND_CTXT_HALT_TIMEOUT 1000  

 
static int hfi1_file_open(struct inode *inode, struct file *fp);
static int hfi1_file_close(struct inode *inode, struct file *fp);
static ssize_t hfi1_write_iter(struct kiocb *kiocb, struct iov_iter *from);
static __poll_t hfi1_poll(struct file *fp, struct poll_table_struct *pt);
static int hfi1_file_mmap(struct file *fp, struct vm_area_struct *vma);

static u64 kvirt_to_phys(void *addr);
static int assign_ctxt(struct hfi1_filedata *fd, unsigned long arg, u32 len);
static void init_subctxts(struct hfi1_ctxtdata *uctxt,
			  const struct hfi1_user_info *uinfo);
static int init_user_ctxt(struct hfi1_filedata *fd,
			  struct hfi1_ctxtdata *uctxt);
static void user_init(struct hfi1_ctxtdata *uctxt);
static int get_ctxt_info(struct hfi1_filedata *fd, unsigned long arg, u32 len);
static int get_base_info(struct hfi1_filedata *fd, unsigned long arg, u32 len);
static int user_exp_rcv_setup(struct hfi1_filedata *fd, unsigned long arg,
			      u32 len);
static int user_exp_rcv_clear(struct hfi1_filedata *fd, unsigned long arg,
			      u32 len);
static int user_exp_rcv_invalid(struct hfi1_filedata *fd, unsigned long arg,
				u32 len);
static int setup_base_ctxt(struct hfi1_filedata *fd,
			   struct hfi1_ctxtdata *uctxt);
static int setup_subctxt(struct hfi1_ctxtdata *uctxt);

static int find_sub_ctxt(struct hfi1_filedata *fd,
			 const struct hfi1_user_info *uinfo);
static int allocate_ctxt(struct hfi1_filedata *fd, struct hfi1_devdata *dd,
			 struct hfi1_user_info *uinfo,
			 struct hfi1_ctxtdata **cd);
static void deallocate_ctxt(struct hfi1_ctxtdata *uctxt);
static __poll_t poll_urgent(struct file *fp, struct poll_table_struct *pt);
static __poll_t poll_next(struct file *fp, struct poll_table_struct *pt);
static int user_event_ack(struct hfi1_ctxtdata *uctxt, u16 subctxt,
			  unsigned long arg);
static int set_ctxt_pkey(struct hfi1_ctxtdata *uctxt, unsigned long arg);
static int ctxt_reset(struct hfi1_ctxtdata *uctxt);
static int manage_rcvq(struct hfi1_ctxtdata *uctxt, u16 subctxt,
		       unsigned long arg);
static vm_fault_t vma_fault(struct vm_fault *vmf);
static long hfi1_file_ioctl(struct file *fp, unsigned int cmd,
			    unsigned long arg);

static const struct file_operations hfi1_file_ops = {
	.owner = THIS_MODULE,
	.write_iter = hfi1_write_iter,
	.open = hfi1_file_open,
	.release = hfi1_file_close,
	.unlocked_ioctl = hfi1_file_ioctl,
	.poll = hfi1_poll,
	.mmap = hfi1_file_mmap,
	.llseek = noop_llseek,
};

static const struct vm_operations_struct vm_ops = {
	.fault = vma_fault,
};

 
enum mmap_types {
	PIO_BUFS = 1,
	PIO_BUFS_SOP,
	PIO_CRED,
	RCV_HDRQ,
	RCV_EGRBUF,
	UREGS,
	EVENTS,
	STATUS,
	RTAIL,
	SUBCTXT_UREGS,
	SUBCTXT_RCV_HDRQ,
	SUBCTXT_EGRBUF,
	SDMA_COMP
};

 
#define HFI1_MMAP_OFFSET_MASK   0xfffULL
#define HFI1_MMAP_OFFSET_SHIFT  0
#define HFI1_MMAP_SUBCTXT_MASK  0xfULL
#define HFI1_MMAP_SUBCTXT_SHIFT 12
#define HFI1_MMAP_CTXT_MASK     0xffULL
#define HFI1_MMAP_CTXT_SHIFT    16
#define HFI1_MMAP_TYPE_MASK     0xfULL
#define HFI1_MMAP_TYPE_SHIFT    24
#define HFI1_MMAP_MAGIC_MASK    0xffffffffULL
#define HFI1_MMAP_MAGIC_SHIFT   32

#define HFI1_MMAP_MAGIC         0xdabbad00

#define HFI1_MMAP_TOKEN_SET(field, val)	\
	(((val) & HFI1_MMAP_##field##_MASK) << HFI1_MMAP_##field##_SHIFT)
#define HFI1_MMAP_TOKEN_GET(field, token) \
	(((token) >> HFI1_MMAP_##field##_SHIFT) & HFI1_MMAP_##field##_MASK)
#define HFI1_MMAP_TOKEN(type, ctxt, subctxt, addr)   \
	(HFI1_MMAP_TOKEN_SET(MAGIC, HFI1_MMAP_MAGIC) | \
	HFI1_MMAP_TOKEN_SET(TYPE, type) | \
	HFI1_MMAP_TOKEN_SET(CTXT, ctxt) | \
	HFI1_MMAP_TOKEN_SET(SUBCTXT, subctxt) | \
	HFI1_MMAP_TOKEN_SET(OFFSET, (offset_in_page(addr))))

#define dbg(fmt, ...)				\
	pr_info(fmt, ##__VA_ARGS__)

static inline int is_valid_mmap(u64 token)
{
	return (HFI1_MMAP_TOKEN_GET(MAGIC, token) == HFI1_MMAP_MAGIC);
}

static int hfi1_file_open(struct inode *inode, struct file *fp)
{
	struct hfi1_filedata *fd;
	struct hfi1_devdata *dd = container_of(inode->i_cdev,
					       struct hfi1_devdata,
					       user_cdev);

	if (!((dd->flags & HFI1_PRESENT) && dd->kregbase1))
		return -EINVAL;

	if (!refcount_inc_not_zero(&dd->user_refcount))
		return -ENXIO;

	 

	fd = kzalloc(sizeof(*fd), GFP_KERNEL);

	if (!fd || init_srcu_struct(&fd->pq_srcu))
		goto nomem;
	spin_lock_init(&fd->pq_rcu_lock);
	spin_lock_init(&fd->tid_lock);
	spin_lock_init(&fd->invalid_lock);
	fd->rec_cpu_num = -1;  
	fd->dd = dd;
	fp->private_data = fd;
	return 0;
nomem:
	kfree(fd);
	fp->private_data = NULL;
	if (refcount_dec_and_test(&dd->user_refcount))
		complete(&dd->user_comp);
	return -ENOMEM;
}

static long hfi1_file_ioctl(struct file *fp, unsigned int cmd,
			    unsigned long arg)
{
	struct hfi1_filedata *fd = fp->private_data;
	struct hfi1_ctxtdata *uctxt = fd->uctxt;
	int ret = 0;
	int uval = 0;

	hfi1_cdbg(IOCTL, "IOCTL recv: 0x%x", cmd);
	if (cmd != HFI1_IOCTL_ASSIGN_CTXT &&
	    cmd != HFI1_IOCTL_GET_VERS &&
	    !uctxt)
		return -EINVAL;

	switch (cmd) {
	case HFI1_IOCTL_ASSIGN_CTXT:
		ret = assign_ctxt(fd, arg, _IOC_SIZE(cmd));
		break;

	case HFI1_IOCTL_CTXT_INFO:
		ret = get_ctxt_info(fd, arg, _IOC_SIZE(cmd));
		break;

	case HFI1_IOCTL_USER_INFO:
		ret = get_base_info(fd, arg, _IOC_SIZE(cmd));
		break;

	case HFI1_IOCTL_CREDIT_UPD:
		if (uctxt)
			sc_return_credits(uctxt->sc);
		break;

	case HFI1_IOCTL_TID_UPDATE:
		ret = user_exp_rcv_setup(fd, arg, _IOC_SIZE(cmd));
		break;

	case HFI1_IOCTL_TID_FREE:
		ret = user_exp_rcv_clear(fd, arg, _IOC_SIZE(cmd));
		break;

	case HFI1_IOCTL_TID_INVAL_READ:
		ret = user_exp_rcv_invalid(fd, arg, _IOC_SIZE(cmd));
		break;

	case HFI1_IOCTL_RECV_CTRL:
		ret = manage_rcvq(uctxt, fd->subctxt, arg);
		break;

	case HFI1_IOCTL_POLL_TYPE:
		if (get_user(uval, (int __user *)arg))
			return -EFAULT;
		uctxt->poll_type = (typeof(uctxt->poll_type))uval;
		break;

	case HFI1_IOCTL_ACK_EVENT:
		ret = user_event_ack(uctxt, fd->subctxt, arg);
		break;

	case HFI1_IOCTL_SET_PKEY:
		ret = set_ctxt_pkey(uctxt, arg);
		break;

	case HFI1_IOCTL_CTXT_RESET:
		ret = ctxt_reset(uctxt);
		break;

	case HFI1_IOCTL_GET_VERS:
		uval = HFI1_USER_SWVERSION;
		if (put_user(uval, (int __user *)arg))
			return -EFAULT;
		break;

	default:
		return -EINVAL;
	}

	return ret;
}

static ssize_t hfi1_write_iter(struct kiocb *kiocb, struct iov_iter *from)
{
	struct hfi1_filedata *fd = kiocb->ki_filp->private_data;
	struct hfi1_user_sdma_pkt_q *pq;
	struct hfi1_user_sdma_comp_q *cq = fd->cq;
	int done = 0, reqs = 0;
	unsigned long dim = from->nr_segs;
	int idx;

	if (!HFI1_CAP_IS_KSET(SDMA))
		return -EINVAL;
	if (!from->user_backed)
		return -EINVAL;
	idx = srcu_read_lock(&fd->pq_srcu);
	pq = srcu_dereference(fd->pq, &fd->pq_srcu);
	if (!cq || !pq) {
		srcu_read_unlock(&fd->pq_srcu, idx);
		return -EIO;
	}

	trace_hfi1_sdma_request(fd->dd, fd->uctxt->ctxt, fd->subctxt, dim);

	if (atomic_read(&pq->n_reqs) == pq->n_max_reqs) {
		srcu_read_unlock(&fd->pq_srcu, idx);
		return -ENOSPC;
	}

	while (dim) {
		const struct iovec *iov = iter_iov(from);
		int ret;
		unsigned long count = 0;

		ret = hfi1_user_sdma_process_request(
			fd, (struct iovec *)(iov + done),
			dim, &count);
		if (ret) {
			reqs = ret;
			break;
		}
		dim -= count;
		done += count;
		reqs++;
	}

	srcu_read_unlock(&fd->pq_srcu, idx);
	return reqs;
}

static inline void mmap_cdbg(u16 ctxt, u8 subctxt, u8 type, u8 mapio, u8 vmf,
			     u64 memaddr, void *memvirt, dma_addr_t memdma,
			     ssize_t memlen, struct vm_area_struct *vma)
{
	hfi1_cdbg(PROC,
		  "%u:%u type:%u io/vf/dma:%d/%d/%d, addr:0x%llx, len:%lu(%lu), flags:0x%lx",
		  ctxt, subctxt, type, mapio, vmf, !!memdma,
		  memaddr ?: (u64)memvirt, memlen,
		  vma->vm_end - vma->vm_start, vma->vm_flags);
}

static int hfi1_file_mmap(struct file *fp, struct vm_area_struct *vma)
{
	struct hfi1_filedata *fd = fp->private_data;
	struct hfi1_ctxtdata *uctxt = fd->uctxt;
	struct hfi1_devdata *dd;
	unsigned long flags;
	u64 token = vma->vm_pgoff << PAGE_SHIFT,
		memaddr = 0;
	void *memvirt = NULL;
	dma_addr_t memdma = 0;
	u8 subctxt, mapio = 0, vmf = 0, type;
	ssize_t memlen = 0;
	int ret = 0;
	u16 ctxt;

	if (!is_valid_mmap(token) || !uctxt ||
	    !(vma->vm_flags & VM_SHARED)) {
		ret = -EINVAL;
		goto done;
	}
	dd = uctxt->dd;
	ctxt = HFI1_MMAP_TOKEN_GET(CTXT, token);
	subctxt = HFI1_MMAP_TOKEN_GET(SUBCTXT, token);
	type = HFI1_MMAP_TOKEN_GET(TYPE, token);
	if (ctxt != uctxt->ctxt || subctxt != fd->subctxt) {
		ret = -EINVAL;
		goto done;
	}

	  
	vma->vm_pgoff = 0;
	flags = vma->vm_flags;

	switch (type) {
	case PIO_BUFS:
	case PIO_BUFS_SOP:
		memaddr = ((dd->physaddr + TXE_PIO_SEND) +
				 
			   (uctxt->sc->hw_context * BIT(16))) +
				 
			(type == PIO_BUFS_SOP ?
				(TXE_PIO_SIZE / 2) : 0);  
		 
		memlen = PAGE_ALIGN(uctxt->sc->credits * PIO_BLOCK_SIZE);
		flags &= ~VM_MAYREAD;
		flags |= VM_DONTCOPY | VM_DONTEXPAND;
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
		mapio = 1;
		break;
	case PIO_CRED: {
		u64 cr_page_offset;
		if (flags & VM_WRITE) {
			ret = -EPERM;
			goto done;
		}
		 
		cr_page_offset = ((u64)uctxt->sc->hw_free -
			  	     (u64)dd->cr_base[uctxt->numa_id].va) &
				   PAGE_MASK;
		memvirt = dd->cr_base[uctxt->numa_id].va + cr_page_offset;
		memdma = dd->cr_base[uctxt->numa_id].dma + cr_page_offset;
		memlen = PAGE_SIZE;
		flags &= ~VM_MAYWRITE;
		flags |= VM_DONTCOPY | VM_DONTEXPAND;
		 
		 
		break;
	}
	case RCV_HDRQ:
		memlen = rcvhdrq_size(uctxt);
		memvirt = uctxt->rcvhdrq;
		memdma = uctxt->rcvhdrq_dma;
		break;
	case RCV_EGRBUF: {
		unsigned long vm_start_save;
		unsigned long vm_end_save;
		int i;
		 
		memlen = uctxt->egrbufs.size;
		if ((vma->vm_end - vma->vm_start) != memlen) {
			dd_dev_err(dd, "Eager buffer map size invalid (%lu != %lu)\n",
				   (vma->vm_end - vma->vm_start), memlen);
			ret = -EINVAL;
			goto done;
		}
		if (vma->vm_flags & VM_WRITE) {
			ret = -EPERM;
			goto done;
		}
		vm_flags_clear(vma, VM_MAYWRITE);
		 
		vm_start_save = vma->vm_start;
		vm_end_save = vma->vm_end;
		vma->vm_end = vma->vm_start;
		for (i = 0 ; i < uctxt->egrbufs.numbufs; i++) {
			memlen = uctxt->egrbufs.buffers[i].len;
			memvirt = uctxt->egrbufs.buffers[i].addr;
			memdma = uctxt->egrbufs.buffers[i].dma;
			vma->vm_end += memlen;
			mmap_cdbg(ctxt, subctxt, type, mapio, vmf, memaddr,
				  memvirt, memdma, memlen, vma);
			ret = dma_mmap_coherent(&dd->pcidev->dev, vma,
						memvirt, memdma, memlen);
			if (ret < 0) {
				vma->vm_start = vm_start_save;
				vma->vm_end = vm_end_save;
				goto done;
			}
			vma->vm_start += memlen;
		}
		vma->vm_start = vm_start_save;
		vma->vm_end = vm_end_save;
		ret = 0;
		goto done;
	}
	case UREGS:
		 
		memaddr = (unsigned long)
			(dd->physaddr + RXE_PER_CONTEXT_USER)
			+ (uctxt->ctxt * RXE_PER_CONTEXT_SIZE);
		 
		memlen = PAGE_SIZE;
		flags |= VM_DONTCOPY | VM_DONTEXPAND;
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		mapio = 1;
		break;
	case EVENTS:
		 
		memaddr = (unsigned long)
			(dd->events + uctxt_offset(uctxt)) & PAGE_MASK;
		memlen = PAGE_SIZE;
		 
		flags |= VM_IO | VM_DONTEXPAND;
		vmf = 1;
		break;
	case STATUS:
		if (flags & VM_WRITE) {
			ret = -EPERM;
			goto done;
		}
		memaddr = kvirt_to_phys((void *)dd->status);
		memlen = PAGE_SIZE;
		flags |= VM_IO | VM_DONTEXPAND;
		break;
	case RTAIL:
		if (!HFI1_CAP_IS_USET(DMA_RTAIL)) {
			 
			ret = -EINVAL;
			goto done;
		}
		if ((flags & VM_WRITE) || !hfi1_rcvhdrtail_kvaddr(uctxt)) {
			ret = -EPERM;
			goto done;
		}
		memlen = PAGE_SIZE;
		memvirt = (void *)hfi1_rcvhdrtail_kvaddr(uctxt);
		memdma = uctxt->rcvhdrqtailaddr_dma;
		flags &= ~VM_MAYWRITE;
		break;
	case SUBCTXT_UREGS:
		memaddr = (u64)uctxt->subctxt_uregbase;
		memlen = PAGE_SIZE;
		flags |= VM_IO | VM_DONTEXPAND;
		vmf = 1;
		break;
	case SUBCTXT_RCV_HDRQ:
		memaddr = (u64)uctxt->subctxt_rcvhdr_base;
		memlen = rcvhdrq_size(uctxt) * uctxt->subctxt_cnt;
		flags |= VM_IO | VM_DONTEXPAND;
		vmf = 1;
		break;
	case SUBCTXT_EGRBUF:
		memaddr = (u64)uctxt->subctxt_rcvegrbuf;
		memlen = uctxt->egrbufs.size * uctxt->subctxt_cnt;
		flags |= VM_IO | VM_DONTEXPAND;
		flags &= ~VM_MAYWRITE;
		vmf = 1;
		break;
	case SDMA_COMP: {
		struct hfi1_user_sdma_comp_q *cq = fd->cq;

		if (!cq) {
			ret = -EFAULT;
			goto done;
		}
		memaddr = (u64)cq->comps;
		memlen = PAGE_ALIGN(sizeof(*cq->comps) * cq->nentries);
		flags |= VM_IO | VM_DONTEXPAND;
		vmf = 1;
		break;
	}
	default:
		ret = -EINVAL;
		break;
	}

	if ((vma->vm_end - vma->vm_start) != memlen) {
		hfi1_cdbg(PROC, "%u:%u Memory size mismatch %lu:%lu",
			  uctxt->ctxt, fd->subctxt,
			  (vma->vm_end - vma->vm_start), memlen);
		ret = -EINVAL;
		goto done;
	}

	vm_flags_reset(vma, flags);
	mmap_cdbg(ctxt, subctxt, type, mapio, vmf, memaddr, memvirt, memdma, 
		  memlen, vma);
	if (vmf) {
		vma->vm_pgoff = PFN_DOWN(memaddr);
		vma->vm_ops = &vm_ops;
		ret = 0;
	} else if (memdma) {
		ret = dma_mmap_coherent(&dd->pcidev->dev, vma,
					memvirt, memdma, memlen);
	} else if (mapio) {
		ret = io_remap_pfn_range(vma, vma->vm_start,
					 PFN_DOWN(memaddr),
					 memlen,
					 vma->vm_page_prot);
	} else if (memvirt) {
		ret = remap_pfn_range(vma, vma->vm_start,
				      PFN_DOWN(__pa(memvirt)),
				      memlen,
				      vma->vm_page_prot);
	} else {
		ret = remap_pfn_range(vma, vma->vm_start,
				      PFN_DOWN(memaddr),
				      memlen,
				      vma->vm_page_prot);
	}
done:
	return ret;
}

 
static vm_fault_t vma_fault(struct vm_fault *vmf)
{
	struct page *page;

	page = vmalloc_to_page((void *)(vmf->pgoff << PAGE_SHIFT));
	if (!page)
		return VM_FAULT_SIGBUS;

	get_page(page);
	vmf->page = page;

	return 0;
}

static __poll_t hfi1_poll(struct file *fp, struct poll_table_struct *pt)
{
	struct hfi1_ctxtdata *uctxt;
	__poll_t pollflag;

	uctxt = ((struct hfi1_filedata *)fp->private_data)->uctxt;
	if (!uctxt)
		pollflag = EPOLLERR;
	else if (uctxt->poll_type == HFI1_POLL_TYPE_URGENT)
		pollflag = poll_urgent(fp, pt);
	else  if (uctxt->poll_type == HFI1_POLL_TYPE_ANYRCV)
		pollflag = poll_next(fp, pt);
	else  
		pollflag = EPOLLERR;

	return pollflag;
}

static int hfi1_file_close(struct inode *inode, struct file *fp)
{
	struct hfi1_filedata *fdata = fp->private_data;
	struct hfi1_ctxtdata *uctxt = fdata->uctxt;
	struct hfi1_devdata *dd = container_of(inode->i_cdev,
					       struct hfi1_devdata,
					       user_cdev);
	unsigned long flags, *ev;

	fp->private_data = NULL;

	if (!uctxt)
		goto done;

	hfi1_cdbg(PROC, "closing ctxt %u:%u", uctxt->ctxt, fdata->subctxt);

	flush_wc();
	 
	hfi1_user_sdma_free_queues(fdata, uctxt);

	 
	hfi1_put_proc_affinity(fdata->rec_cpu_num);

	 
	hfi1_user_exp_rcv_free(fdata);

	 
	fdata->uctxt = NULL;
	hfi1_rcd_put(uctxt);

	 
	ev = dd->events + uctxt_offset(uctxt) + fdata->subctxt;
	*ev = 0;

	spin_lock_irqsave(&dd->uctxt_lock, flags);
	__clear_bit(fdata->subctxt, uctxt->in_use_ctxts);
	if (!bitmap_empty(uctxt->in_use_ctxts, HFI1_MAX_SHARED_CTXTS)) {
		spin_unlock_irqrestore(&dd->uctxt_lock, flags);
		goto done;
	}
	spin_unlock_irqrestore(&dd->uctxt_lock, flags);

	 
	hfi1_rcvctrl(dd, HFI1_RCVCTRL_CTXT_DIS |
		     HFI1_RCVCTRL_TIDFLOW_DIS |
		     HFI1_RCVCTRL_INTRAVAIL_DIS |
		     HFI1_RCVCTRL_TAILUPD_DIS |
		     HFI1_RCVCTRL_ONE_PKT_EGR_DIS |
		     HFI1_RCVCTRL_NO_RHQ_DROP_DIS |
		     HFI1_RCVCTRL_NO_EGR_DROP_DIS |
		     HFI1_RCVCTRL_URGENT_DIS, uctxt);
	 
	hfi1_clear_ctxt_jkey(dd, uctxt);
	 
	if (uctxt->sc) {
		sc_disable(uctxt->sc);
		set_pio_integrity(uctxt->sc);
	}

	hfi1_free_ctxt_rcv_groups(uctxt);
	hfi1_clear_ctxt_pkey(dd, uctxt);

	uctxt->event_flags = 0;

	deallocate_ctxt(uctxt);
done:

	if (refcount_dec_and_test(&dd->user_refcount))
		complete(&dd->user_comp);

	cleanup_srcu_struct(&fdata->pq_srcu);
	kfree(fdata);
	return 0;
}

 
static u64 kvirt_to_phys(void *addr)
{
	struct page *page;
	u64 paddr = 0;

	page = vmalloc_to_page(addr);
	if (page)
		paddr = page_to_pfn(page) << PAGE_SHIFT;

	return paddr;
}

 
static int complete_subctxt(struct hfi1_filedata *fd)
{
	int ret;
	unsigned long flags;

	 
	ret = wait_event_interruptible(
		fd->uctxt->wait,
		!test_bit(HFI1_CTXT_BASE_UNINIT, &fd->uctxt->event_flags));

	if (test_bit(HFI1_CTXT_BASE_FAILED, &fd->uctxt->event_flags))
		ret = -ENOMEM;

	 
	if (!ret) {
		fd->rec_cpu_num = hfi1_get_proc_affinity(fd->uctxt->numa_id);
		ret = init_user_ctxt(fd, fd->uctxt);
	}

	if (ret) {
		spin_lock_irqsave(&fd->dd->uctxt_lock, flags);
		__clear_bit(fd->subctxt, fd->uctxt->in_use_ctxts);
		spin_unlock_irqrestore(&fd->dd->uctxt_lock, flags);
		hfi1_rcd_put(fd->uctxt);
		fd->uctxt = NULL;
	}

	return ret;
}

static int assign_ctxt(struct hfi1_filedata *fd, unsigned long arg, u32 len)
{
	int ret;
	unsigned int swmajor;
	struct hfi1_ctxtdata *uctxt = NULL;
	struct hfi1_user_info uinfo;

	if (fd->uctxt)
		return -EINVAL;

	if (sizeof(uinfo) != len)
		return -EINVAL;

	if (copy_from_user(&uinfo, (void __user *)arg, sizeof(uinfo)))
		return -EFAULT;

	swmajor = uinfo.userversion >> 16;
	if (swmajor != HFI1_USER_SWMAJOR)
		return -ENODEV;

	if (uinfo.subctxt_cnt > HFI1_MAX_SHARED_CTXTS)
		return -EINVAL;

	 
	mutex_lock(&hfi1_mutex);
	 
	ret = find_sub_ctxt(fd, &uinfo);

	 
	if (!ret)
		ret = allocate_ctxt(fd, fd->dd, &uinfo, &uctxt);

	mutex_unlock(&hfi1_mutex);

	 
	switch (ret) {
	case 0:
		ret = setup_base_ctxt(fd, uctxt);
		if (ret)
			deallocate_ctxt(uctxt);
		break;
	case 1:
		ret = complete_subctxt(fd);
		break;
	default:
		break;
	}

	return ret;
}

 
static int match_ctxt(struct hfi1_filedata *fd,
		      const struct hfi1_user_info *uinfo,
		      struct hfi1_ctxtdata *uctxt)
{
	struct hfi1_devdata *dd = fd->dd;
	unsigned long flags;
	u16 subctxt;

	 
	if (uctxt->sc && (uctxt->sc->type == SC_KERNEL))
		return 0;

	 
	if (memcmp(uctxt->uuid, uinfo->uuid, sizeof(uctxt->uuid)) ||
	    uctxt->jkey != generate_jkey(current_uid()) ||
	    uctxt->subctxt_id != uinfo->subctxt_id ||
	    uctxt->subctxt_cnt != uinfo->subctxt_cnt)
		return 0;

	 
	if (uctxt->userversion != uinfo->userversion)
		return -EINVAL;

	 
	spin_lock_irqsave(&dd->uctxt_lock, flags);
	if (bitmap_empty(uctxt->in_use_ctxts, HFI1_MAX_SHARED_CTXTS)) {
		 
		spin_unlock_irqrestore(&dd->uctxt_lock, flags);
		return 0;
	}

	subctxt = find_first_zero_bit(uctxt->in_use_ctxts,
				      HFI1_MAX_SHARED_CTXTS);
	if (subctxt >= uctxt->subctxt_cnt) {
		spin_unlock_irqrestore(&dd->uctxt_lock, flags);
		return -EBUSY;
	}

	fd->subctxt = subctxt;
	__set_bit(fd->subctxt, uctxt->in_use_ctxts);
	spin_unlock_irqrestore(&dd->uctxt_lock, flags);

	fd->uctxt = uctxt;
	hfi1_rcd_get(uctxt);

	return 1;
}

 
static int find_sub_ctxt(struct hfi1_filedata *fd,
			 const struct hfi1_user_info *uinfo)
{
	struct hfi1_ctxtdata *uctxt;
	struct hfi1_devdata *dd = fd->dd;
	u16 i;
	int ret;

	if (!uinfo->subctxt_cnt)
		return 0;

	for (i = dd->first_dyn_alloc_ctxt; i < dd->num_rcv_contexts; i++) {
		uctxt = hfi1_rcd_get_by_index(dd, i);
		if (uctxt) {
			ret = match_ctxt(fd, uinfo, uctxt);
			hfi1_rcd_put(uctxt);
			 
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int allocate_ctxt(struct hfi1_filedata *fd, struct hfi1_devdata *dd,
			 struct hfi1_user_info *uinfo,
			 struct hfi1_ctxtdata **rcd)
{
	struct hfi1_ctxtdata *uctxt;
	int ret, numa;

	if (dd->flags & HFI1_FROZEN) {
		 
		return -EIO;
	}

	if (!dd->freectxts)
		return -EBUSY;

	 
	fd->rec_cpu_num = hfi1_get_proc_affinity(dd->node);
	if (fd->rec_cpu_num != -1)
		numa = cpu_to_node(fd->rec_cpu_num);
	else
		numa = numa_node_id();
	ret = hfi1_create_ctxtdata(dd->pport, numa, &uctxt);
	if (ret < 0) {
		dd_dev_err(dd, "user ctxtdata allocation failed\n");
		return ret;
	}
	hfi1_cdbg(PROC, "[%u:%u] pid %u assigned to CPU %d (NUMA %u)",
		  uctxt->ctxt, fd->subctxt, current->pid, fd->rec_cpu_num,
		  uctxt->numa_id);

	 
	uctxt->sc = sc_alloc(dd, SC_USER, uctxt->rcvhdrqentsize, dd->node);
	if (!uctxt->sc) {
		ret = -ENOMEM;
		goto ctxdata_free;
	}
	hfi1_cdbg(PROC, "allocated send context %u(%u)", uctxt->sc->sw_index,
		  uctxt->sc->hw_context);
	ret = sc_enable(uctxt->sc);
	if (ret)
		goto ctxdata_free;

	 
	__set_bit(0, uctxt->in_use_ctxts);
	if (uinfo->subctxt_cnt)
		init_subctxts(uctxt, uinfo);
	uctxt->userversion = uinfo->userversion;
	uctxt->flags = hfi1_cap_mask;  
	init_waitqueue_head(&uctxt->wait);
	strscpy(uctxt->comm, current->comm, sizeof(uctxt->comm));
	memcpy(uctxt->uuid, uinfo->uuid, sizeof(uctxt->uuid));
	uctxt->jkey = generate_jkey(current_uid());
	hfi1_stats.sps_ctxts++;
	 
	if (dd->freectxts-- == dd->num_user_contexts)
		aspm_disable_all(dd);

	*rcd = uctxt;

	return 0;

ctxdata_free:
	hfi1_free_ctxt(uctxt);
	return ret;
}

static void deallocate_ctxt(struct hfi1_ctxtdata *uctxt)
{
	mutex_lock(&hfi1_mutex);
	hfi1_stats.sps_ctxts--;
	if (++uctxt->dd->freectxts == uctxt->dd->num_user_contexts)
		aspm_enable_all(uctxt->dd);
	mutex_unlock(&hfi1_mutex);

	hfi1_free_ctxt(uctxt);
}

static void init_subctxts(struct hfi1_ctxtdata *uctxt,
			  const struct hfi1_user_info *uinfo)
{
	uctxt->subctxt_cnt = uinfo->subctxt_cnt;
	uctxt->subctxt_id = uinfo->subctxt_id;
	set_bit(HFI1_CTXT_BASE_UNINIT, &uctxt->event_flags);
}

static int setup_subctxt(struct hfi1_ctxtdata *uctxt)
{
	int ret = 0;
	u16 num_subctxts = uctxt->subctxt_cnt;

	uctxt->subctxt_uregbase = vmalloc_user(PAGE_SIZE);
	if (!uctxt->subctxt_uregbase)
		return -ENOMEM;

	 
	uctxt->subctxt_rcvhdr_base = vmalloc_user(rcvhdrq_size(uctxt) *
						  num_subctxts);
	if (!uctxt->subctxt_rcvhdr_base) {
		ret = -ENOMEM;
		goto bail_ureg;
	}

	uctxt->subctxt_rcvegrbuf = vmalloc_user(uctxt->egrbufs.size *
						num_subctxts);
	if (!uctxt->subctxt_rcvegrbuf) {
		ret = -ENOMEM;
		goto bail_rhdr;
	}

	return 0;

bail_rhdr:
	vfree(uctxt->subctxt_rcvhdr_base);
	uctxt->subctxt_rcvhdr_base = NULL;
bail_ureg:
	vfree(uctxt->subctxt_uregbase);
	uctxt->subctxt_uregbase = NULL;

	return ret;
}

static void user_init(struct hfi1_ctxtdata *uctxt)
{
	unsigned int rcvctrl_ops = 0;

	 
	uctxt->urgent = 0;
	uctxt->urgent_poll = 0;

	 
	if (hfi1_rcvhdrtail_kvaddr(uctxt))
		clear_rcvhdrtail(uctxt);

	 
	hfi1_set_ctxt_jkey(uctxt->dd, uctxt, uctxt->jkey);

	rcvctrl_ops = HFI1_RCVCTRL_CTXT_ENB;
	rcvctrl_ops |= HFI1_RCVCTRL_URGENT_ENB;
	if (HFI1_CAP_UGET_MASK(uctxt->flags, HDRSUPP))
		rcvctrl_ops |= HFI1_RCVCTRL_TIDFLOW_ENB;
	 
	if (!HFI1_CAP_UGET_MASK(uctxt->flags, MULTI_PKT_EGR))
		rcvctrl_ops |= HFI1_RCVCTRL_ONE_PKT_EGR_ENB;
	if (HFI1_CAP_UGET_MASK(uctxt->flags, NODROP_EGR_FULL))
		rcvctrl_ops |= HFI1_RCVCTRL_NO_EGR_DROP_ENB;
	if (HFI1_CAP_UGET_MASK(uctxt->flags, NODROP_RHQ_FULL))
		rcvctrl_ops |= HFI1_RCVCTRL_NO_RHQ_DROP_ENB;
	 
	if (HFI1_CAP_UGET_MASK(uctxt->flags, DMA_RTAIL))
		rcvctrl_ops |= HFI1_RCVCTRL_TAILUPD_ENB;
	else
		rcvctrl_ops |= HFI1_RCVCTRL_TAILUPD_DIS;
	hfi1_rcvctrl(uctxt->dd, rcvctrl_ops, uctxt);
}

static int get_ctxt_info(struct hfi1_filedata *fd, unsigned long arg, u32 len)
{
	struct hfi1_ctxt_info cinfo;
	struct hfi1_ctxtdata *uctxt = fd->uctxt;

	if (sizeof(cinfo) != len)
		return -EINVAL;

	memset(&cinfo, 0, sizeof(cinfo));
	cinfo.runtime_flags = (((uctxt->flags >> HFI1_CAP_MISC_SHIFT) &
				HFI1_CAP_MISC_MASK) << HFI1_CAP_USER_SHIFT) |
			HFI1_CAP_UGET_MASK(uctxt->flags, MASK) |
			HFI1_CAP_KGET_MASK(uctxt->flags, K2U);
	 
	if (!fd->use_mn)
		cinfo.runtime_flags |= HFI1_CAP_TID_UNMAP;  

	cinfo.num_active = hfi1_count_active_units();
	cinfo.unit = uctxt->dd->unit;
	cinfo.ctxt = uctxt->ctxt;
	cinfo.subctxt = fd->subctxt;
	cinfo.rcvtids = roundup(uctxt->egrbufs.alloced,
				uctxt->dd->rcv_entries.group_size) +
		uctxt->expected_count;
	cinfo.credits = uctxt->sc->credits;
	cinfo.numa_node = uctxt->numa_id;
	cinfo.rec_cpu = fd->rec_cpu_num;
	cinfo.send_ctxt = uctxt->sc->hw_context;

	cinfo.egrtids = uctxt->egrbufs.alloced;
	cinfo.rcvhdrq_cnt = get_hdrq_cnt(uctxt);
	cinfo.rcvhdrq_entsize = get_hdrqentsize(uctxt) << 2;
	cinfo.sdma_ring_size = fd->cq->nentries;
	cinfo.rcvegr_size = uctxt->egrbufs.rcvtid_size;

	trace_hfi1_ctxt_info(uctxt->dd, uctxt->ctxt, fd->subctxt, &cinfo);
	if (copy_to_user((void __user *)arg, &cinfo, len))
		return -EFAULT;

	return 0;
}

static int init_user_ctxt(struct hfi1_filedata *fd,
			  struct hfi1_ctxtdata *uctxt)
{
	int ret;

	ret = hfi1_user_sdma_alloc_queues(uctxt, fd);
	if (ret)
		return ret;

	ret = hfi1_user_exp_rcv_init(fd, uctxt);
	if (ret)
		hfi1_user_sdma_free_queues(fd, uctxt);

	return ret;
}

static int setup_base_ctxt(struct hfi1_filedata *fd,
			   struct hfi1_ctxtdata *uctxt)
{
	struct hfi1_devdata *dd = uctxt->dd;
	int ret = 0;

	hfi1_init_ctxt(uctxt->sc);

	 
	ret = hfi1_create_rcvhdrq(dd, uctxt);
	if (ret)
		goto done;

	ret = hfi1_setup_eagerbufs(uctxt);
	if (ret)
		goto done;

	 
	if (uctxt->subctxt_cnt)
		ret = setup_subctxt(uctxt);
	if (ret)
		goto done;

	ret = hfi1_alloc_ctxt_rcv_groups(uctxt);
	if (ret)
		goto done;

	ret = init_user_ctxt(fd, uctxt);
	if (ret) {
		hfi1_free_ctxt_rcv_groups(uctxt);
		goto done;
	}

	user_init(uctxt);

	 
	fd->uctxt = uctxt;
	hfi1_rcd_get(uctxt);

done:
	if (uctxt->subctxt_cnt) {
		 
		if (ret)
			set_bit(HFI1_CTXT_BASE_FAILED, &uctxt->event_flags);

		 
		clear_bit(HFI1_CTXT_BASE_UNINIT, &uctxt->event_flags);
		wake_up(&uctxt->wait);
	}

	return ret;
}

static int get_base_info(struct hfi1_filedata *fd, unsigned long arg, u32 len)
{
	struct hfi1_base_info binfo;
	struct hfi1_ctxtdata *uctxt = fd->uctxt;
	struct hfi1_devdata *dd = uctxt->dd;
	unsigned offset;

	trace_hfi1_uctxtdata(uctxt->dd, uctxt, fd->subctxt);

	if (sizeof(binfo) != len)
		return -EINVAL;

	memset(&binfo, 0, sizeof(binfo));
	binfo.hw_version = dd->revision;
	binfo.sw_version = HFI1_USER_SWVERSION;
	binfo.bthqp = RVT_KDETH_QP_PREFIX;
	binfo.jkey = uctxt->jkey;
	 
	offset = ((u64)uctxt->sc->hw_free -
		  (u64)dd->cr_base[uctxt->numa_id].va) % PAGE_SIZE;
	binfo.sc_credits_addr = HFI1_MMAP_TOKEN(PIO_CRED, uctxt->ctxt,
						fd->subctxt, offset);
	binfo.pio_bufbase = HFI1_MMAP_TOKEN(PIO_BUFS, uctxt->ctxt,
					    fd->subctxt,
					    uctxt->sc->base_addr);
	binfo.pio_bufbase_sop = HFI1_MMAP_TOKEN(PIO_BUFS_SOP,
						uctxt->ctxt,
						fd->subctxt,
						uctxt->sc->base_addr);
	binfo.rcvhdr_bufbase = HFI1_MMAP_TOKEN(RCV_HDRQ, uctxt->ctxt,
					       fd->subctxt,
					       uctxt->rcvhdrq);
	binfo.rcvegr_bufbase = HFI1_MMAP_TOKEN(RCV_EGRBUF, uctxt->ctxt,
					       fd->subctxt,
					       uctxt->egrbufs.rcvtids[0].dma);
	binfo.sdma_comp_bufbase = HFI1_MMAP_TOKEN(SDMA_COMP, uctxt->ctxt,
						  fd->subctxt, 0);
	 
	binfo.user_regbase = HFI1_MMAP_TOKEN(UREGS, uctxt->ctxt,
					     fd->subctxt, 0);
	offset = offset_in_page((uctxt_offset(uctxt) + fd->subctxt) *
				sizeof(*dd->events));
	binfo.events_bufbase = HFI1_MMAP_TOKEN(EVENTS, uctxt->ctxt,
					       fd->subctxt,
					       offset);
	binfo.status_bufbase = HFI1_MMAP_TOKEN(STATUS, uctxt->ctxt,
					       fd->subctxt,
					       dd->status);
	if (HFI1_CAP_IS_USET(DMA_RTAIL))
		binfo.rcvhdrtail_base = HFI1_MMAP_TOKEN(RTAIL, uctxt->ctxt,
							fd->subctxt, 0);
	if (uctxt->subctxt_cnt) {
		binfo.subctxt_uregbase = HFI1_MMAP_TOKEN(SUBCTXT_UREGS,
							 uctxt->ctxt,
							 fd->subctxt, 0);
		binfo.subctxt_rcvhdrbuf = HFI1_MMAP_TOKEN(SUBCTXT_RCV_HDRQ,
							  uctxt->ctxt,
							  fd->subctxt, 0);
		binfo.subctxt_rcvegrbuf = HFI1_MMAP_TOKEN(SUBCTXT_EGRBUF,
							  uctxt->ctxt,
							  fd->subctxt, 0);
	}

	if (copy_to_user((void __user *)arg, &binfo, len))
		return -EFAULT;

	return 0;
}

 
static int user_exp_rcv_setup(struct hfi1_filedata *fd, unsigned long arg,
			      u32 len)
{
	int ret;
	unsigned long addr;
	struct hfi1_tid_info tinfo;

	if (sizeof(tinfo) != len)
		return -EINVAL;

	if (copy_from_user(&tinfo, (void __user *)arg, (sizeof(tinfo))))
		return -EFAULT;

	ret = hfi1_user_exp_rcv_setup(fd, &tinfo);
	if (!ret) {
		 
		addr = arg + offsetof(struct hfi1_tid_info, tidcnt);
		if (copy_to_user((void __user *)addr, &tinfo.tidcnt,
				 sizeof(tinfo.tidcnt)))
			ret = -EFAULT;

		addr = arg + offsetof(struct hfi1_tid_info, length);
		if (!ret && copy_to_user((void __user *)addr, &tinfo.length,
				 sizeof(tinfo.length)))
			ret = -EFAULT;

		if (ret)
			hfi1_user_exp_rcv_invalid(fd, &tinfo);
	}

	return ret;
}

 
static int user_exp_rcv_clear(struct hfi1_filedata *fd, unsigned long arg,
			      u32 len)
{
	int ret;
	unsigned long addr;
	struct hfi1_tid_info tinfo;

	if (sizeof(tinfo) != len)
		return -EINVAL;

	if (copy_from_user(&tinfo, (void __user *)arg, (sizeof(tinfo))))
		return -EFAULT;

	ret = hfi1_user_exp_rcv_clear(fd, &tinfo);
	if (!ret) {
		addr = arg + offsetof(struct hfi1_tid_info, tidcnt);
		if (copy_to_user((void __user *)addr, &tinfo.tidcnt,
				 sizeof(tinfo.tidcnt)))
			return -EFAULT;
	}

	return ret;
}

 
static int user_exp_rcv_invalid(struct hfi1_filedata *fd, unsigned long arg,
				u32 len)
{
	int ret;
	unsigned long addr;
	struct hfi1_tid_info tinfo;

	if (sizeof(tinfo) != len)
		return -EINVAL;

	if (!fd->invalid_tids)
		return -EINVAL;

	if (copy_from_user(&tinfo, (void __user *)arg, (sizeof(tinfo))))
		return -EFAULT;

	ret = hfi1_user_exp_rcv_invalid(fd, &tinfo);
	if (ret)
		return ret;

	addr = arg + offsetof(struct hfi1_tid_info, tidcnt);
	if (copy_to_user((void __user *)addr, &tinfo.tidcnt,
			 sizeof(tinfo.tidcnt)))
		ret = -EFAULT;

	return ret;
}

static __poll_t poll_urgent(struct file *fp,
				struct poll_table_struct *pt)
{
	struct hfi1_filedata *fd = fp->private_data;
	struct hfi1_ctxtdata *uctxt = fd->uctxt;
	struct hfi1_devdata *dd = uctxt->dd;
	__poll_t pollflag;

	poll_wait(fp, &uctxt->wait, pt);

	spin_lock_irq(&dd->uctxt_lock);
	if (uctxt->urgent != uctxt->urgent_poll) {
		pollflag = EPOLLIN | EPOLLRDNORM;
		uctxt->urgent_poll = uctxt->urgent;
	} else {
		pollflag = 0;
		set_bit(HFI1_CTXT_WAITING_URG, &uctxt->event_flags);
	}
	spin_unlock_irq(&dd->uctxt_lock);

	return pollflag;
}

static __poll_t poll_next(struct file *fp,
			      struct poll_table_struct *pt)
{
	struct hfi1_filedata *fd = fp->private_data;
	struct hfi1_ctxtdata *uctxt = fd->uctxt;
	struct hfi1_devdata *dd = uctxt->dd;
	__poll_t pollflag;

	poll_wait(fp, &uctxt->wait, pt);

	spin_lock_irq(&dd->uctxt_lock);
	if (hdrqempty(uctxt)) {
		set_bit(HFI1_CTXT_WAITING_RCV, &uctxt->event_flags);
		hfi1_rcvctrl(dd, HFI1_RCVCTRL_INTRAVAIL_ENB, uctxt);
		pollflag = 0;
	} else {
		pollflag = EPOLLIN | EPOLLRDNORM;
	}
	spin_unlock_irq(&dd->uctxt_lock);

	return pollflag;
}

 
int hfi1_set_uevent_bits(struct hfi1_pportdata *ppd, const int evtbit)
{
	struct hfi1_ctxtdata *uctxt;
	struct hfi1_devdata *dd = ppd->dd;
	u16 ctxt;

	if (!dd->events)
		return -EINVAL;

	for (ctxt = dd->first_dyn_alloc_ctxt; ctxt < dd->num_rcv_contexts;
	     ctxt++) {
		uctxt = hfi1_rcd_get_by_index(dd, ctxt);
		if (uctxt) {
			unsigned long *evs;
			int i;
			 
			evs = dd->events + uctxt_offset(uctxt);
			set_bit(evtbit, evs);
			for (i = 1; i < uctxt->subctxt_cnt; i++)
				set_bit(evtbit, evs + i);
			hfi1_rcd_put(uctxt);
		}
	}

	return 0;
}

 
static int manage_rcvq(struct hfi1_ctxtdata *uctxt, u16 subctxt,
		       unsigned long arg)
{
	struct hfi1_devdata *dd = uctxt->dd;
	unsigned int rcvctrl_op;
	int start_stop;

	if (subctxt)
		return 0;

	if (get_user(start_stop, (int __user *)arg))
		return -EFAULT;

	 
	if (start_stop) {
		 
		if (hfi1_rcvhdrtail_kvaddr(uctxt))
			clear_rcvhdrtail(uctxt);
		rcvctrl_op = HFI1_RCVCTRL_CTXT_ENB;
	} else {
		rcvctrl_op = HFI1_RCVCTRL_CTXT_DIS;
	}
	hfi1_rcvctrl(dd, rcvctrl_op, uctxt);
	 

	return 0;
}

 
static int user_event_ack(struct hfi1_ctxtdata *uctxt, u16 subctxt,
			  unsigned long arg)
{
	int i;
	struct hfi1_devdata *dd = uctxt->dd;
	unsigned long *evs;
	unsigned long events;

	if (!dd->events)
		return 0;

	if (get_user(events, (unsigned long __user *)arg))
		return -EFAULT;

	evs = dd->events + uctxt_offset(uctxt) + subctxt;

	for (i = 0; i <= _HFI1_MAX_EVENT_BIT; i++) {
		if (!test_bit(i, &events))
			continue;
		clear_bit(i, evs);
	}
	return 0;
}

static int set_ctxt_pkey(struct hfi1_ctxtdata *uctxt, unsigned long arg)
{
	int i;
	struct hfi1_pportdata *ppd = uctxt->ppd;
	struct hfi1_devdata *dd = uctxt->dd;
	u16 pkey;

	if (!HFI1_CAP_IS_USET(PKEY_CHECK))
		return -EPERM;

	if (get_user(pkey, (u16 __user *)arg))
		return -EFAULT;

	if (pkey == LIM_MGMT_P_KEY || pkey == FULL_MGMT_P_KEY)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(ppd->pkeys); i++)
		if (pkey == ppd->pkeys[i])
			return hfi1_set_ctxt_pkey(dd, uctxt, pkey);

	return -ENOENT;
}

 
static int ctxt_reset(struct hfi1_ctxtdata *uctxt)
{
	struct send_context *sc;
	struct hfi1_devdata *dd;
	int ret = 0;

	if (!uctxt || !uctxt->dd || !uctxt->sc)
		return -EINVAL;

	 
	dd = uctxt->dd;
	sc = uctxt->sc;

	 
	wait_event_interruptible_timeout(
		sc->halt_wait, (sc->flags & SCF_HALTED),
		msecs_to_jiffies(SEND_CTXT_HALT_TIMEOUT));
	if (!(sc->flags & SCF_HALTED))
		return -ENOLCK;

	 
	if (sc->flags & SCF_FROZEN) {
		wait_event_interruptible_timeout(
			dd->event_queue,
			!(READ_ONCE(dd->flags) & HFI1_FROZEN),
			msecs_to_jiffies(SEND_CTXT_HALT_TIMEOUT));
		if (dd->flags & HFI1_FROZEN)
			return -ENOLCK;

		if (dd->flags & HFI1_FORCED_FREEZE)
			 
			return -ENODEV;

		sc_disable(sc);
		ret = sc_enable(sc);
		hfi1_rcvctrl(dd, HFI1_RCVCTRL_CTXT_ENB, uctxt);
	} else {
		ret = sc_restart(sc);
	}
	if (!ret)
		sc_return_credits(sc);

	return ret;
}

static void user_remove(struct hfi1_devdata *dd)
{

	hfi1_cdev_cleanup(&dd->user_cdev, &dd->user_device);
}

static int user_add(struct hfi1_devdata *dd)
{
	char name[10];
	int ret;

	snprintf(name, sizeof(name), "%s_%d", class_name(), dd->unit);
	ret = hfi1_cdev_init(dd->unit, name, &hfi1_file_ops,
			     &dd->user_cdev, &dd->user_device,
			     true, &dd->verbs_dev.rdi.ibdev.dev.kobj);
	if (ret)
		user_remove(dd);

	return ret;
}

 
int hfi1_device_create(struct hfi1_devdata *dd)
{
	return user_add(dd);
}

 
void hfi1_device_remove(struct hfi1_devdata *dd)
{
	user_remove(dd);
}
