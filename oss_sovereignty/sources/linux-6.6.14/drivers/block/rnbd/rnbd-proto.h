

#ifndef RNBD_PROTO_H
#define RNBD_PROTO_H

#include <linux/types.h>
#include <linux/blk-mq.h>
#include <linux/limits.h>
#include <linux/inet.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <rdma/ib.h>

#define RNBD_PROTO_VER_MAJOR 2
#define RNBD_PROTO_VER_MINOR 0


#define RTRS_PORT 1234


enum rnbd_msg_type {
	RNBD_MSG_SESS_INFO,
	RNBD_MSG_SESS_INFO_RSP,
	RNBD_MSG_OPEN,
	RNBD_MSG_OPEN_RSP,
	RNBD_MSG_IO,
	RNBD_MSG_CLOSE,
};


struct rnbd_msg_hdr {
	__le16		type;
	__le16		__padding;
};


enum rnbd_access_mode {
	RNBD_ACCESS_RO,
	RNBD_ACCESS_RW,
	RNBD_ACCESS_MIGRATION,
};

static const __maybe_unused struct {
	enum rnbd_access_mode mode;
	const char *str;
} rnbd_access_modes[] = {
	[RNBD_ACCESS_RO] = {RNBD_ACCESS_RO, "ro"},
	[RNBD_ACCESS_RW] = {RNBD_ACCESS_RW, "rw"},
	[RNBD_ACCESS_MIGRATION] = {RNBD_ACCESS_MIGRATION, "migration"},
};


struct rnbd_msg_sess_info {
	struct rnbd_msg_hdr hdr;
	u8		ver;
	u8		reserved[31];
};


struct rnbd_msg_sess_info_rsp {
	struct rnbd_msg_hdr hdr;
	u8		ver;
	u8		reserved[31];
};


struct rnbd_msg_open {
	struct rnbd_msg_hdr hdr;
	u8		access_mode;
	u8		resv1;
	s8		dev_name[NAME_MAX];
	u8		reserved[3];
};


struct rnbd_msg_close {
	struct rnbd_msg_hdr hdr;
	__le32		device_id;
};

enum rnbd_cache_policy {
	RNBD_FUA = 1 << 0,
	RNBD_WRITEBACK = 1 << 1,
};


struct rnbd_msg_open_rsp {
	struct rnbd_msg_hdr	hdr;
	__le32			device_id;
	__le64			nsectors;
	__le32			max_hw_sectors;
	__le32			max_write_same_sectors;
	__le32			max_discard_sectors;
	__le32			discard_granularity;
	__le32			discard_alignment;
	__le16			physical_block_size;
	__le16			logical_block_size;
	__le16			max_segments;
	__le16			secure_discard;
	u8			obsolete_rotational;
	u8			cache_policy;
	u8			reserved[10];
};


struct rnbd_msg_io {
	struct rnbd_msg_hdr hdr;
	__le32		device_id;
	__le64		sector;
	__le32		rw;
	__le32		bi_size;
	__le16		prio;
};

#define RNBD_OP_BITS  8
#define RNBD_OP_MASK  ((1 << RNBD_OP_BITS) - 1)


enum rnbd_io_flags {

	
	RNBD_OP_READ		= 0,
	RNBD_OP_WRITE		= 1,
	RNBD_OP_FLUSH		= 2,
	RNBD_OP_DISCARD	= 3,
	RNBD_OP_SECURE_ERASE	= 4,
	RNBD_OP_WRITE_SAME	= 5,

	
	RNBD_F_SYNC  = 1<<(RNBD_OP_BITS + 0),
	RNBD_F_FUA   = 1<<(RNBD_OP_BITS + 1),
};

static inline u32 rnbd_op(u32 flags)
{
	return flags & RNBD_OP_MASK;
}

static inline u32 rnbd_flags(u32 flags)
{
	return flags & ~RNBD_OP_MASK;
}

static inline blk_opf_t rnbd_to_bio_flags(u32 rnbd_opf)
{
	blk_opf_t bio_opf;

	switch (rnbd_op(rnbd_opf)) {
	case RNBD_OP_READ:
		bio_opf = REQ_OP_READ;
		break;
	case RNBD_OP_WRITE:
		bio_opf = REQ_OP_WRITE;
		break;
	case RNBD_OP_FLUSH:
		bio_opf = REQ_OP_WRITE | REQ_PREFLUSH;
		break;
	case RNBD_OP_DISCARD:
		bio_opf = REQ_OP_DISCARD;
		break;
	case RNBD_OP_SECURE_ERASE:
		bio_opf = REQ_OP_SECURE_ERASE;
		break;
	default:
		WARN(1, "Unknown RNBD type: %d (flags %d)\n",
		     rnbd_op(rnbd_opf), rnbd_opf);
		bio_opf = 0;
	}

	if (rnbd_opf & RNBD_F_SYNC)
		bio_opf |= REQ_SYNC;

	if (rnbd_opf & RNBD_F_FUA)
		bio_opf |= REQ_FUA;

	return bio_opf;
}

static inline u32 rq_to_rnbd_flags(struct request *rq)
{
	u32 rnbd_opf;

	switch (req_op(rq)) {
	case REQ_OP_READ:
		rnbd_opf = RNBD_OP_READ;
		break;
	case REQ_OP_WRITE:
		rnbd_opf = RNBD_OP_WRITE;
		break;
	case REQ_OP_DISCARD:
		rnbd_opf = RNBD_OP_DISCARD;
		break;
	case REQ_OP_SECURE_ERASE:
		rnbd_opf = RNBD_OP_SECURE_ERASE;
		break;
	case REQ_OP_FLUSH:
		rnbd_opf = RNBD_OP_FLUSH;
		break;
	default:
		WARN(1, "Unknown request type %d (flags %llu)\n",
		     (__force u32)req_op(rq),
		     (__force unsigned long long)rq->cmd_flags);
		rnbd_opf = 0;
	}

	if (op_is_sync(rq->cmd_flags))
		rnbd_opf |= RNBD_F_SYNC;

	if (op_is_flush(rq->cmd_flags))
		rnbd_opf |= RNBD_F_FUA;

	return rnbd_opf;
}

const char *rnbd_access_mode_str(enum rnbd_access_mode mode);

#endif 
