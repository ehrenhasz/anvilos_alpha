
 
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/blk-mq.h>
#include <linux/parser.h>
#include <linux/random.h>
#include <uapi/scsi/fc/fc_fs.h>
#include <uapi/scsi/fc/fc_els.h>

#include "nvmet.h"
#include <linux/nvme-fc-driver.h>
#include <linux/nvme-fc.h>
#include "../host/fc.h"


 


#define NVMET_LS_CTX_COUNT		256

struct nvmet_fc_tgtport;
struct nvmet_fc_tgt_assoc;

struct nvmet_fc_ls_iod {		 
	struct nvmefc_ls_rsp		*lsrsp;
	struct nvmefc_tgt_fcp_req	*fcpreq;	 

	struct list_head		ls_rcv_list;  

	struct nvmet_fc_tgtport		*tgtport;
	struct nvmet_fc_tgt_assoc	*assoc;
	void				*hosthandle;

	union nvmefc_ls_requests	*rqstbuf;
	union nvmefc_ls_responses	*rspbuf;
	u16				rqstdatalen;
	dma_addr_t			rspdma;

	struct scatterlist		sg[2];

	struct work_struct		work;
} __aligned(sizeof(unsigned long long));

struct nvmet_fc_ls_req_op {		 
	struct nvmefc_ls_req		ls_req;

	struct nvmet_fc_tgtport		*tgtport;
	void				*hosthandle;

	int				ls_error;
	struct list_head		lsreq_list;  
	bool				req_queued;
};


 
#define NVMET_FC_MAX_SEQ_LENGTH		(256 * 1024)

enum nvmet_fcp_datadir {
	NVMET_FCP_NODATA,
	NVMET_FCP_WRITE,
	NVMET_FCP_READ,
	NVMET_FCP_ABORTED,
};

struct nvmet_fc_fcp_iod {
	struct nvmefc_tgt_fcp_req	*fcpreq;

	struct nvme_fc_cmd_iu		cmdiubuf;
	struct nvme_fc_ersp_iu		rspiubuf;
	dma_addr_t			rspdma;
	struct scatterlist		*next_sg;
	struct scatterlist		*data_sg;
	int				data_sg_cnt;
	u32				offset;
	enum nvmet_fcp_datadir		io_dir;
	bool				active;
	bool				abort;
	bool				aborted;
	bool				writedataactive;
	spinlock_t			flock;

	struct nvmet_req		req;
	struct work_struct		defer_work;

	struct nvmet_fc_tgtport		*tgtport;
	struct nvmet_fc_tgt_queue	*queue;

	struct list_head		fcp_list;	 
};

struct nvmet_fc_tgtport {
	struct nvmet_fc_target_port	fc_target_port;

	struct list_head		tgt_list;  
	struct device			*dev;	 
	struct nvmet_fc_target_template	*ops;

	struct nvmet_fc_ls_iod		*iod;
	spinlock_t			lock;
	struct list_head		ls_rcv_list;
	struct list_head		ls_req_list;
	struct list_head		ls_busylist;
	struct list_head		assoc_list;
	struct list_head		host_list;
	struct ida			assoc_cnt;
	struct nvmet_fc_port_entry	*pe;
	struct kref			ref;
	u32				max_sg_cnt;
};

struct nvmet_fc_port_entry {
	struct nvmet_fc_tgtport		*tgtport;
	struct nvmet_port		*port;
	u64				node_name;
	u64				port_name;
	struct list_head		pe_list;
};

struct nvmet_fc_defer_fcp_req {
	struct list_head		req_list;
	struct nvmefc_tgt_fcp_req	*fcp_req;
};

struct nvmet_fc_tgt_queue {
	bool				ninetypercent;
	u16				qid;
	u16				sqsize;
	u16				ersp_ratio;
	__le16				sqhd;
	atomic_t			connected;
	atomic_t			sqtail;
	atomic_t			zrspcnt;
	atomic_t			rsn;
	spinlock_t			qlock;
	struct nvmet_cq			nvme_cq;
	struct nvmet_sq			nvme_sq;
	struct nvmet_fc_tgt_assoc	*assoc;
	struct list_head		fod_list;
	struct list_head		pending_cmd_list;
	struct list_head		avail_defer_list;
	struct workqueue_struct		*work_q;
	struct kref			ref;
	struct rcu_head			rcu;
	struct nvmet_fc_fcp_iod		fod[];		 
} __aligned(sizeof(unsigned long long));

struct nvmet_fc_hostport {
	struct nvmet_fc_tgtport		*tgtport;
	void				*hosthandle;
	struct list_head		host_list;
	struct kref			ref;
	u8				invalid;
};

struct nvmet_fc_tgt_assoc {
	u64				association_id;
	u32				a_id;
	atomic_t			terminating;
	struct nvmet_fc_tgtport		*tgtport;
	struct nvmet_fc_hostport	*hostport;
	struct nvmet_fc_ls_iod		*rcv_disconn;
	struct list_head		a_list;
	struct nvmet_fc_tgt_queue __rcu	*queues[NVMET_NR_QUEUES + 1];
	struct kref			ref;
	struct work_struct		del_work;
	struct rcu_head			rcu;
};


static inline int
nvmet_fc_iodnum(struct nvmet_fc_ls_iod *iodptr)
{
	return (iodptr - iodptr->tgtport->iod);
}

static inline int
nvmet_fc_fodnum(struct nvmet_fc_fcp_iod *fodptr)
{
	return (fodptr - fodptr->queue->fod);
}


 
#define BYTES_FOR_QID			sizeof(u16)
#define BYTES_FOR_QID_SHIFT		(BYTES_FOR_QID * 8)
#define NVMET_FC_QUEUEID_MASK		((u64)((1 << BYTES_FOR_QID_SHIFT) - 1))

static inline u64
nvmet_fc_makeconnid(struct nvmet_fc_tgt_assoc *assoc, u16 qid)
{
	return (assoc->association_id | qid);
}

static inline u64
nvmet_fc_getassociationid(u64 connectionid)
{
	return connectionid & ~NVMET_FC_QUEUEID_MASK;
}

static inline u16
nvmet_fc_getqueueid(u64 connectionid)
{
	return (u16)(connectionid & NVMET_FC_QUEUEID_MASK);
}

static inline struct nvmet_fc_tgtport *
targetport_to_tgtport(struct nvmet_fc_target_port *targetport)
{
	return container_of(targetport, struct nvmet_fc_tgtport,
				 fc_target_port);
}

static inline struct nvmet_fc_fcp_iod *
nvmet_req_to_fod(struct nvmet_req *nvme_req)
{
	return container_of(nvme_req, struct nvmet_fc_fcp_iod, req);
}


 


static DEFINE_SPINLOCK(nvmet_fc_tgtlock);

static LIST_HEAD(nvmet_fc_target_list);
static DEFINE_IDA(nvmet_fc_tgtport_cnt);
static LIST_HEAD(nvmet_fc_portentry_list);


static void nvmet_fc_handle_ls_rqst_work(struct work_struct *work);
static void nvmet_fc_fcp_rqst_op_defer_work(struct work_struct *work);
static void nvmet_fc_tgt_a_put(struct nvmet_fc_tgt_assoc *assoc);
static int nvmet_fc_tgt_a_get(struct nvmet_fc_tgt_assoc *assoc);
static void nvmet_fc_tgt_q_put(struct nvmet_fc_tgt_queue *queue);
static int nvmet_fc_tgt_q_get(struct nvmet_fc_tgt_queue *queue);
static void nvmet_fc_tgtport_put(struct nvmet_fc_tgtport *tgtport);
static int nvmet_fc_tgtport_get(struct nvmet_fc_tgtport *tgtport);
static void nvmet_fc_handle_fcp_rqst(struct nvmet_fc_tgtport *tgtport,
					struct nvmet_fc_fcp_iod *fod);
static void nvmet_fc_delete_target_assoc(struct nvmet_fc_tgt_assoc *assoc);
static void nvmet_fc_xmt_ls_rsp(struct nvmet_fc_tgtport *tgtport,
				struct nvmet_fc_ls_iod *iod);


 

 

static inline dma_addr_t
fc_dma_map_single(struct device *dev, void *ptr, size_t size,
		enum dma_data_direction dir)
{
	return dev ? dma_map_single(dev, ptr, size, dir) : (dma_addr_t)0L;
}

static inline int
fc_dma_mapping_error(struct device *dev, dma_addr_t dma_addr)
{
	return dev ? dma_mapping_error(dev, dma_addr) : 0;
}

static inline void
fc_dma_unmap_single(struct device *dev, dma_addr_t addr, size_t size,
	enum dma_data_direction dir)
{
	if (dev)
		dma_unmap_single(dev, addr, size, dir);
}

static inline void
fc_dma_sync_single_for_cpu(struct device *dev, dma_addr_t addr, size_t size,
		enum dma_data_direction dir)
{
	if (dev)
		dma_sync_single_for_cpu(dev, addr, size, dir);
}

static inline void
fc_dma_sync_single_for_device(struct device *dev, dma_addr_t addr, size_t size,
		enum dma_data_direction dir)
{
	if (dev)
		dma_sync_single_for_device(dev, addr, size, dir);
}

 
static int
fc_map_sg(struct scatterlist *sg, int nents)
{
	struct scatterlist *s;
	int i;

	WARN_ON(nents == 0 || sg[0].length == 0);

	for_each_sg(sg, s, nents, i) {
		s->dma_address = 0L;
#ifdef CONFIG_NEED_SG_DMA_LENGTH
		s->dma_length = s->length;
#endif
	}
	return nents;
}

static inline int
fc_dma_map_sg(struct device *dev, struct scatterlist *sg, int nents,
		enum dma_data_direction dir)
{
	return dev ? dma_map_sg(dev, sg, nents, dir) : fc_map_sg(sg, nents);
}

static inline void
fc_dma_unmap_sg(struct device *dev, struct scatterlist *sg, int nents,
		enum dma_data_direction dir)
{
	if (dev)
		dma_unmap_sg(dev, sg, nents, dir);
}


 


static void
__nvmet_fc_finish_ls_req(struct nvmet_fc_ls_req_op *lsop)
{
	struct nvmet_fc_tgtport *tgtport = lsop->tgtport;
	struct nvmefc_ls_req *lsreq = &lsop->ls_req;
	unsigned long flags;

	spin_lock_irqsave(&tgtport->lock, flags);

	if (!lsop->req_queued) {
		spin_unlock_irqrestore(&tgtport->lock, flags);
		return;
	}

	list_del(&lsop->lsreq_list);

	lsop->req_queued = false;

	spin_unlock_irqrestore(&tgtport->lock, flags);

	fc_dma_unmap_single(tgtport->dev, lsreq->rqstdma,
				  (lsreq->rqstlen + lsreq->rsplen),
				  DMA_BIDIRECTIONAL);

	nvmet_fc_tgtport_put(tgtport);
}

static int
__nvmet_fc_send_ls_req(struct nvmet_fc_tgtport *tgtport,
		struct nvmet_fc_ls_req_op *lsop,
		void (*done)(struct nvmefc_ls_req *req, int status))
{
	struct nvmefc_ls_req *lsreq = &lsop->ls_req;
	unsigned long flags;
	int ret = 0;

	if (!tgtport->ops->ls_req)
		return -EOPNOTSUPP;

	if (!nvmet_fc_tgtport_get(tgtport))
		return -ESHUTDOWN;

	lsreq->done = done;
	lsop->req_queued = false;
	INIT_LIST_HEAD(&lsop->lsreq_list);

	lsreq->rqstdma = fc_dma_map_single(tgtport->dev, lsreq->rqstaddr,
				  lsreq->rqstlen + lsreq->rsplen,
				  DMA_BIDIRECTIONAL);
	if (fc_dma_mapping_error(tgtport->dev, lsreq->rqstdma)) {
		ret = -EFAULT;
		goto out_puttgtport;
	}
	lsreq->rspdma = lsreq->rqstdma + lsreq->rqstlen;

	spin_lock_irqsave(&tgtport->lock, flags);

	list_add_tail(&lsop->lsreq_list, &tgtport->ls_req_list);

	lsop->req_queued = true;

	spin_unlock_irqrestore(&tgtport->lock, flags);

	ret = tgtport->ops->ls_req(&tgtport->fc_target_port, lsop->hosthandle,
				   lsreq);
	if (ret)
		goto out_unlink;

	return 0;

out_unlink:
	lsop->ls_error = ret;
	spin_lock_irqsave(&tgtport->lock, flags);
	lsop->req_queued = false;
	list_del(&lsop->lsreq_list);
	spin_unlock_irqrestore(&tgtport->lock, flags);
	fc_dma_unmap_single(tgtport->dev, lsreq->rqstdma,
				  (lsreq->rqstlen + lsreq->rsplen),
				  DMA_BIDIRECTIONAL);
out_puttgtport:
	nvmet_fc_tgtport_put(tgtport);

	return ret;
}

static int
nvmet_fc_send_ls_req_async(struct nvmet_fc_tgtport *tgtport,
		struct nvmet_fc_ls_req_op *lsop,
		void (*done)(struct nvmefc_ls_req *req, int status))
{
	 

	return __nvmet_fc_send_ls_req(tgtport, lsop, done);
}

static void
nvmet_fc_disconnect_assoc_done(struct nvmefc_ls_req *lsreq, int status)
{
	struct nvmet_fc_ls_req_op *lsop =
		container_of(lsreq, struct nvmet_fc_ls_req_op, ls_req);

	__nvmet_fc_finish_ls_req(lsop);

	 

	kfree(lsop);
}

 
static void
nvmet_fc_xmt_disconnect_assoc(struct nvmet_fc_tgt_assoc *assoc)
{
	struct nvmet_fc_tgtport *tgtport = assoc->tgtport;
	struct fcnvme_ls_disconnect_assoc_rqst *discon_rqst;
	struct fcnvme_ls_disconnect_assoc_acc *discon_acc;
	struct nvmet_fc_ls_req_op *lsop;
	struct nvmefc_ls_req *lsreq;
	int ret;

	 
	if (!tgtport->ops->ls_req || !assoc->hostport ||
	    assoc->hostport->invalid)
		return;

	lsop = kzalloc((sizeof(*lsop) +
			sizeof(*discon_rqst) + sizeof(*discon_acc) +
			tgtport->ops->lsrqst_priv_sz), GFP_KERNEL);
	if (!lsop) {
		dev_info(tgtport->dev,
			"{%d:%d} send Disconnect Association failed: ENOMEM\n",
			tgtport->fc_target_port.port_num, assoc->a_id);
		return;
	}

	discon_rqst = (struct fcnvme_ls_disconnect_assoc_rqst *)&lsop[1];
	discon_acc = (struct fcnvme_ls_disconnect_assoc_acc *)&discon_rqst[1];
	lsreq = &lsop->ls_req;
	if (tgtport->ops->lsrqst_priv_sz)
		lsreq->private = (void *)&discon_acc[1];
	else
		lsreq->private = NULL;

	lsop->tgtport = tgtport;
	lsop->hosthandle = assoc->hostport->hosthandle;

	nvmefc_fmt_lsreq_discon_assoc(lsreq, discon_rqst, discon_acc,
				assoc->association_id);

	ret = nvmet_fc_send_ls_req_async(tgtport, lsop,
				nvmet_fc_disconnect_assoc_done);
	if (ret) {
		dev_info(tgtport->dev,
			"{%d:%d} XMT Disconnect Association failed: %d\n",
			tgtport->fc_target_port.port_num, assoc->a_id, ret);
		kfree(lsop);
	}
}


 


static int
nvmet_fc_alloc_ls_iodlist(struct nvmet_fc_tgtport *tgtport)
{
	struct nvmet_fc_ls_iod *iod;
	int i;

	iod = kcalloc(NVMET_LS_CTX_COUNT, sizeof(struct nvmet_fc_ls_iod),
			GFP_KERNEL);
	if (!iod)
		return -ENOMEM;

	tgtport->iod = iod;

	for (i = 0; i < NVMET_LS_CTX_COUNT; iod++, i++) {
		INIT_WORK(&iod->work, nvmet_fc_handle_ls_rqst_work);
		iod->tgtport = tgtport;
		list_add_tail(&iod->ls_rcv_list, &tgtport->ls_rcv_list);

		iod->rqstbuf = kzalloc(sizeof(union nvmefc_ls_requests) +
				       sizeof(union nvmefc_ls_responses),
				       GFP_KERNEL);
		if (!iod->rqstbuf)
			goto out_fail;

		iod->rspbuf = (union nvmefc_ls_responses *)&iod->rqstbuf[1];

		iod->rspdma = fc_dma_map_single(tgtport->dev, iod->rspbuf,
						sizeof(*iod->rspbuf),
						DMA_TO_DEVICE);
		if (fc_dma_mapping_error(tgtport->dev, iod->rspdma))
			goto out_fail;
	}

	return 0;

out_fail:
	kfree(iod->rqstbuf);
	list_del(&iod->ls_rcv_list);
	for (iod--, i--; i >= 0; iod--, i--) {
		fc_dma_unmap_single(tgtport->dev, iod->rspdma,
				sizeof(*iod->rspbuf), DMA_TO_DEVICE);
		kfree(iod->rqstbuf);
		list_del(&iod->ls_rcv_list);
	}

	kfree(iod);

	return -EFAULT;
}

static void
nvmet_fc_free_ls_iodlist(struct nvmet_fc_tgtport *tgtport)
{
	struct nvmet_fc_ls_iod *iod = tgtport->iod;
	int i;

	for (i = 0; i < NVMET_LS_CTX_COUNT; iod++, i++) {
		fc_dma_unmap_single(tgtport->dev,
				iod->rspdma, sizeof(*iod->rspbuf),
				DMA_TO_DEVICE);
		kfree(iod->rqstbuf);
		list_del(&iod->ls_rcv_list);
	}
	kfree(tgtport->iod);
}

static struct nvmet_fc_ls_iod *
nvmet_fc_alloc_ls_iod(struct nvmet_fc_tgtport *tgtport)
{
	struct nvmet_fc_ls_iod *iod;
	unsigned long flags;

	spin_lock_irqsave(&tgtport->lock, flags);
	iod = list_first_entry_or_null(&tgtport->ls_rcv_list,
					struct nvmet_fc_ls_iod, ls_rcv_list);
	if (iod)
		list_move_tail(&iod->ls_rcv_list, &tgtport->ls_busylist);
	spin_unlock_irqrestore(&tgtport->lock, flags);
	return iod;
}


static void
nvmet_fc_free_ls_iod(struct nvmet_fc_tgtport *tgtport,
			struct nvmet_fc_ls_iod *iod)
{
	unsigned long flags;

	spin_lock_irqsave(&tgtport->lock, flags);
	list_move(&iod->ls_rcv_list, &tgtport->ls_rcv_list);
	spin_unlock_irqrestore(&tgtport->lock, flags);
}

static void
nvmet_fc_prep_fcp_iodlist(struct nvmet_fc_tgtport *tgtport,
				struct nvmet_fc_tgt_queue *queue)
{
	struct nvmet_fc_fcp_iod *fod = queue->fod;
	int i;

	for (i = 0; i < queue->sqsize; fod++, i++) {
		INIT_WORK(&fod->defer_work, nvmet_fc_fcp_rqst_op_defer_work);
		fod->tgtport = tgtport;
		fod->queue = queue;
		fod->active = false;
		fod->abort = false;
		fod->aborted = false;
		fod->fcpreq = NULL;
		list_add_tail(&fod->fcp_list, &queue->fod_list);
		spin_lock_init(&fod->flock);

		fod->rspdma = fc_dma_map_single(tgtport->dev, &fod->rspiubuf,
					sizeof(fod->rspiubuf), DMA_TO_DEVICE);
		if (fc_dma_mapping_error(tgtport->dev, fod->rspdma)) {
			list_del(&fod->fcp_list);
			for (fod--, i--; i >= 0; fod--, i--) {
				fc_dma_unmap_single(tgtport->dev, fod->rspdma,
						sizeof(fod->rspiubuf),
						DMA_TO_DEVICE);
				fod->rspdma = 0L;
				list_del(&fod->fcp_list);
			}

			return;
		}
	}
}

static void
nvmet_fc_destroy_fcp_iodlist(struct nvmet_fc_tgtport *tgtport,
				struct nvmet_fc_tgt_queue *queue)
{
	struct nvmet_fc_fcp_iod *fod = queue->fod;
	int i;

	for (i = 0; i < queue->sqsize; fod++, i++) {
		if (fod->rspdma)
			fc_dma_unmap_single(tgtport->dev, fod->rspdma,
				sizeof(fod->rspiubuf), DMA_TO_DEVICE);
	}
}

static struct nvmet_fc_fcp_iod *
nvmet_fc_alloc_fcp_iod(struct nvmet_fc_tgt_queue *queue)
{
	struct nvmet_fc_fcp_iod *fod;

	lockdep_assert_held(&queue->qlock);

	fod = list_first_entry_or_null(&queue->fod_list,
					struct nvmet_fc_fcp_iod, fcp_list);
	if (fod) {
		list_del(&fod->fcp_list);
		fod->active = true;
		 
	}
	return fod;
}


static void
nvmet_fc_queue_fcp_req(struct nvmet_fc_tgtport *tgtport,
		       struct nvmet_fc_tgt_queue *queue,
		       struct nvmefc_tgt_fcp_req *fcpreq)
{
	struct nvmet_fc_fcp_iod *fod = fcpreq->nvmet_fc_private;

	 
	fcpreq->hwqid = queue->qid ?
			((queue->qid - 1) % tgtport->ops->max_hw_queues) : 0;

	nvmet_fc_handle_fcp_rqst(tgtport, fod);
}

static void
nvmet_fc_fcp_rqst_op_defer_work(struct work_struct *work)
{
	struct nvmet_fc_fcp_iod *fod =
		container_of(work, struct nvmet_fc_fcp_iod, defer_work);

	 
	nvmet_fc_queue_fcp_req(fod->tgtport, fod->queue, fod->fcpreq);

}

static void
nvmet_fc_free_fcp_iod(struct nvmet_fc_tgt_queue *queue,
			struct nvmet_fc_fcp_iod *fod)
{
	struct nvmefc_tgt_fcp_req *fcpreq = fod->fcpreq;
	struct nvmet_fc_tgtport *tgtport = fod->tgtport;
	struct nvmet_fc_defer_fcp_req *deferfcp;
	unsigned long flags;

	fc_dma_sync_single_for_cpu(tgtport->dev, fod->rspdma,
				sizeof(fod->rspiubuf), DMA_TO_DEVICE);

	fcpreq->nvmet_fc_private = NULL;

	fod->active = false;
	fod->abort = false;
	fod->aborted = false;
	fod->writedataactive = false;
	fod->fcpreq = NULL;

	tgtport->ops->fcp_req_release(&tgtport->fc_target_port, fcpreq);

	 
	nvmet_fc_tgt_q_put(queue);

	spin_lock_irqsave(&queue->qlock, flags);
	deferfcp = list_first_entry_or_null(&queue->pending_cmd_list,
				struct nvmet_fc_defer_fcp_req, req_list);
	if (!deferfcp) {
		list_add_tail(&fod->fcp_list, &fod->queue->fod_list);
		spin_unlock_irqrestore(&queue->qlock, flags);
		return;
	}

	 
	list_del(&deferfcp->req_list);

	fcpreq = deferfcp->fcp_req;

	 
	list_add_tail(&deferfcp->req_list, &queue->avail_defer_list);

	spin_unlock_irqrestore(&queue->qlock, flags);

	 
	memcpy(&fod->cmdiubuf, fcpreq->rspaddr, fcpreq->rsplen);

	 
	fcpreq->rspaddr = NULL;
	fcpreq->rsplen  = 0;
	fcpreq->nvmet_fc_private = fod;
	fod->fcpreq = fcpreq;
	fod->active = true;

	 
	tgtport->ops->defer_rcv(&tgtport->fc_target_port, fcpreq);

	 

	queue_work(queue->work_q, &fod->defer_work);
}

static struct nvmet_fc_tgt_queue *
nvmet_fc_alloc_target_queue(struct nvmet_fc_tgt_assoc *assoc,
			u16 qid, u16 sqsize)
{
	struct nvmet_fc_tgt_queue *queue;
	int ret;

	if (qid > NVMET_NR_QUEUES)
		return NULL;

	queue = kzalloc(struct_size(queue, fod, sqsize), GFP_KERNEL);
	if (!queue)
		return NULL;

	if (!nvmet_fc_tgt_a_get(assoc))
		goto out_free_queue;

	queue->work_q = alloc_workqueue("ntfc%d.%d.%d", 0, 0,
				assoc->tgtport->fc_target_port.port_num,
				assoc->a_id, qid);
	if (!queue->work_q)
		goto out_a_put;

	queue->qid = qid;
	queue->sqsize = sqsize;
	queue->assoc = assoc;
	INIT_LIST_HEAD(&queue->fod_list);
	INIT_LIST_HEAD(&queue->avail_defer_list);
	INIT_LIST_HEAD(&queue->pending_cmd_list);
	atomic_set(&queue->connected, 0);
	atomic_set(&queue->sqtail, 0);
	atomic_set(&queue->rsn, 1);
	atomic_set(&queue->zrspcnt, 0);
	spin_lock_init(&queue->qlock);
	kref_init(&queue->ref);

	nvmet_fc_prep_fcp_iodlist(assoc->tgtport, queue);

	ret = nvmet_sq_init(&queue->nvme_sq);
	if (ret)
		goto out_fail_iodlist;

	WARN_ON(assoc->queues[qid]);
	rcu_assign_pointer(assoc->queues[qid], queue);

	return queue;

out_fail_iodlist:
	nvmet_fc_destroy_fcp_iodlist(assoc->tgtport, queue);
	destroy_workqueue(queue->work_q);
out_a_put:
	nvmet_fc_tgt_a_put(assoc);
out_free_queue:
	kfree(queue);
	return NULL;
}


static void
nvmet_fc_tgt_queue_free(struct kref *ref)
{
	struct nvmet_fc_tgt_queue *queue =
		container_of(ref, struct nvmet_fc_tgt_queue, ref);

	rcu_assign_pointer(queue->assoc->queues[queue->qid], NULL);

	nvmet_fc_destroy_fcp_iodlist(queue->assoc->tgtport, queue);

	nvmet_fc_tgt_a_put(queue->assoc);

	destroy_workqueue(queue->work_q);

	kfree_rcu(queue, rcu);
}

static void
nvmet_fc_tgt_q_put(struct nvmet_fc_tgt_queue *queue)
{
	kref_put(&queue->ref, nvmet_fc_tgt_queue_free);
}

static int
nvmet_fc_tgt_q_get(struct nvmet_fc_tgt_queue *queue)
{
	return kref_get_unless_zero(&queue->ref);
}


static void
nvmet_fc_delete_target_queue(struct nvmet_fc_tgt_queue *queue)
{
	struct nvmet_fc_tgtport *tgtport = queue->assoc->tgtport;
	struct nvmet_fc_fcp_iod *fod = queue->fod;
	struct nvmet_fc_defer_fcp_req *deferfcp, *tempptr;
	unsigned long flags;
	int i;
	bool disconnect;

	disconnect = atomic_xchg(&queue->connected, 0);

	 
	if (!disconnect)
		return;

	spin_lock_irqsave(&queue->qlock, flags);
	 
	for (i = 0; i < queue->sqsize; fod++, i++) {
		if (fod->active) {
			spin_lock(&fod->flock);
			fod->abort = true;
			 
			if (fod->writedataactive) {
				fod->aborted = true;
				spin_unlock(&fod->flock);
				tgtport->ops->fcp_abort(
					&tgtport->fc_target_port, fod->fcpreq);
			} else
				spin_unlock(&fod->flock);
		}
	}

	 
	list_for_each_entry_safe(deferfcp, tempptr, &queue->avail_defer_list,
				req_list) {
		list_del(&deferfcp->req_list);
		kfree(deferfcp);
	}

	for (;;) {
		deferfcp = list_first_entry_or_null(&queue->pending_cmd_list,
				struct nvmet_fc_defer_fcp_req, req_list);
		if (!deferfcp)
			break;

		list_del(&deferfcp->req_list);
		spin_unlock_irqrestore(&queue->qlock, flags);

		tgtport->ops->defer_rcv(&tgtport->fc_target_port,
				deferfcp->fcp_req);

		tgtport->ops->fcp_abort(&tgtport->fc_target_port,
				deferfcp->fcp_req);

		tgtport->ops->fcp_req_release(&tgtport->fc_target_port,
				deferfcp->fcp_req);

		 
		nvmet_fc_tgt_q_put(queue);

		kfree(deferfcp);

		spin_lock_irqsave(&queue->qlock, flags);
	}
	spin_unlock_irqrestore(&queue->qlock, flags);

	flush_workqueue(queue->work_q);

	nvmet_sq_destroy(&queue->nvme_sq);

	nvmet_fc_tgt_q_put(queue);
}

static struct nvmet_fc_tgt_queue *
nvmet_fc_find_target_queue(struct nvmet_fc_tgtport *tgtport,
				u64 connection_id)
{
	struct nvmet_fc_tgt_assoc *assoc;
	struct nvmet_fc_tgt_queue *queue;
	u64 association_id = nvmet_fc_getassociationid(connection_id);
	u16 qid = nvmet_fc_getqueueid(connection_id);

	if (qid > NVMET_NR_QUEUES)
		return NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(assoc, &tgtport->assoc_list, a_list) {
		if (association_id == assoc->association_id) {
			queue = rcu_dereference(assoc->queues[qid]);
			if (queue &&
			    (!atomic_read(&queue->connected) ||
			     !nvmet_fc_tgt_q_get(queue)))
				queue = NULL;
			rcu_read_unlock();
			return queue;
		}
	}
	rcu_read_unlock();
	return NULL;
}

static void
nvmet_fc_hostport_free(struct kref *ref)
{
	struct nvmet_fc_hostport *hostport =
		container_of(ref, struct nvmet_fc_hostport, ref);
	struct nvmet_fc_tgtport *tgtport = hostport->tgtport;
	unsigned long flags;

	spin_lock_irqsave(&tgtport->lock, flags);
	list_del(&hostport->host_list);
	spin_unlock_irqrestore(&tgtport->lock, flags);
	if (tgtport->ops->host_release && hostport->invalid)
		tgtport->ops->host_release(hostport->hosthandle);
	kfree(hostport);
	nvmet_fc_tgtport_put(tgtport);
}

static void
nvmet_fc_hostport_put(struct nvmet_fc_hostport *hostport)
{
	kref_put(&hostport->ref, nvmet_fc_hostport_free);
}

static int
nvmet_fc_hostport_get(struct nvmet_fc_hostport *hostport)
{
	return kref_get_unless_zero(&hostport->ref);
}

static void
nvmet_fc_free_hostport(struct nvmet_fc_hostport *hostport)
{
	 
	if (!hostport || !hostport->hosthandle)
		return;

	nvmet_fc_hostport_put(hostport);
}

static struct nvmet_fc_hostport *
nvmet_fc_match_hostport(struct nvmet_fc_tgtport *tgtport, void *hosthandle)
{
	struct nvmet_fc_hostport *host;

	lockdep_assert_held(&tgtport->lock);

	list_for_each_entry(host, &tgtport->host_list, host_list) {
		if (host->hosthandle == hosthandle && !host->invalid) {
			if (nvmet_fc_hostport_get(host))
				return (host);
		}
	}

	return NULL;
}

static struct nvmet_fc_hostport *
nvmet_fc_alloc_hostport(struct nvmet_fc_tgtport *tgtport, void *hosthandle)
{
	struct nvmet_fc_hostport *newhost, *match = NULL;
	unsigned long flags;

	 
	if (!hosthandle)
		return NULL;

	 
	if (!nvmet_fc_tgtport_get(tgtport))
		return ERR_PTR(-EINVAL);

	spin_lock_irqsave(&tgtport->lock, flags);
	match = nvmet_fc_match_hostport(tgtport, hosthandle);
	spin_unlock_irqrestore(&tgtport->lock, flags);

	if (match) {
		 
		nvmet_fc_tgtport_put(tgtport);
		return match;
	}

	newhost = kzalloc(sizeof(*newhost), GFP_KERNEL);
	if (!newhost) {
		 
		nvmet_fc_tgtport_put(tgtport);
		return ERR_PTR(-ENOMEM);
	}

	spin_lock_irqsave(&tgtport->lock, flags);
	match = nvmet_fc_match_hostport(tgtport, hosthandle);
	if (match) {
		 
		kfree(newhost);
		newhost = match;
		 
		nvmet_fc_tgtport_put(tgtport);
	} else {
		newhost->tgtport = tgtport;
		newhost->hosthandle = hosthandle;
		INIT_LIST_HEAD(&newhost->host_list);
		kref_init(&newhost->ref);

		list_add_tail(&newhost->host_list, &tgtport->host_list);
	}
	spin_unlock_irqrestore(&tgtport->lock, flags);

	return newhost;
}

static void
nvmet_fc_delete_assoc(struct work_struct *work)
{
	struct nvmet_fc_tgt_assoc *assoc =
		container_of(work, struct nvmet_fc_tgt_assoc, del_work);

	nvmet_fc_delete_target_assoc(assoc);
	nvmet_fc_tgt_a_put(assoc);
}

static struct nvmet_fc_tgt_assoc *
nvmet_fc_alloc_target_assoc(struct nvmet_fc_tgtport *tgtport, void *hosthandle)
{
	struct nvmet_fc_tgt_assoc *assoc, *tmpassoc;
	unsigned long flags;
	u64 ran;
	int idx;
	bool needrandom = true;

	assoc = kzalloc(sizeof(*assoc), GFP_KERNEL);
	if (!assoc)
		return NULL;

	idx = ida_alloc(&tgtport->assoc_cnt, GFP_KERNEL);
	if (idx < 0)
		goto out_free_assoc;

	if (!nvmet_fc_tgtport_get(tgtport))
		goto out_ida;

	assoc->hostport = nvmet_fc_alloc_hostport(tgtport, hosthandle);
	if (IS_ERR(assoc->hostport))
		goto out_put;

	assoc->tgtport = tgtport;
	assoc->a_id = idx;
	INIT_LIST_HEAD(&assoc->a_list);
	kref_init(&assoc->ref);
	INIT_WORK(&assoc->del_work, nvmet_fc_delete_assoc);
	atomic_set(&assoc->terminating, 0);

	while (needrandom) {
		get_random_bytes(&ran, sizeof(ran) - BYTES_FOR_QID);
		ran = ran << BYTES_FOR_QID_SHIFT;

		spin_lock_irqsave(&tgtport->lock, flags);
		needrandom = false;
		list_for_each_entry(tmpassoc, &tgtport->assoc_list, a_list) {
			if (ran == tmpassoc->association_id) {
				needrandom = true;
				break;
			}
		}
		if (!needrandom) {
			assoc->association_id = ran;
			list_add_tail_rcu(&assoc->a_list, &tgtport->assoc_list);
		}
		spin_unlock_irqrestore(&tgtport->lock, flags);
	}

	return assoc;

out_put:
	nvmet_fc_tgtport_put(tgtport);
out_ida:
	ida_free(&tgtport->assoc_cnt, idx);
out_free_assoc:
	kfree(assoc);
	return NULL;
}

static void
nvmet_fc_target_assoc_free(struct kref *ref)
{
	struct nvmet_fc_tgt_assoc *assoc =
		container_of(ref, struct nvmet_fc_tgt_assoc, ref);
	struct nvmet_fc_tgtport *tgtport = assoc->tgtport;
	struct nvmet_fc_ls_iod	*oldls;
	unsigned long flags;

	 
	nvmet_fc_xmt_disconnect_assoc(assoc);

	nvmet_fc_free_hostport(assoc->hostport);
	spin_lock_irqsave(&tgtport->lock, flags);
	list_del_rcu(&assoc->a_list);
	oldls = assoc->rcv_disconn;
	spin_unlock_irqrestore(&tgtport->lock, flags);
	 
	if (oldls)
		nvmet_fc_xmt_ls_rsp(tgtport, oldls);
	ida_free(&tgtport->assoc_cnt, assoc->a_id);
	dev_info(tgtport->dev,
		"{%d:%d} Association freed\n",
		tgtport->fc_target_port.port_num, assoc->a_id);
	kfree_rcu(assoc, rcu);
	nvmet_fc_tgtport_put(tgtport);
}

static void
nvmet_fc_tgt_a_put(struct nvmet_fc_tgt_assoc *assoc)
{
	kref_put(&assoc->ref, nvmet_fc_target_assoc_free);
}

static int
nvmet_fc_tgt_a_get(struct nvmet_fc_tgt_assoc *assoc)
{
	return kref_get_unless_zero(&assoc->ref);
}

static void
nvmet_fc_delete_target_assoc(struct nvmet_fc_tgt_assoc *assoc)
{
	struct nvmet_fc_tgtport *tgtport = assoc->tgtport;
	struct nvmet_fc_tgt_queue *queue;
	int i, terminating;

	terminating = atomic_xchg(&assoc->terminating, 1);

	 
	if (terminating)
		return;


	for (i = NVMET_NR_QUEUES; i >= 0; i--) {
		rcu_read_lock();
		queue = rcu_dereference(assoc->queues[i]);
		if (!queue) {
			rcu_read_unlock();
			continue;
		}

		if (!nvmet_fc_tgt_q_get(queue)) {
			rcu_read_unlock();
			continue;
		}
		rcu_read_unlock();
		nvmet_fc_delete_target_queue(queue);
		nvmet_fc_tgt_q_put(queue);
	}

	dev_info(tgtport->dev,
		"{%d:%d} Association deleted\n",
		tgtport->fc_target_port.port_num, assoc->a_id);

	nvmet_fc_tgt_a_put(assoc);
}

static struct nvmet_fc_tgt_assoc *
nvmet_fc_find_target_assoc(struct nvmet_fc_tgtport *tgtport,
				u64 association_id)
{
	struct nvmet_fc_tgt_assoc *assoc;
	struct nvmet_fc_tgt_assoc *ret = NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(assoc, &tgtport->assoc_list, a_list) {
		if (association_id == assoc->association_id) {
			ret = assoc;
			if (!nvmet_fc_tgt_a_get(assoc))
				ret = NULL;
			break;
		}
	}
	rcu_read_unlock();

	return ret;
}

static void
nvmet_fc_portentry_bind(struct nvmet_fc_tgtport *tgtport,
			struct nvmet_fc_port_entry *pe,
			struct nvmet_port *port)
{
	lockdep_assert_held(&nvmet_fc_tgtlock);

	pe->tgtport = tgtport;
	tgtport->pe = pe;

	pe->port = port;
	port->priv = pe;

	pe->node_name = tgtport->fc_target_port.node_name;
	pe->port_name = tgtport->fc_target_port.port_name;
	INIT_LIST_HEAD(&pe->pe_list);

	list_add_tail(&pe->pe_list, &nvmet_fc_portentry_list);
}

static void
nvmet_fc_portentry_unbind(struct nvmet_fc_port_entry *pe)
{
	unsigned long flags;

	spin_lock_irqsave(&nvmet_fc_tgtlock, flags);
	if (pe->tgtport)
		pe->tgtport->pe = NULL;
	list_del(&pe->pe_list);
	spin_unlock_irqrestore(&nvmet_fc_tgtlock, flags);
}

 
static void
nvmet_fc_portentry_unbind_tgt(struct nvmet_fc_tgtport *tgtport)
{
	struct nvmet_fc_port_entry *pe;
	unsigned long flags;

	spin_lock_irqsave(&nvmet_fc_tgtlock, flags);
	pe = tgtport->pe;
	if (pe)
		pe->tgtport = NULL;
	tgtport->pe = NULL;
	spin_unlock_irqrestore(&nvmet_fc_tgtlock, flags);
}

 
static void
nvmet_fc_portentry_rebind_tgt(struct nvmet_fc_tgtport *tgtport)
{
	struct nvmet_fc_port_entry *pe;
	unsigned long flags;

	spin_lock_irqsave(&nvmet_fc_tgtlock, flags);
	list_for_each_entry(pe, &nvmet_fc_portentry_list, pe_list) {
		if (tgtport->fc_target_port.node_name == pe->node_name &&
		    tgtport->fc_target_port.port_name == pe->port_name) {
			WARN_ON(pe->tgtport);
			tgtport->pe = pe;
			pe->tgtport = tgtport;
			break;
		}
	}
	spin_unlock_irqrestore(&nvmet_fc_tgtlock, flags);
}

 
int
nvmet_fc_register_targetport(struct nvmet_fc_port_info *pinfo,
			struct nvmet_fc_target_template *template,
			struct device *dev,
			struct nvmet_fc_target_port **portptr)
{
	struct nvmet_fc_tgtport *newrec;
	unsigned long flags;
	int ret, idx;

	if (!template->xmt_ls_rsp || !template->fcp_op ||
	    !template->fcp_abort ||
	    !template->fcp_req_release || !template->targetport_delete ||
	    !template->max_hw_queues || !template->max_sgl_segments ||
	    !template->max_dif_sgl_segments || !template->dma_boundary) {
		ret = -EINVAL;
		goto out_regtgt_failed;
	}

	newrec = kzalloc((sizeof(*newrec) + template->target_priv_sz),
			 GFP_KERNEL);
	if (!newrec) {
		ret = -ENOMEM;
		goto out_regtgt_failed;
	}

	idx = ida_alloc(&nvmet_fc_tgtport_cnt, GFP_KERNEL);
	if (idx < 0) {
		ret = -ENOSPC;
		goto out_fail_kfree;
	}

	if (!get_device(dev) && dev) {
		ret = -ENODEV;
		goto out_ida_put;
	}

	newrec->fc_target_port.node_name = pinfo->node_name;
	newrec->fc_target_port.port_name = pinfo->port_name;
	if (template->target_priv_sz)
		newrec->fc_target_port.private = &newrec[1];
	else
		newrec->fc_target_port.private = NULL;
	newrec->fc_target_port.port_id = pinfo->port_id;
	newrec->fc_target_port.port_num = idx;
	INIT_LIST_HEAD(&newrec->tgt_list);
	newrec->dev = dev;
	newrec->ops = template;
	spin_lock_init(&newrec->lock);
	INIT_LIST_HEAD(&newrec->ls_rcv_list);
	INIT_LIST_HEAD(&newrec->ls_req_list);
	INIT_LIST_HEAD(&newrec->ls_busylist);
	INIT_LIST_HEAD(&newrec->assoc_list);
	INIT_LIST_HEAD(&newrec->host_list);
	kref_init(&newrec->ref);
	ida_init(&newrec->assoc_cnt);
	newrec->max_sg_cnt = template->max_sgl_segments;

	ret = nvmet_fc_alloc_ls_iodlist(newrec);
	if (ret) {
		ret = -ENOMEM;
		goto out_free_newrec;
	}

	nvmet_fc_portentry_rebind_tgt(newrec);

	spin_lock_irqsave(&nvmet_fc_tgtlock, flags);
	list_add_tail(&newrec->tgt_list, &nvmet_fc_target_list);
	spin_unlock_irqrestore(&nvmet_fc_tgtlock, flags);

	*portptr = &newrec->fc_target_port;
	return 0;

out_free_newrec:
	put_device(dev);
out_ida_put:
	ida_free(&nvmet_fc_tgtport_cnt, idx);
out_fail_kfree:
	kfree(newrec);
out_regtgt_failed:
	*portptr = NULL;
	return ret;
}
EXPORT_SYMBOL_GPL(nvmet_fc_register_targetport);


static void
nvmet_fc_free_tgtport(struct kref *ref)
{
	struct nvmet_fc_tgtport *tgtport =
		container_of(ref, struct nvmet_fc_tgtport, ref);
	struct device *dev = tgtport->dev;
	unsigned long flags;

	spin_lock_irqsave(&nvmet_fc_tgtlock, flags);
	list_del(&tgtport->tgt_list);
	spin_unlock_irqrestore(&nvmet_fc_tgtlock, flags);

	nvmet_fc_free_ls_iodlist(tgtport);

	 
	tgtport->ops->targetport_delete(&tgtport->fc_target_port);

	ida_free(&nvmet_fc_tgtport_cnt,
			tgtport->fc_target_port.port_num);

	ida_destroy(&tgtport->assoc_cnt);

	kfree(tgtport);

	put_device(dev);
}

static void
nvmet_fc_tgtport_put(struct nvmet_fc_tgtport *tgtport)
{
	kref_put(&tgtport->ref, nvmet_fc_free_tgtport);
}

static int
nvmet_fc_tgtport_get(struct nvmet_fc_tgtport *tgtport)
{
	return kref_get_unless_zero(&tgtport->ref);
}

static void
__nvmet_fc_free_assocs(struct nvmet_fc_tgtport *tgtport)
{
	struct nvmet_fc_tgt_assoc *assoc;

	rcu_read_lock();
	list_for_each_entry_rcu(assoc, &tgtport->assoc_list, a_list) {
		if (!nvmet_fc_tgt_a_get(assoc))
			continue;
		if (!queue_work(nvmet_wq, &assoc->del_work))
			 
			nvmet_fc_tgt_a_put(assoc);
	}
	rcu_read_unlock();
}

 
void
nvmet_fc_invalidate_host(struct nvmet_fc_target_port *target_port,
			void *hosthandle)
{
	struct nvmet_fc_tgtport *tgtport = targetport_to_tgtport(target_port);
	struct nvmet_fc_tgt_assoc *assoc, *next;
	unsigned long flags;
	bool noassoc = true;

	spin_lock_irqsave(&tgtport->lock, flags);
	list_for_each_entry_safe(assoc, next,
				&tgtport->assoc_list, a_list) {
		if (!assoc->hostport ||
		    assoc->hostport->hosthandle != hosthandle)
			continue;
		if (!nvmet_fc_tgt_a_get(assoc))
			continue;
		assoc->hostport->invalid = 1;
		noassoc = false;
		if (!queue_work(nvmet_wq, &assoc->del_work))
			 
			nvmet_fc_tgt_a_put(assoc);
	}
	spin_unlock_irqrestore(&tgtport->lock, flags);

	 
	if (noassoc && tgtport->ops->host_release)
		tgtport->ops->host_release(hosthandle);
}
EXPORT_SYMBOL_GPL(nvmet_fc_invalidate_host);

 
static void
nvmet_fc_delete_ctrl(struct nvmet_ctrl *ctrl)
{
	struct nvmet_fc_tgtport *tgtport, *next;
	struct nvmet_fc_tgt_assoc *assoc;
	struct nvmet_fc_tgt_queue *queue;
	unsigned long flags;
	bool found_ctrl = false;

	 
	spin_lock_irqsave(&nvmet_fc_tgtlock, flags);
	list_for_each_entry_safe(tgtport, next, &nvmet_fc_target_list,
			tgt_list) {
		if (!nvmet_fc_tgtport_get(tgtport))
			continue;
		spin_unlock_irqrestore(&nvmet_fc_tgtlock, flags);

		rcu_read_lock();
		list_for_each_entry_rcu(assoc, &tgtport->assoc_list, a_list) {
			queue = rcu_dereference(assoc->queues[0]);
			if (queue && queue->nvme_sq.ctrl == ctrl) {
				if (nvmet_fc_tgt_a_get(assoc))
					found_ctrl = true;
				break;
			}
		}
		rcu_read_unlock();

		nvmet_fc_tgtport_put(tgtport);

		if (found_ctrl) {
			if (!queue_work(nvmet_wq, &assoc->del_work))
				 
				nvmet_fc_tgt_a_put(assoc);
			return;
		}

		spin_lock_irqsave(&nvmet_fc_tgtlock, flags);
	}
	spin_unlock_irqrestore(&nvmet_fc_tgtlock, flags);
}

 
int
nvmet_fc_unregister_targetport(struct nvmet_fc_target_port *target_port)
{
	struct nvmet_fc_tgtport *tgtport = targetport_to_tgtport(target_port);

	nvmet_fc_portentry_unbind_tgt(tgtport);

	 
	__nvmet_fc_free_assocs(tgtport);

	 

	nvmet_fc_tgtport_put(tgtport);

	return 0;
}
EXPORT_SYMBOL_GPL(nvmet_fc_unregister_targetport);


 


static void
nvmet_fc_ls_create_association(struct nvmet_fc_tgtport *tgtport,
			struct nvmet_fc_ls_iod *iod)
{
	struct fcnvme_ls_cr_assoc_rqst *rqst = &iod->rqstbuf->rq_cr_assoc;
	struct fcnvme_ls_cr_assoc_acc *acc = &iod->rspbuf->rsp_cr_assoc;
	struct nvmet_fc_tgt_queue *queue;
	int ret = 0;

	memset(acc, 0, sizeof(*acc));

	 
	if (iod->rqstdatalen < FCNVME_LSDESC_CRA_RQST_MINLEN)
		ret = VERR_CR_ASSOC_LEN;
	else if (be32_to_cpu(rqst->desc_list_len) <
			FCNVME_LSDESC_CRA_RQST_MIN_LISTLEN)
		ret = VERR_CR_ASSOC_RQST_LEN;
	else if (rqst->assoc_cmd.desc_tag !=
			cpu_to_be32(FCNVME_LSDESC_CREATE_ASSOC_CMD))
		ret = VERR_CR_ASSOC_CMD;
	else if (be32_to_cpu(rqst->assoc_cmd.desc_len) <
			FCNVME_LSDESC_CRA_CMD_DESC_MIN_DESCLEN)
		ret = VERR_CR_ASSOC_CMD_LEN;
	else if (!rqst->assoc_cmd.ersp_ratio ||
		 (be16_to_cpu(rqst->assoc_cmd.ersp_ratio) >=
				be16_to_cpu(rqst->assoc_cmd.sqsize)))
		ret = VERR_ERSP_RATIO;

	else {
		 
		iod->assoc = nvmet_fc_alloc_target_assoc(
						tgtport, iod->hosthandle);
		if (!iod->assoc)
			ret = VERR_ASSOC_ALLOC_FAIL;
		else {
			queue = nvmet_fc_alloc_target_queue(iod->assoc, 0,
					be16_to_cpu(rqst->assoc_cmd.sqsize));
			if (!queue) {
				ret = VERR_QUEUE_ALLOC_FAIL;
				nvmet_fc_tgt_a_put(iod->assoc);
			}
		}
	}

	if (ret) {
		dev_err(tgtport->dev,
			"Create Association LS failed: %s\n",
			validation_errors[ret]);
		iod->lsrsp->rsplen = nvme_fc_format_rjt(acc,
				sizeof(*acc), rqst->w0.ls_cmd,
				FCNVME_RJT_RC_LOGIC,
				FCNVME_RJT_EXP_NONE, 0);
		return;
	}

	queue->ersp_ratio = be16_to_cpu(rqst->assoc_cmd.ersp_ratio);
	atomic_set(&queue->connected, 1);
	queue->sqhd = 0;	 

	dev_info(tgtport->dev,
		"{%d:%d} Association created\n",
		tgtport->fc_target_port.port_num, iod->assoc->a_id);

	 

	iod->lsrsp->rsplen = sizeof(*acc);

	nvme_fc_format_rsp_hdr(acc, FCNVME_LS_ACC,
			fcnvme_lsdesc_len(
				sizeof(struct fcnvme_ls_cr_assoc_acc)),
			FCNVME_LS_CREATE_ASSOCIATION);
	acc->associd.desc_tag = cpu_to_be32(FCNVME_LSDESC_ASSOC_ID);
	acc->associd.desc_len =
			fcnvme_lsdesc_len(
				sizeof(struct fcnvme_lsdesc_assoc_id));
	acc->associd.association_id =
			cpu_to_be64(nvmet_fc_makeconnid(iod->assoc, 0));
	acc->connectid.desc_tag = cpu_to_be32(FCNVME_LSDESC_CONN_ID);
	acc->connectid.desc_len =
			fcnvme_lsdesc_len(
				sizeof(struct fcnvme_lsdesc_conn_id));
	acc->connectid.connection_id = acc->associd.association_id;
}

static void
nvmet_fc_ls_create_connection(struct nvmet_fc_tgtport *tgtport,
			struct nvmet_fc_ls_iod *iod)
{
	struct fcnvme_ls_cr_conn_rqst *rqst = &iod->rqstbuf->rq_cr_conn;
	struct fcnvme_ls_cr_conn_acc *acc = &iod->rspbuf->rsp_cr_conn;
	struct nvmet_fc_tgt_queue *queue;
	int ret = 0;

	memset(acc, 0, sizeof(*acc));

	if (iod->rqstdatalen < sizeof(struct fcnvme_ls_cr_conn_rqst))
		ret = VERR_CR_CONN_LEN;
	else if (rqst->desc_list_len !=
			fcnvme_lsdesc_len(
				sizeof(struct fcnvme_ls_cr_conn_rqst)))
		ret = VERR_CR_CONN_RQST_LEN;
	else if (rqst->associd.desc_tag != cpu_to_be32(FCNVME_LSDESC_ASSOC_ID))
		ret = VERR_ASSOC_ID;
	else if (rqst->associd.desc_len !=
			fcnvme_lsdesc_len(
				sizeof(struct fcnvme_lsdesc_assoc_id)))
		ret = VERR_ASSOC_ID_LEN;
	else if (rqst->connect_cmd.desc_tag !=
			cpu_to_be32(FCNVME_LSDESC_CREATE_CONN_CMD))
		ret = VERR_CR_CONN_CMD;
	else if (rqst->connect_cmd.desc_len !=
			fcnvme_lsdesc_len(
				sizeof(struct fcnvme_lsdesc_cr_conn_cmd)))
		ret = VERR_CR_CONN_CMD_LEN;
	else if (!rqst->connect_cmd.ersp_ratio ||
		 (be16_to_cpu(rqst->connect_cmd.ersp_ratio) >=
				be16_to_cpu(rqst->connect_cmd.sqsize)))
		ret = VERR_ERSP_RATIO;

	else {
		 
		iod->assoc = nvmet_fc_find_target_assoc(tgtport,
				be64_to_cpu(rqst->associd.association_id));
		if (!iod->assoc)
			ret = VERR_NO_ASSOC;
		else {
			queue = nvmet_fc_alloc_target_queue(iod->assoc,
					be16_to_cpu(rqst->connect_cmd.qid),
					be16_to_cpu(rqst->connect_cmd.sqsize));
			if (!queue)
				ret = VERR_QUEUE_ALLOC_FAIL;

			 
			nvmet_fc_tgt_a_put(iod->assoc);
		}
	}

	if (ret) {
		dev_err(tgtport->dev,
			"Create Connection LS failed: %s\n",
			validation_errors[ret]);
		iod->lsrsp->rsplen = nvme_fc_format_rjt(acc,
				sizeof(*acc), rqst->w0.ls_cmd,
				(ret == VERR_NO_ASSOC) ?
					FCNVME_RJT_RC_INV_ASSOC :
					FCNVME_RJT_RC_LOGIC,
				FCNVME_RJT_EXP_NONE, 0);
		return;
	}

	queue->ersp_ratio = be16_to_cpu(rqst->connect_cmd.ersp_ratio);
	atomic_set(&queue->connected, 1);
	queue->sqhd = 0;	 

	 

	iod->lsrsp->rsplen = sizeof(*acc);

	nvme_fc_format_rsp_hdr(acc, FCNVME_LS_ACC,
			fcnvme_lsdesc_len(sizeof(struct fcnvme_ls_cr_conn_acc)),
			FCNVME_LS_CREATE_CONNECTION);
	acc->connectid.desc_tag = cpu_to_be32(FCNVME_LSDESC_CONN_ID);
	acc->connectid.desc_len =
			fcnvme_lsdesc_len(
				sizeof(struct fcnvme_lsdesc_conn_id));
	acc->connectid.connection_id =
			cpu_to_be64(nvmet_fc_makeconnid(iod->assoc,
				be16_to_cpu(rqst->connect_cmd.qid)));
}

 
static int
nvmet_fc_ls_disconnect(struct nvmet_fc_tgtport *tgtport,
			struct nvmet_fc_ls_iod *iod)
{
	struct fcnvme_ls_disconnect_assoc_rqst *rqst =
						&iod->rqstbuf->rq_dis_assoc;
	struct fcnvme_ls_disconnect_assoc_acc *acc =
						&iod->rspbuf->rsp_dis_assoc;
	struct nvmet_fc_tgt_assoc *assoc = NULL;
	struct nvmet_fc_ls_iod *oldls = NULL;
	unsigned long flags;
	int ret = 0;

	memset(acc, 0, sizeof(*acc));

	ret = nvmefc_vldt_lsreq_discon_assoc(iod->rqstdatalen, rqst);
	if (!ret) {
		 
		assoc = nvmet_fc_find_target_assoc(tgtport,
				be64_to_cpu(rqst->associd.association_id));
		iod->assoc = assoc;
		if (!assoc)
			ret = VERR_NO_ASSOC;
	}

	if (ret || !assoc) {
		dev_err(tgtport->dev,
			"Disconnect LS failed: %s\n",
			validation_errors[ret]);
		iod->lsrsp->rsplen = nvme_fc_format_rjt(acc,
				sizeof(*acc), rqst->w0.ls_cmd,
				(ret == VERR_NO_ASSOC) ?
					FCNVME_RJT_RC_INV_ASSOC :
					FCNVME_RJT_RC_LOGIC,
				FCNVME_RJT_EXP_NONE, 0);
		return true;
	}

	 

	iod->lsrsp->rsplen = sizeof(*acc);

	nvme_fc_format_rsp_hdr(acc, FCNVME_LS_ACC,
			fcnvme_lsdesc_len(
				sizeof(struct fcnvme_ls_disconnect_assoc_acc)),
			FCNVME_LS_DISCONNECT_ASSOC);

	 
	nvmet_fc_tgt_a_put(assoc);

	 
	spin_lock_irqsave(&tgtport->lock, flags);
	oldls = assoc->rcv_disconn;
	assoc->rcv_disconn = iod;
	spin_unlock_irqrestore(&tgtport->lock, flags);

	nvmet_fc_delete_target_assoc(assoc);

	if (oldls) {
		dev_info(tgtport->dev,
			"{%d:%d} Multiple Disconnect Association LS's "
			"received\n",
			tgtport->fc_target_port.port_num, assoc->a_id);
		 
		oldls->lsrsp->rsplen = nvme_fc_format_rjt(oldls->rspbuf,
						sizeof(*iod->rspbuf),
						 
						rqst->w0.ls_cmd,
						FCNVME_RJT_RC_UNAB,
						FCNVME_RJT_EXP_NONE, 0);
		nvmet_fc_xmt_ls_rsp(tgtport, oldls);
	}

	return false;
}


 


static void nvmet_fc_fcp_nvme_cmd_done(struct nvmet_req *nvme_req);

static const struct nvmet_fabrics_ops nvmet_fc_tgt_fcp_ops;

static void
nvmet_fc_xmt_ls_rsp_done(struct nvmefc_ls_rsp *lsrsp)
{
	struct nvmet_fc_ls_iod *iod = lsrsp->nvme_fc_private;
	struct nvmet_fc_tgtport *tgtport = iod->tgtport;

	fc_dma_sync_single_for_cpu(tgtport->dev, iod->rspdma,
				sizeof(*iod->rspbuf), DMA_TO_DEVICE);
	nvmet_fc_free_ls_iod(tgtport, iod);
	nvmet_fc_tgtport_put(tgtport);
}

static void
nvmet_fc_xmt_ls_rsp(struct nvmet_fc_tgtport *tgtport,
				struct nvmet_fc_ls_iod *iod)
{
	int ret;

	fc_dma_sync_single_for_device(tgtport->dev, iod->rspdma,
				  sizeof(*iod->rspbuf), DMA_TO_DEVICE);

	ret = tgtport->ops->xmt_ls_rsp(&tgtport->fc_target_port, iod->lsrsp);
	if (ret)
		nvmet_fc_xmt_ls_rsp_done(iod->lsrsp);
}

 
static void
nvmet_fc_handle_ls_rqst(struct nvmet_fc_tgtport *tgtport,
			struct nvmet_fc_ls_iod *iod)
{
	struct fcnvme_ls_rqst_w0 *w0 = &iod->rqstbuf->rq_cr_assoc.w0;
	bool sendrsp = true;

	iod->lsrsp->nvme_fc_private = iod;
	iod->lsrsp->rspbuf = iod->rspbuf;
	iod->lsrsp->rspdma = iod->rspdma;
	iod->lsrsp->done = nvmet_fc_xmt_ls_rsp_done;
	 
	iod->lsrsp->rsplen = 0;

	iod->assoc = NULL;

	 
	switch (w0->ls_cmd) {
	case FCNVME_LS_CREATE_ASSOCIATION:
		 
		nvmet_fc_ls_create_association(tgtport, iod);
		break;
	case FCNVME_LS_CREATE_CONNECTION:
		 
		nvmet_fc_ls_create_connection(tgtport, iod);
		break;
	case FCNVME_LS_DISCONNECT_ASSOC:
		 
		sendrsp = nvmet_fc_ls_disconnect(tgtport, iod);
		break;
	default:
		iod->lsrsp->rsplen = nvme_fc_format_rjt(iod->rspbuf,
				sizeof(*iod->rspbuf), w0->ls_cmd,
				FCNVME_RJT_RC_INVAL, FCNVME_RJT_EXP_NONE, 0);
	}

	if (sendrsp)
		nvmet_fc_xmt_ls_rsp(tgtport, iod);
}

 
static void
nvmet_fc_handle_ls_rqst_work(struct work_struct *work)
{
	struct nvmet_fc_ls_iod *iod =
		container_of(work, struct nvmet_fc_ls_iod, work);
	struct nvmet_fc_tgtport *tgtport = iod->tgtport;

	nvmet_fc_handle_ls_rqst(tgtport, iod);
}


 
int
nvmet_fc_rcv_ls_req(struct nvmet_fc_target_port *target_port,
			void *hosthandle,
			struct nvmefc_ls_rsp *lsrsp,
			void *lsreqbuf, u32 lsreqbuf_len)
{
	struct nvmet_fc_tgtport *tgtport = targetport_to_tgtport(target_port);
	struct nvmet_fc_ls_iod *iod;
	struct fcnvme_ls_rqst_w0 *w0 = (struct fcnvme_ls_rqst_w0 *)lsreqbuf;

	if (lsreqbuf_len > sizeof(union nvmefc_ls_requests)) {
		dev_info(tgtport->dev,
			"RCV %s LS failed: payload too large (%d)\n",
			(w0->ls_cmd <= NVME_FC_LAST_LS_CMD_VALUE) ?
				nvmefc_ls_names[w0->ls_cmd] : "",
			lsreqbuf_len);
		return -E2BIG;
	}

	if (!nvmet_fc_tgtport_get(tgtport)) {
		dev_info(tgtport->dev,
			"RCV %s LS failed: target deleting\n",
			(w0->ls_cmd <= NVME_FC_LAST_LS_CMD_VALUE) ?
				nvmefc_ls_names[w0->ls_cmd] : "");
		return -ESHUTDOWN;
	}

	iod = nvmet_fc_alloc_ls_iod(tgtport);
	if (!iod) {
		dev_info(tgtport->dev,
			"RCV %s LS failed: context allocation failed\n",
			(w0->ls_cmd <= NVME_FC_LAST_LS_CMD_VALUE) ?
				nvmefc_ls_names[w0->ls_cmd] : "");
		nvmet_fc_tgtport_put(tgtport);
		return -ENOENT;
	}

	iod->lsrsp = lsrsp;
	iod->fcpreq = NULL;
	memcpy(iod->rqstbuf, lsreqbuf, lsreqbuf_len);
	iod->rqstdatalen = lsreqbuf_len;
	iod->hosthandle = hosthandle;

	queue_work(nvmet_wq, &iod->work);

	return 0;
}
EXPORT_SYMBOL_GPL(nvmet_fc_rcv_ls_req);


 

static int
nvmet_fc_alloc_tgt_pgs(struct nvmet_fc_fcp_iod *fod)
{
	struct scatterlist *sg;
	unsigned int nent;

	sg = sgl_alloc(fod->req.transfer_len, GFP_KERNEL, &nent);
	if (!sg)
		goto out;

	fod->data_sg = sg;
	fod->data_sg_cnt = nent;
	fod->data_sg_cnt = fc_dma_map_sg(fod->tgtport->dev, sg, nent,
				((fod->io_dir == NVMET_FCP_WRITE) ?
					DMA_FROM_DEVICE : DMA_TO_DEVICE));
				 
	fod->next_sg = fod->data_sg;

	return 0;

out:
	return NVME_SC_INTERNAL;
}

static void
nvmet_fc_free_tgt_pgs(struct nvmet_fc_fcp_iod *fod)
{
	if (!fod->data_sg || !fod->data_sg_cnt)
		return;

	fc_dma_unmap_sg(fod->tgtport->dev, fod->data_sg, fod->data_sg_cnt,
				((fod->io_dir == NVMET_FCP_WRITE) ?
					DMA_FROM_DEVICE : DMA_TO_DEVICE));
	sgl_free(fod->data_sg);
	fod->data_sg = NULL;
	fod->data_sg_cnt = 0;
}


static bool
queue_90percent_full(struct nvmet_fc_tgt_queue *q, u32 sqhd)
{
	u32 sqtail, used;

	 
	sqtail = atomic_read(&q->sqtail) % q->sqsize;

	used = (sqtail < sqhd) ? (sqtail + q->sqsize - sqhd) : (sqtail - sqhd);
	return ((used * 10) >= (((u32)(q->sqsize - 1) * 9)));
}

 
static void
nvmet_fc_prep_fcp_rsp(struct nvmet_fc_tgtport *tgtport,
				struct nvmet_fc_fcp_iod *fod)
{
	struct nvme_fc_ersp_iu *ersp = &fod->rspiubuf;
	struct nvme_common_command *sqe = &fod->cmdiubuf.sqe.common;
	struct nvme_completion *cqe = &ersp->cqe;
	u32 *cqewd = (u32 *)cqe;
	bool send_ersp = false;
	u32 rsn, rspcnt, xfr_length;

	if (fod->fcpreq->op == NVMET_FCOP_READDATA_RSP)
		xfr_length = fod->req.transfer_len;
	else
		xfr_length = fod->offset;

	 
	rspcnt = atomic_inc_return(&fod->queue->zrspcnt);
	if (!(rspcnt % fod->queue->ersp_ratio) ||
	    nvme_is_fabrics((struct nvme_command *) sqe) ||
	    xfr_length != fod->req.transfer_len ||
	    (le16_to_cpu(cqe->status) & 0xFFFE) || cqewd[0] || cqewd[1] ||
	    (sqe->flags & (NVME_CMD_FUSE_FIRST | NVME_CMD_FUSE_SECOND)) ||
	    queue_90percent_full(fod->queue, le16_to_cpu(cqe->sq_head)))
		send_ersp = true;

	 
	fod->fcpreq->rspaddr = ersp;
	fod->fcpreq->rspdma = fod->rspdma;

	if (!send_ersp) {
		memset(ersp, 0, NVME_FC_SIZEOF_ZEROS_RSP);
		fod->fcpreq->rsplen = NVME_FC_SIZEOF_ZEROS_RSP;
	} else {
		ersp->iu_len = cpu_to_be16(sizeof(*ersp)/sizeof(u32));
		rsn = atomic_inc_return(&fod->queue->rsn);
		ersp->rsn = cpu_to_be32(rsn);
		ersp->xfrd_len = cpu_to_be32(xfr_length);
		fod->fcpreq->rsplen = sizeof(*ersp);
	}

	fc_dma_sync_single_for_device(tgtport->dev, fod->rspdma,
				  sizeof(fod->rspiubuf), DMA_TO_DEVICE);
}

static void nvmet_fc_xmt_fcp_op_done(struct nvmefc_tgt_fcp_req *fcpreq);

static void
nvmet_fc_abort_op(struct nvmet_fc_tgtport *tgtport,
				struct nvmet_fc_fcp_iod *fod)
{
	struct nvmefc_tgt_fcp_req *fcpreq = fod->fcpreq;

	 
	nvmet_fc_free_tgt_pgs(fod);

	 
	 
	if (!fod->aborted)
		tgtport->ops->fcp_abort(&tgtport->fc_target_port, fcpreq);

	nvmet_fc_free_fcp_iod(fod->queue, fod);
}

static void
nvmet_fc_xmt_fcp_rsp(struct nvmet_fc_tgtport *tgtport,
				struct nvmet_fc_fcp_iod *fod)
{
	int ret;

	fod->fcpreq->op = NVMET_FCOP_RSP;
	fod->fcpreq->timeout = 0;

	nvmet_fc_prep_fcp_rsp(tgtport, fod);

	ret = tgtport->ops->fcp_op(&tgtport->fc_target_port, fod->fcpreq);
	if (ret)
		nvmet_fc_abort_op(tgtport, fod);
}

static void
nvmet_fc_transfer_fcp_data(struct nvmet_fc_tgtport *tgtport,
				struct nvmet_fc_fcp_iod *fod, u8 op)
{
	struct nvmefc_tgt_fcp_req *fcpreq = fod->fcpreq;
	struct scatterlist *sg = fod->next_sg;
	unsigned long flags;
	u32 remaininglen = fod->req.transfer_len - fod->offset;
	u32 tlen = 0;
	int ret;

	fcpreq->op = op;
	fcpreq->offset = fod->offset;
	fcpreq->timeout = NVME_FC_TGTOP_TIMEOUT_SEC;

	 
	fcpreq->sg = sg;
	fcpreq->sg_cnt = 0;
	while (tlen < remaininglen &&
	       fcpreq->sg_cnt < tgtport->max_sg_cnt &&
	       tlen + sg_dma_len(sg) < NVMET_FC_MAX_SEQ_LENGTH) {
		fcpreq->sg_cnt++;
		tlen += sg_dma_len(sg);
		sg = sg_next(sg);
	}
	if (tlen < remaininglen && fcpreq->sg_cnt == 0) {
		fcpreq->sg_cnt++;
		tlen += min_t(u32, sg_dma_len(sg), remaininglen);
		sg = sg_next(sg);
	}
	if (tlen < remaininglen)
		fod->next_sg = sg;
	else
		fod->next_sg = NULL;

	fcpreq->transfer_length = tlen;
	fcpreq->transferred_length = 0;
	fcpreq->fcp_error = 0;
	fcpreq->rsplen = 0;

	 
	if ((op == NVMET_FCOP_READDATA) &&
	    ((fod->offset + fcpreq->transfer_length) == fod->req.transfer_len) &&
	    (tgtport->ops->target_features & NVMET_FCTGTFEAT_READDATA_RSP)) {
		fcpreq->op = NVMET_FCOP_READDATA_RSP;
		nvmet_fc_prep_fcp_rsp(tgtport, fod);
	}

	ret = tgtport->ops->fcp_op(&tgtport->fc_target_port, fod->fcpreq);
	if (ret) {
		 
		fod->abort = true;

		if (op == NVMET_FCOP_WRITEDATA) {
			spin_lock_irqsave(&fod->flock, flags);
			fod->writedataactive = false;
			spin_unlock_irqrestore(&fod->flock, flags);
			nvmet_req_complete(&fod->req, NVME_SC_INTERNAL);
		} else   {
			fcpreq->fcp_error = ret;
			fcpreq->transferred_length = 0;
			nvmet_fc_xmt_fcp_op_done(fod->fcpreq);
		}
	}
}

static inline bool
__nvmet_fc_fod_op_abort(struct nvmet_fc_fcp_iod *fod, bool abort)
{
	struct nvmefc_tgt_fcp_req *fcpreq = fod->fcpreq;
	struct nvmet_fc_tgtport *tgtport = fod->tgtport;

	 
	if (abort) {
		if (fcpreq->op == NVMET_FCOP_WRITEDATA) {
			nvmet_req_complete(&fod->req, NVME_SC_INTERNAL);
			return true;
		}

		nvmet_fc_abort_op(tgtport, fod);
		return true;
	}

	return false;
}

 
static void
nvmet_fc_fod_op_done(struct nvmet_fc_fcp_iod *fod)
{
	struct nvmefc_tgt_fcp_req *fcpreq = fod->fcpreq;
	struct nvmet_fc_tgtport *tgtport = fod->tgtport;
	unsigned long flags;
	bool abort;

	spin_lock_irqsave(&fod->flock, flags);
	abort = fod->abort;
	fod->writedataactive = false;
	spin_unlock_irqrestore(&fod->flock, flags);

	switch (fcpreq->op) {

	case NVMET_FCOP_WRITEDATA:
		if (__nvmet_fc_fod_op_abort(fod, abort))
			return;
		if (fcpreq->fcp_error ||
		    fcpreq->transferred_length != fcpreq->transfer_length) {
			spin_lock_irqsave(&fod->flock, flags);
			fod->abort = true;
			spin_unlock_irqrestore(&fod->flock, flags);

			nvmet_req_complete(&fod->req, NVME_SC_INTERNAL);
			return;
		}

		fod->offset += fcpreq->transferred_length;
		if (fod->offset != fod->req.transfer_len) {
			spin_lock_irqsave(&fod->flock, flags);
			fod->writedataactive = true;
			spin_unlock_irqrestore(&fod->flock, flags);

			 
			nvmet_fc_transfer_fcp_data(tgtport, fod,
						NVMET_FCOP_WRITEDATA);
			return;
		}

		 
		fod->req.execute(&fod->req);
		break;

	case NVMET_FCOP_READDATA:
	case NVMET_FCOP_READDATA_RSP:
		if (__nvmet_fc_fod_op_abort(fod, abort))
			return;
		if (fcpreq->fcp_error ||
		    fcpreq->transferred_length != fcpreq->transfer_length) {
			nvmet_fc_abort_op(tgtport, fod);
			return;
		}

		 

		if (fcpreq->op == NVMET_FCOP_READDATA_RSP) {
			 
			nvmet_fc_free_tgt_pgs(fod);
			nvmet_fc_free_fcp_iod(fod->queue, fod);
			return;
		}

		fod->offset += fcpreq->transferred_length;
		if (fod->offset != fod->req.transfer_len) {
			 
			nvmet_fc_transfer_fcp_data(tgtport, fod,
						NVMET_FCOP_READDATA);
			return;
		}

		 

		 
		nvmet_fc_free_tgt_pgs(fod);

		nvmet_fc_xmt_fcp_rsp(tgtport, fod);

		break;

	case NVMET_FCOP_RSP:
		if (__nvmet_fc_fod_op_abort(fod, abort))
			return;
		nvmet_fc_free_fcp_iod(fod->queue, fod);
		break;

	default:
		break;
	}
}

static void
nvmet_fc_xmt_fcp_op_done(struct nvmefc_tgt_fcp_req *fcpreq)
{
	struct nvmet_fc_fcp_iod *fod = fcpreq->nvmet_fc_private;

	nvmet_fc_fod_op_done(fod);
}

 
static void
__nvmet_fc_fcp_nvme_cmd_done(struct nvmet_fc_tgtport *tgtport,
			struct nvmet_fc_fcp_iod *fod, int status)
{
	struct nvme_common_command *sqe = &fod->cmdiubuf.sqe.common;
	struct nvme_completion *cqe = &fod->rspiubuf.cqe;
	unsigned long flags;
	bool abort;

	spin_lock_irqsave(&fod->flock, flags);
	abort = fod->abort;
	spin_unlock_irqrestore(&fod->flock, flags);

	 
	if (!status)
		fod->queue->sqhd = cqe->sq_head;

	if (abort) {
		nvmet_fc_abort_op(tgtport, fod);
		return;
	}

	 
	if (status) {
		 
		memset(cqe, 0, sizeof(*cqe));
		cqe->sq_head = fod->queue->sqhd;	 
		cqe->sq_id = cpu_to_le16(fod->queue->qid);
		cqe->command_id = sqe->command_id;
		cqe->status = cpu_to_le16(status);
	} else {

		 
		if ((fod->io_dir == NVMET_FCP_READ) && (fod->data_sg_cnt)) {
			 
			nvmet_fc_transfer_fcp_data(tgtport, fod,
						NVMET_FCOP_READDATA);
			return;
		}

		 
	}

	 
	nvmet_fc_free_tgt_pgs(fod);

	nvmet_fc_xmt_fcp_rsp(tgtport, fod);
}


static void
nvmet_fc_fcp_nvme_cmd_done(struct nvmet_req *nvme_req)
{
	struct nvmet_fc_fcp_iod *fod = nvmet_req_to_fod(nvme_req);
	struct nvmet_fc_tgtport *tgtport = fod->tgtport;

	__nvmet_fc_fcp_nvme_cmd_done(tgtport, fod, 0);
}


 
static void
nvmet_fc_handle_fcp_rqst(struct nvmet_fc_tgtport *tgtport,
			struct nvmet_fc_fcp_iod *fod)
{
	struct nvme_fc_cmd_iu *cmdiu = &fod->cmdiubuf;
	u32 xfrlen = be32_to_cpu(cmdiu->data_len);
	int ret;

	 

	fod->fcpreq->done = nvmet_fc_xmt_fcp_op_done;

	if (cmdiu->flags & FCNVME_CMD_FLAGS_WRITE) {
		fod->io_dir = NVMET_FCP_WRITE;
		if (!nvme_is_write(&cmdiu->sqe))
			goto transport_error;
	} else if (cmdiu->flags & FCNVME_CMD_FLAGS_READ) {
		fod->io_dir = NVMET_FCP_READ;
		if (nvme_is_write(&cmdiu->sqe))
			goto transport_error;
	} else {
		fod->io_dir = NVMET_FCP_NODATA;
		if (xfrlen)
			goto transport_error;
	}

	fod->req.cmd = &fod->cmdiubuf.sqe;
	fod->req.cqe = &fod->rspiubuf.cqe;
	if (tgtport->pe)
		fod->req.port = tgtport->pe->port;

	 
	memset(&fod->rspiubuf, 0, sizeof(fod->rspiubuf));

	fod->data_sg = NULL;
	fod->data_sg_cnt = 0;

	ret = nvmet_req_init(&fod->req,
				&fod->queue->nvme_cq,
				&fod->queue->nvme_sq,
				&nvmet_fc_tgt_fcp_ops);
	if (!ret) {
		 
		 
		return;
	}

	fod->req.transfer_len = xfrlen;

	 
	atomic_inc(&fod->queue->sqtail);

	if (fod->req.transfer_len) {
		ret = nvmet_fc_alloc_tgt_pgs(fod);
		if (ret) {
			nvmet_req_complete(&fod->req, ret);
			return;
		}
	}
	fod->req.sg = fod->data_sg;
	fod->req.sg_cnt = fod->data_sg_cnt;
	fod->offset = 0;

	if (fod->io_dir == NVMET_FCP_WRITE) {
		 
		nvmet_fc_transfer_fcp_data(tgtport, fod, NVMET_FCOP_WRITEDATA);
		return;
	}

	 
	fod->req.execute(&fod->req);
	return;

transport_error:
	nvmet_fc_abort_op(tgtport, fod);
}

 
int
nvmet_fc_rcv_fcp_req(struct nvmet_fc_target_port *target_port,
			struct nvmefc_tgt_fcp_req *fcpreq,
			void *cmdiubuf, u32 cmdiubuf_len)
{
	struct nvmet_fc_tgtport *tgtport = targetport_to_tgtport(target_port);
	struct nvme_fc_cmd_iu *cmdiu = cmdiubuf;
	struct nvmet_fc_tgt_queue *queue;
	struct nvmet_fc_fcp_iod *fod;
	struct nvmet_fc_defer_fcp_req *deferfcp;
	unsigned long flags;

	 
	if ((cmdiubuf_len != sizeof(*cmdiu)) ||
			(cmdiu->format_id != NVME_CMD_FORMAT_ID) ||
			(cmdiu->fc_id != NVME_CMD_FC_ID) ||
			(be16_to_cpu(cmdiu->iu_len) != (sizeof(*cmdiu)/4)))
		return -EIO;

	queue = nvmet_fc_find_target_queue(tgtport,
				be64_to_cpu(cmdiu->connection_id));
	if (!queue)
		return -ENOTCONN;

	 

	spin_lock_irqsave(&queue->qlock, flags);

	fod = nvmet_fc_alloc_fcp_iod(queue);
	if (fod) {
		spin_unlock_irqrestore(&queue->qlock, flags);

		fcpreq->nvmet_fc_private = fod;
		fod->fcpreq = fcpreq;

		memcpy(&fod->cmdiubuf, cmdiubuf, cmdiubuf_len);

		nvmet_fc_queue_fcp_req(tgtport, queue, fcpreq);

		return 0;
	}

	if (!tgtport->ops->defer_rcv) {
		spin_unlock_irqrestore(&queue->qlock, flags);
		 
		nvmet_fc_tgt_q_put(queue);
		return -ENOENT;
	}

	deferfcp = list_first_entry_or_null(&queue->avail_defer_list,
			struct nvmet_fc_defer_fcp_req, req_list);
	if (deferfcp) {
		 
		list_del(&deferfcp->req_list);
	} else {
		spin_unlock_irqrestore(&queue->qlock, flags);

		 
		deferfcp = kmalloc(sizeof(*deferfcp), GFP_KERNEL);
		if (!deferfcp) {
			 
			nvmet_fc_tgt_q_put(queue);
			return -ENOMEM;
		}
		spin_lock_irqsave(&queue->qlock, flags);
	}

	 
	fcpreq->rspaddr = cmdiubuf;
	fcpreq->rsplen  = cmdiubuf_len;
	deferfcp->fcp_req = fcpreq;

	 
	list_add_tail(&deferfcp->req_list, &queue->pending_cmd_list);

	 

	spin_unlock_irqrestore(&queue->qlock, flags);

	return -EOVERFLOW;
}
EXPORT_SYMBOL_GPL(nvmet_fc_rcv_fcp_req);

 
void
nvmet_fc_rcv_fcp_abort(struct nvmet_fc_target_port *target_port,
			struct nvmefc_tgt_fcp_req *fcpreq)
{
	struct nvmet_fc_fcp_iod *fod = fcpreq->nvmet_fc_private;
	struct nvmet_fc_tgt_queue *queue;
	unsigned long flags;

	if (!fod || fod->fcpreq != fcpreq)
		 
		return;

	queue = fod->queue;

	spin_lock_irqsave(&queue->qlock, flags);
	if (fod->active) {
		 
		spin_lock(&fod->flock);
		fod->abort = true;
		fod->aborted = true;
		spin_unlock(&fod->flock);
	}
	spin_unlock_irqrestore(&queue->qlock, flags);
}
EXPORT_SYMBOL_GPL(nvmet_fc_rcv_fcp_abort);


struct nvmet_fc_traddr {
	u64	nn;
	u64	pn;
};

static int
__nvme_fc_parse_u64(substring_t *sstr, u64 *val)
{
	u64 token64;

	if (match_u64(sstr, &token64))
		return -EINVAL;
	*val = token64;

	return 0;
}

 
static int
nvme_fc_parse_traddr(struct nvmet_fc_traddr *traddr, char *buf, size_t blen)
{
	char name[2 + NVME_FC_TRADDR_HEXNAMELEN + 1];
	substring_t wwn = { name, &name[sizeof(name)-1] };
	int nnoffset, pnoffset;

	 
	if (strnlen(buf, blen) == NVME_FC_TRADDR_MAXLENGTH &&
			!strncmp(buf, "nn-0x", NVME_FC_TRADDR_OXNNLEN) &&
			!strncmp(&buf[NVME_FC_TRADDR_MAX_PN_OFFSET],
				"pn-0x", NVME_FC_TRADDR_OXNNLEN)) {
		nnoffset = NVME_FC_TRADDR_OXNNLEN;
		pnoffset = NVME_FC_TRADDR_MAX_PN_OFFSET +
						NVME_FC_TRADDR_OXNNLEN;
	} else if ((strnlen(buf, blen) == NVME_FC_TRADDR_MINLENGTH &&
			!strncmp(buf, "nn-", NVME_FC_TRADDR_NNLEN) &&
			!strncmp(&buf[NVME_FC_TRADDR_MIN_PN_OFFSET],
				"pn-", NVME_FC_TRADDR_NNLEN))) {
		nnoffset = NVME_FC_TRADDR_NNLEN;
		pnoffset = NVME_FC_TRADDR_MIN_PN_OFFSET + NVME_FC_TRADDR_NNLEN;
	} else
		goto out_einval;

	name[0] = '0';
	name[1] = 'x';
	name[2 + NVME_FC_TRADDR_HEXNAMELEN] = 0;

	memcpy(&name[2], &buf[nnoffset], NVME_FC_TRADDR_HEXNAMELEN);
	if (__nvme_fc_parse_u64(&wwn, &traddr->nn))
		goto out_einval;

	memcpy(&name[2], &buf[pnoffset], NVME_FC_TRADDR_HEXNAMELEN);
	if (__nvme_fc_parse_u64(&wwn, &traddr->pn))
		goto out_einval;

	return 0;

out_einval:
	pr_warn("%s: bad traddr string\n", __func__);
	return -EINVAL;
}

static int
nvmet_fc_add_port(struct nvmet_port *port)
{
	struct nvmet_fc_tgtport *tgtport;
	struct nvmet_fc_port_entry *pe;
	struct nvmet_fc_traddr traddr = { 0L, 0L };
	unsigned long flags;
	int ret;

	 
	if ((port->disc_addr.trtype != NVMF_TRTYPE_FC) ||
	    (port->disc_addr.adrfam != NVMF_ADDR_FAMILY_FC))
		return -EINVAL;

	 

	ret = nvme_fc_parse_traddr(&traddr, port->disc_addr.traddr,
			sizeof(port->disc_addr.traddr));
	if (ret)
		return ret;

	pe = kzalloc(sizeof(*pe), GFP_KERNEL);
	if (!pe)
		return -ENOMEM;

	ret = -ENXIO;
	spin_lock_irqsave(&nvmet_fc_tgtlock, flags);
	list_for_each_entry(tgtport, &nvmet_fc_target_list, tgt_list) {
		if ((tgtport->fc_target_port.node_name == traddr.nn) &&
		    (tgtport->fc_target_port.port_name == traddr.pn)) {
			 
			if (!tgtport->pe) {
				nvmet_fc_portentry_bind(tgtport, pe, port);
				ret = 0;
			} else
				ret = -EALREADY;
			break;
		}
	}
	spin_unlock_irqrestore(&nvmet_fc_tgtlock, flags);

	if (ret)
		kfree(pe);

	return ret;
}

static void
nvmet_fc_remove_port(struct nvmet_port *port)
{
	struct nvmet_fc_port_entry *pe = port->priv;

	nvmet_fc_portentry_unbind(pe);

	kfree(pe);
}

static void
nvmet_fc_discovery_chg(struct nvmet_port *port)
{
	struct nvmet_fc_port_entry *pe = port->priv;
	struct nvmet_fc_tgtport *tgtport = pe->tgtport;

	if (tgtport && tgtport->ops->discovery_event)
		tgtport->ops->discovery_event(&tgtport->fc_target_port);
}

static const struct nvmet_fabrics_ops nvmet_fc_tgt_fcp_ops = {
	.owner			= THIS_MODULE,
	.type			= NVMF_TRTYPE_FC,
	.msdbd			= 1,
	.add_port		= nvmet_fc_add_port,
	.remove_port		= nvmet_fc_remove_port,
	.queue_response		= nvmet_fc_fcp_nvme_cmd_done,
	.delete_ctrl		= nvmet_fc_delete_ctrl,
	.discovery_chg		= nvmet_fc_discovery_chg,
};

static int __init nvmet_fc_init_module(void)
{
	return nvmet_register_transport(&nvmet_fc_tgt_fcp_ops);
}

static void __exit nvmet_fc_exit_module(void)
{
	 
	if (!list_empty(&nvmet_fc_target_list))
		pr_warn("%s: targetport list not empty\n", __func__);

	nvmet_unregister_transport(&nvmet_fc_tgt_fcp_ops);

	ida_destroy(&nvmet_fc_tgtport_cnt);
}

module_init(nvmet_fc_init_module);
module_exit(nvmet_fc_exit_module);

MODULE_LICENSE("GPL v2");
