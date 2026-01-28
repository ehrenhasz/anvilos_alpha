#ifndef _VDEC_MSG_QUEUE_H_
#define _VDEC_MSG_QUEUE_H_
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <media/videobuf2-v4l2.h>
#define NUM_BUFFER_COUNT 3
struct vdec_lat_buf;
struct mtk_vcodec_dec_ctx;
struct mtk_vcodec_dec_dev;
typedef int (*core_decode_cb_t)(struct vdec_lat_buf *lat_buf);
enum core_ctx_status {
	CONTEXT_LIST_EMPTY = 0,
	CONTEXT_LIST_QUEUED,
	CONTEXT_LIST_DEC_DONE,
};
struct vdec_msg_queue_ctx {
	wait_queue_head_t ready_to_use;
	struct list_head ready_queue;
	spinlock_t ready_lock;
	int ready_num;
	int hardware_index;
};
struct vdec_lat_buf {
	struct mtk_vcodec_mem wdma_err_addr;
	struct mtk_vcodec_mem slice_bc_addr;
	struct mtk_vcodec_mem rd_mv_addr;
	struct mtk_vcodec_mem tile_addr;
	struct vb2_v4l2_buffer ts_info;
	struct media_request *src_buf_req;
	void *private_data;
	struct mtk_vcodec_dec_ctx *ctx;
	core_decode_cb_t core_decode;
	struct list_head lat_list;
	struct list_head core_list;
	bool is_last_frame;
};
struct vdec_msg_queue {
	struct vdec_lat_buf lat_buf[NUM_BUFFER_COUNT];
	struct mtk_vcodec_mem wdma_addr;
	u64 wdma_rptr_addr;
	u64 wdma_wptr_addr;
	struct work_struct core_work;
	struct vdec_msg_queue_ctx lat_ctx;
	struct vdec_msg_queue_ctx core_ctx;
	atomic_t lat_list_cnt;
	atomic_t core_list_cnt;
	bool flush_done;
	struct vdec_lat_buf empty_lat_buf;
	wait_queue_head_t core_dec_done;
	int status;
	struct mtk_vcodec_dec_ctx *ctx;
};
int vdec_msg_queue_init(struct vdec_msg_queue *msg_queue,
			struct mtk_vcodec_dec_ctx *ctx, core_decode_cb_t core_decode,
			int private_size);
void vdec_msg_queue_init_ctx(struct vdec_msg_queue_ctx *ctx, int hardware_index);
int vdec_msg_queue_qbuf(struct vdec_msg_queue_ctx *ctx, struct vdec_lat_buf *buf);
struct vdec_lat_buf *vdec_msg_queue_dqbuf(struct vdec_msg_queue_ctx *ctx);
void vdec_msg_queue_update_ube_rptr(struct vdec_msg_queue *msg_queue, uint64_t ube_rptr);
void vdec_msg_queue_update_ube_wptr(struct vdec_msg_queue *msg_queue, uint64_t ube_wptr);
bool vdec_msg_queue_wait_lat_buf_full(struct vdec_msg_queue *msg_queue);
void vdec_msg_queue_deinit(struct vdec_msg_queue *msg_queue,
			   struct mtk_vcodec_dec_ctx *ctx);
#endif
