
 

#define pr_fmt(fmt) KBUILD_MODNAME ":%s, %d: " fmt, __func__, __LINE__

#include <linux/types.h>
#include <linux/printk.h>
#include <linux/ratelimit.h>

#include "vidtv_pes.h"
#include "vidtv_common.h"
#include "vidtv_encoder.h"
#include "vidtv_ts.h"

#define PRIVATE_STREAM_1_ID 0xbd  
#define PES_HEADER_MAX_STUFFING_BYTES 32
#define PES_TS_HEADER_MAX_STUFFING_BYTES 182

static u32 vidtv_pes_op_get_len(bool send_pts, bool send_dts)
{
	u32 len = 0;

	 
	len += sizeof(struct vidtv_pes_optional);

	 
	if (send_pts && send_dts)
		len += sizeof(struct vidtv_pes_optional_pts_dts);
	else if (send_pts)
		len += sizeof(struct vidtv_pes_optional_pts);

	return len;
}

#define SIZE_PCR (6 + sizeof(struct vidtv_mpeg_ts_adaption))

static u32 vidtv_pes_h_get_len(bool send_pts, bool send_dts)
{
	u32 len = 0;

	 

	len += sizeof(struct vidtv_mpeg_pes);
	len += vidtv_pes_op_get_len(send_pts, send_dts);

	return len;
}

static u32 vidtv_pes_write_header_stuffing(struct pes_header_write_args *args)
{
	 
	if (args->n_pes_h_s_bytes > PES_HEADER_MAX_STUFFING_BYTES) {
		pr_warn_ratelimited("More than %d stuffing bytes in PES packet header\n",
				    PES_HEADER_MAX_STUFFING_BYTES);
		args->n_pes_h_s_bytes = PES_HEADER_MAX_STUFFING_BYTES;
	}

	return vidtv_memset(args->dest_buf,
			    args->dest_offset,
			    args->dest_buf_sz,
			    TS_FILL_BYTE,
			    args->n_pes_h_s_bytes);
}

static u32 vidtv_pes_write_pts_dts(struct pes_header_write_args *args)
{
	u32 nbytes = 0;   

	struct vidtv_pes_optional_pts pts = {};
	struct vidtv_pes_optional_pts_dts pts_dts = {};
	void *op = NULL;
	size_t op_sz = 0;
	u64 mask1;
	u64 mask2;
	u64 mask3;

	if (!args->send_pts && args->send_dts)
		return 0;

	mask1 = GENMASK_ULL(32, 30);
	mask2 = GENMASK_ULL(29, 15);
	mask3 = GENMASK_ULL(14, 0);

	 
	if (args->send_pts && args->send_dts) {
		pts_dts.pts1 = (0x3 << 4) | ((args->pts & mask1) >> 29) | 0x1;
		pts_dts.pts2 = cpu_to_be16(((args->pts & mask2) >> 14) | 0x1);
		pts_dts.pts3 = cpu_to_be16(((args->pts & mask3) << 1) | 0x1);

		pts_dts.dts1 = (0x1 << 4) | ((args->dts & mask1) >> 29) | 0x1;
		pts_dts.dts2 = cpu_to_be16(((args->dts & mask2) >> 14) | 0x1);
		pts_dts.dts3 = cpu_to_be16(((args->dts & mask3) << 1) | 0x1);

		op = &pts_dts;
		op_sz = sizeof(pts_dts);

	} else if (args->send_pts) {
		pts.pts1 = (0x1 << 5) | ((args->pts & mask1) >> 29) | 0x1;
		pts.pts2 = cpu_to_be16(((args->pts & mask2) >> 14) | 0x1);
		pts.pts3 = cpu_to_be16(((args->pts & mask3) << 1) | 0x1);

		op = &pts;
		op_sz = sizeof(pts);
	}

	 
	nbytes += vidtv_memcpy(args->dest_buf,
			       args->dest_offset + nbytes,
			       args->dest_buf_sz,
			       op,
			       op_sz);

	return nbytes;
}

static u32 vidtv_pes_write_h(struct pes_header_write_args *args)
{
	u32 nbytes = 0;   

	struct vidtv_mpeg_pes pes_header          = {};
	struct vidtv_pes_optional pes_optional    = {};
	struct pes_header_write_args pts_dts_args;
	u32 stream_id = (args->encoder_id == S302M) ? PRIVATE_STREAM_1_ID : args->stream_id;
	u16 pes_opt_bitfield = 0x01 << 15;

	pes_header.bitfield = cpu_to_be32((PES_START_CODE_PREFIX << 8) | stream_id);

	pes_header.length = cpu_to_be16(vidtv_pes_op_get_len(args->send_pts,
							     args->send_dts) +
							     args->access_unit_len);

	if (args->send_pts && args->send_dts)
		pes_opt_bitfield |= (0x3 << 6);
	else if (args->send_pts)
		pes_opt_bitfield |= (0x1 << 7);

	pes_optional.bitfield = cpu_to_be16(pes_opt_bitfield);
	pes_optional.length = vidtv_pes_op_get_len(args->send_pts, args->send_dts) +
			      args->n_pes_h_s_bytes -
			      sizeof(struct vidtv_pes_optional);

	 
	nbytes += vidtv_memcpy(args->dest_buf,
			       args->dest_offset + nbytes,
			       args->dest_buf_sz,
			       &pes_header,
			       sizeof(pes_header));

	 
	nbytes += vidtv_memcpy(args->dest_buf,
			       args->dest_offset + nbytes,
			       args->dest_buf_sz,
			       &pes_optional,
			       sizeof(pes_optional));

	 
	pts_dts_args = *args;
	pts_dts_args.dest_offset = args->dest_offset + nbytes;
	nbytes += vidtv_pes_write_pts_dts(&pts_dts_args);

	 
	nbytes += vidtv_pes_write_header_stuffing(args);

	return nbytes;
}

static u32 vidtv_pes_write_pcr_bits(u8 *to, u32 to_offset, u64 pcr)
{
	 
	u64 div;
	u64 rem;
	u8 *buf = to + to_offset;
	u64 pcr_low;
	u64 pcr_high;

	div = div64_u64_rem(pcr, 300, &rem);

	pcr_low = rem;  
	pcr_high = div;  

	*buf++ = pcr_high >> 25;
	*buf++ = pcr_high >> 17;
	*buf++ = pcr_high >>  9;
	*buf++ = pcr_high >>  1;
	*buf++ = pcr_high <<  7 | pcr_low >> 8 | 0x7e;
	*buf++ = pcr_low;

	return 6;
}

static u32 vidtv_pes_write_stuffing(struct pes_ts_header_write_args *args,
				    u32 dest_offset, bool need_pcr,
				    u64 *last_pcr)
{
	struct vidtv_mpeg_ts_adaption ts_adap = {};
	int stuff_nbytes;
	u32 nbytes = 0;

	if (!args->n_stuffing_bytes)
		return 0;

	ts_adap.random_access = 1;

	 
	if (need_pcr) {
		ts_adap.PCR = 1;
		ts_adap.length = SIZE_PCR;
	} else {
		ts_adap.length = sizeof(ts_adap);
	}
	stuff_nbytes = args->n_stuffing_bytes - ts_adap.length;

	ts_adap.length -= sizeof(ts_adap.length);

	if (unlikely(stuff_nbytes < 0))
		stuff_nbytes = 0;

	ts_adap.length += stuff_nbytes;

	 
	nbytes += vidtv_memcpy(args->dest_buf,
			       dest_offset + nbytes,
			       args->dest_buf_sz,
			       &ts_adap,
			       sizeof(ts_adap));

	 
	if (need_pcr) {
		nbytes += vidtv_pes_write_pcr_bits(args->dest_buf,
						   dest_offset + nbytes,
						   args->pcr);

		*last_pcr = args->pcr;
	}

	 
	if (stuff_nbytes)
		nbytes += vidtv_memset(args->dest_buf,
				       dest_offset + nbytes,
				       args->dest_buf_sz,
				       TS_FILL_BYTE,
				       stuff_nbytes);

	 
	if (nbytes != args->n_stuffing_bytes)
		pr_warn_ratelimited("write size was %d, expected %d\n",
				    nbytes, args->n_stuffing_bytes);

	return nbytes;
}

static u32 vidtv_pes_write_ts_h(struct pes_ts_header_write_args args,
				bool need_pcr, u64 *last_pcr)
{
	 
	u32 nbytes = 0;
	struct vidtv_mpeg_ts ts_header = {};
	u16 payload_start = !args.wrote_pes_header;

	ts_header.sync_byte        = TS_SYNC_BYTE;
	ts_header.bitfield         = cpu_to_be16((payload_start << 14) | args.pid);
	ts_header.scrambling       = 0;
	ts_header.adaptation_field = (args.n_stuffing_bytes) > 0;
	ts_header.payload          = (args.n_stuffing_bytes) < PES_TS_HEADER_MAX_STUFFING_BYTES;

	ts_header.continuity_counter = *args.continuity_counter;

	vidtv_ts_inc_cc(args.continuity_counter);

	 
	nbytes += vidtv_memcpy(args.dest_buf,
			       args.dest_offset + nbytes,
			       args.dest_buf_sz,
			       &ts_header,
			       sizeof(ts_header));

	 
	nbytes += vidtv_pes_write_stuffing(&args, args.dest_offset + nbytes,
					   need_pcr, last_pcr);

	return nbytes;
}

u32 vidtv_pes_write_into(struct pes_write_args *args)
{
	u32 unaligned_bytes = (args->dest_offset % TS_PACKET_LEN);
	struct pes_ts_header_write_args ts_header_args = {
		.dest_buf		= args->dest_buf,
		.dest_buf_sz		= args->dest_buf_sz,
		.pid			= args->pid,
		.pcr			= args->pcr,
		.continuity_counter	= args->continuity_counter,
	};
	struct pes_header_write_args pes_header_args = {
		.dest_buf		= args->dest_buf,
		.dest_buf_sz		= args->dest_buf_sz,
		.encoder_id		= args->encoder_id,
		.send_pts		= args->send_pts,
		.pts			= args->pts,
		.send_dts		= args->send_dts,
		.dts			= args->dts,
		.stream_id		= args->stream_id,
		.n_pes_h_s_bytes	= args->n_pes_h_s_bytes,
		.access_unit_len	= args->access_unit_len,
	};
	u32 remaining_len = args->access_unit_len;
	bool wrote_pes_header = false;
	u64 last_pcr = args->pcr;
	bool need_pcr = true;
	u32 available_space;
	u32 payload_size;
	u32 stuff_bytes;
	u32 nbytes = 0;

	if (unaligned_bytes) {
		pr_warn_ratelimited("buffer is misaligned, while starting PES\n");

		 
		nbytes += vidtv_memset(args->dest_buf,
				       args->dest_offset + nbytes,
				       args->dest_buf_sz,
				       TS_FILL_BYTE,
				       TS_PACKET_LEN - unaligned_bytes);
	}

	while (remaining_len) {
		available_space = TS_PAYLOAD_LEN;
		 
		if (!wrote_pes_header)
			available_space -= vidtv_pes_h_get_len(args->send_pts,
							       args->send_dts);

		 
		available_space -= args->n_pes_h_s_bytes;

		 
		if (need_pcr) {
			available_space -= SIZE_PCR;
			stuff_bytes = SIZE_PCR;
		} else {
			stuff_bytes = 0;
		}

		 
		if (remaining_len >= available_space) {
			payload_size = available_space;
		} else {
			 
			payload_size = remaining_len;
			stuff_bytes += available_space - payload_size;

			 
			if (stuff_bytes > PES_TS_HEADER_MAX_STUFFING_BYTES) {
				u32 tmp = stuff_bytes - PES_TS_HEADER_MAX_STUFFING_BYTES;

				stuff_bytes = PES_TS_HEADER_MAX_STUFFING_BYTES;
				payload_size -= tmp;
			}
		}

		 
		ts_header_args.dest_offset = args->dest_offset + nbytes;
		ts_header_args.wrote_pes_header	= wrote_pes_header;
		ts_header_args.n_stuffing_bytes	= stuff_bytes;

		nbytes += vidtv_pes_write_ts_h(ts_header_args, need_pcr,
					       &last_pcr);

		need_pcr = false;

		if (!wrote_pes_header) {
			 
			pes_header_args.dest_offset = args->dest_offset +
						      nbytes;
			nbytes += vidtv_pes_write_h(&pes_header_args);
			wrote_pes_header = true;
		}

		 
		nbytes += vidtv_memcpy(args->dest_buf,
				       args->dest_offset + nbytes,
				       args->dest_buf_sz,
				       args->from,
				       payload_size);

		args->from += payload_size;

		remaining_len -= payload_size;
	}

	return nbytes;
}
