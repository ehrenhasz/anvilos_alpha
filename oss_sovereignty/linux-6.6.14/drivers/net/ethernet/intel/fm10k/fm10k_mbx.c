
 

#include "fm10k_common.h"

 
static void fm10k_fifo_init(struct fm10k_mbx_fifo *fifo, u32 *buffer, u16 size)
{
	fifo->buffer = buffer;
	fifo->size = size;
	fifo->head = 0;
	fifo->tail = 0;
}

 
static u16 fm10k_fifo_used(struct fm10k_mbx_fifo *fifo)
{
	return fifo->tail - fifo->head;
}

 
static u16 fm10k_fifo_unused(struct fm10k_mbx_fifo *fifo)
{
	return fifo->size + fifo->head - fifo->tail;
}

 
static bool fm10k_fifo_empty(struct fm10k_mbx_fifo *fifo)
{
	return fifo->head == fifo->tail;
}

 
static u16 fm10k_fifo_head_offset(struct fm10k_mbx_fifo *fifo, u16 offset)
{
	return (fifo->head + offset) & (fifo->size - 1);
}

 
static u16 fm10k_fifo_tail_offset(struct fm10k_mbx_fifo *fifo, u16 offset)
{
	return (fifo->tail + offset) & (fifo->size - 1);
}

 
static u16 fm10k_fifo_head_len(struct fm10k_mbx_fifo *fifo)
{
	u32 *head = fifo->buffer + fm10k_fifo_head_offset(fifo, 0);

	 
	if (fm10k_fifo_empty(fifo))
		return 0;

	 
	return FM10K_TLV_DWORD_LEN(*head);
}

 
static u16 fm10k_fifo_head_drop(struct fm10k_mbx_fifo *fifo)
{
	u16 len = fm10k_fifo_head_len(fifo);

	 
	fifo->head += len;

	return len;
}

 
static void fm10k_fifo_drop_all(struct fm10k_mbx_fifo *fifo)
{
	fifo->head = fifo->tail;
}

 
static u16 fm10k_mbx_index_len(struct fm10k_mbx_info *mbx, u16 head, u16 tail)
{
	u16 len = tail - head;

	 
	if (len > tail)
		len -= 2;

	return len & ((mbx->mbmem_len << 1) - 1);
}

 
static u16 fm10k_mbx_tail_add(struct fm10k_mbx_info *mbx, u16 offset)
{
	u16 tail = (mbx->tail + offset + 1) & ((mbx->mbmem_len << 1) - 1);

	 
	return (tail > mbx->tail) ? --tail : ++tail;
}

 
static u16 fm10k_mbx_tail_sub(struct fm10k_mbx_info *mbx, u16 offset)
{
	u16 tail = (mbx->tail - offset - 1) & ((mbx->mbmem_len << 1) - 1);

	 
	return (tail < mbx->tail) ? ++tail : --tail;
}

 
static u16 fm10k_mbx_head_add(struct fm10k_mbx_info *mbx, u16 offset)
{
	u16 head = (mbx->head + offset + 1) & ((mbx->mbmem_len << 1) - 1);

	 
	return (head > mbx->head) ? --head : ++head;
}

 
static u16 fm10k_mbx_head_sub(struct fm10k_mbx_info *mbx, u16 offset)
{
	u16 head = (mbx->head - offset - 1) & ((mbx->mbmem_len << 1) - 1);

	 
	return (head < mbx->head) ? ++head : --head;
}

 
static u16 fm10k_mbx_pushed_tail_len(struct fm10k_mbx_info *mbx)
{
	u32 *tail = mbx->rx.buffer + fm10k_fifo_tail_offset(&mbx->rx, 0);

	 
	if (!mbx->pushed)
		return 0;

	return FM10K_TLV_DWORD_LEN(*tail);
}

 
static void fm10k_fifo_write_copy(struct fm10k_mbx_fifo *fifo,
				  const u32 *msg, u16 tail_offset, u16 len)
{
	u16 end = fm10k_fifo_tail_offset(fifo, tail_offset);
	u32 *tail = fifo->buffer + end;

	 
	end = fifo->size - end;

	 
	if (end < len)
		memcpy(fifo->buffer, msg + end, (len - end) << 2);
	else
		end = len;

	 
	memcpy(tail, msg, end << 2);
}

 
static s32 fm10k_fifo_enqueue(struct fm10k_mbx_fifo *fifo, const u32 *msg)
{
	u16 len = FM10K_TLV_DWORD_LEN(*msg);

	 
	if (len > fifo->size)
		return FM10K_MBX_ERR_SIZE;

	 
	if (len > fm10k_fifo_unused(fifo))
		return FM10K_MBX_ERR_NO_SPACE;

	 
	fm10k_fifo_write_copy(fifo, msg, 0, len);

	 
	wmb();

	 
	fifo->tail += len;

	return 0;
}

 
static u16 fm10k_mbx_validate_msg_size(struct fm10k_mbx_info *mbx, u16 len)
{
	struct fm10k_mbx_fifo *fifo = &mbx->rx;
	u16 total_len = 0, msg_len;

	 
	len += mbx->pushed;

	 
	do {
		u32 *msg;

		msg = fifo->buffer + fm10k_fifo_tail_offset(fifo, total_len);
		msg_len = FM10K_TLV_DWORD_LEN(*msg);
		total_len += msg_len;
	} while (total_len < len);

	 
	if ((len < total_len) && (msg_len <= mbx->max_size))
		return 0;

	 
	return (len < total_len) ? len : (len - total_len);
}

 
static void fm10k_mbx_write_copy(struct fm10k_hw *hw,
				 struct fm10k_mbx_info *mbx)
{
	struct fm10k_mbx_fifo *fifo = &mbx->tx;
	u32 mbmem = mbx->mbmem_reg;
	u32 *head = fifo->buffer;
	u16 end, len, tail, mask;

	if (!mbx->tail_len)
		return;

	 
	mask = mbx->mbmem_len - 1;
	len = mbx->tail_len;
	tail = fm10k_mbx_tail_sub(mbx, len);
	if (tail > mask)
		tail++;

	 
	end = fm10k_fifo_head_offset(fifo, mbx->pulled);
	head += end;

	 
	rmb();

	 
	for (end = fifo->size - end; len; head = fifo->buffer) {
		do {
			 
			tail &= mask;
			if (!tail)
				tail++;

			mbx->tx_mbmem_pulled++;

			 
			fm10k_write_reg(hw, mbmem + tail++, *(head++));
		} while (--len && --end);
	}
}

 
static void fm10k_mbx_pull_head(struct fm10k_hw *hw,
				struct fm10k_mbx_info *mbx, u16 head)
{
	u16 mbmem_len, len, ack = fm10k_mbx_index_len(mbx, head, mbx->tail);
	struct fm10k_mbx_fifo *fifo = &mbx->tx;

	 
	mbx->pulled += mbx->tail_len - ack;

	 
	mbmem_len = mbx->mbmem_len - 1;
	len = fm10k_fifo_used(fifo) - mbx->pulled;
	if (len > mbmem_len)
		len = mbmem_len;

	 
	mbx->tail = fm10k_mbx_tail_add(mbx, len - ack);
	mbx->tail_len = len;

	 
	for (len = fm10k_fifo_head_len(fifo);
	     len && (mbx->pulled >= len);
	     len = fm10k_fifo_head_len(fifo)) {
		mbx->pulled -= fm10k_fifo_head_drop(fifo);
		mbx->tx_messages++;
		mbx->tx_dwords += len;
	}

	 
	fm10k_mbx_write_copy(hw, mbx);
}

 
static void fm10k_mbx_read_copy(struct fm10k_hw *hw,
				struct fm10k_mbx_info *mbx)
{
	struct fm10k_mbx_fifo *fifo = &mbx->rx;
	u32 mbmem = mbx->mbmem_reg ^ mbx->mbmem_len;
	u32 *tail = fifo->buffer;
	u16 end, len, head;

	 
	len = mbx->head_len;
	head = fm10k_mbx_head_sub(mbx, len);
	if (head >= mbx->mbmem_len)
		head++;

	 
	end = fm10k_fifo_tail_offset(fifo, mbx->pushed);
	tail += end;

	 
	for (end = fifo->size - end; len; tail = fifo->buffer) {
		do {
			 
			head &= mbx->mbmem_len - 1;
			if (!head)
				head++;

			mbx->rx_mbmem_pushed++;

			 
			*(tail++) = fm10k_read_reg(hw, mbmem + head++);
		} while (--len && --end);
	}

	 
	wmb();
}

 
static s32 fm10k_mbx_push_tail(struct fm10k_hw *hw,
			       struct fm10k_mbx_info *mbx,
			       u16 tail)
{
	struct fm10k_mbx_fifo *fifo = &mbx->rx;
	u16 len, seq = fm10k_mbx_index_len(mbx, mbx->head, tail);

	 
	len = fm10k_fifo_unused(fifo) - mbx->pushed;
	if (len > seq)
		len = seq;

	 
	mbx->head = fm10k_mbx_head_add(mbx, len);
	mbx->head_len = len;

	 
	if (!len)
		return 0;

	 
	fm10k_mbx_read_copy(hw, mbx);

	 
	if (fm10k_mbx_validate_msg_size(mbx, len))
		return FM10K_MBX_ERR_SIZE;

	 
	mbx->pushed += len;

	 
	for (len = fm10k_mbx_pushed_tail_len(mbx);
	     len && (mbx->pushed >= len);
	     len = fm10k_mbx_pushed_tail_len(mbx)) {
		fifo->tail += len;
		mbx->pushed -= len;
		mbx->rx_messages++;
		mbx->rx_dwords += len;
	}

	return 0;
}

 
static const u16 fm10k_crc_16b_table[256] = {
	0x0000, 0x7956, 0xF2AC, 0x8BFA, 0xBC6D, 0xC53B, 0x4EC1, 0x3797,
	0x21EF, 0x58B9, 0xD343, 0xAA15, 0x9D82, 0xE4D4, 0x6F2E, 0x1678,
	0x43DE, 0x3A88, 0xB172, 0xC824, 0xFFB3, 0x86E5, 0x0D1F, 0x7449,
	0x6231, 0x1B67, 0x909D, 0xE9CB, 0xDE5C, 0xA70A, 0x2CF0, 0x55A6,
	0x87BC, 0xFEEA, 0x7510, 0x0C46, 0x3BD1, 0x4287, 0xC97D, 0xB02B,
	0xA653, 0xDF05, 0x54FF, 0x2DA9, 0x1A3E, 0x6368, 0xE892, 0x91C4,
	0xC462, 0xBD34, 0x36CE, 0x4F98, 0x780F, 0x0159, 0x8AA3, 0xF3F5,
	0xE58D, 0x9CDB, 0x1721, 0x6E77, 0x59E0, 0x20B6, 0xAB4C, 0xD21A,
	0x564D, 0x2F1B, 0xA4E1, 0xDDB7, 0xEA20, 0x9376, 0x188C, 0x61DA,
	0x77A2, 0x0EF4, 0x850E, 0xFC58, 0xCBCF, 0xB299, 0x3963, 0x4035,
	0x1593, 0x6CC5, 0xE73F, 0x9E69, 0xA9FE, 0xD0A8, 0x5B52, 0x2204,
	0x347C, 0x4D2A, 0xC6D0, 0xBF86, 0x8811, 0xF147, 0x7ABD, 0x03EB,
	0xD1F1, 0xA8A7, 0x235D, 0x5A0B, 0x6D9C, 0x14CA, 0x9F30, 0xE666,
	0xF01E, 0x8948, 0x02B2, 0x7BE4, 0x4C73, 0x3525, 0xBEDF, 0xC789,
	0x922F, 0xEB79, 0x6083, 0x19D5, 0x2E42, 0x5714, 0xDCEE, 0xA5B8,
	0xB3C0, 0xCA96, 0x416C, 0x383A, 0x0FAD, 0x76FB, 0xFD01, 0x8457,
	0xAC9A, 0xD5CC, 0x5E36, 0x2760, 0x10F7, 0x69A1, 0xE25B, 0x9B0D,
	0x8D75, 0xF423, 0x7FD9, 0x068F, 0x3118, 0x484E, 0xC3B4, 0xBAE2,
	0xEF44, 0x9612, 0x1DE8, 0x64BE, 0x5329, 0x2A7F, 0xA185, 0xD8D3,
	0xCEAB, 0xB7FD, 0x3C07, 0x4551, 0x72C6, 0x0B90, 0x806A, 0xF93C,
	0x2B26, 0x5270, 0xD98A, 0xA0DC, 0x974B, 0xEE1D, 0x65E7, 0x1CB1,
	0x0AC9, 0x739F, 0xF865, 0x8133, 0xB6A4, 0xCFF2, 0x4408, 0x3D5E,
	0x68F8, 0x11AE, 0x9A54, 0xE302, 0xD495, 0xADC3, 0x2639, 0x5F6F,
	0x4917, 0x3041, 0xBBBB, 0xC2ED, 0xF57A, 0x8C2C, 0x07D6, 0x7E80,
	0xFAD7, 0x8381, 0x087B, 0x712D, 0x46BA, 0x3FEC, 0xB416, 0xCD40,
	0xDB38, 0xA26E, 0x2994, 0x50C2, 0x6755, 0x1E03, 0x95F9, 0xECAF,
	0xB909, 0xC05F, 0x4BA5, 0x32F3, 0x0564, 0x7C32, 0xF7C8, 0x8E9E,
	0x98E6, 0xE1B0, 0x6A4A, 0x131C, 0x248B, 0x5DDD, 0xD627, 0xAF71,
	0x7D6B, 0x043D, 0x8FC7, 0xF691, 0xC106, 0xB850, 0x33AA, 0x4AFC,
	0x5C84, 0x25D2, 0xAE28, 0xD77E, 0xE0E9, 0x99BF, 0x1245, 0x6B13,
	0x3EB5, 0x47E3, 0xCC19, 0xB54F, 0x82D8, 0xFB8E, 0x7074, 0x0922,
	0x1F5A, 0x660C, 0xEDF6, 0x94A0, 0xA337, 0xDA61, 0x519B, 0x28CD };

 
static u16 fm10k_crc_16b(const u32 *data, u16 seed, u16 len)
{
	u32 result = seed;

	while (len--) {
		result ^= *(data++);
		result = (result >> 8) ^ fm10k_crc_16b_table[result & 0xFF];
		result = (result >> 8) ^ fm10k_crc_16b_table[result & 0xFF];

		if (!(len--))
			break;

		result = (result >> 8) ^ fm10k_crc_16b_table[result & 0xFF];
		result = (result >> 8) ^ fm10k_crc_16b_table[result & 0xFF];
	}

	return (u16)result;
}

 
static u16 fm10k_fifo_crc(struct fm10k_mbx_fifo *fifo, u16 offset,
			  u16 len, u16 seed)
{
	u32 *data = fifo->buffer + offset;

	 
	offset = fifo->size - offset;

	 
	if (offset < len) {
		seed = fm10k_crc_16b(data, seed, offset * 2);
		data = fifo->buffer;
		len -= offset;
	}

	 
	return fm10k_crc_16b(data, seed, len * 2);
}

 
static void fm10k_mbx_update_local_crc(struct fm10k_mbx_info *mbx, u16 head)
{
	u16 len = mbx->tail_len - fm10k_mbx_index_len(mbx, head, mbx->tail);

	 
	head = fm10k_fifo_head_offset(&mbx->tx, mbx->pulled);

	 
	mbx->local = fm10k_fifo_crc(&mbx->tx, head, len, mbx->local);
}

 
static s32 fm10k_mbx_verify_remote_crc(struct fm10k_mbx_info *mbx)
{
	struct fm10k_mbx_fifo *fifo = &mbx->rx;
	u16 len = mbx->head_len;
	u16 offset = fm10k_fifo_tail_offset(fifo, mbx->pushed) - len;
	u16 crc;

	 
	if (len)
		mbx->remote = fm10k_fifo_crc(fifo, offset, len, mbx->remote);

	 
	crc = fm10k_crc_16b(&mbx->mbx_hdr, mbx->remote, 1);

	 
	return crc ? FM10K_MBX_ERR_CRC : 0;
}

 
static bool fm10k_mbx_rx_ready(struct fm10k_mbx_info *mbx)
{
	u16 msg_size = fm10k_fifo_head_len(&mbx->rx);

	return msg_size && (fm10k_fifo_used(&mbx->rx) >= msg_size);
}

 
static bool fm10k_mbx_tx_ready(struct fm10k_mbx_info *mbx, u16 len)
{
	u16 fifo_unused = fm10k_fifo_unused(&mbx->tx);

	return (mbx->state == FM10K_STATE_OPEN) && (fifo_unused >= len);
}

 
static bool fm10k_mbx_tx_complete(struct fm10k_mbx_info *mbx)
{
	return fm10k_fifo_empty(&mbx->tx);
}

 
static u16 fm10k_mbx_dequeue_rx(struct fm10k_hw *hw,
				struct fm10k_mbx_info *mbx)
{
	struct fm10k_mbx_fifo *fifo = &mbx->rx;
	s32 err;
	u16 cnt;

	 
	for (cnt = 0; !fm10k_fifo_empty(fifo); cnt++) {
		err = fm10k_tlv_msg_parse(hw, fifo->buffer + fifo->head,
					  mbx, mbx->msg_data);
		if (err < 0)
			mbx->rx_parse_err++;

		fm10k_fifo_head_drop(fifo);
	}

	 
	memmove(fifo->buffer, fifo->buffer + fifo->tail, mbx->pushed << 2);

	 
	fifo->tail -= fifo->head;
	fifo->head = 0;

	return cnt;
}

 
static s32 fm10k_mbx_enqueue_tx(struct fm10k_hw *hw,
				struct fm10k_mbx_info *mbx, const u32 *msg)
{
	u32 countdown = mbx->timeout;
	s32 err;

	switch (mbx->state) {
	case FM10K_STATE_CLOSED:
	case FM10K_STATE_DISCONNECT:
		return FM10K_MBX_ERR_NO_MBX;
	default:
		break;
	}

	 
	err = fm10k_fifo_enqueue(&mbx->tx, msg);

	 
	while (err && countdown) {
		countdown--;
		udelay(mbx->udelay);
		mbx->ops.process(hw, mbx);
		err = fm10k_fifo_enqueue(&mbx->tx, msg);
	}

	 
	if (err) {
		mbx->timeout = 0;
		mbx->tx_busy++;
	}

	 
	if (!mbx->tail_len)
		mbx->ops.process(hw, mbx);

	return 0;
}

 
static s32 fm10k_mbx_read(struct fm10k_hw *hw, struct fm10k_mbx_info *mbx)
{
	 
	if (mbx->mbx_hdr)
		return FM10K_MBX_ERR_BUSY;

	 
	if (fm10k_read_reg(hw, mbx->mbx_reg) & FM10K_MBX_REQ_INTERRUPT)
		mbx->mbx_lock = FM10K_MBX_ACK;

	 
	fm10k_write_reg(hw, mbx->mbx_reg,
			FM10K_MBX_REQ_INTERRUPT | FM10K_MBX_ACK_INTERRUPT);

	 
	mbx->mbx_hdr = fm10k_read_reg(hw, mbx->mbmem_reg ^ mbx->mbmem_len);

	return 0;
}

 
static void fm10k_mbx_write(struct fm10k_hw *hw, struct fm10k_mbx_info *mbx)
{
	u32 mbmem = mbx->mbmem_reg;

	 
	fm10k_write_reg(hw, mbmem, mbx->mbx_hdr);

	 
	if (mbx->mbx_lock)
		fm10k_write_reg(hw, mbx->mbx_reg, mbx->mbx_lock);

	 
	mbx->mbx_hdr = 0;
	mbx->mbx_lock = 0;
}

 
static void fm10k_mbx_create_connect_hdr(struct fm10k_mbx_info *mbx)
{
	mbx->mbx_lock |= FM10K_MBX_REQ;

	mbx->mbx_hdr = FM10K_MSG_HDR_FIELD_SET(FM10K_MSG_CONNECT, TYPE) |
		       FM10K_MSG_HDR_FIELD_SET(mbx->head, HEAD) |
		       FM10K_MSG_HDR_FIELD_SET(mbx->rx.size - 1, CONNECT_SIZE);
}

 
static void fm10k_mbx_create_data_hdr(struct fm10k_mbx_info *mbx)
{
	u32 hdr = FM10K_MSG_HDR_FIELD_SET(FM10K_MSG_DATA, TYPE) |
		  FM10K_MSG_HDR_FIELD_SET(mbx->tail, TAIL) |
		  FM10K_MSG_HDR_FIELD_SET(mbx->head, HEAD);
	struct fm10k_mbx_fifo *fifo = &mbx->tx;
	u16 crc;

	if (mbx->tail_len)
		mbx->mbx_lock |= FM10K_MBX_REQ;

	 
	crc = fm10k_fifo_crc(fifo, fm10k_fifo_head_offset(fifo, mbx->pulled),
			     mbx->tail_len, mbx->local);
	crc = fm10k_crc_16b(&hdr, crc, 1);

	 
	mbx->mbx_hdr = hdr | FM10K_MSG_HDR_FIELD_SET(crc, CRC);
}

 
static void fm10k_mbx_create_disconnect_hdr(struct fm10k_mbx_info *mbx)
{
	u32 hdr = FM10K_MSG_HDR_FIELD_SET(FM10K_MSG_DISCONNECT, TYPE) |
		  FM10K_MSG_HDR_FIELD_SET(mbx->tail, TAIL) |
		  FM10K_MSG_HDR_FIELD_SET(mbx->head, HEAD);
	u16 crc = fm10k_crc_16b(&hdr, mbx->local, 1);

	mbx->mbx_lock |= FM10K_MBX_ACK;

	 
	mbx->mbx_hdr = hdr | FM10K_MSG_HDR_FIELD_SET(crc, CRC);
}

 
static void fm10k_mbx_create_fake_disconnect_hdr(struct fm10k_mbx_info *mbx)
{
	u32 hdr = FM10K_MSG_HDR_FIELD_SET(FM10K_MSG_DISCONNECT, TYPE) |
		  FM10K_MSG_HDR_FIELD_SET(mbx->head, TAIL) |
		  FM10K_MSG_HDR_FIELD_SET(mbx->tail, HEAD);
	u16 crc = fm10k_crc_16b(&hdr, mbx->local, 1);

	mbx->mbx_lock |= FM10K_MBX_ACK;

	 
	mbx->mbx_hdr = hdr | FM10K_MSG_HDR_FIELD_SET(crc, CRC);
}

 
static void fm10k_mbx_create_error_msg(struct fm10k_mbx_info *mbx, s32 err)
{
	 
	switch (err) {
	case FM10K_MBX_ERR_TAIL:
	case FM10K_MBX_ERR_HEAD:
	case FM10K_MBX_ERR_TYPE:
	case FM10K_MBX_ERR_SIZE:
	case FM10K_MBX_ERR_RSVD0:
	case FM10K_MBX_ERR_CRC:
		break;
	default:
		return;
	}

	mbx->mbx_lock |= FM10K_MBX_REQ;

	mbx->mbx_hdr = FM10K_MSG_HDR_FIELD_SET(FM10K_MSG_ERROR, TYPE) |
		       FM10K_MSG_HDR_FIELD_SET(err, ERR_NO) |
		       FM10K_MSG_HDR_FIELD_SET(mbx->head, HEAD);
}

 
static s32 fm10k_mbx_validate_msg_hdr(struct fm10k_mbx_info *mbx)
{
	u16 type, rsvd0, head, tail, size;
	const u32 *hdr = &mbx->mbx_hdr;

	type = FM10K_MSG_HDR_FIELD_GET(*hdr, TYPE);
	rsvd0 = FM10K_MSG_HDR_FIELD_GET(*hdr, RSVD0);
	tail = FM10K_MSG_HDR_FIELD_GET(*hdr, TAIL);
	head = FM10K_MSG_HDR_FIELD_GET(*hdr, HEAD);
	size = FM10K_MSG_HDR_FIELD_GET(*hdr, CONNECT_SIZE);

	if (rsvd0)
		return FM10K_MBX_ERR_RSVD0;

	switch (type) {
	case FM10K_MSG_DISCONNECT:
		 
		if (tail != mbx->head)
			return FM10K_MBX_ERR_TAIL;

		fallthrough;
	case FM10K_MSG_DATA:
		 
		if (!head || (head == FM10K_MSG_HDR_MASK(HEAD)))
			return FM10K_MBX_ERR_HEAD;
		if (fm10k_mbx_index_len(mbx, head, mbx->tail) > mbx->tail_len)
			return FM10K_MBX_ERR_HEAD;

		 
		if (!tail || (tail == FM10K_MSG_HDR_MASK(TAIL)))
			return FM10K_MBX_ERR_TAIL;
		if (fm10k_mbx_index_len(mbx, mbx->head, tail) < mbx->mbmem_len)
			break;

		return FM10K_MBX_ERR_TAIL;
	case FM10K_MSG_CONNECT:
		 
		if ((size < FM10K_VFMBX_MSG_MTU) || (size & (size + 1)))
			return FM10K_MBX_ERR_SIZE;

		fallthrough;
	case FM10K_MSG_ERROR:
		if (!head || (head == FM10K_MSG_HDR_MASK(HEAD)))
			return FM10K_MBX_ERR_HEAD;
		 
		if (tail)
			return FM10K_MBX_ERR_TAIL;

		break;
	default:
		return FM10K_MBX_ERR_TYPE;
	}

	return 0;
}

 
static s32 fm10k_mbx_create_reply(struct fm10k_hw *hw,
				  struct fm10k_mbx_info *mbx, u16 head)
{
	switch (mbx->state) {
	case FM10K_STATE_OPEN:
	case FM10K_STATE_DISCONNECT:
		 
		fm10k_mbx_update_local_crc(mbx, head);

		 
		fm10k_mbx_pull_head(hw, mbx, head);

		 
		if (mbx->tail_len || (mbx->state == FM10K_STATE_OPEN))
			fm10k_mbx_create_data_hdr(mbx);
		else
			fm10k_mbx_create_disconnect_hdr(mbx);
		break;
	case FM10K_STATE_CONNECT:
		 
		fm10k_mbx_create_connect_hdr(mbx);
		break;
	case FM10K_STATE_CLOSED:
		 
		fm10k_mbx_create_disconnect_hdr(mbx);
		break;
	default:
		break;
	}

	return 0;
}

 
static void fm10k_mbx_reset_work(struct fm10k_mbx_info *mbx)
{
	u16 len, head, ack;

	 
	mbx->max_size = mbx->rx.size - 1;

	 
	head = FM10K_MSG_HDR_FIELD_GET(mbx->mbx_hdr, HEAD);
	ack = fm10k_mbx_index_len(mbx, head, mbx->tail);
	mbx->pulled += mbx->tail_len - ack;

	 
	while (fm10k_fifo_head_len(&mbx->tx) && mbx->pulled) {
		len = fm10k_fifo_head_drop(&mbx->tx);
		mbx->tx_dropped++;
		if (mbx->pulled >= len)
			mbx->pulled -= len;
		else
			mbx->pulled = 0;
	}

	 
	mbx->pushed = 0;
	mbx->pulled = 0;
	mbx->tail_len = 0;
	mbx->head_len = 0;
	mbx->rx.tail = 0;
	mbx->rx.head = 0;
}

 
static void fm10k_mbx_update_max_size(struct fm10k_mbx_info *mbx, u16 size)
{
	u16 len;

	mbx->max_size = size;

	 
	for (len = fm10k_fifo_head_len(&mbx->tx);
	     len > size;
	     len = fm10k_fifo_head_len(&mbx->tx)) {
		fm10k_fifo_head_drop(&mbx->tx);
		mbx->tx_dropped++;
	}
}

 
static void fm10k_mbx_connect_reset(struct fm10k_mbx_info *mbx)
{
	 
	fm10k_mbx_reset_work(mbx);

	 
	mbx->local = FM10K_MBX_CRC_SEED;
	mbx->remote = FM10K_MBX_CRC_SEED;

	 
	if (mbx->state == FM10K_STATE_OPEN)
		mbx->state = FM10K_STATE_CONNECT;
	else
		mbx->state = FM10K_STATE_CLOSED;
}

 
static s32 fm10k_mbx_process_connect(struct fm10k_hw *hw,
				     struct fm10k_mbx_info *mbx)
{
	const enum fm10k_mbx_state state = mbx->state;
	const u32 *hdr = &mbx->mbx_hdr;
	u16 size, head;

	 
	size = FM10K_MSG_HDR_FIELD_GET(*hdr, CONNECT_SIZE);
	head = FM10K_MSG_HDR_FIELD_GET(*hdr, HEAD);

	switch (state) {
	case FM10K_STATE_DISCONNECT:
	case FM10K_STATE_OPEN:
		 
		fm10k_mbx_connect_reset(mbx);
		break;
	case FM10K_STATE_CONNECT:
		 
		if (size > mbx->rx.size) {
			mbx->max_size = mbx->rx.size - 1;
		} else {
			 
			mbx->state = FM10K_STATE_OPEN;

			fm10k_mbx_update_max_size(mbx, size);
		}
		break;
	default:
		break;
	}

	 
	mbx->tail = head;

	return fm10k_mbx_create_reply(hw, mbx, head);
}

 
static s32 fm10k_mbx_process_data(struct fm10k_hw *hw,
				  struct fm10k_mbx_info *mbx)
{
	const u32 *hdr = &mbx->mbx_hdr;
	u16 head, tail;
	s32 err;

	 
	head = FM10K_MSG_HDR_FIELD_GET(*hdr, HEAD);
	tail = FM10K_MSG_HDR_FIELD_GET(*hdr, TAIL);

	 
	if (mbx->state == FM10K_STATE_CONNECT) {
		mbx->tail = head;
		mbx->state = FM10K_STATE_OPEN;
	}

	 
	err = fm10k_mbx_push_tail(hw, mbx, tail);
	if (err < 0)
		return err;

	 
	err = fm10k_mbx_verify_remote_crc(mbx);
	if (err)
		return err;

	 
	fm10k_mbx_dequeue_rx(hw, mbx);

	return fm10k_mbx_create_reply(hw, mbx, head);
}

 
static s32 fm10k_mbx_process_disconnect(struct fm10k_hw *hw,
					struct fm10k_mbx_info *mbx)
{
	const enum fm10k_mbx_state state = mbx->state;
	const u32 *hdr = &mbx->mbx_hdr;
	u16 head;
	s32 err;

	 
	head = FM10K_MSG_HDR_FIELD_GET(*hdr, HEAD);

	 
	if (mbx->pushed)
		return FM10K_MBX_ERR_TAIL;

	 
	mbx->head_len = 0;

	 
	err = fm10k_mbx_verify_remote_crc(mbx);
	if (err)
		return err;

	switch (state) {
	case FM10K_STATE_DISCONNECT:
	case FM10K_STATE_OPEN:
		 
		if (!fm10k_mbx_tx_complete(mbx))
			break;

		 
		if (head != mbx->tail)
			return FM10K_MBX_ERR_HEAD;

		 
		fm10k_mbx_connect_reset(mbx);
		break;
	default:
		break;
	}

	return fm10k_mbx_create_reply(hw, mbx, head);
}

 
static s32 fm10k_mbx_process_error(struct fm10k_hw *hw,
				   struct fm10k_mbx_info *mbx)
{
	const u32 *hdr = &mbx->mbx_hdr;
	u16 head;

	 
	head = FM10K_MSG_HDR_FIELD_GET(*hdr, HEAD);

	switch (mbx->state) {
	case FM10K_STATE_OPEN:
	case FM10K_STATE_DISCONNECT:
		 
		fm10k_mbx_reset_work(mbx);

		 
		mbx->local = FM10K_MBX_CRC_SEED;
		mbx->remote = FM10K_MBX_CRC_SEED;

		 
		mbx->tail = head;

		 
		if (mbx->state == FM10K_STATE_OPEN) {
			mbx->state = FM10K_STATE_CONNECT;
			break;
		}

		 
		fm10k_mbx_create_connect_hdr(mbx);
		return 0;
	default:
		break;
	}

	return fm10k_mbx_create_reply(hw, mbx, mbx->tail);
}

 
static s32 fm10k_mbx_process(struct fm10k_hw *hw,
			     struct fm10k_mbx_info *mbx)
{
	s32 err;

	 
	if (mbx->state == FM10K_STATE_CLOSED)
		return 0;

	 
	err = fm10k_mbx_read(hw, mbx);
	if (err)
		return err;

	 
	err = fm10k_mbx_validate_msg_hdr(mbx);
	if (err < 0)
		goto msg_err;

	switch (FM10K_MSG_HDR_FIELD_GET(mbx->mbx_hdr, TYPE)) {
	case FM10K_MSG_CONNECT:
		err = fm10k_mbx_process_connect(hw, mbx);
		break;
	case FM10K_MSG_DATA:
		err = fm10k_mbx_process_data(hw, mbx);
		break;
	case FM10K_MSG_DISCONNECT:
		err = fm10k_mbx_process_disconnect(hw, mbx);
		break;
	case FM10K_MSG_ERROR:
		err = fm10k_mbx_process_error(hw, mbx);
		break;
	default:
		err = FM10K_MBX_ERR_TYPE;
		break;
	}

msg_err:
	 
	if (err < 0)
		fm10k_mbx_create_error_msg(mbx, err);

	 
	fm10k_mbx_write(hw, mbx);

	return err;
}

 
static void fm10k_mbx_disconnect(struct fm10k_hw *hw,
				 struct fm10k_mbx_info *mbx)
{
	int timeout = mbx->timeout ? FM10K_MBX_DISCONNECT_TIMEOUT : 0;

	 
	mbx->state = FM10K_STATE_DISCONNECT;

	 
	fm10k_write_reg(hw, mbx->mbx_reg, FM10K_MBX_REQ |
					  FM10K_MBX_INTERRUPT_DISABLE);
	do {
		udelay(FM10K_MBX_POLL_DELAY);
		mbx->ops.process(hw, mbx);
		timeout -= FM10K_MBX_POLL_DELAY;
	} while ((timeout > 0) && (mbx->state != FM10K_STATE_CLOSED));

	 
	fm10k_mbx_connect_reset(mbx);
	fm10k_fifo_drop_all(&mbx->tx);

	fm10k_write_reg(hw, mbx->mbmem_reg, 0);
}

 
static s32 fm10k_mbx_connect(struct fm10k_hw *hw, struct fm10k_mbx_info *mbx)
{
	 
	if (!mbx->rx.buffer)
		return FM10K_MBX_ERR_NO_SPACE;

	 
	if (mbx->state != FM10K_STATE_CLOSED)
		return FM10K_MBX_ERR_BUSY;

	 
	mbx->timeout = FM10K_MBX_INIT_TIMEOUT;

	 
	mbx->state = FM10K_STATE_CONNECT;

	fm10k_mbx_reset_work(mbx);

	 
	fm10k_mbx_create_fake_disconnect_hdr(mbx);
	fm10k_write_reg(hw, mbx->mbmem_reg ^ mbx->mbmem_len, mbx->mbx_hdr);

	 
	mbx->mbx_lock = FM10K_MBX_REQ_INTERRUPT | FM10K_MBX_ACK_INTERRUPT |
			FM10K_MBX_INTERRUPT_ENABLE;

	 
	fm10k_mbx_create_connect_hdr(mbx);
	fm10k_mbx_write(hw, mbx);

	return 0;
}

 
static s32 fm10k_mbx_validate_handlers(const struct fm10k_msg_data *msg_data)
{
	const struct fm10k_tlv_attr *attr;
	unsigned int id;

	 
	if (!msg_data)
		return 0;

	while (msg_data->id != FM10K_TLV_ERROR) {
		 
		if (!msg_data->func)
			return FM10K_ERR_PARAM;

		 
		attr = msg_data->attr;
		if (attr) {
			while (attr->id != FM10K_TLV_ERROR) {
				id = attr->id;
				attr++;
				 
				if (id >= attr->id)
					return FM10K_ERR_PARAM;
				 
				if (id >= FM10K_TLV_RESULTS_MAX)
					return FM10K_ERR_PARAM;
			}

			 
			if (attr->id != FM10K_TLV_ERROR)
				return FM10K_ERR_PARAM;
		}

		id = msg_data->id;
		msg_data++;
		 
		if (id >= msg_data->id)
			return FM10K_ERR_PARAM;
	}

	 
	if ((msg_data->id != FM10K_TLV_ERROR) || !msg_data->func)
		return FM10K_ERR_PARAM;

	return 0;
}

 
static s32 fm10k_mbx_register_handlers(struct fm10k_mbx_info *mbx,
				       const struct fm10k_msg_data *msg_data)
{
	 
	if (fm10k_mbx_validate_handlers(msg_data))
		return FM10K_ERR_PARAM;

	 
	mbx->msg_data = msg_data;

	return 0;
}

 
s32 fm10k_pfvf_mbx_init(struct fm10k_hw *hw, struct fm10k_mbx_info *mbx,
			const struct fm10k_msg_data *msg_data, u8 id)
{
	 
	switch (hw->mac.type) {
	case fm10k_mac_vf:
		mbx->mbx_reg = FM10K_VFMBX;
		mbx->mbmem_reg = FM10K_VFMBMEM(FM10K_VFMBMEM_VF_XOR);
		break;
	case fm10k_mac_pf:
		 
		if (id < 64) {
			mbx->mbx_reg = FM10K_MBX(id);
			mbx->mbmem_reg = FM10K_MBMEM_VF(id, 0);
			break;
		}
		fallthrough;
	default:
		return FM10K_MBX_ERR_NO_MBX;
	}

	 
	mbx->state = FM10K_STATE_CLOSED;

	 
	if (fm10k_mbx_validate_handlers(msg_data))
		return FM10K_ERR_PARAM;

	 
	mbx->msg_data = msg_data;

	 
	mbx->timeout = 0;
	mbx->udelay = FM10K_MBX_INIT_DELAY;

	 
	mbx->tail = 1;
	mbx->head = 1;

	 
	mbx->local = FM10K_MBX_CRC_SEED;
	mbx->remote = FM10K_MBX_CRC_SEED;

	 
	mbx->max_size = FM10K_MBX_MSG_MAX_SIZE;
	mbx->mbmem_len = FM10K_VFMBMEM_VF_XOR;

	 
	fm10k_fifo_init(&mbx->tx, mbx->buffer, FM10K_MBX_TX_BUFFER_SIZE);
	fm10k_fifo_init(&mbx->rx, &mbx->buffer[FM10K_MBX_TX_BUFFER_SIZE],
			FM10K_MBX_RX_BUFFER_SIZE);

	 
	mbx->ops.connect = fm10k_mbx_connect;
	mbx->ops.disconnect = fm10k_mbx_disconnect;
	mbx->ops.rx_ready = fm10k_mbx_rx_ready;
	mbx->ops.tx_ready = fm10k_mbx_tx_ready;
	mbx->ops.tx_complete = fm10k_mbx_tx_complete;
	mbx->ops.enqueue_tx = fm10k_mbx_enqueue_tx;
	mbx->ops.process = fm10k_mbx_process;
	mbx->ops.register_handlers = fm10k_mbx_register_handlers;

	return 0;
}

 
static void fm10k_sm_mbx_create_data_hdr(struct fm10k_mbx_info *mbx)
{
	if (mbx->tail_len)
		mbx->mbx_lock |= FM10K_MBX_REQ;

	mbx->mbx_hdr = FM10K_MSG_HDR_FIELD_SET(mbx->tail, SM_TAIL) |
		       FM10K_MSG_HDR_FIELD_SET(mbx->remote, SM_VER) |
		       FM10K_MSG_HDR_FIELD_SET(mbx->head, SM_HEAD);
}

 
static void fm10k_sm_mbx_create_connect_hdr(struct fm10k_mbx_info *mbx, u8 err)
{
	if (mbx->local)
		mbx->mbx_lock |= FM10K_MBX_REQ;

	mbx->mbx_hdr = FM10K_MSG_HDR_FIELD_SET(mbx->tail, SM_TAIL) |
		       FM10K_MSG_HDR_FIELD_SET(mbx->remote, SM_VER) |
		       FM10K_MSG_HDR_FIELD_SET(mbx->head, SM_HEAD) |
		       FM10K_MSG_HDR_FIELD_SET(err, SM_ERR);
}

 
static void fm10k_sm_mbx_connect_reset(struct fm10k_mbx_info *mbx)
{
	 
	fm10k_mbx_reset_work(mbx);

	 
	mbx->local = FM10K_SM_MBX_VERSION;
	mbx->remote = 0;

	 
	mbx->tail = 1;
	mbx->head = 1;

	 
	mbx->state = FM10K_STATE_CONNECT;
}

 
static s32 fm10k_sm_mbx_connect(struct fm10k_hw *hw, struct fm10k_mbx_info *mbx)
{
	 
	if (!mbx->rx.buffer)
		return FM10K_MBX_ERR_NO_SPACE;

	 
	if (mbx->state != FM10K_STATE_CLOSED)
		return FM10K_MBX_ERR_BUSY;

	 
	mbx->timeout = FM10K_MBX_INIT_TIMEOUT;

	 
	mbx->state = FM10K_STATE_CONNECT;
	mbx->max_size = FM10K_MBX_MSG_MAX_SIZE;

	 
	fm10k_sm_mbx_connect_reset(mbx);

	 
	mbx->mbx_lock = FM10K_MBX_REQ_INTERRUPT | FM10K_MBX_ACK_INTERRUPT |
			FM10K_MBX_INTERRUPT_ENABLE;

	 
	fm10k_sm_mbx_create_connect_hdr(mbx, 0);
	fm10k_mbx_write(hw, mbx);

	return 0;
}

 
static void fm10k_sm_mbx_disconnect(struct fm10k_hw *hw,
				    struct fm10k_mbx_info *mbx)
{
	int timeout = mbx->timeout ? FM10K_MBX_DISCONNECT_TIMEOUT : 0;

	 
	mbx->state = FM10K_STATE_DISCONNECT;

	 
	fm10k_write_reg(hw, mbx->mbx_reg, FM10K_MBX_REQ |
					  FM10K_MBX_INTERRUPT_DISABLE);
	do {
		udelay(FM10K_MBX_POLL_DELAY);
		mbx->ops.process(hw, mbx);
		timeout -= FM10K_MBX_POLL_DELAY;
	} while ((timeout > 0) && (mbx->state != FM10K_STATE_CLOSED));

	 
	mbx->state = FM10K_STATE_CLOSED;
	mbx->remote = 0;
	fm10k_mbx_reset_work(mbx);
	fm10k_fifo_drop_all(&mbx->tx);

	fm10k_write_reg(hw, mbx->mbmem_reg, 0);
}

 
static s32 fm10k_sm_mbx_validate_fifo_hdr(struct fm10k_mbx_info *mbx)
{
	const u32 *hdr = &mbx->mbx_hdr;
	u16 tail, head, ver;

	tail = FM10K_MSG_HDR_FIELD_GET(*hdr, SM_TAIL);
	ver = FM10K_MSG_HDR_FIELD_GET(*hdr, SM_VER);
	head = FM10K_MSG_HDR_FIELD_GET(*hdr, SM_HEAD);

	switch (ver) {
	case 0:
		break;
	case FM10K_SM_MBX_VERSION:
		if (!head || head > FM10K_SM_MBX_FIFO_LEN)
			return FM10K_MBX_ERR_HEAD;
		if (!tail || tail > FM10K_SM_MBX_FIFO_LEN)
			return FM10K_MBX_ERR_TAIL;
		if (mbx->tail < head)
			head += mbx->mbmem_len - 1;
		if (tail < mbx->head)
			tail += mbx->mbmem_len - 1;
		if (fm10k_mbx_index_len(mbx, head, mbx->tail) > mbx->tail_len)
			return FM10K_MBX_ERR_HEAD;
		if (fm10k_mbx_index_len(mbx, mbx->head, tail) < mbx->mbmem_len)
			break;
		return FM10K_MBX_ERR_TAIL;
	default:
		return FM10K_MBX_ERR_SRC;
	}

	return 0;
}

 
static void fm10k_sm_mbx_process_error(struct fm10k_mbx_info *mbx)
{
	const enum fm10k_mbx_state state = mbx->state;

	switch (state) {
	case FM10K_STATE_DISCONNECT:
		 
		mbx->remote = 0;
		break;
	case FM10K_STATE_OPEN:
		 
		fm10k_sm_mbx_connect_reset(mbx);
		break;
	case FM10K_STATE_CONNECT:
		 
		if (mbx->remote) {
			while (mbx->local > 1)
				mbx->local--;
			mbx->remote = 0;
		}
		break;
	default:
		break;
	}

	fm10k_sm_mbx_create_connect_hdr(mbx, 0);
}

 
static void fm10k_sm_mbx_create_error_msg(struct fm10k_mbx_info *mbx, s32 err)
{
	 
	switch (err) {
	case FM10K_MBX_ERR_TAIL:
	case FM10K_MBX_ERR_HEAD:
	case FM10K_MBX_ERR_SRC:
	case FM10K_MBX_ERR_SIZE:
	case FM10K_MBX_ERR_RSVD0:
		break;
	default:
		return;
	}

	 
	fm10k_sm_mbx_process_error(mbx);
	fm10k_sm_mbx_create_connect_hdr(mbx, 1);
}

 
static s32 fm10k_sm_mbx_receive(struct fm10k_hw *hw,
				struct fm10k_mbx_info *mbx,
				u16 tail)
{
	 
	u16 mbmem_len = mbx->mbmem_len - 1;
	s32 err;

	 
	if (tail < mbx->head)
		tail += mbmem_len;

	 
	err = fm10k_mbx_push_tail(hw, mbx, tail);
	if (err < 0)
		return err;

	 
	fm10k_mbx_dequeue_rx(hw, mbx);

	 
	mbx->head = fm10k_mbx_head_sub(mbx, mbx->pushed);
	mbx->pushed = 0;

	 
	if (mbx->head > mbmem_len)
		mbx->head -= mbmem_len;

	return err;
}

 
static void fm10k_sm_mbx_transmit(struct fm10k_hw *hw,
				  struct fm10k_mbx_info *mbx, u16 head)
{
	struct fm10k_mbx_fifo *fifo = &mbx->tx;
	 
	u16 mbmem_len = mbx->mbmem_len - 1;
	u16 tail_len, len = 0;

	 
	if (mbx->tail < head)
		head += mbmem_len;

	fm10k_mbx_pull_head(hw, mbx, head);

	 
	do {
		u32 *msg;

		msg = fifo->buffer + fm10k_fifo_head_offset(fifo, len);
		tail_len = len;
		len += FM10K_TLV_DWORD_LEN(*msg);
	} while ((len <= mbx->tail_len) && (len < mbmem_len));

	 
	if (mbx->tail_len > tail_len) {
		mbx->tail = fm10k_mbx_tail_sub(mbx, mbx->tail_len - tail_len);
		mbx->tail_len = tail_len;
	}

	 
	if (mbx->tail > mbmem_len)
		mbx->tail -= mbmem_len;
}

 
static void fm10k_sm_mbx_create_reply(struct fm10k_hw *hw,
				      struct fm10k_mbx_info *mbx, u16 head)
{
	switch (mbx->state) {
	case FM10K_STATE_OPEN:
	case FM10K_STATE_DISCONNECT:
		 
		fm10k_sm_mbx_transmit(hw, mbx, head);

		 
		if (mbx->tail_len || (mbx->state == FM10K_STATE_OPEN)) {
			fm10k_sm_mbx_create_data_hdr(mbx);
		} else {
			mbx->remote = 0;
			fm10k_sm_mbx_create_connect_hdr(mbx, 0);
		}
		break;
	case FM10K_STATE_CONNECT:
	case FM10K_STATE_CLOSED:
		fm10k_sm_mbx_create_connect_hdr(mbx, 0);
		break;
	default:
		break;
	}
}

 
static s32 fm10k_sm_mbx_process_reset(struct fm10k_hw *hw,
				      struct fm10k_mbx_info *mbx)
{
	s32 err = 0;
	const enum fm10k_mbx_state state = mbx->state;

	switch (state) {
	case FM10K_STATE_DISCONNECT:
		 
		mbx->state = FM10K_STATE_CLOSED;
		mbx->remote = 0;
		mbx->local = 0;
		break;
	case FM10K_STATE_OPEN:
		 
		fm10k_sm_mbx_connect_reset(mbx);
		err = FM10K_ERR_RESET_REQUESTED;
		break;
	case FM10K_STATE_CONNECT:
		 
		mbx->remote = mbx->local;
		break;
	default:
		break;
	}

	fm10k_sm_mbx_create_reply(hw, mbx, mbx->tail);

	return err;
}

 
static s32 fm10k_sm_mbx_process_version_1(struct fm10k_hw *hw,
					  struct fm10k_mbx_info *mbx)
{
	const u32 *hdr = &mbx->mbx_hdr;
	u16 head, tail;
	s32 len;

	 
	tail = FM10K_MSG_HDR_FIELD_GET(*hdr, SM_TAIL);
	head = FM10K_MSG_HDR_FIELD_GET(*hdr, SM_HEAD);

	 
	if (mbx->state == FM10K_STATE_CONNECT) {
		if (!mbx->remote)
			goto send_reply;
		if (mbx->remote != 1)
			return FM10K_MBX_ERR_SRC;

		mbx->state = FM10K_STATE_OPEN;
	}

	do {
		 
		len = fm10k_sm_mbx_receive(hw, mbx, tail);
		if (len < 0)
			return len;

		 
	} while (len);

send_reply:
	fm10k_sm_mbx_create_reply(hw, mbx, head);

	return 0;
}

 
static s32 fm10k_sm_mbx_process(struct fm10k_hw *hw,
				struct fm10k_mbx_info *mbx)
{
	s32 err;

	 
	if (mbx->state == FM10K_STATE_CLOSED)
		return 0;

	 
	err = fm10k_mbx_read(hw, mbx);
	if (err)
		return err;

	err = fm10k_sm_mbx_validate_fifo_hdr(mbx);
	if (err < 0)
		goto fifo_err;

	if (FM10K_MSG_HDR_FIELD_GET(mbx->mbx_hdr, SM_ERR)) {
		fm10k_sm_mbx_process_error(mbx);
		goto fifo_err;
	}

	switch (FM10K_MSG_HDR_FIELD_GET(mbx->mbx_hdr, SM_VER)) {
	case 0:
		err = fm10k_sm_mbx_process_reset(hw, mbx);
		break;
	case FM10K_SM_MBX_VERSION:
		err = fm10k_sm_mbx_process_version_1(hw, mbx);
		break;
	}

fifo_err:
	if (err < 0)
		fm10k_sm_mbx_create_error_msg(mbx, err);

	 
	fm10k_mbx_write(hw, mbx);

	return err;
}

 
s32 fm10k_sm_mbx_init(struct fm10k_hw __always_unused *hw,
		      struct fm10k_mbx_info *mbx,
		      const struct fm10k_msg_data *msg_data)
{
	mbx->mbx_reg = FM10K_GMBX;
	mbx->mbmem_reg = FM10K_MBMEM_PF(0);

	 
	mbx->state = FM10K_STATE_CLOSED;

	 
	if (fm10k_mbx_validate_handlers(msg_data))
		return FM10K_ERR_PARAM;

	 
	mbx->msg_data = msg_data;

	 
	mbx->timeout = 0;
	mbx->udelay = FM10K_MBX_INIT_DELAY;

	 
	mbx->max_size = FM10K_MBX_MSG_MAX_SIZE;
	mbx->mbmem_len = FM10K_MBMEM_PF_XOR;

	 
	fm10k_fifo_init(&mbx->tx, mbx->buffer, FM10K_MBX_TX_BUFFER_SIZE);
	fm10k_fifo_init(&mbx->rx, &mbx->buffer[FM10K_MBX_TX_BUFFER_SIZE],
			FM10K_MBX_RX_BUFFER_SIZE);

	 
	mbx->ops.connect = fm10k_sm_mbx_connect;
	mbx->ops.disconnect = fm10k_sm_mbx_disconnect;
	mbx->ops.rx_ready = fm10k_mbx_rx_ready;
	mbx->ops.tx_ready = fm10k_mbx_tx_ready;
	mbx->ops.tx_complete = fm10k_mbx_tx_complete;
	mbx->ops.enqueue_tx = fm10k_mbx_enqueue_tx;
	mbx->ops.process = fm10k_sm_mbx_process;
	mbx->ops.register_handlers = fm10k_mbx_register_handlers;

	return 0;
}
