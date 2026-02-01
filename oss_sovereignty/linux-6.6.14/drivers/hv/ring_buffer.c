
 
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/hyperv.h>
#include <linux/uio.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/prefetch.h>
#include <linux/io.h>
#include <asm/mshyperv.h>

#include "hyperv_vmbus.h"

#define VMBUS_PKT_TRAILER	8

 

static void hv_signal_on_write(u32 old_write, struct vmbus_channel *channel)
{
	struct hv_ring_buffer_info *rbi = &channel->outbound;

	virt_mb();
	if (READ_ONCE(rbi->ring_buffer->interrupt_mask))
		return;

	 
	virt_rmb();
	 
	if (old_write == READ_ONCE(rbi->ring_buffer->read_index)) {
		++channel->intr_out_empty;
		vmbus_setevent(channel);
	}
}

 
static inline u32
hv_get_next_write_location(struct hv_ring_buffer_info *ring_info)
{
	u32 next = ring_info->ring_buffer->write_index;

	return next;
}

 
static inline void
hv_set_next_write_location(struct hv_ring_buffer_info *ring_info,
		     u32 next_write_location)
{
	ring_info->ring_buffer->write_index = next_write_location;
}

 
static inline u32
hv_get_ring_buffersize(const struct hv_ring_buffer_info *ring_info)
{
	return ring_info->ring_datasize;
}

 
static inline u64
hv_get_ring_bufferindices(struct hv_ring_buffer_info *ring_info)
{
	return (u64)ring_info->ring_buffer->write_index << 32;
}

 
static u32 hv_copyto_ringbuffer(
	struct hv_ring_buffer_info	*ring_info,
	u32				start_write_offset,
	const void			*src,
	u32				srclen)
{
	void *ring_buffer = hv_get_ring_buffer(ring_info);
	u32 ring_buffer_size = hv_get_ring_buffersize(ring_info);

	memcpy(ring_buffer + start_write_offset, src, srclen);

	start_write_offset += srclen;
	if (start_write_offset >= ring_buffer_size)
		start_write_offset -= ring_buffer_size;

	return start_write_offset;
}

 
static void
hv_get_ringbuffer_availbytes(const struct hv_ring_buffer_info *rbi,
			     u32 *read, u32 *write)
{
	u32 read_loc, write_loc, dsize;

	 
	read_loc = READ_ONCE(rbi->ring_buffer->read_index);
	write_loc = READ_ONCE(rbi->ring_buffer->write_index);
	dsize = rbi->ring_datasize;

	*write = write_loc >= read_loc ? dsize - (write_loc - read_loc) :
		read_loc - write_loc;
	*read = dsize - *write;
}

 
int hv_ringbuffer_get_debuginfo(struct hv_ring_buffer_info *ring_info,
				struct hv_ring_buffer_debug_info *debug_info)
{
	u32 bytes_avail_towrite;
	u32 bytes_avail_toread;

	mutex_lock(&ring_info->ring_buffer_mutex);

	if (!ring_info->ring_buffer) {
		mutex_unlock(&ring_info->ring_buffer_mutex);
		return -EINVAL;
	}

	hv_get_ringbuffer_availbytes(ring_info,
				     &bytes_avail_toread,
				     &bytes_avail_towrite);
	debug_info->bytes_avail_toread = bytes_avail_toread;
	debug_info->bytes_avail_towrite = bytes_avail_towrite;
	debug_info->current_read_index = ring_info->ring_buffer->read_index;
	debug_info->current_write_index = ring_info->ring_buffer->write_index;
	debug_info->current_interrupt_mask
		= ring_info->ring_buffer->interrupt_mask;
	mutex_unlock(&ring_info->ring_buffer_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(hv_ringbuffer_get_debuginfo);

 
void hv_ringbuffer_pre_init(struct vmbus_channel *channel)
{
	mutex_init(&channel->inbound.ring_buffer_mutex);
	mutex_init(&channel->outbound.ring_buffer_mutex);
}

 
int hv_ringbuffer_init(struct hv_ring_buffer_info *ring_info,
		       struct page *pages, u32 page_cnt, u32 max_pkt_size)
{
	struct page **pages_wraparound;
	int i;

	BUILD_BUG_ON((sizeof(struct hv_ring_buffer) != PAGE_SIZE));

	 
	pages_wraparound = kcalloc(page_cnt * 2 - 1,
				   sizeof(struct page *),
				   GFP_KERNEL);
	if (!pages_wraparound)
		return -ENOMEM;

	pages_wraparound[0] = pages;
	for (i = 0; i < 2 * (page_cnt - 1); i++)
		pages_wraparound[i + 1] =
			&pages[i % (page_cnt - 1) + 1];

	ring_info->ring_buffer = (struct hv_ring_buffer *)
		vmap(pages_wraparound, page_cnt * 2 - 1, VM_MAP,
			pgprot_decrypted(PAGE_KERNEL));

	kfree(pages_wraparound);
	if (!ring_info->ring_buffer)
		return -ENOMEM;

	 
	memset(ring_info->ring_buffer, 0, HV_HYP_PAGE_SIZE);

	ring_info->ring_buffer->read_index =
		ring_info->ring_buffer->write_index = 0;

	 
	ring_info->ring_buffer->feature_bits.value = 1;

	ring_info->ring_size = page_cnt << PAGE_SHIFT;
	ring_info->ring_size_div10_reciprocal =
		reciprocal_value(ring_info->ring_size / 10);
	ring_info->ring_datasize = ring_info->ring_size -
		sizeof(struct hv_ring_buffer);
	ring_info->priv_read_index = 0;

	 
	if (max_pkt_size) {
		ring_info->pkt_buffer = kzalloc(max_pkt_size, GFP_KERNEL);
		if (!ring_info->pkt_buffer)
			return -ENOMEM;
		ring_info->pkt_buffer_size = max_pkt_size;
	}

	spin_lock_init(&ring_info->ring_lock);

	return 0;
}

 
void hv_ringbuffer_cleanup(struct hv_ring_buffer_info *ring_info)
{
	mutex_lock(&ring_info->ring_buffer_mutex);
	vunmap(ring_info->ring_buffer);
	ring_info->ring_buffer = NULL;
	mutex_unlock(&ring_info->ring_buffer_mutex);

	kfree(ring_info->pkt_buffer);
	ring_info->pkt_buffer = NULL;
	ring_info->pkt_buffer_size = 0;
}

 

bool hv_ringbuffer_spinlock_busy(struct vmbus_channel *channel)
{
	struct hv_ring_buffer_info *rinfo = &channel->outbound;

	return spin_is_locked(&rinfo->ring_lock);
}
EXPORT_SYMBOL_GPL(hv_ringbuffer_spinlock_busy);

 
int hv_ringbuffer_write(struct vmbus_channel *channel,
			const struct kvec *kv_list, u32 kv_count,
			u64 requestid, u64 *trans_id)
{
	int i;
	u32 bytes_avail_towrite;
	u32 totalbytes_towrite = sizeof(u64);
	u32 next_write_location;
	u32 old_write;
	u64 prev_indices;
	unsigned long flags;
	struct hv_ring_buffer_info *outring_info = &channel->outbound;
	struct vmpacket_descriptor *desc = kv_list[0].iov_base;
	u64 __trans_id, rqst_id = VMBUS_NO_RQSTOR;

	if (channel->rescind)
		return -ENODEV;

	for (i = 0; i < kv_count; i++)
		totalbytes_towrite += kv_list[i].iov_len;

	spin_lock_irqsave(&outring_info->ring_lock, flags);

	bytes_avail_towrite = hv_get_bytes_to_write(outring_info);

	 
	if (bytes_avail_towrite <= totalbytes_towrite) {
		++channel->out_full_total;

		if (!channel->out_full_flag) {
			++channel->out_full_first;
			channel->out_full_flag = true;
		}

		spin_unlock_irqrestore(&outring_info->ring_lock, flags);
		return -EAGAIN;
	}

	channel->out_full_flag = false;

	 
	next_write_location = hv_get_next_write_location(outring_info);

	old_write = next_write_location;

	for (i = 0; i < kv_count; i++) {
		next_write_location = hv_copyto_ringbuffer(outring_info,
						     next_write_location,
						     kv_list[i].iov_base,
						     kv_list[i].iov_len);
	}

	 

	if (desc->flags == VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED) {
		if (channel->next_request_id_callback != NULL) {
			rqst_id = channel->next_request_id_callback(channel, requestid);
			if (rqst_id == VMBUS_RQST_ERROR) {
				spin_unlock_irqrestore(&outring_info->ring_lock, flags);
				return -EAGAIN;
			}
		}
	}
	desc = hv_get_ring_buffer(outring_info) + old_write;
	__trans_id = (rqst_id == VMBUS_NO_RQSTOR) ? requestid : rqst_id;
	 
	WRITE_ONCE(desc->trans_id, __trans_id);
	if (trans_id)
		*trans_id = __trans_id;

	 
	prev_indices = hv_get_ring_bufferindices(outring_info);

	next_write_location = hv_copyto_ringbuffer(outring_info,
					     next_write_location,
					     &prev_indices,
					     sizeof(u64));

	 
	virt_mb();

	 
	hv_set_next_write_location(outring_info, next_write_location);


	spin_unlock_irqrestore(&outring_info->ring_lock, flags);

	hv_signal_on_write(old_write, channel);

	if (channel->rescind) {
		if (rqst_id != VMBUS_NO_RQSTOR) {
			 
			if (channel->request_addr_callback != NULL)
				channel->request_addr_callback(channel, rqst_id);
		}
		return -ENODEV;
	}

	return 0;
}

int hv_ringbuffer_read(struct vmbus_channel *channel,
		       void *buffer, u32 buflen, u32 *buffer_actual_len,
		       u64 *requestid, bool raw)
{
	struct vmpacket_descriptor *desc;
	u32 packetlen, offset;

	if (unlikely(buflen == 0))
		return -EINVAL;

	*buffer_actual_len = 0;
	*requestid = 0;

	 
	desc = hv_pkt_iter_first(channel);
	if (desc == NULL) {
		 
		return 0;
	}

	offset = raw ? 0 : (desc->offset8 << 3);
	packetlen = (desc->len8 << 3) - offset;
	*buffer_actual_len = packetlen;
	*requestid = desc->trans_id;

	if (unlikely(packetlen > buflen))
		return -ENOBUFS;

	 
	memcpy(buffer, (const char *)desc + offset, packetlen);

	 
	__hv_pkt_iter_next(channel, desc);

	 
	hv_pkt_iter_close(channel);

	return 0;
}

 
static u32 hv_pkt_iter_avail(const struct hv_ring_buffer_info *rbi)
{
	u32 priv_read_loc = rbi->priv_read_index;
	u32 write_loc;

	 
	write_loc = virt_load_acquire(&rbi->ring_buffer->write_index);

	if (write_loc >= priv_read_loc)
		return write_loc - priv_read_loc;
	else
		return (rbi->ring_datasize - priv_read_loc) + write_loc;
}

 
struct vmpacket_descriptor *hv_pkt_iter_first(struct vmbus_channel *channel)
{
	struct hv_ring_buffer_info *rbi = &channel->inbound;
	struct vmpacket_descriptor *desc, *desc_copy;
	u32 bytes_avail, pkt_len, pkt_offset;

	hv_debug_delay_test(channel, MESSAGE_DELAY);

	bytes_avail = hv_pkt_iter_avail(rbi);
	if (bytes_avail < sizeof(struct vmpacket_descriptor))
		return NULL;
	bytes_avail = min(rbi->pkt_buffer_size, bytes_avail);

	desc = (struct vmpacket_descriptor *)(hv_get_ring_buffer(rbi) + rbi->priv_read_index);

	 
	pkt_len = READ_ONCE(desc->len8) << 3;
	pkt_offset = READ_ONCE(desc->offset8) << 3;

	 
	if (pkt_len < sizeof(struct vmpacket_descriptor) || pkt_len > bytes_avail)
		pkt_len = bytes_avail;

	 
	if (pkt_offset < sizeof(struct vmpacket_descriptor) || pkt_offset > pkt_len)
		pkt_offset = sizeof(struct vmpacket_descriptor);

	 
	desc_copy = (struct vmpacket_descriptor *)rbi->pkt_buffer;
	memcpy(desc_copy, desc, pkt_len);

	 
	desc_copy->len8 = pkt_len >> 3;
	desc_copy->offset8 = pkt_offset >> 3;

	return desc_copy;
}
EXPORT_SYMBOL_GPL(hv_pkt_iter_first);

 
struct vmpacket_descriptor *
__hv_pkt_iter_next(struct vmbus_channel *channel,
		   const struct vmpacket_descriptor *desc)
{
	struct hv_ring_buffer_info *rbi = &channel->inbound;
	u32 packetlen = desc->len8 << 3;
	u32 dsize = rbi->ring_datasize;

	hv_debug_delay_test(channel, MESSAGE_DELAY);
	 
	rbi->priv_read_index += packetlen + VMBUS_PKT_TRAILER;
	if (rbi->priv_read_index >= dsize)
		rbi->priv_read_index -= dsize;

	 
	return hv_pkt_iter_first(channel);
}
EXPORT_SYMBOL_GPL(__hv_pkt_iter_next);

 
static u32 hv_pkt_iter_bytes_read(const struct hv_ring_buffer_info *rbi,
					u32 start_read_index)
{
	if (rbi->priv_read_index >= start_read_index)
		return rbi->priv_read_index - start_read_index;
	else
		return rbi->ring_datasize - start_read_index +
			rbi->priv_read_index;
}

 
void hv_pkt_iter_close(struct vmbus_channel *channel)
{
	struct hv_ring_buffer_info *rbi = &channel->inbound;
	u32 curr_write_sz, pending_sz, bytes_read, start_read_index;

	 
	virt_rmb();
	start_read_index = rbi->ring_buffer->read_index;
	rbi->ring_buffer->read_index = rbi->priv_read_index;

	 
	if (!rbi->ring_buffer->feature_bits.feat_pending_send_sz)
		return;

	 
	virt_mb();

	 
	pending_sz = READ_ONCE(rbi->ring_buffer->pending_send_sz);
	if (!pending_sz)
		return;

	 
	virt_rmb();
	curr_write_sz = hv_get_bytes_to_write(rbi);
	bytes_read = hv_pkt_iter_bytes_read(rbi, start_read_index);

	 
	if (curr_write_sz - bytes_read > pending_sz)
		return;

	 
	if (curr_write_sz <= pending_sz)
		return;

	++channel->intr_in_full;
	vmbus_setevent(channel);
}
EXPORT_SYMBOL_GPL(hv_pkt_iter_close);
