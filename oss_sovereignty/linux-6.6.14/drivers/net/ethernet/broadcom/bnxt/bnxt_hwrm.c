 

#include <asm/byteorder.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/errno.h>
#include <linux/ethtool.h>
#include <linux/if_ether.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/skbuff.h>

#include "bnxt_hsi.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"

static u64 hwrm_calc_sentinel(struct bnxt_hwrm_ctx *ctx, u16 req_type)
{
	return (((uintptr_t)ctx) + req_type) ^ BNXT_HWRM_SENTINEL;
}

 
int __hwrm_req_init(struct bnxt *bp, void **req, u16 req_type, u32 req_len)
{
	struct bnxt_hwrm_ctx *ctx;
	dma_addr_t dma_handle;
	u8 *req_addr;

	if (req_len > BNXT_HWRM_CTX_OFFSET)
		return -E2BIG;

	req_addr = dma_pool_alloc(bp->hwrm_dma_pool, GFP_KERNEL | __GFP_ZERO,
				  &dma_handle);
	if (!req_addr)
		return -ENOMEM;

	ctx = (struct bnxt_hwrm_ctx *)(req_addr + BNXT_HWRM_CTX_OFFSET);
	 
	ctx->sentinel = hwrm_calc_sentinel(ctx, req_type);
	ctx->req_len = req_len;
	ctx->req = (struct input *)req_addr;
	ctx->resp = (struct output *)(req_addr + BNXT_HWRM_RESP_OFFSET);
	ctx->dma_handle = dma_handle;
	ctx->flags = 0;  
	ctx->timeout = bp->hwrm_cmd_timeout ?: DFLT_HWRM_CMD_TIMEOUT;
	ctx->allocated = BNXT_HWRM_DMA_SIZE - BNXT_HWRM_CTX_OFFSET;
	ctx->gfp = GFP_KERNEL;
	ctx->slice_addr = NULL;

	 
	ctx->req->req_type = cpu_to_le16(req_type);
	ctx->req->resp_addr = cpu_to_le64(dma_handle + BNXT_HWRM_RESP_OFFSET);
	ctx->req->cmpl_ring = cpu_to_le16(BNXT_HWRM_NO_CMPL_RING);
	ctx->req->target_id = cpu_to_le16(BNXT_HWRM_TARGET);
	*req = ctx->req;

	return 0;
}

static struct bnxt_hwrm_ctx *__hwrm_ctx(struct bnxt *bp, u8 *req_addr)
{
	void *ctx_addr = req_addr + BNXT_HWRM_CTX_OFFSET;
	struct input *req = (struct input *)req_addr;
	struct bnxt_hwrm_ctx *ctx = ctx_addr;
	u64 sentinel;

	if (!req) {
		 
		netdev_err(bp->dev, "null HWRM request");
		dump_stack();
		return NULL;
	}

	 
	sentinel = hwrm_calc_sentinel(ctx, le16_to_cpu(req->req_type));
	if (ctx->sentinel != sentinel) {
		 
		netdev_err(bp->dev, "HWRM sentinel mismatch, req_type = %u\n",
			   (u32)le16_to_cpu(req->req_type));
		dump_stack();
		return NULL;
	}

	return ctx;
}

 
void hwrm_req_timeout(struct bnxt *bp, void *req, unsigned int timeout)
{
	struct bnxt_hwrm_ctx *ctx = __hwrm_ctx(bp, req);

	if (ctx)
		ctx->timeout = timeout;
}

 
void hwrm_req_alloc_flags(struct bnxt *bp, void *req, gfp_t gfp)
{
	struct bnxt_hwrm_ctx *ctx = __hwrm_ctx(bp, req);

	if (ctx)
		ctx->gfp = gfp;
}

 
int hwrm_req_replace(struct bnxt *bp, void *req, void *new_req, u32 len)
{
	struct bnxt_hwrm_ctx *ctx = __hwrm_ctx(bp, req);
	struct input *internal_req = req;
	u16 req_type;

	if (!ctx)
		return -EINVAL;

	if (len > BNXT_HWRM_CTX_OFFSET)
		return -E2BIG;

	 
	ctx->allocated = BNXT_HWRM_DMA_SIZE - BNXT_HWRM_CTX_OFFSET;
	if (ctx->slice_addr) {
		dma_free_coherent(&bp->pdev->dev, ctx->slice_size,
				  ctx->slice_addr, ctx->slice_handle);
		ctx->slice_addr = NULL;
	}
	ctx->gfp = GFP_KERNEL;

	if ((bp->fw_cap & BNXT_FW_CAP_SHORT_CMD) || len > BNXT_HWRM_MAX_REQ_LEN) {
		memcpy(internal_req, new_req, len);
	} else {
		internal_req->req_type = ((struct input *)new_req)->req_type;
		ctx->req = new_req;
	}

	ctx->req_len = len;
	ctx->req->resp_addr = cpu_to_le64(ctx->dma_handle +
					  BNXT_HWRM_RESP_OFFSET);

	 
	req_type = le16_to_cpu(internal_req->req_type);
	ctx->sentinel = hwrm_calc_sentinel(ctx, req_type);

	return 0;
}

 
void hwrm_req_flags(struct bnxt *bp, void *req, enum bnxt_hwrm_ctx_flags flags)
{
	struct bnxt_hwrm_ctx *ctx = __hwrm_ctx(bp, req);

	if (ctx)
		ctx->flags |= (flags & HWRM_API_FLAGS);
}

 
void *hwrm_req_hold(struct bnxt *bp, void *req)
{
	struct bnxt_hwrm_ctx *ctx = __hwrm_ctx(bp, req);
	struct input *input = (struct input *)req;

	if (!ctx)
		return NULL;

	if (ctx->flags & BNXT_HWRM_INTERNAL_CTX_OWNED) {
		 
		netdev_err(bp->dev, "HWRM context already owned, req_type = %u\n",
			   (u32)le16_to_cpu(input->req_type));
		dump_stack();
		return NULL;
	}

	ctx->flags |= BNXT_HWRM_INTERNAL_CTX_OWNED;
	return ((u8 *)req) + BNXT_HWRM_RESP_OFFSET;
}

static void __hwrm_ctx_drop(struct bnxt *bp, struct bnxt_hwrm_ctx *ctx)
{
	void *addr = ((u8 *)ctx) - BNXT_HWRM_CTX_OFFSET;
	dma_addr_t dma_handle = ctx->dma_handle;  

	 
	if (ctx->slice_addr)
		dma_free_coherent(&bp->pdev->dev, ctx->slice_size,
				  ctx->slice_addr, ctx->slice_handle);

	 
	memset(ctx, 0, sizeof(struct bnxt_hwrm_ctx));

	 
	if (dma_handle)
		dma_pool_free(bp->hwrm_dma_pool, addr, dma_handle);
}

 
void hwrm_req_drop(struct bnxt *bp, void *req)
{
	struct bnxt_hwrm_ctx *ctx = __hwrm_ctx(bp, req);

	if (ctx)
		__hwrm_ctx_drop(bp, ctx);
}

static int __hwrm_to_stderr(u32 hwrm_err)
{
	switch (hwrm_err) {
	case HWRM_ERR_CODE_SUCCESS:
		return 0;
	case HWRM_ERR_CODE_RESOURCE_LOCKED:
		return -EROFS;
	case HWRM_ERR_CODE_RESOURCE_ACCESS_DENIED:
		return -EACCES;
	case HWRM_ERR_CODE_RESOURCE_ALLOC_ERROR:
		return -ENOSPC;
	case HWRM_ERR_CODE_INVALID_PARAMS:
	case HWRM_ERR_CODE_INVALID_FLAGS:
	case HWRM_ERR_CODE_INVALID_ENABLES:
	case HWRM_ERR_CODE_UNSUPPORTED_TLV:
	case HWRM_ERR_CODE_UNSUPPORTED_OPTION_ERR:
		return -EINVAL;
	case HWRM_ERR_CODE_NO_BUFFER:
		return -ENOMEM;
	case HWRM_ERR_CODE_HOT_RESET_PROGRESS:
	case HWRM_ERR_CODE_BUSY:
		return -EAGAIN;
	case HWRM_ERR_CODE_CMD_NOT_SUPPORTED:
		return -EOPNOTSUPP;
	case HWRM_ERR_CODE_PF_UNAVAILABLE:
		return -ENODEV;
	default:
		return -EIO;
	}
}

static struct bnxt_hwrm_wait_token *
__hwrm_acquire_token(struct bnxt *bp, enum bnxt_hwrm_chnl dst)
{
	struct bnxt_hwrm_wait_token *token;

	token = kzalloc(sizeof(*token), GFP_KERNEL);
	if (!token)
		return NULL;

	mutex_lock(&bp->hwrm_cmd_lock);

	token->dst = dst;
	token->state = BNXT_HWRM_PENDING;
	if (dst == BNXT_HWRM_CHNL_CHIMP) {
		token->seq_id = bp->hwrm_cmd_seq++;
		hlist_add_head_rcu(&token->node, &bp->hwrm_pending_list);
	} else {
		token->seq_id = bp->hwrm_cmd_kong_seq++;
	}

	return token;
}

static void
__hwrm_release_token(struct bnxt *bp, struct bnxt_hwrm_wait_token *token)
{
	if (token->dst == BNXT_HWRM_CHNL_CHIMP) {
		hlist_del_rcu(&token->node);
		kfree_rcu(token, rcu);
	} else {
		kfree(token);
	}
	mutex_unlock(&bp->hwrm_cmd_lock);
}

void
hwrm_update_token(struct bnxt *bp, u16 seq_id, enum bnxt_hwrm_wait_state state)
{
	struct bnxt_hwrm_wait_token *token;

	rcu_read_lock();
	hlist_for_each_entry_rcu(token, &bp->hwrm_pending_list, node) {
		if (token->seq_id == seq_id) {
			WRITE_ONCE(token->state, state);
			rcu_read_unlock();
			return;
		}
	}
	rcu_read_unlock();
	netdev_err(bp->dev, "Invalid hwrm seq id %d\n", seq_id);
}

static void hwrm_req_dbg(struct bnxt *bp, struct input *req)
{
	u32 ring = le16_to_cpu(req->cmpl_ring);
	u32 type = le16_to_cpu(req->req_type);
	u32 tgt = le16_to_cpu(req->target_id);
	u32 seq = le16_to_cpu(req->seq_id);
	char opt[32] = "\n";

	if (unlikely(ring != (u16)BNXT_HWRM_NO_CMPL_RING))
		snprintf(opt, 16, " ring %d\n", ring);

	if (unlikely(tgt != BNXT_HWRM_TARGET))
		snprintf(opt + strlen(opt) - 1, 16, " tgt 0x%x\n", tgt);

	netdev_dbg(bp->dev, "sent hwrm req_type 0x%x seq id 0x%x%s",
		   type, seq, opt);
}

#define hwrm_err(bp, ctx, fmt, ...)				       \
	do {							       \
		if ((ctx)->flags & BNXT_HWRM_CTX_SILENT)	       \
			netdev_dbg((bp)->dev, fmt, __VA_ARGS__);       \
		else						       \
			netdev_err((bp)->dev, fmt, __VA_ARGS__);       \
	} while (0)

static bool hwrm_wait_must_abort(struct bnxt *bp, u32 req_type, u32 *fw_status)
{
	if (req_type == HWRM_VER_GET)
		return false;

	if (!bp->fw_health || !bp->fw_health->status_reliable)
		return false;

	*fw_status = bnxt_fw_health_readl(bp, BNXT_FW_HEALTH_REG);
	return *fw_status && !BNXT_FW_IS_HEALTHY(*fw_status);
}

static int __hwrm_send(struct bnxt *bp, struct bnxt_hwrm_ctx *ctx)
{
	u32 doorbell_offset = BNXT_GRCPF_REG_CHIMP_COMM_TRIGGER;
	enum bnxt_hwrm_chnl dst = BNXT_HWRM_CHNL_CHIMP;
	u32 bar_offset = BNXT_GRCPF_REG_CHIMP_COMM;
	struct bnxt_hwrm_wait_token *token = NULL;
	struct hwrm_short_input short_input = {0};
	u16 max_req_len = BNXT_HWRM_MAX_REQ_LEN;
	unsigned int i, timeout, tmo_count;
	u32 *data = (u32 *)ctx->req;
	u32 msg_len = ctx->req_len;
	u32 req_type, sts;
	int rc = -EBUSY;
	u16 len = 0;
	u8 *valid;

	if (ctx->flags & BNXT_HWRM_INTERNAL_RESP_DIRTY)
		memset(ctx->resp, 0, PAGE_SIZE);

	req_type = le16_to_cpu(ctx->req->req_type);
	if (BNXT_NO_FW_ACCESS(bp) &&
	    (req_type != HWRM_FUNC_RESET && req_type != HWRM_VER_GET)) {
		netdev_dbg(bp->dev, "hwrm req_type 0x%x skipped, FW channel down\n",
			   req_type);
		goto exit;
	}

	if (msg_len > BNXT_HWRM_MAX_REQ_LEN &&
	    msg_len > bp->hwrm_max_ext_req_len) {
		rc = -E2BIG;
		goto exit;
	}

	if (bnxt_kong_hwrm_message(bp, ctx->req)) {
		dst = BNXT_HWRM_CHNL_KONG;
		bar_offset = BNXT_GRCPF_REG_KONG_COMM;
		doorbell_offset = BNXT_GRCPF_REG_KONG_COMM_TRIGGER;
		if (le16_to_cpu(ctx->req->cmpl_ring) != INVALID_HW_RING_ID) {
			netdev_err(bp->dev, "Ring completions not supported for KONG commands, req_type = %d\n",
				   req_type);
			rc = -EINVAL;
			goto exit;
		}
	}

	token = __hwrm_acquire_token(bp, dst);
	if (!token) {
		rc = -ENOMEM;
		goto exit;
	}
	ctx->req->seq_id = cpu_to_le16(token->seq_id);

	if ((bp->fw_cap & BNXT_FW_CAP_SHORT_CMD) ||
	    msg_len > BNXT_HWRM_MAX_REQ_LEN) {
		short_input.req_type = ctx->req->req_type;
		short_input.signature =
				cpu_to_le16(SHORT_REQ_SIGNATURE_SHORT_CMD);
		short_input.size = cpu_to_le16(msg_len);
		short_input.req_addr = cpu_to_le64(ctx->dma_handle);

		data = (u32 *)&short_input;
		msg_len = sizeof(short_input);

		max_req_len = BNXT_HWRM_SHORT_REQ_LEN;
	}

	 
	wmb();

	 
	__iowrite32_copy(bp->bar0 + bar_offset, data, msg_len / 4);

	for (i = msg_len; i < max_req_len; i += 4)
		writel(0, bp->bar0 + bar_offset + i);

	 
	writel(1, bp->bar0 + doorbell_offset);

	hwrm_req_dbg(bp, ctx->req);

	if (!pci_is_enabled(bp->pdev)) {
		rc = -ENODEV;
		goto exit;
	}

	 
	timeout = min(ctx->timeout, bp->hwrm_cmd_max_timeout ?: HWRM_CMD_MAX_TIMEOUT);
	 
	timeout *= 1000;

	i = 0;
	 
	tmo_count = HWRM_SHORT_TIMEOUT_COUNTER;
	timeout = timeout - HWRM_SHORT_MIN_TIMEOUT * HWRM_SHORT_TIMEOUT_COUNTER;
	tmo_count += DIV_ROUND_UP(timeout, HWRM_MIN_TIMEOUT);

	if (le16_to_cpu(ctx->req->cmpl_ring) != INVALID_HW_RING_ID) {
		 
		while (READ_ONCE(token->state) < BNXT_HWRM_COMPLETE &&
		       i++ < tmo_count) {
			 
			if (test_bit(BNXT_STATE_FW_FATAL_COND, &bp->state))
				goto exit;
			 
			if (i < HWRM_SHORT_TIMEOUT_COUNTER) {
				usleep_range(HWRM_SHORT_MIN_TIMEOUT,
					     HWRM_SHORT_MAX_TIMEOUT);
			} else {
				if (hwrm_wait_must_abort(bp, req_type, &sts)) {
					hwrm_err(bp, ctx, "Resp cmpl intr abandoning msg: 0x%x due to firmware status: 0x%x\n",
						 req_type, sts);
					goto exit;
				}
				usleep_range(HWRM_MIN_TIMEOUT,
					     HWRM_MAX_TIMEOUT);
			}
		}

		if (READ_ONCE(token->state) != BNXT_HWRM_COMPLETE) {
			hwrm_err(bp, ctx, "Resp cmpl intr err msg: 0x%x\n",
				 req_type);
			goto exit;
		}
		len = le16_to_cpu(READ_ONCE(ctx->resp->resp_len));
		valid = ((u8 *)ctx->resp) + len - 1;
	} else {
		__le16 seen_out_of_seq = ctx->req->seq_id;  
		int j;

		 
		for (i = 0; i < tmo_count; i++) {
			 
			if (test_bit(BNXT_STATE_FW_FATAL_COND, &bp->state))
				goto exit;

			if (token &&
			    READ_ONCE(token->state) == BNXT_HWRM_DEFERRED) {
				__hwrm_release_token(bp, token);
				token = NULL;
			}

			len = le16_to_cpu(READ_ONCE(ctx->resp->resp_len));
			if (len) {
				__le16 resp_seq = READ_ONCE(ctx->resp->seq_id);

				if (resp_seq == ctx->req->seq_id)
					break;
				if (resp_seq != seen_out_of_seq) {
					netdev_warn(bp->dev, "Discarding out of seq response: 0x%x for msg {0x%x 0x%x}\n",
						    le16_to_cpu(resp_seq),
						    req_type,
						    le16_to_cpu(ctx->req->seq_id));
					seen_out_of_seq = resp_seq;
				}
			}

			 
			if (i < HWRM_SHORT_TIMEOUT_COUNTER) {
				usleep_range(HWRM_SHORT_MIN_TIMEOUT,
					     HWRM_SHORT_MAX_TIMEOUT);
			} else {
				if (hwrm_wait_must_abort(bp, req_type, &sts)) {
					hwrm_err(bp, ctx, "Abandoning msg {0x%x 0x%x} len: %d due to firmware status: 0x%x\n",
						 req_type,
						 le16_to_cpu(ctx->req->seq_id),
						 len, sts);
					goto exit;
				}
				usleep_range(HWRM_MIN_TIMEOUT,
					     HWRM_MAX_TIMEOUT);
			}
		}

		if (i >= tmo_count) {
			hwrm_err(bp, ctx, "Error (timeout: %u) msg {0x%x 0x%x} len:%d\n",
				 hwrm_total_timeout(i), req_type,
				 le16_to_cpu(ctx->req->seq_id), len);
			goto exit;
		}

		 
		valid = ((u8 *)ctx->resp) + len - 1;
		for (j = 0; j < HWRM_VALID_BIT_DELAY_USEC; ) {
			 
			dma_rmb();
			if (*valid)
				break;
			if (j < 10) {
				udelay(1);
				j++;
			} else {
				usleep_range(20, 30);
				j += 20;
			}
		}

		if (j >= HWRM_VALID_BIT_DELAY_USEC) {
			hwrm_err(bp, ctx, "Error (timeout: %u) msg {0x%x 0x%x} len:%d v:%d\n",
				 hwrm_total_timeout(i) + j, req_type,
				 le16_to_cpu(ctx->req->seq_id), len, *valid);
			goto exit;
		}
	}

	 
	*valid = 0;
	rc = le16_to_cpu(ctx->resp->error_code);
	if (rc == HWRM_ERR_CODE_BUSY && !(ctx->flags & BNXT_HWRM_CTX_SILENT))
		netdev_warn(bp->dev, "FW returned busy, hwrm req_type 0x%x\n",
			    req_type);
	else if (rc && rc != HWRM_ERR_CODE_PF_UNAVAILABLE)
		hwrm_err(bp, ctx, "hwrm req_type 0x%x seq id 0x%x error 0x%x\n",
			 req_type, token->seq_id, rc);
	rc = __hwrm_to_stderr(rc);
exit:
	if (token)
		__hwrm_release_token(bp, token);
	if (ctx->flags & BNXT_HWRM_INTERNAL_CTX_OWNED)
		ctx->flags |= BNXT_HWRM_INTERNAL_RESP_DIRTY;
	else
		__hwrm_ctx_drop(bp, ctx);
	return rc;
}

 
int hwrm_req_send(struct bnxt *bp, void *req)
{
	struct bnxt_hwrm_ctx *ctx = __hwrm_ctx(bp, req);

	if (!ctx)
		return -EINVAL;

	return __hwrm_send(bp, ctx);
}

 
int hwrm_req_send_silent(struct bnxt *bp, void *req)
{
	hwrm_req_flags(bp, req, BNXT_HWRM_CTX_SILENT);
	return hwrm_req_send(bp, req);
}

 
void *
hwrm_req_dma_slice(struct bnxt *bp, void *req, u32 size, dma_addr_t *dma_handle)
{
	struct bnxt_hwrm_ctx *ctx = __hwrm_ctx(bp, req);
	u8 *end = ((u8 *)req) + BNXT_HWRM_DMA_SIZE;
	struct input *input = req;
	u8 *addr, *req_addr = req;
	u32 max_offset, offset;

	if (!ctx)
		return NULL;

	max_offset = BNXT_HWRM_DMA_SIZE - ctx->allocated;
	offset = max_offset - size;
	offset = ALIGN_DOWN(offset, BNXT_HWRM_DMA_ALIGN);
	addr = req_addr + offset;

	if (addr < req_addr + max_offset && req_addr + ctx->req_len <= addr) {
		ctx->allocated = end - addr;
		*dma_handle = ctx->dma_handle + offset;
		return addr;
	}

	 
	if (ctx->slice_addr) {
		 
		netdev_err(bp->dev, "HWRM refusing to reallocate DMA slice, req_type = %u\n",
			   (u32)le16_to_cpu(input->req_type));
		dump_stack();
		return NULL;
	}

	addr = dma_alloc_coherent(&bp->pdev->dev, size, dma_handle, ctx->gfp);

	if (!addr)
		return NULL;

	ctx->slice_addr = addr;
	ctx->slice_size = size;
	ctx->slice_handle = *dma_handle;

	return addr;
}
