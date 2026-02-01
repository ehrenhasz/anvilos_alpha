 

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/random.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/iopoll.h>

#if IS_ENABLED(CONFIG_DRM_DEBUG_DP_MST_TOPOLOGY_REFS)
#include <linux/stacktrace.h>
#include <linux/sort.h>
#include <linux/timekeeping.h>
#include <linux/math64.h>
#endif

#include <drm/display/drm_dp_mst_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_edid.h>
#include <drm/drm_print.h>
#include <drm/drm_probe_helper.h>

#include "drm_dp_helper_internal.h"
#include "drm_dp_mst_topology_internal.h"

 
struct drm_dp_pending_up_req {
	struct drm_dp_sideband_msg_hdr hdr;
	struct drm_dp_sideband_msg_req_body msg;
	struct list_head next;
};

static bool dump_dp_payload_table(struct drm_dp_mst_topology_mgr *mgr,
				  char *buf);

static void drm_dp_mst_topology_put_port(struct drm_dp_mst_port *port);

static int drm_dp_dpcd_write_payload(struct drm_dp_mst_topology_mgr *mgr,
				     int id, u8 start_slot, u8 num_slots);

static int drm_dp_send_dpcd_read(struct drm_dp_mst_topology_mgr *mgr,
				 struct drm_dp_mst_port *port,
				 int offset, int size, u8 *bytes);
static int drm_dp_send_dpcd_write(struct drm_dp_mst_topology_mgr *mgr,
				  struct drm_dp_mst_port *port,
				  int offset, int size, u8 *bytes);

static int drm_dp_send_link_address(struct drm_dp_mst_topology_mgr *mgr,
				    struct drm_dp_mst_branch *mstb);

static void
drm_dp_send_clear_payload_id_table(struct drm_dp_mst_topology_mgr *mgr,
				   struct drm_dp_mst_branch *mstb);

static int drm_dp_send_enum_path_resources(struct drm_dp_mst_topology_mgr *mgr,
					   struct drm_dp_mst_branch *mstb,
					   struct drm_dp_mst_port *port);
static bool drm_dp_validate_guid(struct drm_dp_mst_topology_mgr *mgr,
				 u8 *guid);

static int drm_dp_mst_register_i2c_bus(struct drm_dp_mst_port *port);
static void drm_dp_mst_unregister_i2c_bus(struct drm_dp_mst_port *port);
static void drm_dp_mst_kick_tx(struct drm_dp_mst_topology_mgr *mgr);

static bool drm_dp_mst_port_downstream_of_branch(struct drm_dp_mst_port *port,
						 struct drm_dp_mst_branch *branch);

#define DBG_PREFIX "[dp_mst]"

#define DP_STR(x) [DP_ ## x] = #x

static const char *drm_dp_mst_req_type_str(u8 req_type)
{
	static const char * const req_type_str[] = {
		DP_STR(GET_MSG_TRANSACTION_VERSION),
		DP_STR(LINK_ADDRESS),
		DP_STR(CONNECTION_STATUS_NOTIFY),
		DP_STR(ENUM_PATH_RESOURCES),
		DP_STR(ALLOCATE_PAYLOAD),
		DP_STR(QUERY_PAYLOAD),
		DP_STR(RESOURCE_STATUS_NOTIFY),
		DP_STR(CLEAR_PAYLOAD_ID_TABLE),
		DP_STR(REMOTE_DPCD_READ),
		DP_STR(REMOTE_DPCD_WRITE),
		DP_STR(REMOTE_I2C_READ),
		DP_STR(REMOTE_I2C_WRITE),
		DP_STR(POWER_UP_PHY),
		DP_STR(POWER_DOWN_PHY),
		DP_STR(SINK_EVENT_NOTIFY),
		DP_STR(QUERY_STREAM_ENC_STATUS),
	};

	if (req_type >= ARRAY_SIZE(req_type_str) ||
	    !req_type_str[req_type])
		return "unknown";

	return req_type_str[req_type];
}

#undef DP_STR
#define DP_STR(x) [DP_NAK_ ## x] = #x

static const char *drm_dp_mst_nak_reason_str(u8 nak_reason)
{
	static const char * const nak_reason_str[] = {
		DP_STR(WRITE_FAILURE),
		DP_STR(INVALID_READ),
		DP_STR(CRC_FAILURE),
		DP_STR(BAD_PARAM),
		DP_STR(DEFER),
		DP_STR(LINK_FAILURE),
		DP_STR(NO_RESOURCES),
		DP_STR(DPCD_FAIL),
		DP_STR(I2C_NAK),
		DP_STR(ALLOCATE_FAIL),
	};

	if (nak_reason >= ARRAY_SIZE(nak_reason_str) ||
	    !nak_reason_str[nak_reason])
		return "unknown";

	return nak_reason_str[nak_reason];
}

#undef DP_STR
#define DP_STR(x) [DRM_DP_SIDEBAND_TX_ ## x] = #x

static const char *drm_dp_mst_sideband_tx_state_str(int state)
{
	static const char * const sideband_reason_str[] = {
		DP_STR(QUEUED),
		DP_STR(START_SEND),
		DP_STR(SENT),
		DP_STR(RX),
		DP_STR(TIMEOUT),
	};

	if (state >= ARRAY_SIZE(sideband_reason_str) ||
	    !sideband_reason_str[state])
		return "unknown";

	return sideband_reason_str[state];
}

static int
drm_dp_mst_rad_to_str(const u8 rad[8], u8 lct, char *out, size_t len)
{
	int i;
	u8 unpacked_rad[16];

	for (i = 0; i < lct; i++) {
		if (i % 2)
			unpacked_rad[i] = rad[i / 2] >> 4;
		else
			unpacked_rad[i] = rad[i / 2] & BIT_MASK(4);
	}

	 
	return snprintf(out, len, "%*phC", lct, unpacked_rad);
}

 
static u8 drm_dp_msg_header_crc4(const uint8_t *data, size_t num_nibbles)
{
	u8 bitmask = 0x80;
	u8 bitshift = 7;
	u8 array_index = 0;
	int number_of_bits = num_nibbles * 4;
	u8 remainder = 0;

	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		remainder |= (data[array_index] & bitmask) >> bitshift;
		bitmask >>= 1;
		bitshift--;
		if (bitmask == 0) {
			bitmask = 0x80;
			bitshift = 7;
			array_index++;
		}
		if ((remainder & 0x10) == 0x10)
			remainder ^= 0x13;
	}

	number_of_bits = 4;
	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		if ((remainder & 0x10) != 0)
			remainder ^= 0x13;
	}

	return remainder;
}

static u8 drm_dp_msg_data_crc4(const uint8_t *data, u8 number_of_bytes)
{
	u8 bitmask = 0x80;
	u8 bitshift = 7;
	u8 array_index = 0;
	int number_of_bits = number_of_bytes * 8;
	u16 remainder = 0;

	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		remainder |= (data[array_index] & bitmask) >> bitshift;
		bitmask >>= 1;
		bitshift--;
		if (bitmask == 0) {
			bitmask = 0x80;
			bitshift = 7;
			array_index++;
		}
		if ((remainder & 0x100) == 0x100)
			remainder ^= 0xd5;
	}

	number_of_bits = 8;
	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		if ((remainder & 0x100) != 0)
			remainder ^= 0xd5;
	}

	return remainder & 0xff;
}
static inline u8 drm_dp_calc_sb_hdr_size(struct drm_dp_sideband_msg_hdr *hdr)
{
	u8 size = 3;

	size += (hdr->lct / 2);
	return size;
}

static void drm_dp_encode_sideband_msg_hdr(struct drm_dp_sideband_msg_hdr *hdr,
					   u8 *buf, int *len)
{
	int idx = 0;
	int i;
	u8 crc4;

	buf[idx++] = ((hdr->lct & 0xf) << 4) | (hdr->lcr & 0xf);
	for (i = 0; i < (hdr->lct / 2); i++)
		buf[idx++] = hdr->rad[i];
	buf[idx++] = (hdr->broadcast << 7) | (hdr->path_msg << 6) |
		(hdr->msg_len & 0x3f);
	buf[idx++] = (hdr->somt << 7) | (hdr->eomt << 6) | (hdr->seqno << 4);

	crc4 = drm_dp_msg_header_crc4(buf, (idx * 2) - 1);
	buf[idx - 1] |= (crc4 & 0xf);

	*len = idx;
}

static bool drm_dp_decode_sideband_msg_hdr(const struct drm_dp_mst_topology_mgr *mgr,
					   struct drm_dp_sideband_msg_hdr *hdr,
					   u8 *buf, int buflen, u8 *hdrlen)
{
	u8 crc4;
	u8 len;
	int i;
	u8 idx;

	if (buf[0] == 0)
		return false;
	len = 3;
	len += ((buf[0] & 0xf0) >> 4) / 2;
	if (len > buflen)
		return false;
	crc4 = drm_dp_msg_header_crc4(buf, (len * 2) - 1);

	if ((crc4 & 0xf) != (buf[len - 1] & 0xf)) {
		drm_dbg_kms(mgr->dev, "crc4 mismatch 0x%x 0x%x\n", crc4, buf[len - 1]);
		return false;
	}

	hdr->lct = (buf[0] & 0xf0) >> 4;
	hdr->lcr = (buf[0] & 0xf);
	idx = 1;
	for (i = 0; i < (hdr->lct / 2); i++)
		hdr->rad[i] = buf[idx++];
	hdr->broadcast = (buf[idx] >> 7) & 0x1;
	hdr->path_msg = (buf[idx] >> 6) & 0x1;
	hdr->msg_len = buf[idx] & 0x3f;
	idx++;
	hdr->somt = (buf[idx] >> 7) & 0x1;
	hdr->eomt = (buf[idx] >> 6) & 0x1;
	hdr->seqno = (buf[idx] >> 4) & 0x1;
	idx++;
	*hdrlen = idx;
	return true;
}

void
drm_dp_encode_sideband_req(const struct drm_dp_sideband_msg_req_body *req,
			   struct drm_dp_sideband_msg_tx *raw)
{
	int idx = 0;
	int i;
	u8 *buf = raw->msg;

	buf[idx++] = req->req_type & 0x7f;

	switch (req->req_type) {
	case DP_ENUM_PATH_RESOURCES:
	case DP_POWER_DOWN_PHY:
	case DP_POWER_UP_PHY:
		buf[idx] = (req->u.port_num.port_number & 0xf) << 4;
		idx++;
		break;
	case DP_ALLOCATE_PAYLOAD:
		buf[idx] = (req->u.allocate_payload.port_number & 0xf) << 4 |
			(req->u.allocate_payload.number_sdp_streams & 0xf);
		idx++;
		buf[idx] = (req->u.allocate_payload.vcpi & 0x7f);
		idx++;
		buf[idx] = (req->u.allocate_payload.pbn >> 8);
		idx++;
		buf[idx] = (req->u.allocate_payload.pbn & 0xff);
		idx++;
		for (i = 0; i < req->u.allocate_payload.number_sdp_streams / 2; i++) {
			buf[idx] = ((req->u.allocate_payload.sdp_stream_sink[i * 2] & 0xf) << 4) |
				(req->u.allocate_payload.sdp_stream_sink[i * 2 + 1] & 0xf);
			idx++;
		}
		if (req->u.allocate_payload.number_sdp_streams & 1) {
			i = req->u.allocate_payload.number_sdp_streams - 1;
			buf[idx] = (req->u.allocate_payload.sdp_stream_sink[i] & 0xf) << 4;
			idx++;
		}
		break;
	case DP_QUERY_PAYLOAD:
		buf[idx] = (req->u.query_payload.port_number & 0xf) << 4;
		idx++;
		buf[idx] = (req->u.query_payload.vcpi & 0x7f);
		idx++;
		break;
	case DP_REMOTE_DPCD_READ:
		buf[idx] = (req->u.dpcd_read.port_number & 0xf) << 4;
		buf[idx] |= ((req->u.dpcd_read.dpcd_address & 0xf0000) >> 16) & 0xf;
		idx++;
		buf[idx] = (req->u.dpcd_read.dpcd_address & 0xff00) >> 8;
		idx++;
		buf[idx] = (req->u.dpcd_read.dpcd_address & 0xff);
		idx++;
		buf[idx] = (req->u.dpcd_read.num_bytes);
		idx++;
		break;

	case DP_REMOTE_DPCD_WRITE:
		buf[idx] = (req->u.dpcd_write.port_number & 0xf) << 4;
		buf[idx] |= ((req->u.dpcd_write.dpcd_address & 0xf0000) >> 16) & 0xf;
		idx++;
		buf[idx] = (req->u.dpcd_write.dpcd_address & 0xff00) >> 8;
		idx++;
		buf[idx] = (req->u.dpcd_write.dpcd_address & 0xff);
		idx++;
		buf[idx] = (req->u.dpcd_write.num_bytes);
		idx++;
		memcpy(&buf[idx], req->u.dpcd_write.bytes, req->u.dpcd_write.num_bytes);
		idx += req->u.dpcd_write.num_bytes;
		break;
	case DP_REMOTE_I2C_READ:
		buf[idx] = (req->u.i2c_read.port_number & 0xf) << 4;
		buf[idx] |= (req->u.i2c_read.num_transactions & 0x3);
		idx++;
		for (i = 0; i < (req->u.i2c_read.num_transactions & 0x3); i++) {
			buf[idx] = req->u.i2c_read.transactions[i].i2c_dev_id & 0x7f;
			idx++;
			buf[idx] = req->u.i2c_read.transactions[i].num_bytes;
			idx++;
			memcpy(&buf[idx], req->u.i2c_read.transactions[i].bytes, req->u.i2c_read.transactions[i].num_bytes);
			idx += req->u.i2c_read.transactions[i].num_bytes;

			buf[idx] = (req->u.i2c_read.transactions[i].no_stop_bit & 0x1) << 4;
			buf[idx] |= (req->u.i2c_read.transactions[i].i2c_transaction_delay & 0xf);
			idx++;
		}
		buf[idx] = (req->u.i2c_read.read_i2c_device_id) & 0x7f;
		idx++;
		buf[idx] = (req->u.i2c_read.num_bytes_read);
		idx++;
		break;

	case DP_REMOTE_I2C_WRITE:
		buf[idx] = (req->u.i2c_write.port_number & 0xf) << 4;
		idx++;
		buf[idx] = (req->u.i2c_write.write_i2c_device_id) & 0x7f;
		idx++;
		buf[idx] = (req->u.i2c_write.num_bytes);
		idx++;
		memcpy(&buf[idx], req->u.i2c_write.bytes, req->u.i2c_write.num_bytes);
		idx += req->u.i2c_write.num_bytes;
		break;
	case DP_QUERY_STREAM_ENC_STATUS: {
		const struct drm_dp_query_stream_enc_status *msg;

		msg = &req->u.enc_status;
		buf[idx] = msg->stream_id;
		idx++;
		memcpy(&buf[idx], msg->client_id, sizeof(msg->client_id));
		idx += sizeof(msg->client_id);
		buf[idx] = 0;
		buf[idx] |= FIELD_PREP(GENMASK(1, 0), msg->stream_event);
		buf[idx] |= msg->valid_stream_event ? BIT(2) : 0;
		buf[idx] |= FIELD_PREP(GENMASK(4, 3), msg->stream_behavior);
		buf[idx] |= msg->valid_stream_behavior ? BIT(5) : 0;
		idx++;
		}
		break;
	}
	raw->cur_len = idx;
}
EXPORT_SYMBOL_FOR_TESTS_ONLY(drm_dp_encode_sideband_req);

 
int
drm_dp_decode_sideband_req(const struct drm_dp_sideband_msg_tx *raw,
			   struct drm_dp_sideband_msg_req_body *req)
{
	const u8 *buf = raw->msg;
	int i, idx = 0;

	req->req_type = buf[idx++] & 0x7f;
	switch (req->req_type) {
	case DP_ENUM_PATH_RESOURCES:
	case DP_POWER_DOWN_PHY:
	case DP_POWER_UP_PHY:
		req->u.port_num.port_number = (buf[idx] >> 4) & 0xf;
		break;
	case DP_ALLOCATE_PAYLOAD:
		{
			struct drm_dp_allocate_payload *a =
				&req->u.allocate_payload;

			a->number_sdp_streams = buf[idx] & 0xf;
			a->port_number = (buf[idx] >> 4) & 0xf;

			WARN_ON(buf[++idx] & 0x80);
			a->vcpi = buf[idx] & 0x7f;

			a->pbn = buf[++idx] << 8;
			a->pbn |= buf[++idx];

			idx++;
			for (i = 0; i < a->number_sdp_streams; i++) {
				a->sdp_stream_sink[i] =
					(buf[idx + (i / 2)] >> ((i % 2) ? 0 : 4)) & 0xf;
			}
		}
		break;
	case DP_QUERY_PAYLOAD:
		req->u.query_payload.port_number = (buf[idx] >> 4) & 0xf;
		WARN_ON(buf[++idx] & 0x80);
		req->u.query_payload.vcpi = buf[idx] & 0x7f;
		break;
	case DP_REMOTE_DPCD_READ:
		{
			struct drm_dp_remote_dpcd_read *r = &req->u.dpcd_read;

			r->port_number = (buf[idx] >> 4) & 0xf;

			r->dpcd_address = (buf[idx] << 16) & 0xf0000;
			r->dpcd_address |= (buf[++idx] << 8) & 0xff00;
			r->dpcd_address |= buf[++idx] & 0xff;

			r->num_bytes = buf[++idx];
		}
		break;
	case DP_REMOTE_DPCD_WRITE:
		{
			struct drm_dp_remote_dpcd_write *w =
				&req->u.dpcd_write;

			w->port_number = (buf[idx] >> 4) & 0xf;

			w->dpcd_address = (buf[idx] << 16) & 0xf0000;
			w->dpcd_address |= (buf[++idx] << 8) & 0xff00;
			w->dpcd_address |= buf[++idx] & 0xff;

			w->num_bytes = buf[++idx];

			w->bytes = kmemdup(&buf[++idx], w->num_bytes,
					   GFP_KERNEL);
			if (!w->bytes)
				return -ENOMEM;
		}
		break;
	case DP_REMOTE_I2C_READ:
		{
			struct drm_dp_remote_i2c_read *r = &req->u.i2c_read;
			struct drm_dp_remote_i2c_read_tx *tx;
			bool failed = false;

			r->num_transactions = buf[idx] & 0x3;
			r->port_number = (buf[idx] >> 4) & 0xf;
			for (i = 0; i < r->num_transactions; i++) {
				tx = &r->transactions[i];

				tx->i2c_dev_id = buf[++idx] & 0x7f;
				tx->num_bytes = buf[++idx];
				tx->bytes = kmemdup(&buf[++idx],
						    tx->num_bytes,
						    GFP_KERNEL);
				if (!tx->bytes) {
					failed = true;
					break;
				}
				idx += tx->num_bytes;
				tx->no_stop_bit = (buf[idx] >> 5) & 0x1;
				tx->i2c_transaction_delay = buf[idx] & 0xf;
			}

			if (failed) {
				for (i = 0; i < r->num_transactions; i++) {
					tx = &r->transactions[i];
					kfree(tx->bytes);
				}
				return -ENOMEM;
			}

			r->read_i2c_device_id = buf[++idx] & 0x7f;
			r->num_bytes_read = buf[++idx];
		}
		break;
	case DP_REMOTE_I2C_WRITE:
		{
			struct drm_dp_remote_i2c_write *w = &req->u.i2c_write;

			w->port_number = (buf[idx] >> 4) & 0xf;
			w->write_i2c_device_id = buf[++idx] & 0x7f;
			w->num_bytes = buf[++idx];
			w->bytes = kmemdup(&buf[++idx], w->num_bytes,
					   GFP_KERNEL);
			if (!w->bytes)
				return -ENOMEM;
		}
		break;
	case DP_QUERY_STREAM_ENC_STATUS:
		req->u.enc_status.stream_id = buf[idx++];
		for (i = 0; i < sizeof(req->u.enc_status.client_id); i++)
			req->u.enc_status.client_id[i] = buf[idx++];

		req->u.enc_status.stream_event = FIELD_GET(GENMASK(1, 0),
							   buf[idx]);
		req->u.enc_status.valid_stream_event = FIELD_GET(BIT(2),
								 buf[idx]);
		req->u.enc_status.stream_behavior = FIELD_GET(GENMASK(4, 3),
							      buf[idx]);
		req->u.enc_status.valid_stream_behavior = FIELD_GET(BIT(5),
								    buf[idx]);
		break;
	}

	return 0;
}
EXPORT_SYMBOL_FOR_TESTS_ONLY(drm_dp_decode_sideband_req);

void
drm_dp_dump_sideband_msg_req_body(const struct drm_dp_sideband_msg_req_body *req,
				  int indent, struct drm_printer *printer)
{
	int i;

#define P(f, ...) drm_printf_indent(printer, indent, f, ##__VA_ARGS__)
	if (req->req_type == DP_LINK_ADDRESS) {
		 
		P("type=%s\n", drm_dp_mst_req_type_str(req->req_type));
		return;
	}

	P("type=%s contents:\n", drm_dp_mst_req_type_str(req->req_type));
	indent++;

	switch (req->req_type) {
	case DP_ENUM_PATH_RESOURCES:
	case DP_POWER_DOWN_PHY:
	case DP_POWER_UP_PHY:
		P("port=%d\n", req->u.port_num.port_number);
		break;
	case DP_ALLOCATE_PAYLOAD:
		P("port=%d vcpi=%d pbn=%d sdp_streams=%d %*ph\n",
		  req->u.allocate_payload.port_number,
		  req->u.allocate_payload.vcpi, req->u.allocate_payload.pbn,
		  req->u.allocate_payload.number_sdp_streams,
		  req->u.allocate_payload.number_sdp_streams,
		  req->u.allocate_payload.sdp_stream_sink);
		break;
	case DP_QUERY_PAYLOAD:
		P("port=%d vcpi=%d\n",
		  req->u.query_payload.port_number,
		  req->u.query_payload.vcpi);
		break;
	case DP_REMOTE_DPCD_READ:
		P("port=%d dpcd_addr=%05x len=%d\n",
		  req->u.dpcd_read.port_number, req->u.dpcd_read.dpcd_address,
		  req->u.dpcd_read.num_bytes);
		break;
	case DP_REMOTE_DPCD_WRITE:
		P("port=%d addr=%05x len=%d: %*ph\n",
		  req->u.dpcd_write.port_number,
		  req->u.dpcd_write.dpcd_address,
		  req->u.dpcd_write.num_bytes, req->u.dpcd_write.num_bytes,
		  req->u.dpcd_write.bytes);
		break;
	case DP_REMOTE_I2C_READ:
		P("port=%d num_tx=%d id=%d size=%d:\n",
		  req->u.i2c_read.port_number,
		  req->u.i2c_read.num_transactions,
		  req->u.i2c_read.read_i2c_device_id,
		  req->u.i2c_read.num_bytes_read);

		indent++;
		for (i = 0; i < req->u.i2c_read.num_transactions; i++) {
			const struct drm_dp_remote_i2c_read_tx *rtx =
				&req->u.i2c_read.transactions[i];

			P("%d: id=%03d size=%03d no_stop_bit=%d tx_delay=%03d: %*ph\n",
			  i, rtx->i2c_dev_id, rtx->num_bytes,
			  rtx->no_stop_bit, rtx->i2c_transaction_delay,
			  rtx->num_bytes, rtx->bytes);
		}
		break;
	case DP_REMOTE_I2C_WRITE:
		P("port=%d id=%d size=%d: %*ph\n",
		  req->u.i2c_write.port_number,
		  req->u.i2c_write.write_i2c_device_id,
		  req->u.i2c_write.num_bytes, req->u.i2c_write.num_bytes,
		  req->u.i2c_write.bytes);
		break;
	case DP_QUERY_STREAM_ENC_STATUS:
		P("stream_id=%u client_id=%*ph stream_event=%x "
		  "valid_event=%d stream_behavior=%x valid_behavior=%d",
		  req->u.enc_status.stream_id,
		  (int)ARRAY_SIZE(req->u.enc_status.client_id),
		  req->u.enc_status.client_id, req->u.enc_status.stream_event,
		  req->u.enc_status.valid_stream_event,
		  req->u.enc_status.stream_behavior,
		  req->u.enc_status.valid_stream_behavior);
		break;
	default:
		P("???\n");
		break;
	}
#undef P
}
EXPORT_SYMBOL_FOR_TESTS_ONLY(drm_dp_dump_sideband_msg_req_body);

static inline void
drm_dp_mst_dump_sideband_msg_tx(struct drm_printer *p,
				const struct drm_dp_sideband_msg_tx *txmsg)
{
	struct drm_dp_sideband_msg_req_body req;
	char buf[64];
	int ret;
	int i;

	drm_dp_mst_rad_to_str(txmsg->dst->rad, txmsg->dst->lct, buf,
			      sizeof(buf));
	drm_printf(p, "txmsg cur_offset=%x cur_len=%x seqno=%x state=%s path_msg=%d dst=%s\n",
		   txmsg->cur_offset, txmsg->cur_len, txmsg->seqno,
		   drm_dp_mst_sideband_tx_state_str(txmsg->state),
		   txmsg->path_msg, buf);

	ret = drm_dp_decode_sideband_req(txmsg, &req);
	if (ret) {
		drm_printf(p, "<failed to decode sideband req: %d>\n", ret);
		return;
	}
	drm_dp_dump_sideband_msg_req_body(&req, 1, p);

	switch (req.req_type) {
	case DP_REMOTE_DPCD_WRITE:
		kfree(req.u.dpcd_write.bytes);
		break;
	case DP_REMOTE_I2C_READ:
		for (i = 0; i < req.u.i2c_read.num_transactions; i++)
			kfree(req.u.i2c_read.transactions[i].bytes);
		break;
	case DP_REMOTE_I2C_WRITE:
		kfree(req.u.i2c_write.bytes);
		break;
	}
}

static void drm_dp_crc_sideband_chunk_req(u8 *msg, u8 len)
{
	u8 crc4;

	crc4 = drm_dp_msg_data_crc4(msg, len);
	msg[len] = crc4;
}

static void drm_dp_encode_sideband_reply(struct drm_dp_sideband_msg_reply_body *rep,
					 struct drm_dp_sideband_msg_tx *raw)
{
	int idx = 0;
	u8 *buf = raw->msg;

	buf[idx++] = (rep->reply_type & 0x1) << 7 | (rep->req_type & 0x7f);

	raw->cur_len = idx;
}

static int drm_dp_sideband_msg_set_header(struct drm_dp_sideband_msg_rx *msg,
					  struct drm_dp_sideband_msg_hdr *hdr,
					  u8 hdrlen)
{
	 
	if (!hdr->somt && !msg->have_somt)
		return false;

	 
	msg->curchunk_idx = 0;
	msg->curchunk_len = hdr->msg_len;
	msg->curchunk_hdrlen = hdrlen;

	 
	if (hdr->somt && msg->have_somt)
		return false;

	if (hdr->somt) {
		memcpy(&msg->initial_hdr, hdr,
		       sizeof(struct drm_dp_sideband_msg_hdr));
		msg->have_somt = true;
	}
	if (hdr->eomt)
		msg->have_eomt = true;

	return true;
}

 
static bool drm_dp_sideband_append_payload(struct drm_dp_sideband_msg_rx *msg,
					   u8 *replybuf, u8 replybuflen)
{
	u8 crc4;

	memcpy(&msg->chunk[msg->curchunk_idx], replybuf, replybuflen);
	msg->curchunk_idx += replybuflen;

	if (msg->curchunk_idx >= msg->curchunk_len) {
		 
		crc4 = drm_dp_msg_data_crc4(msg->chunk, msg->curchunk_len - 1);
		if (crc4 != msg->chunk[msg->curchunk_len - 1])
			print_hex_dump(KERN_DEBUG, "wrong crc",
				       DUMP_PREFIX_NONE, 16, 1,
				       msg->chunk,  msg->curchunk_len, false);
		 
		memcpy(&msg->msg[msg->curlen], msg->chunk, msg->curchunk_len - 1);
		msg->curlen += msg->curchunk_len - 1;
	}
	return true;
}

static bool drm_dp_sideband_parse_link_address(const struct drm_dp_mst_topology_mgr *mgr,
					       struct drm_dp_sideband_msg_rx *raw,
					       struct drm_dp_sideband_msg_reply_body *repmsg)
{
	int idx = 1;
	int i;

	memcpy(repmsg->u.link_addr.guid, &raw->msg[idx], 16);
	idx += 16;
	repmsg->u.link_addr.nports = raw->msg[idx] & 0xf;
	idx++;
	if (idx > raw->curlen)
		goto fail_len;
	for (i = 0; i < repmsg->u.link_addr.nports; i++) {
		if (raw->msg[idx] & 0x80)
			repmsg->u.link_addr.ports[i].input_port = 1;

		repmsg->u.link_addr.ports[i].peer_device_type = (raw->msg[idx] >> 4) & 0x7;
		repmsg->u.link_addr.ports[i].port_number = (raw->msg[idx] & 0xf);

		idx++;
		if (idx > raw->curlen)
			goto fail_len;
		repmsg->u.link_addr.ports[i].mcs = (raw->msg[idx] >> 7) & 0x1;
		repmsg->u.link_addr.ports[i].ddps = (raw->msg[idx] >> 6) & 0x1;
		if (repmsg->u.link_addr.ports[i].input_port == 0)
			repmsg->u.link_addr.ports[i].legacy_device_plug_status = (raw->msg[idx] >> 5) & 0x1;
		idx++;
		if (idx > raw->curlen)
			goto fail_len;
		if (repmsg->u.link_addr.ports[i].input_port == 0) {
			repmsg->u.link_addr.ports[i].dpcd_revision = (raw->msg[idx]);
			idx++;
			if (idx > raw->curlen)
				goto fail_len;
			memcpy(repmsg->u.link_addr.ports[i].peer_guid, &raw->msg[idx], 16);
			idx += 16;
			if (idx > raw->curlen)
				goto fail_len;
			repmsg->u.link_addr.ports[i].num_sdp_streams = (raw->msg[idx] >> 4) & 0xf;
			repmsg->u.link_addr.ports[i].num_sdp_stream_sinks = (raw->msg[idx] & 0xf);
			idx++;

		}
		if (idx > raw->curlen)
			goto fail_len;
	}

	return true;
fail_len:
	DRM_DEBUG_KMS("link address reply parse length fail %d %d\n", idx, raw->curlen);
	return false;
}

static bool drm_dp_sideband_parse_remote_dpcd_read(struct drm_dp_sideband_msg_rx *raw,
						   struct drm_dp_sideband_msg_reply_body *repmsg)
{
	int idx = 1;

	repmsg->u.remote_dpcd_read_ack.port_number = raw->msg[idx] & 0xf;
	idx++;
	if (idx > raw->curlen)
		goto fail_len;
	repmsg->u.remote_dpcd_read_ack.num_bytes = raw->msg[idx];
	idx++;
	if (idx > raw->curlen)
		goto fail_len;

	memcpy(repmsg->u.remote_dpcd_read_ack.bytes, &raw->msg[idx], repmsg->u.remote_dpcd_read_ack.num_bytes);
	return true;
fail_len:
	DRM_DEBUG_KMS("link address reply parse length fail %d %d\n", idx, raw->curlen);
	return false;
}

static bool drm_dp_sideband_parse_remote_dpcd_write(struct drm_dp_sideband_msg_rx *raw,
						      struct drm_dp_sideband_msg_reply_body *repmsg)
{
	int idx = 1;

	repmsg->u.remote_dpcd_write_ack.port_number = raw->msg[idx] & 0xf;
	idx++;
	if (idx > raw->curlen)
		goto fail_len;
	return true;
fail_len:
	DRM_DEBUG_KMS("parse length fail %d %d\n", idx, raw->curlen);
	return false;
}

static bool drm_dp_sideband_parse_remote_i2c_read_ack(struct drm_dp_sideband_msg_rx *raw,
						      struct drm_dp_sideband_msg_reply_body *repmsg)
{
	int idx = 1;

	repmsg->u.remote_i2c_read_ack.port_number = (raw->msg[idx] & 0xf);
	idx++;
	if (idx > raw->curlen)
		goto fail_len;
	repmsg->u.remote_i2c_read_ack.num_bytes = raw->msg[idx];
	idx++;
	 
	memcpy(repmsg->u.remote_i2c_read_ack.bytes, &raw->msg[idx], repmsg->u.remote_i2c_read_ack.num_bytes);
	return true;
fail_len:
	DRM_DEBUG_KMS("remote i2c reply parse length fail %d %d\n", idx, raw->curlen);
	return false;
}

static bool drm_dp_sideband_parse_enum_path_resources_ack(struct drm_dp_sideband_msg_rx *raw,
							  struct drm_dp_sideband_msg_reply_body *repmsg)
{
	int idx = 1;

	repmsg->u.path_resources.port_number = (raw->msg[idx] >> 4) & 0xf;
	repmsg->u.path_resources.fec_capable = raw->msg[idx] & 0x1;
	idx++;
	if (idx > raw->curlen)
		goto fail_len;
	repmsg->u.path_resources.full_payload_bw_number = (raw->msg[idx] << 8) | (raw->msg[idx+1]);
	idx += 2;
	if (idx > raw->curlen)
		goto fail_len;
	repmsg->u.path_resources.avail_payload_bw_number = (raw->msg[idx] << 8) | (raw->msg[idx+1]);
	idx += 2;
	if (idx > raw->curlen)
		goto fail_len;
	return true;
fail_len:
	DRM_DEBUG_KMS("enum resource parse length fail %d %d\n", idx, raw->curlen);
	return false;
}

static bool drm_dp_sideband_parse_allocate_payload_ack(struct drm_dp_sideband_msg_rx *raw,
							  struct drm_dp_sideband_msg_reply_body *repmsg)
{
	int idx = 1;

	repmsg->u.allocate_payload.port_number = (raw->msg[idx] >> 4) & 0xf;
	idx++;
	if (idx > raw->curlen)
		goto fail_len;
	repmsg->u.allocate_payload.vcpi = raw->msg[idx];
	idx++;
	if (idx > raw->curlen)
		goto fail_len;
	repmsg->u.allocate_payload.allocated_pbn = (raw->msg[idx] << 8) | (raw->msg[idx+1]);
	idx += 2;
	if (idx > raw->curlen)
		goto fail_len;
	return true;
fail_len:
	DRM_DEBUG_KMS("allocate payload parse length fail %d %d\n", idx, raw->curlen);
	return false;
}

static bool drm_dp_sideband_parse_query_payload_ack(struct drm_dp_sideband_msg_rx *raw,
						    struct drm_dp_sideband_msg_reply_body *repmsg)
{
	int idx = 1;

	repmsg->u.query_payload.port_number = (raw->msg[idx] >> 4) & 0xf;
	idx++;
	if (idx > raw->curlen)
		goto fail_len;
	repmsg->u.query_payload.allocated_pbn = (raw->msg[idx] << 8) | (raw->msg[idx + 1]);
	idx += 2;
	if (idx > raw->curlen)
		goto fail_len;
	return true;
fail_len:
	DRM_DEBUG_KMS("query payload parse length fail %d %d\n", idx, raw->curlen);
	return false;
}

static bool drm_dp_sideband_parse_power_updown_phy_ack(struct drm_dp_sideband_msg_rx *raw,
						       struct drm_dp_sideband_msg_reply_body *repmsg)
{
	int idx = 1;

	repmsg->u.port_number.port_number = (raw->msg[idx] >> 4) & 0xf;
	idx++;
	if (idx > raw->curlen) {
		DRM_DEBUG_KMS("power up/down phy parse length fail %d %d\n",
			      idx, raw->curlen);
		return false;
	}
	return true;
}

static bool
drm_dp_sideband_parse_query_stream_enc_status(
				struct drm_dp_sideband_msg_rx *raw,
				struct drm_dp_sideband_msg_reply_body *repmsg)
{
	struct drm_dp_query_stream_enc_status_ack_reply *reply;

	reply = &repmsg->u.enc_status;

	reply->stream_id = raw->msg[3];

	reply->reply_signed = raw->msg[2] & BIT(0);

	 
	reply->hdcp_1x_device_present = raw->msg[2] & BIT(4);
	reply->hdcp_2x_device_present = raw->msg[2] & BIT(3);

	reply->query_capable_device_present = raw->msg[2] & BIT(5);
	reply->legacy_device_present = raw->msg[2] & BIT(6);
	reply->unauthorizable_device_present = raw->msg[2] & BIT(7);

	reply->auth_completed = !!(raw->msg[1] & BIT(3));
	reply->encryption_enabled = !!(raw->msg[1] & BIT(4));
	reply->repeater_present = !!(raw->msg[1] & BIT(5));
	reply->state = (raw->msg[1] & GENMASK(7, 6)) >> 6;

	return true;
}

static bool drm_dp_sideband_parse_reply(const struct drm_dp_mst_topology_mgr *mgr,
					struct drm_dp_sideband_msg_rx *raw,
					struct drm_dp_sideband_msg_reply_body *msg)
{
	memset(msg, 0, sizeof(*msg));
	msg->reply_type = (raw->msg[0] & 0x80) >> 7;
	msg->req_type = (raw->msg[0] & 0x7f);

	if (msg->reply_type == DP_SIDEBAND_REPLY_NAK) {
		memcpy(msg->u.nak.guid, &raw->msg[1], 16);
		msg->u.nak.reason = raw->msg[17];
		msg->u.nak.nak_data = raw->msg[18];
		return false;
	}

	switch (msg->req_type) {
	case DP_LINK_ADDRESS:
		return drm_dp_sideband_parse_link_address(mgr, raw, msg);
	case DP_QUERY_PAYLOAD:
		return drm_dp_sideband_parse_query_payload_ack(raw, msg);
	case DP_REMOTE_DPCD_READ:
		return drm_dp_sideband_parse_remote_dpcd_read(raw, msg);
	case DP_REMOTE_DPCD_WRITE:
		return drm_dp_sideband_parse_remote_dpcd_write(raw, msg);
	case DP_REMOTE_I2C_READ:
		return drm_dp_sideband_parse_remote_i2c_read_ack(raw, msg);
	case DP_REMOTE_I2C_WRITE:
		return true;  
	case DP_ENUM_PATH_RESOURCES:
		return drm_dp_sideband_parse_enum_path_resources_ack(raw, msg);
	case DP_ALLOCATE_PAYLOAD:
		return drm_dp_sideband_parse_allocate_payload_ack(raw, msg);
	case DP_POWER_DOWN_PHY:
	case DP_POWER_UP_PHY:
		return drm_dp_sideband_parse_power_updown_phy_ack(raw, msg);
	case DP_CLEAR_PAYLOAD_ID_TABLE:
		return true;  
	case DP_QUERY_STREAM_ENC_STATUS:
		return drm_dp_sideband_parse_query_stream_enc_status(raw, msg);
	default:
		drm_err(mgr->dev, "Got unknown reply 0x%02x (%s)\n",
			msg->req_type, drm_dp_mst_req_type_str(msg->req_type));
		return false;
	}
}

static bool
drm_dp_sideband_parse_connection_status_notify(const struct drm_dp_mst_topology_mgr *mgr,
					       struct drm_dp_sideband_msg_rx *raw,
					       struct drm_dp_sideband_msg_req_body *msg)
{
	int idx = 1;

	msg->u.conn_stat.port_number = (raw->msg[idx] & 0xf0) >> 4;
	idx++;
	if (idx > raw->curlen)
		goto fail_len;

	memcpy(msg->u.conn_stat.guid, &raw->msg[idx], 16);
	idx += 16;
	if (idx > raw->curlen)
		goto fail_len;

	msg->u.conn_stat.legacy_device_plug_status = (raw->msg[idx] >> 6) & 0x1;
	msg->u.conn_stat.displayport_device_plug_status = (raw->msg[idx] >> 5) & 0x1;
	msg->u.conn_stat.message_capability_status = (raw->msg[idx] >> 4) & 0x1;
	msg->u.conn_stat.input_port = (raw->msg[idx] >> 3) & 0x1;
	msg->u.conn_stat.peer_device_type = (raw->msg[idx] & 0x7);
	idx++;
	return true;
fail_len:
	drm_dbg_kms(mgr->dev, "connection status reply parse length fail %d %d\n",
		    idx, raw->curlen);
	return false;
}

static bool drm_dp_sideband_parse_resource_status_notify(const struct drm_dp_mst_topology_mgr *mgr,
							 struct drm_dp_sideband_msg_rx *raw,
							 struct drm_dp_sideband_msg_req_body *msg)
{
	int idx = 1;

	msg->u.resource_stat.port_number = (raw->msg[idx] & 0xf0) >> 4;
	idx++;
	if (idx > raw->curlen)
		goto fail_len;

	memcpy(msg->u.resource_stat.guid, &raw->msg[idx], 16);
	idx += 16;
	if (idx > raw->curlen)
		goto fail_len;

	msg->u.resource_stat.available_pbn = (raw->msg[idx] << 8) | (raw->msg[idx + 1]);
	idx++;
	return true;
fail_len:
	drm_dbg_kms(mgr->dev, "resource status reply parse length fail %d %d\n", idx, raw->curlen);
	return false;
}

static bool drm_dp_sideband_parse_req(const struct drm_dp_mst_topology_mgr *mgr,
				      struct drm_dp_sideband_msg_rx *raw,
				      struct drm_dp_sideband_msg_req_body *msg)
{
	memset(msg, 0, sizeof(*msg));
	msg->req_type = (raw->msg[0] & 0x7f);

	switch (msg->req_type) {
	case DP_CONNECTION_STATUS_NOTIFY:
		return drm_dp_sideband_parse_connection_status_notify(mgr, raw, msg);
	case DP_RESOURCE_STATUS_NOTIFY:
		return drm_dp_sideband_parse_resource_status_notify(mgr, raw, msg);
	default:
		drm_err(mgr->dev, "Got unknown request 0x%02x (%s)\n",
			msg->req_type, drm_dp_mst_req_type_str(msg->req_type));
		return false;
	}
}

static void build_dpcd_write(struct drm_dp_sideband_msg_tx *msg,
			     u8 port_num, u32 offset, u8 num_bytes, u8 *bytes)
{
	struct drm_dp_sideband_msg_req_body req;

	req.req_type = DP_REMOTE_DPCD_WRITE;
	req.u.dpcd_write.port_number = port_num;
	req.u.dpcd_write.dpcd_address = offset;
	req.u.dpcd_write.num_bytes = num_bytes;
	req.u.dpcd_write.bytes = bytes;
	drm_dp_encode_sideband_req(&req, msg);
}

static void build_link_address(struct drm_dp_sideband_msg_tx *msg)
{
	struct drm_dp_sideband_msg_req_body req;

	req.req_type = DP_LINK_ADDRESS;
	drm_dp_encode_sideband_req(&req, msg);
}

static void build_clear_payload_id_table(struct drm_dp_sideband_msg_tx *msg)
{
	struct drm_dp_sideband_msg_req_body req;

	req.req_type = DP_CLEAR_PAYLOAD_ID_TABLE;
	drm_dp_encode_sideband_req(&req, msg);
	msg->path_msg = true;
}

static int build_enum_path_resources(struct drm_dp_sideband_msg_tx *msg,
				     int port_num)
{
	struct drm_dp_sideband_msg_req_body req;

	req.req_type = DP_ENUM_PATH_RESOURCES;
	req.u.port_num.port_number = port_num;
	drm_dp_encode_sideband_req(&req, msg);
	msg->path_msg = true;
	return 0;
}

static void build_allocate_payload(struct drm_dp_sideband_msg_tx *msg,
				   int port_num,
				   u8 vcpi, uint16_t pbn,
				   u8 number_sdp_streams,
				   u8 *sdp_stream_sink)
{
	struct drm_dp_sideband_msg_req_body req;

	memset(&req, 0, sizeof(req));
	req.req_type = DP_ALLOCATE_PAYLOAD;
	req.u.allocate_payload.port_number = port_num;
	req.u.allocate_payload.vcpi = vcpi;
	req.u.allocate_payload.pbn = pbn;
	req.u.allocate_payload.number_sdp_streams = number_sdp_streams;
	memcpy(req.u.allocate_payload.sdp_stream_sink, sdp_stream_sink,
		   number_sdp_streams);
	drm_dp_encode_sideband_req(&req, msg);
	msg->path_msg = true;
}

static void build_power_updown_phy(struct drm_dp_sideband_msg_tx *msg,
				   int port_num, bool power_up)
{
	struct drm_dp_sideband_msg_req_body req;

	if (power_up)
		req.req_type = DP_POWER_UP_PHY;
	else
		req.req_type = DP_POWER_DOWN_PHY;

	req.u.port_num.port_number = port_num;
	drm_dp_encode_sideband_req(&req, msg);
	msg->path_msg = true;
}

static int
build_query_stream_enc_status(struct drm_dp_sideband_msg_tx *msg, u8 stream_id,
			      u8 *q_id)
{
	struct drm_dp_sideband_msg_req_body req;

	req.req_type = DP_QUERY_STREAM_ENC_STATUS;
	req.u.enc_status.stream_id = stream_id;
	memcpy(req.u.enc_status.client_id, q_id,
	       sizeof(req.u.enc_status.client_id));
	req.u.enc_status.stream_event = 0;
	req.u.enc_status.valid_stream_event = false;
	req.u.enc_status.stream_behavior = 0;
	req.u.enc_status.valid_stream_behavior = false;

	drm_dp_encode_sideband_req(&req, msg);
	return 0;
}

static bool check_txmsg_state(struct drm_dp_mst_topology_mgr *mgr,
			      struct drm_dp_sideband_msg_tx *txmsg)
{
	unsigned int state;

	 
	state = READ_ONCE(txmsg->state);
	return (state == DRM_DP_SIDEBAND_TX_RX ||
		state == DRM_DP_SIDEBAND_TX_TIMEOUT);
}

static int drm_dp_mst_wait_tx_reply(struct drm_dp_mst_branch *mstb,
				    struct drm_dp_sideband_msg_tx *txmsg)
{
	struct drm_dp_mst_topology_mgr *mgr = mstb->mgr;
	unsigned long wait_timeout = msecs_to_jiffies(4000);
	unsigned long wait_expires = jiffies + wait_timeout;
	int ret;

	for (;;) {
		 
		ret = wait_event_timeout(mgr->tx_waitq,
					 check_txmsg_state(mgr, txmsg),
					 mgr->cbs->poll_hpd_irq ?
						msecs_to_jiffies(50) :
						wait_timeout);

		if (ret || !mgr->cbs->poll_hpd_irq ||
		    time_after(jiffies, wait_expires))
			break;

		mgr->cbs->poll_hpd_irq(mgr);
	}

	mutex_lock(&mgr->qlock);
	if (ret > 0) {
		if (txmsg->state == DRM_DP_SIDEBAND_TX_TIMEOUT) {
			ret = -EIO;
			goto out;
		}
	} else {
		drm_dbg_kms(mgr->dev, "timedout msg send %p %d %d\n",
			    txmsg, txmsg->state, txmsg->seqno);

		 
		ret = -EIO;

		 
		if (txmsg->state == DRM_DP_SIDEBAND_TX_QUEUED ||
		    txmsg->state == DRM_DP_SIDEBAND_TX_START_SEND ||
		    txmsg->state == DRM_DP_SIDEBAND_TX_SENT)
			list_del(&txmsg->next);
	}
out:
	if (unlikely(ret == -EIO) && drm_debug_enabled(DRM_UT_DP)) {
		struct drm_printer p = drm_debug_printer(DBG_PREFIX);

		drm_dp_mst_dump_sideband_msg_tx(&p, txmsg);
	}
	mutex_unlock(&mgr->qlock);

	drm_dp_mst_kick_tx(mgr);
	return ret;
}

static struct drm_dp_mst_branch *drm_dp_add_mst_branch_device(u8 lct, u8 *rad)
{
	struct drm_dp_mst_branch *mstb;

	mstb = kzalloc(sizeof(*mstb), GFP_KERNEL);
	if (!mstb)
		return NULL;

	mstb->lct = lct;
	if (lct > 1)
		memcpy(mstb->rad, rad, lct / 2);
	INIT_LIST_HEAD(&mstb->ports);
	kref_init(&mstb->topology_kref);
	kref_init(&mstb->malloc_kref);
	return mstb;
}

static void drm_dp_free_mst_branch_device(struct kref *kref)
{
	struct drm_dp_mst_branch *mstb =
		container_of(kref, struct drm_dp_mst_branch, malloc_kref);

	if (mstb->port_parent)
		drm_dp_mst_put_port_malloc(mstb->port_parent);

	kfree(mstb);
}

 

 
static void
drm_dp_mst_get_mstb_malloc(struct drm_dp_mst_branch *mstb)
{
	kref_get(&mstb->malloc_kref);
	drm_dbg(mstb->mgr->dev, "mstb %p (%d)\n", mstb, kref_read(&mstb->malloc_kref));
}

 
static void
drm_dp_mst_put_mstb_malloc(struct drm_dp_mst_branch *mstb)
{
	drm_dbg(mstb->mgr->dev, "mstb %p (%d)\n", mstb, kref_read(&mstb->malloc_kref) - 1);
	kref_put(&mstb->malloc_kref, drm_dp_free_mst_branch_device);
}

static void drm_dp_free_mst_port(struct kref *kref)
{
	struct drm_dp_mst_port *port =
		container_of(kref, struct drm_dp_mst_port, malloc_kref);

	drm_dp_mst_put_mstb_malloc(port->parent);
	kfree(port);
}

 
void
drm_dp_mst_get_port_malloc(struct drm_dp_mst_port *port)
{
	kref_get(&port->malloc_kref);
	drm_dbg(port->mgr->dev, "port %p (%d)\n", port, kref_read(&port->malloc_kref));
}
EXPORT_SYMBOL(drm_dp_mst_get_port_malloc);

 
void
drm_dp_mst_put_port_malloc(struct drm_dp_mst_port *port)
{
	drm_dbg(port->mgr->dev, "port %p (%d)\n", port, kref_read(&port->malloc_kref) - 1);
	kref_put(&port->malloc_kref, drm_dp_free_mst_port);
}
EXPORT_SYMBOL(drm_dp_mst_put_port_malloc);

#if IS_ENABLED(CONFIG_DRM_DEBUG_DP_MST_TOPOLOGY_REFS)

#define STACK_DEPTH 8

static noinline void
__topology_ref_save(struct drm_dp_mst_topology_mgr *mgr,
		    struct drm_dp_mst_topology_ref_history *history,
		    enum drm_dp_mst_topology_ref_type type)
{
	struct drm_dp_mst_topology_ref_entry *entry = NULL;
	depot_stack_handle_t backtrace;
	ulong stack_entries[STACK_DEPTH];
	uint n;
	int i;

	n = stack_trace_save(stack_entries, ARRAY_SIZE(stack_entries), 1);
	backtrace = stack_depot_save(stack_entries, n, GFP_KERNEL);
	if (!backtrace)
		return;

	 
	for (i = 0; i < history->len; i++) {
		if (history->entries[i].backtrace == backtrace) {
			entry = &history->entries[i];
			break;
		}
	}

	 
	if (!entry) {
		struct drm_dp_mst_topology_ref_entry *new;
		int new_len = history->len + 1;

		new = krealloc(history->entries, sizeof(*new) * new_len,
			       GFP_KERNEL);
		if (!new)
			return;

		entry = &new[history->len];
		history->len = new_len;
		history->entries = new;

		entry->backtrace = backtrace;
		entry->type = type;
		entry->count = 0;
	}
	entry->count++;
	entry->ts_nsec = ktime_get_ns();
}

static int
topology_ref_history_cmp(const void *a, const void *b)
{
	const struct drm_dp_mst_topology_ref_entry *entry_a = a, *entry_b = b;

	if (entry_a->ts_nsec > entry_b->ts_nsec)
		return 1;
	else if (entry_a->ts_nsec < entry_b->ts_nsec)
		return -1;
	else
		return 0;
}

static inline const char *
topology_ref_type_to_str(enum drm_dp_mst_topology_ref_type type)
{
	if (type == DRM_DP_MST_TOPOLOGY_REF_GET)
		return "get";
	else
		return "put";
}

static void
__dump_topology_ref_history(struct drm_dp_mst_topology_ref_history *history,
			    void *ptr, const char *type_str)
{
	struct drm_printer p = drm_debug_printer(DBG_PREFIX);
	char *buf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	int i;

	if (!buf)
		return;

	if (!history->len)
		goto out;

	 
	sort(history->entries, history->len, sizeof(*history->entries),
	     topology_ref_history_cmp, NULL);

	drm_printf(&p, "%s (%p) topology count reached 0, dumping history:\n",
		   type_str, ptr);

	for (i = 0; i < history->len; i++) {
		const struct drm_dp_mst_topology_ref_entry *entry =
			&history->entries[i];
		u64 ts_nsec = entry->ts_nsec;
		u32 rem_nsec = do_div(ts_nsec, 1000000000);

		stack_depot_snprint(entry->backtrace, buf, PAGE_SIZE, 4);

		drm_printf(&p, "  %d %ss (last at %5llu.%06u):\n%s",
			   entry->count,
			   topology_ref_type_to_str(entry->type),
			   ts_nsec, rem_nsec / 1000, buf);
	}

	 
	kfree(history->entries);
out:
	kfree(buf);
}

static __always_inline void
drm_dp_mst_dump_mstb_topology_history(struct drm_dp_mst_branch *mstb)
{
	__dump_topology_ref_history(&mstb->topology_ref_history, mstb,
				    "MSTB");
}

static __always_inline void
drm_dp_mst_dump_port_topology_history(struct drm_dp_mst_port *port)
{
	__dump_topology_ref_history(&port->topology_ref_history, port,
				    "Port");
}

static __always_inline void
save_mstb_topology_ref(struct drm_dp_mst_branch *mstb,
		       enum drm_dp_mst_topology_ref_type type)
{
	__topology_ref_save(mstb->mgr, &mstb->topology_ref_history, type);
}

static __always_inline void
save_port_topology_ref(struct drm_dp_mst_port *port,
		       enum drm_dp_mst_topology_ref_type type)
{
	__topology_ref_save(port->mgr, &port->topology_ref_history, type);
}

static inline void
topology_ref_history_lock(struct drm_dp_mst_topology_mgr *mgr)
{
	mutex_lock(&mgr->topology_ref_history_lock);
}

static inline void
topology_ref_history_unlock(struct drm_dp_mst_topology_mgr *mgr)
{
	mutex_unlock(&mgr->topology_ref_history_lock);
}
#else
static inline void
topology_ref_history_lock(struct drm_dp_mst_topology_mgr *mgr) {}
static inline void
topology_ref_history_unlock(struct drm_dp_mst_topology_mgr *mgr) {}
static inline void
drm_dp_mst_dump_mstb_topology_history(struct drm_dp_mst_branch *mstb) {}
static inline void
drm_dp_mst_dump_port_topology_history(struct drm_dp_mst_port *port) {}
#define save_mstb_topology_ref(mstb, type)
#define save_port_topology_ref(port, type)
#endif

struct drm_dp_mst_atomic_payload *
drm_atomic_get_mst_payload_state(struct drm_dp_mst_topology_state *state,
				 struct drm_dp_mst_port *port)
{
	struct drm_dp_mst_atomic_payload *payload;

	list_for_each_entry(payload, &state->payloads, next)
		if (payload->port == port)
			return payload;

	return NULL;
}
EXPORT_SYMBOL(drm_atomic_get_mst_payload_state);

static void drm_dp_destroy_mst_branch_device(struct kref *kref)
{
	struct drm_dp_mst_branch *mstb =
		container_of(kref, struct drm_dp_mst_branch, topology_kref);
	struct drm_dp_mst_topology_mgr *mgr = mstb->mgr;

	drm_dp_mst_dump_mstb_topology_history(mstb);

	INIT_LIST_HEAD(&mstb->destroy_next);

	 
	mutex_lock(&mgr->delayed_destroy_lock);
	list_add(&mstb->destroy_next, &mgr->destroy_branch_device_list);
	mutex_unlock(&mgr->delayed_destroy_lock);
	queue_work(mgr->delayed_destroy_wq, &mgr->delayed_destroy_work);
}

 
static int __must_check
drm_dp_mst_topology_try_get_mstb(struct drm_dp_mst_branch *mstb)
{
	int ret;

	topology_ref_history_lock(mstb->mgr);
	ret = kref_get_unless_zero(&mstb->topology_kref);
	if (ret) {
		drm_dbg(mstb->mgr->dev, "mstb %p (%d)\n", mstb, kref_read(&mstb->topology_kref));
		save_mstb_topology_ref(mstb, DRM_DP_MST_TOPOLOGY_REF_GET);
	}

	topology_ref_history_unlock(mstb->mgr);

	return ret;
}

 
static void drm_dp_mst_topology_get_mstb(struct drm_dp_mst_branch *mstb)
{
	topology_ref_history_lock(mstb->mgr);

	save_mstb_topology_ref(mstb, DRM_DP_MST_TOPOLOGY_REF_GET);
	WARN_ON(kref_read(&mstb->topology_kref) == 0);
	kref_get(&mstb->topology_kref);
	drm_dbg(mstb->mgr->dev, "mstb %p (%d)\n", mstb, kref_read(&mstb->topology_kref));

	topology_ref_history_unlock(mstb->mgr);
}

 
static void
drm_dp_mst_topology_put_mstb(struct drm_dp_mst_branch *mstb)
{
	topology_ref_history_lock(mstb->mgr);

	drm_dbg(mstb->mgr->dev, "mstb %p (%d)\n", mstb, kref_read(&mstb->topology_kref) - 1);
	save_mstb_topology_ref(mstb, DRM_DP_MST_TOPOLOGY_REF_PUT);

	topology_ref_history_unlock(mstb->mgr);
	kref_put(&mstb->topology_kref, drm_dp_destroy_mst_branch_device);
}

static void drm_dp_destroy_port(struct kref *kref)
{
	struct drm_dp_mst_port *port =
		container_of(kref, struct drm_dp_mst_port, topology_kref);
	struct drm_dp_mst_topology_mgr *mgr = port->mgr;

	drm_dp_mst_dump_port_topology_history(port);

	 
	if (port->input) {
		drm_dp_mst_put_port_malloc(port);
		return;
	}

	drm_edid_free(port->cached_edid);

	 
	mutex_lock(&mgr->delayed_destroy_lock);
	list_add(&port->next, &mgr->destroy_port_list);
	mutex_unlock(&mgr->delayed_destroy_lock);
	queue_work(mgr->delayed_destroy_wq, &mgr->delayed_destroy_work);
}

 
static int __must_check
drm_dp_mst_topology_try_get_port(struct drm_dp_mst_port *port)
{
	int ret;

	topology_ref_history_lock(port->mgr);
	ret = kref_get_unless_zero(&port->topology_kref);
	if (ret) {
		drm_dbg(port->mgr->dev, "port %p (%d)\n", port, kref_read(&port->topology_kref));
		save_port_topology_ref(port, DRM_DP_MST_TOPOLOGY_REF_GET);
	}

	topology_ref_history_unlock(port->mgr);
	return ret;
}

 
static void drm_dp_mst_topology_get_port(struct drm_dp_mst_port *port)
{
	topology_ref_history_lock(port->mgr);

	WARN_ON(kref_read(&port->topology_kref) == 0);
	kref_get(&port->topology_kref);
	drm_dbg(port->mgr->dev, "port %p (%d)\n", port, kref_read(&port->topology_kref));
	save_port_topology_ref(port, DRM_DP_MST_TOPOLOGY_REF_GET);

	topology_ref_history_unlock(port->mgr);
}

 
static void drm_dp_mst_topology_put_port(struct drm_dp_mst_port *port)
{
	topology_ref_history_lock(port->mgr);

	drm_dbg(port->mgr->dev, "port %p (%d)\n", port, kref_read(&port->topology_kref) - 1);
	save_port_topology_ref(port, DRM_DP_MST_TOPOLOGY_REF_PUT);

	topology_ref_history_unlock(port->mgr);
	kref_put(&port->topology_kref, drm_dp_destroy_port);
}

static struct drm_dp_mst_branch *
drm_dp_mst_topology_get_mstb_validated_locked(struct drm_dp_mst_branch *mstb,
					      struct drm_dp_mst_branch *to_find)
{
	struct drm_dp_mst_port *port;
	struct drm_dp_mst_branch *rmstb;

	if (to_find == mstb)
		return mstb;

	list_for_each_entry(port, &mstb->ports, next) {
		if (port->mstb) {
			rmstb = drm_dp_mst_topology_get_mstb_validated_locked(
			    port->mstb, to_find);
			if (rmstb)
				return rmstb;
		}
	}
	return NULL;
}

static struct drm_dp_mst_branch *
drm_dp_mst_topology_get_mstb_validated(struct drm_dp_mst_topology_mgr *mgr,
				       struct drm_dp_mst_branch *mstb)
{
	struct drm_dp_mst_branch *rmstb = NULL;

	mutex_lock(&mgr->lock);
	if (mgr->mst_primary) {
		rmstb = drm_dp_mst_topology_get_mstb_validated_locked(
		    mgr->mst_primary, mstb);

		if (rmstb && !drm_dp_mst_topology_try_get_mstb(rmstb))
			rmstb = NULL;
	}
	mutex_unlock(&mgr->lock);
	return rmstb;
}

static struct drm_dp_mst_port *
drm_dp_mst_topology_get_port_validated_locked(struct drm_dp_mst_branch *mstb,
					      struct drm_dp_mst_port *to_find)
{
	struct drm_dp_mst_port *port, *mport;

	list_for_each_entry(port, &mstb->ports, next) {
		if (port == to_find)
			return port;

		if (port->mstb) {
			mport = drm_dp_mst_topology_get_port_validated_locked(
			    port->mstb, to_find);
			if (mport)
				return mport;
		}
	}
	return NULL;
}

static struct drm_dp_mst_port *
drm_dp_mst_topology_get_port_validated(struct drm_dp_mst_topology_mgr *mgr,
				       struct drm_dp_mst_port *port)
{
	struct drm_dp_mst_port *rport = NULL;

	mutex_lock(&mgr->lock);
	if (mgr->mst_primary) {
		rport = drm_dp_mst_topology_get_port_validated_locked(
		    mgr->mst_primary, port);

		if (rport && !drm_dp_mst_topology_try_get_port(rport))
			rport = NULL;
	}
	mutex_unlock(&mgr->lock);
	return rport;
}

static struct drm_dp_mst_port *drm_dp_get_port(struct drm_dp_mst_branch *mstb, u8 port_num)
{
	struct drm_dp_mst_port *port;
	int ret;

	list_for_each_entry(port, &mstb->ports, next) {
		if (port->port_num == port_num) {
			ret = drm_dp_mst_topology_try_get_port(port);
			return ret ? port : NULL;
		}
	}

	return NULL;
}

 
static u8 drm_dp_calculate_rad(struct drm_dp_mst_port *port,
				 u8 *rad)
{
	int parent_lct = port->parent->lct;
	int shift = 4;
	int idx = (parent_lct - 1) / 2;

	if (parent_lct > 1) {
		memcpy(rad, port->parent->rad, idx + 1);
		shift = (parent_lct % 2) ? 4 : 0;
	} else
		rad[0] = 0;

	rad[idx] |= port->port_num << shift;
	return parent_lct + 1;
}

static bool drm_dp_mst_is_end_device(u8 pdt, bool mcs)
{
	switch (pdt) {
	case DP_PEER_DEVICE_DP_LEGACY_CONV:
	case DP_PEER_DEVICE_SST_SINK:
		return true;
	case DP_PEER_DEVICE_MST_BRANCHING:
		 
		if (!mcs)
			return true;

		return false;
	}
	return true;
}

static int
drm_dp_port_set_pdt(struct drm_dp_mst_port *port, u8 new_pdt,
		    bool new_mcs)
{
	struct drm_dp_mst_topology_mgr *mgr = port->mgr;
	struct drm_dp_mst_branch *mstb;
	u8 rad[8], lct;
	int ret = 0;

	if (port->pdt == new_pdt && port->mcs == new_mcs)
		return 0;

	 
	if (port->pdt != DP_PEER_DEVICE_NONE) {
		if (drm_dp_mst_is_end_device(port->pdt, port->mcs)) {
			 
			if (new_pdt != DP_PEER_DEVICE_NONE &&
			    drm_dp_mst_is_end_device(new_pdt, new_mcs)) {
				port->pdt = new_pdt;
				port->mcs = new_mcs;
				return 0;
			}

			 
			drm_dp_mst_unregister_i2c_bus(port);
		} else {
			mutex_lock(&mgr->lock);
			drm_dp_mst_topology_put_mstb(port->mstb);
			port->mstb = NULL;
			mutex_unlock(&mgr->lock);
		}
	}

	port->pdt = new_pdt;
	port->mcs = new_mcs;

	if (port->pdt != DP_PEER_DEVICE_NONE) {
		if (drm_dp_mst_is_end_device(port->pdt, port->mcs)) {
			 
			ret = drm_dp_mst_register_i2c_bus(port);
		} else {
			lct = drm_dp_calculate_rad(port, rad);
			mstb = drm_dp_add_mst_branch_device(lct, rad);
			if (!mstb) {
				ret = -ENOMEM;
				drm_err(mgr->dev, "Failed to create MSTB for port %p", port);
				goto out;
			}

			mutex_lock(&mgr->lock);
			port->mstb = mstb;
			mstb->mgr = port->mgr;
			mstb->port_parent = port;

			 
			drm_dp_mst_get_port_malloc(port);
			mutex_unlock(&mgr->lock);

			 
			ret = 1;
		}
	}

out:
	if (ret < 0)
		port->pdt = DP_PEER_DEVICE_NONE;
	return ret;
}

 
ssize_t drm_dp_mst_dpcd_read(struct drm_dp_aux *aux,
			     unsigned int offset, void *buffer, size_t size)
{
	struct drm_dp_mst_port *port = container_of(aux, struct drm_dp_mst_port,
						    aux);

	return drm_dp_send_dpcd_read(port->mgr, port,
				     offset, size, buffer);
}

 
ssize_t drm_dp_mst_dpcd_write(struct drm_dp_aux *aux,
			      unsigned int offset, void *buffer, size_t size)
{
	struct drm_dp_mst_port *port = container_of(aux, struct drm_dp_mst_port,
						    aux);

	return drm_dp_send_dpcd_write(port->mgr, port,
				      offset, size, buffer);
}

static int drm_dp_check_mstb_guid(struct drm_dp_mst_branch *mstb, u8 *guid)
{
	int ret = 0;

	memcpy(mstb->guid, guid, 16);

	if (!drm_dp_validate_guid(mstb->mgr, mstb->guid)) {
		if (mstb->port_parent) {
			ret = drm_dp_send_dpcd_write(mstb->mgr,
						     mstb->port_parent,
						     DP_GUID, 16, mstb->guid);
		} else {
			ret = drm_dp_dpcd_write(mstb->mgr->aux,
						DP_GUID, mstb->guid, 16);
		}
	}

	if (ret < 16 && ret > 0)
		return -EPROTO;

	return ret == 16 ? 0 : ret;
}

static void build_mst_prop_path(const struct drm_dp_mst_branch *mstb,
				int pnum,
				char *proppath,
				size_t proppath_size)
{
	int i;
	char temp[8];

	snprintf(proppath, proppath_size, "mst:%d", mstb->mgr->conn_base_id);
	for (i = 0; i < (mstb->lct - 1); i++) {
		int shift = (i % 2) ? 0 : 4;
		int port_num = (mstb->rad[i / 2] >> shift) & 0xf;

		snprintf(temp, sizeof(temp), "-%d", port_num);
		strlcat(proppath, temp, proppath_size);
	}
	snprintf(temp, sizeof(temp), "-%d", pnum);
	strlcat(proppath, temp, proppath_size);
}

 
int drm_dp_mst_connector_late_register(struct drm_connector *connector,
				       struct drm_dp_mst_port *port)
{
	drm_dbg_kms(port->mgr->dev, "registering %s remote bus for %s\n",
		    port->aux.name, connector->kdev->kobj.name);

	port->aux.dev = connector->kdev;
	return drm_dp_aux_register_devnode(&port->aux);
}
EXPORT_SYMBOL(drm_dp_mst_connector_late_register);

 
void drm_dp_mst_connector_early_unregister(struct drm_connector *connector,
					   struct drm_dp_mst_port *port)
{
	drm_dbg_kms(port->mgr->dev, "unregistering %s remote bus for %s\n",
		    port->aux.name, connector->kdev->kobj.name);
	drm_dp_aux_unregister_devnode(&port->aux);
}
EXPORT_SYMBOL(drm_dp_mst_connector_early_unregister);

static void
drm_dp_mst_port_add_connector(struct drm_dp_mst_branch *mstb,
			      struct drm_dp_mst_port *port)
{
	struct drm_dp_mst_topology_mgr *mgr = port->mgr;
	char proppath[255];
	int ret;

	build_mst_prop_path(mstb, port->port_num, proppath, sizeof(proppath));
	port->connector = mgr->cbs->add_connector(mgr, port, proppath);
	if (!port->connector) {
		ret = -ENOMEM;
		goto error;
	}

	if (port->pdt != DP_PEER_DEVICE_NONE &&
	    drm_dp_mst_is_end_device(port->pdt, port->mcs) &&
	    port->port_num >= DP_MST_LOGICAL_PORT_0)
		port->cached_edid = drm_edid_read_ddc(port->connector,
						      &port->aux.ddc);

	drm_connector_register(port->connector);
	return;

error:
	drm_err(mgr->dev, "Failed to create connector for port %p: %d\n", port, ret);
}

 
static void
drm_dp_mst_topology_unlink_port(struct drm_dp_mst_topology_mgr *mgr,
				struct drm_dp_mst_port *port)
{
	mutex_lock(&mgr->lock);
	port->parent->num_ports--;
	list_del(&port->next);
	mutex_unlock(&mgr->lock);
	drm_dp_mst_topology_put_port(port);
}

static struct drm_dp_mst_port *
drm_dp_mst_add_port(struct drm_device *dev,
		    struct drm_dp_mst_topology_mgr *mgr,
		    struct drm_dp_mst_branch *mstb, u8 port_number)
{
	struct drm_dp_mst_port *port = kzalloc(sizeof(*port), GFP_KERNEL);

	if (!port)
		return NULL;

	kref_init(&port->topology_kref);
	kref_init(&port->malloc_kref);
	port->parent = mstb;
	port->port_num = port_number;
	port->mgr = mgr;
	port->aux.name = "DPMST";
	port->aux.dev = dev->dev;
	port->aux.is_remote = true;

	 
	port->aux.drm_dev = dev;
	drm_dp_remote_aux_init(&port->aux);

	 
	drm_dp_mst_get_mstb_malloc(mstb);

	return port;
}

static int
drm_dp_mst_handle_link_address_port(struct drm_dp_mst_branch *mstb,
				    struct drm_device *dev,
				    struct drm_dp_link_addr_reply_port *port_msg)
{
	struct drm_dp_mst_topology_mgr *mgr = mstb->mgr;
	struct drm_dp_mst_port *port;
	int old_ddps = 0, ret;
	u8 new_pdt = DP_PEER_DEVICE_NONE;
	bool new_mcs = 0;
	bool created = false, send_link_addr = false, changed = false;

	port = drm_dp_get_port(mstb, port_msg->port_number);
	if (!port) {
		port = drm_dp_mst_add_port(dev, mgr, mstb,
					   port_msg->port_number);
		if (!port)
			return -ENOMEM;
		created = true;
		changed = true;
	} else if (!port->input && port_msg->input_port && port->connector) {
		 
		drm_dp_mst_topology_unlink_port(mgr, port);
		drm_dp_mst_topology_put_port(port);
		port = drm_dp_mst_add_port(dev, mgr, mstb,
					   port_msg->port_number);
		if (!port)
			return -ENOMEM;
		changed = true;
		created = true;
	} else if (port->input && !port_msg->input_port) {
		changed = true;
	} else if (port->connector) {
		 
		drm_modeset_lock(&mgr->base.lock, NULL);

		old_ddps = port->ddps;
		changed = port->ddps != port_msg->ddps ||
			(port->ddps &&
			 (port->ldps != port_msg->legacy_device_plug_status ||
			  port->dpcd_rev != port_msg->dpcd_revision ||
			  port->mcs != port_msg->mcs ||
			  port->pdt != port_msg->peer_device_type ||
			  port->num_sdp_stream_sinks !=
			  port_msg->num_sdp_stream_sinks));
	}

	port->input = port_msg->input_port;
	if (!port->input)
		new_pdt = port_msg->peer_device_type;
	new_mcs = port_msg->mcs;
	port->ddps = port_msg->ddps;
	port->ldps = port_msg->legacy_device_plug_status;
	port->dpcd_rev = port_msg->dpcd_revision;
	port->num_sdp_streams = port_msg->num_sdp_streams;
	port->num_sdp_stream_sinks = port_msg->num_sdp_stream_sinks;

	 
	if (created) {
		mutex_lock(&mgr->lock);
		drm_dp_mst_topology_get_port(port);
		list_add(&port->next, &mstb->ports);
		mstb->num_ports++;
		mutex_unlock(&mgr->lock);
	}

	 
	if (old_ddps != port->ddps || !created) {
		if (port->ddps && !port->input) {
			ret = drm_dp_send_enum_path_resources(mgr, mstb,
							      port);
			if (ret == 1)
				changed = true;
		} else {
			port->full_pbn = 0;
		}
	}

	ret = drm_dp_port_set_pdt(port, new_pdt, new_mcs);
	if (ret == 1) {
		send_link_addr = true;
	} else if (ret < 0) {
		drm_err(dev, "Failed to change PDT on port %p: %d\n", port, ret);
		goto fail;
	}

	 
	if (!created && port->pdt == DP_PEER_DEVICE_MST_BRANCHING &&
	    port->mcs)
		send_link_addr = true;

	if (port->connector)
		drm_modeset_unlock(&mgr->base.lock);
	else if (!port->input)
		drm_dp_mst_port_add_connector(mstb, port);

	if (send_link_addr && port->mstb) {
		ret = drm_dp_send_link_address(mgr, port->mstb);
		if (ret == 1)  
			changed = true;
		else if (ret < 0)
			goto fail_put;
	}

	 
	drm_dp_mst_topology_put_port(port);
	return changed;

fail:
	drm_dp_mst_topology_unlink_port(mgr, port);
	if (port->connector)
		drm_modeset_unlock(&mgr->base.lock);
fail_put:
	drm_dp_mst_topology_put_port(port);
	return ret;
}

static int
drm_dp_mst_handle_conn_stat(struct drm_dp_mst_branch *mstb,
			    struct drm_dp_connection_status_notify *conn_stat)
{
	struct drm_dp_mst_topology_mgr *mgr = mstb->mgr;
	struct drm_dp_mst_port *port;
	int old_ddps, ret;
	u8 new_pdt;
	bool new_mcs;
	bool dowork = false, create_connector = false;

	port = drm_dp_get_port(mstb, conn_stat->port_number);
	if (!port)
		return 0;

	if (port->connector) {
		if (!port->input && conn_stat->input_port) {
			 
			drm_dp_mst_topology_unlink_port(mgr, port);
			mstb->link_address_sent = false;
			dowork = true;
			goto out;
		}

		 
		drm_modeset_lock(&mgr->base.lock, NULL);
	} else if (port->input && !conn_stat->input_port) {
		create_connector = true;
		 
		mstb->link_address_sent = false;
		dowork = true;
	}

	old_ddps = port->ddps;
	port->input = conn_stat->input_port;
	port->ldps = conn_stat->legacy_device_plug_status;
	port->ddps = conn_stat->displayport_device_plug_status;

	if (old_ddps != port->ddps) {
		if (port->ddps && !port->input)
			drm_dp_send_enum_path_resources(mgr, mstb, port);
		else
			port->full_pbn = 0;
	}

	new_pdt = port->input ? DP_PEER_DEVICE_NONE : conn_stat->peer_device_type;
	new_mcs = conn_stat->message_capability_status;
	ret = drm_dp_port_set_pdt(port, new_pdt, new_mcs);
	if (ret == 1) {
		dowork = true;
	} else if (ret < 0) {
		drm_err(mgr->dev, "Failed to change PDT for port %p: %d\n", port, ret);
		dowork = false;
	}

	if (port->connector)
		drm_modeset_unlock(&mgr->base.lock);
	else if (create_connector)
		drm_dp_mst_port_add_connector(mstb, port);

out:
	drm_dp_mst_topology_put_port(port);
	return dowork;
}

static struct drm_dp_mst_branch *drm_dp_get_mst_branch_device(struct drm_dp_mst_topology_mgr *mgr,
							       u8 lct, u8 *rad)
{
	struct drm_dp_mst_branch *mstb;
	struct drm_dp_mst_port *port;
	int i, ret;
	 

	mutex_lock(&mgr->lock);
	mstb = mgr->mst_primary;

	if (!mstb)
		goto out;

	for (i = 0; i < lct - 1; i++) {
		int shift = (i % 2) ? 0 : 4;
		int port_num = (rad[i / 2] >> shift) & 0xf;

		list_for_each_entry(port, &mstb->ports, next) {
			if (port->port_num == port_num) {
				mstb = port->mstb;
				if (!mstb) {
					drm_err(mgr->dev,
						"failed to lookup MSTB with lct %d, rad %02x\n",
						lct, rad[0]);
					goto out;
				}

				break;
			}
		}
	}
	ret = drm_dp_mst_topology_try_get_mstb(mstb);
	if (!ret)
		mstb = NULL;
out:
	mutex_unlock(&mgr->lock);
	return mstb;
}

static struct drm_dp_mst_branch *get_mst_branch_device_by_guid_helper(
	struct drm_dp_mst_branch *mstb,
	const uint8_t *guid)
{
	struct drm_dp_mst_branch *found_mstb;
	struct drm_dp_mst_port *port;

	if (!mstb)
		return NULL;

	if (memcmp(mstb->guid, guid, 16) == 0)
		return mstb;


	list_for_each_entry(port, &mstb->ports, next) {
		found_mstb = get_mst_branch_device_by_guid_helper(port->mstb, guid);

		if (found_mstb)
			return found_mstb;
	}

	return NULL;
}

static struct drm_dp_mst_branch *
drm_dp_get_mst_branch_device_by_guid(struct drm_dp_mst_topology_mgr *mgr,
				     const uint8_t *guid)
{
	struct drm_dp_mst_branch *mstb;
	int ret;

	 
	mutex_lock(&mgr->lock);

	mstb = get_mst_branch_device_by_guid_helper(mgr->mst_primary, guid);
	if (mstb) {
		ret = drm_dp_mst_topology_try_get_mstb(mstb);
		if (!ret)
			mstb = NULL;
	}

	mutex_unlock(&mgr->lock);
	return mstb;
}

static int drm_dp_check_and_send_link_address(struct drm_dp_mst_topology_mgr *mgr,
					       struct drm_dp_mst_branch *mstb)
{
	struct drm_dp_mst_port *port;
	int ret;
	bool changed = false;

	if (!mstb->link_address_sent) {
		ret = drm_dp_send_link_address(mgr, mstb);
		if (ret == 1)
			changed = true;
		else if (ret < 0)
			return ret;
	}

	list_for_each_entry(port, &mstb->ports, next) {
		if (port->input || !port->ddps || !port->mstb)
			continue;

		ret = drm_dp_check_and_send_link_address(mgr, port->mstb);
		if (ret == 1)
			changed = true;
		else if (ret < 0)
			return ret;
	}

	return changed;
}

static void drm_dp_mst_link_probe_work(struct work_struct *work)
{
	struct drm_dp_mst_topology_mgr *mgr =
		container_of(work, struct drm_dp_mst_topology_mgr, work);
	struct drm_device *dev = mgr->dev;
	struct drm_dp_mst_branch *mstb;
	int ret;
	bool clear_payload_id_table;

	mutex_lock(&mgr->probe_lock);

	mutex_lock(&mgr->lock);
	clear_payload_id_table = !mgr->payload_id_table_cleared;
	mgr->payload_id_table_cleared = true;

	mstb = mgr->mst_primary;
	if (mstb) {
		ret = drm_dp_mst_topology_try_get_mstb(mstb);
		if (!ret)
			mstb = NULL;
	}
	mutex_unlock(&mgr->lock);
	if (!mstb) {
		mutex_unlock(&mgr->probe_lock);
		return;
	}

	 
	if (clear_payload_id_table) {
		drm_dbg_kms(dev, "Clearing payload ID table\n");
		drm_dp_send_clear_payload_id_table(mgr, mstb);
	}

	ret = drm_dp_check_and_send_link_address(mgr, mstb);
	drm_dp_mst_topology_put_mstb(mstb);

	mutex_unlock(&mgr->probe_lock);
	if (ret > 0)
		drm_kms_helper_hotplug_event(dev);
}

static bool drm_dp_validate_guid(struct drm_dp_mst_topology_mgr *mgr,
				 u8 *guid)
{
	u64 salt;

	if (memchr_inv(guid, 0, 16))
		return true;

	salt = get_jiffies_64();

	memcpy(&guid[0], &salt, sizeof(u64));
	memcpy(&guid[8], &salt, sizeof(u64));

	return false;
}

static void build_dpcd_read(struct drm_dp_sideband_msg_tx *msg,
			    u8 port_num, u32 offset, u8 num_bytes)
{
	struct drm_dp_sideband_msg_req_body req;

	req.req_type = DP_REMOTE_DPCD_READ;
	req.u.dpcd_read.port_number = port_num;
	req.u.dpcd_read.dpcd_address = offset;
	req.u.dpcd_read.num_bytes = num_bytes;
	drm_dp_encode_sideband_req(&req, msg);
}

static int drm_dp_send_sideband_msg(struct drm_dp_mst_topology_mgr *mgr,
				    bool up, u8 *msg, int len)
{
	int ret;
	int regbase = up ? DP_SIDEBAND_MSG_UP_REP_BASE : DP_SIDEBAND_MSG_DOWN_REQ_BASE;
	int tosend, total, offset;
	int retries = 0;

retry:
	total = len;
	offset = 0;
	do {
		tosend = min3(mgr->max_dpcd_transaction_bytes, 16, total);

		ret = drm_dp_dpcd_write(mgr->aux, regbase + offset,
					&msg[offset],
					tosend);
		if (ret != tosend) {
			if (ret == -EIO && retries < 5) {
				retries++;
				goto retry;
			}
			drm_dbg_kms(mgr->dev, "failed to dpcd write %d %d\n", tosend, ret);

			return -EIO;
		}
		offset += tosend;
		total -= tosend;
	} while (total > 0);
	return 0;
}

static int set_hdr_from_dst_qlock(struct drm_dp_sideband_msg_hdr *hdr,
				  struct drm_dp_sideband_msg_tx *txmsg)
{
	struct drm_dp_mst_branch *mstb = txmsg->dst;
	u8 req_type;

	req_type = txmsg->msg[0] & 0x7f;
	if (req_type == DP_CONNECTION_STATUS_NOTIFY ||
		req_type == DP_RESOURCE_STATUS_NOTIFY ||
		req_type == DP_CLEAR_PAYLOAD_ID_TABLE)
		hdr->broadcast = 1;
	else
		hdr->broadcast = 0;
	hdr->path_msg = txmsg->path_msg;
	if (hdr->broadcast) {
		hdr->lct = 1;
		hdr->lcr = 6;
	} else {
		hdr->lct = mstb->lct;
		hdr->lcr = mstb->lct - 1;
	}

	memcpy(hdr->rad, mstb->rad, hdr->lct / 2);

	return 0;
}
 
static int process_single_tx_qlock(struct drm_dp_mst_topology_mgr *mgr,
				   struct drm_dp_sideband_msg_tx *txmsg,
				   bool up)
{
	u8 chunk[48];
	struct drm_dp_sideband_msg_hdr hdr;
	int len, space, idx, tosend;
	int ret;

	if (txmsg->state == DRM_DP_SIDEBAND_TX_SENT)
		return 0;

	memset(&hdr, 0, sizeof(struct drm_dp_sideband_msg_hdr));

	if (txmsg->state == DRM_DP_SIDEBAND_TX_QUEUED)
		txmsg->state = DRM_DP_SIDEBAND_TX_START_SEND;

	 
	ret = set_hdr_from_dst_qlock(&hdr, txmsg);
	if (ret < 0)
		return ret;

	 
	len = txmsg->cur_len - txmsg->cur_offset;

	 
	space = 48 - 1 - drm_dp_calc_sb_hdr_size(&hdr);

	tosend = min(len, space);
	if (len == txmsg->cur_len)
		hdr.somt = 1;
	if (space >= len)
		hdr.eomt = 1;


	hdr.msg_len = tosend + 1;
	drm_dp_encode_sideband_msg_hdr(&hdr, chunk, &idx);
	memcpy(&chunk[idx], &txmsg->msg[txmsg->cur_offset], tosend);
	 
	drm_dp_crc_sideband_chunk_req(&chunk[idx], tosend);
	idx += tosend + 1;

	ret = drm_dp_send_sideband_msg(mgr, up, chunk, idx);
	if (ret) {
		if (drm_debug_enabled(DRM_UT_DP)) {
			struct drm_printer p = drm_debug_printer(DBG_PREFIX);

			drm_printf(&p, "sideband msg failed to send\n");
			drm_dp_mst_dump_sideband_msg_tx(&p, txmsg);
		}
		return ret;
	}

	txmsg->cur_offset += tosend;
	if (txmsg->cur_offset == txmsg->cur_len) {
		txmsg->state = DRM_DP_SIDEBAND_TX_SENT;
		return 1;
	}
	return 0;
}

static void process_single_down_tx_qlock(struct drm_dp_mst_topology_mgr *mgr)
{
	struct drm_dp_sideband_msg_tx *txmsg;
	int ret;

	WARN_ON(!mutex_is_locked(&mgr->qlock));

	 
	if (list_empty(&mgr->tx_msg_downq))
		return;

	txmsg = list_first_entry(&mgr->tx_msg_downq,
				 struct drm_dp_sideband_msg_tx, next);
	ret = process_single_tx_qlock(mgr, txmsg, false);
	if (ret < 0) {
		drm_dbg_kms(mgr->dev, "failed to send msg in q %d\n", ret);
		list_del(&txmsg->next);
		txmsg->state = DRM_DP_SIDEBAND_TX_TIMEOUT;
		wake_up_all(&mgr->tx_waitq);
	}
}

static void drm_dp_queue_down_tx(struct drm_dp_mst_topology_mgr *mgr,
				 struct drm_dp_sideband_msg_tx *txmsg)
{
	mutex_lock(&mgr->qlock);
	list_add_tail(&txmsg->next, &mgr->tx_msg_downq);

	if (drm_debug_enabled(DRM_UT_DP)) {
		struct drm_printer p = drm_debug_printer(DBG_PREFIX);

		drm_dp_mst_dump_sideband_msg_tx(&p, txmsg);
	}

	if (list_is_singular(&mgr->tx_msg_downq))
		process_single_down_tx_qlock(mgr);
	mutex_unlock(&mgr->qlock);
}

static void
drm_dp_dump_link_address(const struct drm_dp_mst_topology_mgr *mgr,
			 struct drm_dp_link_address_ack_reply *reply)
{
	struct drm_dp_link_addr_reply_port *port_reply;
	int i;

	for (i = 0; i < reply->nports; i++) {
		port_reply = &reply->ports[i];
		drm_dbg_kms(mgr->dev,
			    "port %d: input %d, pdt: %d, pn: %d, dpcd_rev: %02x, mcs: %d, ddps: %d, ldps %d, sdp %d/%d\n",
			    i,
			    port_reply->input_port,
			    port_reply->peer_device_type,
			    port_reply->port_number,
			    port_reply->dpcd_revision,
			    port_reply->mcs,
			    port_reply->ddps,
			    port_reply->legacy_device_plug_status,
			    port_reply->num_sdp_streams,
			    port_reply->num_sdp_stream_sinks);
	}
}

static int drm_dp_send_link_address(struct drm_dp_mst_topology_mgr *mgr,
				     struct drm_dp_mst_branch *mstb)
{
	struct drm_dp_sideband_msg_tx *txmsg;
	struct drm_dp_link_address_ack_reply *reply;
	struct drm_dp_mst_port *port, *tmp;
	int i, ret, port_mask = 0;
	bool changed = false;

	txmsg = kzalloc(sizeof(*txmsg), GFP_KERNEL);
	if (!txmsg)
		return -ENOMEM;

	txmsg->dst = mstb;
	build_link_address(txmsg);

	mstb->link_address_sent = true;
	drm_dp_queue_down_tx(mgr, txmsg);

	 
	ret = drm_dp_mst_wait_tx_reply(mstb, txmsg);
	if (ret <= 0) {
		drm_err(mgr->dev, "Sending link address failed with %d\n", ret);
		goto out;
	}
	if (txmsg->reply.reply_type == DP_SIDEBAND_REPLY_NAK) {
		drm_err(mgr->dev, "link address NAK received\n");
		ret = -EIO;
		goto out;
	}

	reply = &txmsg->reply.u.link_addr;
	drm_dbg_kms(mgr->dev, "link address reply: %d\n", reply->nports);
	drm_dp_dump_link_address(mgr, reply);

	ret = drm_dp_check_mstb_guid(mstb, reply->guid);
	if (ret) {
		char buf[64];

		drm_dp_mst_rad_to_str(mstb->rad, mstb->lct, buf, sizeof(buf));
		drm_err(mgr->dev, "GUID check on %s failed: %d\n", buf, ret);
		goto out;
	}

	for (i = 0; i < reply->nports; i++) {
		port_mask |= BIT(reply->ports[i].port_number);
		ret = drm_dp_mst_handle_link_address_port(mstb, mgr->dev,
							  &reply->ports[i]);
		if (ret == 1)
			changed = true;
		else if (ret < 0)
			goto out;
	}

	 
	mutex_lock(&mgr->lock);
	list_for_each_entry_safe(port, tmp, &mstb->ports, next) {
		if (port_mask & BIT(port->port_num))
			continue;

		drm_dbg_kms(mgr->dev, "port %d was not in link address, removing\n",
			    port->port_num);
		list_del(&port->next);
		drm_dp_mst_topology_put_port(port);
		changed = true;
	}
	mutex_unlock(&mgr->lock);

out:
	if (ret <= 0)
		mstb->link_address_sent = false;
	kfree(txmsg);
	return ret < 0 ? ret : changed;
}

static void
drm_dp_send_clear_payload_id_table(struct drm_dp_mst_topology_mgr *mgr,
				   struct drm_dp_mst_branch *mstb)
{
	struct drm_dp_sideband_msg_tx *txmsg;
	int ret;

	txmsg = kzalloc(sizeof(*txmsg), GFP_KERNEL);
	if (!txmsg)
		return;

	txmsg->dst = mstb;
	build_clear_payload_id_table(txmsg);

	drm_dp_queue_down_tx(mgr, txmsg);

	ret = drm_dp_mst_wait_tx_reply(mstb, txmsg);
	if (ret > 0 && txmsg->reply.reply_type == DP_SIDEBAND_REPLY_NAK)
		drm_dbg_kms(mgr->dev, "clear payload table id nak received\n");

	kfree(txmsg);
}

static int
drm_dp_send_enum_path_resources(struct drm_dp_mst_topology_mgr *mgr,
				struct drm_dp_mst_branch *mstb,
				struct drm_dp_mst_port *port)
{
	struct drm_dp_enum_path_resources_ack_reply *path_res;
	struct drm_dp_sideband_msg_tx *txmsg;
	int ret;

	txmsg = kzalloc(sizeof(*txmsg), GFP_KERNEL);
	if (!txmsg)
		return -ENOMEM;

	txmsg->dst = mstb;
	build_enum_path_resources(txmsg, port->port_num);

	drm_dp_queue_down_tx(mgr, txmsg);

	ret = drm_dp_mst_wait_tx_reply(mstb, txmsg);
	if (ret > 0) {
		ret = 0;
		path_res = &txmsg->reply.u.path_resources;

		if (txmsg->reply.reply_type == DP_SIDEBAND_REPLY_NAK) {
			drm_dbg_kms(mgr->dev, "enum path resources nak received\n");
		} else {
			if (port->port_num != path_res->port_number)
				DRM_ERROR("got incorrect port in response\n");

			drm_dbg_kms(mgr->dev, "enum path resources %d: %d %d\n",
				    path_res->port_number,
				    path_res->full_payload_bw_number,
				    path_res->avail_payload_bw_number);

			 
			if (port->full_pbn != path_res->full_payload_bw_number ||
			    port->fec_capable != path_res->fec_capable)
				ret = 1;

			port->full_pbn = path_res->full_payload_bw_number;
			port->fec_capable = path_res->fec_capable;
		}
	}

	kfree(txmsg);
	return ret;
}

static struct drm_dp_mst_port *drm_dp_get_last_connected_port_to_mstb(struct drm_dp_mst_branch *mstb)
{
	if (!mstb->port_parent)
		return NULL;

	if (mstb->port_parent->mstb != mstb)
		return mstb->port_parent;

	return drm_dp_get_last_connected_port_to_mstb(mstb->port_parent->parent);
}

 
static struct drm_dp_mst_branch *
drm_dp_get_last_connected_port_and_mstb(struct drm_dp_mst_topology_mgr *mgr,
					struct drm_dp_mst_branch *mstb,
					int *port_num)
{
	struct drm_dp_mst_branch *rmstb = NULL;
	struct drm_dp_mst_port *found_port;

	mutex_lock(&mgr->lock);
	if (!mgr->mst_primary)
		goto out;

	do {
		found_port = drm_dp_get_last_connected_port_to_mstb(mstb);
		if (!found_port)
			break;

		if (drm_dp_mst_topology_try_get_mstb(found_port->parent)) {
			rmstb = found_port->parent;
			*port_num = found_port->port_num;
		} else {
			 
			mstb = found_port->parent;
		}
	} while (!rmstb);
out:
	mutex_unlock(&mgr->lock);
	return rmstb;
}

static int drm_dp_payload_send_msg(struct drm_dp_mst_topology_mgr *mgr,
				   struct drm_dp_mst_port *port,
				   int id,
				   int pbn)
{
	struct drm_dp_sideband_msg_tx *txmsg;
	struct drm_dp_mst_branch *mstb;
	int ret, port_num;
	u8 sinks[DRM_DP_MAX_SDP_STREAMS];
	int i;

	port_num = port->port_num;
	mstb = drm_dp_mst_topology_get_mstb_validated(mgr, port->parent);
	if (!mstb) {
		mstb = drm_dp_get_last_connected_port_and_mstb(mgr,
							       port->parent,
							       &port_num);

		if (!mstb)
			return -EINVAL;
	}

	txmsg = kzalloc(sizeof(*txmsg), GFP_KERNEL);
	if (!txmsg) {
		ret = -ENOMEM;
		goto fail_put;
	}

	for (i = 0; i < port->num_sdp_streams; i++)
		sinks[i] = i;

	txmsg->dst = mstb;
	build_allocate_payload(txmsg, port_num,
			       id,
			       pbn, port->num_sdp_streams, sinks);

	drm_dp_queue_down_tx(mgr, txmsg);

	 
	ret = drm_dp_mst_wait_tx_reply(mstb, txmsg);
	if (ret > 0) {
		if (txmsg->reply.reply_type == DP_SIDEBAND_REPLY_NAK)
			ret = -EINVAL;
		else
			ret = 0;
	}
	kfree(txmsg);
fail_put:
	drm_dp_mst_topology_put_mstb(mstb);
	return ret;
}

int drm_dp_send_power_updown_phy(struct drm_dp_mst_topology_mgr *mgr,
				 struct drm_dp_mst_port *port, bool power_up)
{
	struct drm_dp_sideband_msg_tx *txmsg;
	int ret;

	port = drm_dp_mst_topology_get_port_validated(mgr, port);
	if (!port)
		return -EINVAL;

	txmsg = kzalloc(sizeof(*txmsg), GFP_KERNEL);
	if (!txmsg) {
		drm_dp_mst_topology_put_port(port);
		return -ENOMEM;
	}

	txmsg->dst = port->parent;
	build_power_updown_phy(txmsg, port->port_num, power_up);
	drm_dp_queue_down_tx(mgr, txmsg);

	ret = drm_dp_mst_wait_tx_reply(port->parent, txmsg);
	if (ret > 0) {
		if (txmsg->reply.reply_type == DP_SIDEBAND_REPLY_NAK)
			ret = -EINVAL;
		else
			ret = 0;
	}
	kfree(txmsg);
	drm_dp_mst_topology_put_port(port);

	return ret;
}
EXPORT_SYMBOL(drm_dp_send_power_updown_phy);

int drm_dp_send_query_stream_enc_status(struct drm_dp_mst_topology_mgr *mgr,
		struct drm_dp_mst_port *port,
		struct drm_dp_query_stream_enc_status_ack_reply *status)
{
	struct drm_dp_mst_topology_state *state;
	struct drm_dp_mst_atomic_payload *payload;
	struct drm_dp_sideband_msg_tx *txmsg;
	u8 nonce[7];
	int ret;

	txmsg = kzalloc(sizeof(*txmsg), GFP_KERNEL);
	if (!txmsg)
		return -ENOMEM;

	port = drm_dp_mst_topology_get_port_validated(mgr, port);
	if (!port) {
		ret = -EINVAL;
		goto out_get_port;
	}

	get_random_bytes(nonce, sizeof(nonce));

	drm_modeset_lock(&mgr->base.lock, NULL);
	state = to_drm_dp_mst_topology_state(mgr->base.state);
	payload = drm_atomic_get_mst_payload_state(state, port);

	 
	txmsg->dst = mgr->mst_primary;

	build_query_stream_enc_status(txmsg, payload->vcpi, nonce);

	drm_dp_queue_down_tx(mgr, txmsg);

	ret = drm_dp_mst_wait_tx_reply(mgr->mst_primary, txmsg);
	if (ret < 0) {
		goto out;
	} else if (txmsg->reply.reply_type == DP_SIDEBAND_REPLY_NAK) {
		drm_dbg_kms(mgr->dev, "query encryption status nak received\n");
		ret = -ENXIO;
		goto out;
	}

	ret = 0;
	memcpy(status, &txmsg->reply.u.enc_status, sizeof(*status));

out:
	drm_modeset_unlock(&mgr->base.lock);
	drm_dp_mst_topology_put_port(port);
out_get_port:
	kfree(txmsg);
	return ret;
}
EXPORT_SYMBOL(drm_dp_send_query_stream_enc_status);

static int drm_dp_create_payload_step1(struct drm_dp_mst_topology_mgr *mgr,
				       struct drm_dp_mst_atomic_payload *payload)
{
	return drm_dp_dpcd_write_payload(mgr, payload->vcpi, payload->vc_start_slot,
					 payload->time_slots);
}

static int drm_dp_create_payload_step2(struct drm_dp_mst_topology_mgr *mgr,
				       struct drm_dp_mst_atomic_payload *payload)
{
	int ret;
	struct drm_dp_mst_port *port = drm_dp_mst_topology_get_port_validated(mgr, payload->port);

	if (!port)
		return -EIO;

	ret = drm_dp_payload_send_msg(mgr, port, payload->vcpi, payload->pbn);
	drm_dp_mst_topology_put_port(port);
	return ret;
}

static int drm_dp_destroy_payload_step1(struct drm_dp_mst_topology_mgr *mgr,
					struct drm_dp_mst_topology_state *mst_state,
					struct drm_dp_mst_atomic_payload *payload)
{
	drm_dbg_kms(mgr->dev, "\n");

	 
	drm_dp_payload_send_msg(mgr, payload->port, payload->vcpi, 0);
	drm_dp_dpcd_write_payload(mgr, payload->vcpi, payload->vc_start_slot, 0);

	return 0;
}

 
int drm_dp_add_payload_part1(struct drm_dp_mst_topology_mgr *mgr,
			     struct drm_dp_mst_topology_state *mst_state,
			     struct drm_dp_mst_atomic_payload *payload)
{
	struct drm_dp_mst_port *port;
	int ret;

	port = drm_dp_mst_topology_get_port_validated(mgr, payload->port);
	if (!port) {
		drm_dbg_kms(mgr->dev,
			    "VCPI %d for port %p not in topology, not creating a payload\n",
			    payload->vcpi, payload->port);
		payload->vc_start_slot = -1;
		return 0;
	}

	if (mgr->payload_count == 0)
		mgr->next_start_slot = mst_state->start_slot;

	payload->vc_start_slot = mgr->next_start_slot;

	ret = drm_dp_create_payload_step1(mgr, payload);
	drm_dp_mst_topology_put_port(port);
	if (ret < 0) {
		drm_warn(mgr->dev, "Failed to create MST payload for port %p: %d\n",
			 payload->port, ret);
		payload->vc_start_slot = -1;
		return ret;
	}

	mgr->payload_count++;
	mgr->next_start_slot += payload->time_slots;

	return 0;
}
EXPORT_SYMBOL(drm_dp_add_payload_part1);

 
void drm_dp_remove_payload(struct drm_dp_mst_topology_mgr *mgr,
			   struct drm_dp_mst_topology_state *mst_state,
			   const struct drm_dp_mst_atomic_payload *old_payload,
			   struct drm_dp_mst_atomic_payload *new_payload)
{
	struct drm_dp_mst_atomic_payload *pos;
	bool send_remove = false;

	 
	if (new_payload->vc_start_slot == -1)
		return;

	mutex_lock(&mgr->lock);
	send_remove = drm_dp_mst_port_downstream_of_branch(new_payload->port, mgr->mst_primary);
	mutex_unlock(&mgr->lock);

	if (send_remove)
		drm_dp_destroy_payload_step1(mgr, mst_state, new_payload);
	else
		drm_dbg_kms(mgr->dev, "Payload for VCPI %d not in topology, not sending remove\n",
			    new_payload->vcpi);

	list_for_each_entry(pos, &mst_state->payloads, next) {
		if (pos != new_payload && pos->vc_start_slot > new_payload->vc_start_slot)
			pos->vc_start_slot -= old_payload->time_slots;
	}
	new_payload->vc_start_slot = -1;

	mgr->payload_count--;
	mgr->next_start_slot -= old_payload->time_slots;

	if (new_payload->delete)
		drm_dp_mst_put_port_malloc(new_payload->port);
}
EXPORT_SYMBOL(drm_dp_remove_payload);

 
int drm_dp_add_payload_part2(struct drm_dp_mst_topology_mgr *mgr,
			     struct drm_atomic_state *state,
			     struct drm_dp_mst_atomic_payload *payload)
{
	int ret = 0;

	 
	if (payload->vc_start_slot == -1) {
		drm_dbg_kms(mgr->dev, "Part 1 of payload creation for %s failed, skipping part 2\n",
			    payload->port->connector->name);
		return -EIO;
	}

	ret = drm_dp_create_payload_step2(mgr, payload);
	if (ret < 0) {
		if (!payload->delete)
			drm_err(mgr->dev, "Step 2 of creating MST payload for %p failed: %d\n",
				payload->port, ret);
		else
			drm_dbg_kms(mgr->dev, "Step 2 of removing MST payload for %p failed: %d\n",
				    payload->port, ret);
	}

	return ret;
}
EXPORT_SYMBOL(drm_dp_add_payload_part2);

static int drm_dp_send_dpcd_read(struct drm_dp_mst_topology_mgr *mgr,
				 struct drm_dp_mst_port *port,
				 int offset, int size, u8 *bytes)
{
	int ret = 0;
	struct drm_dp_sideband_msg_tx *txmsg;
	struct drm_dp_mst_branch *mstb;

	mstb = drm_dp_mst_topology_get_mstb_validated(mgr, port->parent);
	if (!mstb)
		return -EINVAL;

	txmsg = kzalloc(sizeof(*txmsg), GFP_KERNEL);
	if (!txmsg) {
		ret = -ENOMEM;
		goto fail_put;
	}

	build_dpcd_read(txmsg, port->port_num, offset, size);
	txmsg->dst = port->parent;

	drm_dp_queue_down_tx(mgr, txmsg);

	ret = drm_dp_mst_wait_tx_reply(mstb, txmsg);
	if (ret < 0)
		goto fail_free;

	if (txmsg->reply.reply_type == 1) {
		drm_dbg_kms(mgr->dev, "mstb %p port %d: DPCD read on addr 0x%x for %d bytes NAKed\n",
			    mstb, port->port_num, offset, size);
		ret = -EIO;
		goto fail_free;
	}

	if (txmsg->reply.u.remote_dpcd_read_ack.num_bytes != size) {
		ret = -EPROTO;
		goto fail_free;
	}

	ret = min_t(size_t, txmsg->reply.u.remote_dpcd_read_ack.num_bytes,
		    size);
	memcpy(bytes, txmsg->reply.u.remote_dpcd_read_ack.bytes, ret);

fail_free:
	kfree(txmsg);
fail_put:
	drm_dp_mst_topology_put_mstb(mstb);

	return ret;
}

static int drm_dp_send_dpcd_write(struct drm_dp_mst_topology_mgr *mgr,
				  struct drm_dp_mst_port *port,
				  int offset, int size, u8 *bytes)
{
	int ret;
	struct drm_dp_sideband_msg_tx *txmsg;
	struct drm_dp_mst_branch *mstb;

	mstb = drm_dp_mst_topology_get_mstb_validated(mgr, port->parent);
	if (!mstb)
		return -EINVAL;

	txmsg = kzalloc(sizeof(*txmsg), GFP_KERNEL);
	if (!txmsg) {
		ret = -ENOMEM;
		goto fail_put;
	}

	build_dpcd_write(txmsg, port->port_num, offset, size, bytes);
	txmsg->dst = mstb;

	drm_dp_queue_down_tx(mgr, txmsg);

	ret = drm_dp_mst_wait_tx_reply(mstb, txmsg);
	if (ret > 0) {
		if (txmsg->reply.reply_type == DP_SIDEBAND_REPLY_NAK)
			ret = -EIO;
		else
			ret = size;
	}

	kfree(txmsg);
fail_put:
	drm_dp_mst_topology_put_mstb(mstb);
	return ret;
}

static int drm_dp_encode_up_ack_reply(struct drm_dp_sideband_msg_tx *msg, u8 req_type)
{
	struct drm_dp_sideband_msg_reply_body reply;

	reply.reply_type = DP_SIDEBAND_REPLY_ACK;
	reply.req_type = req_type;
	drm_dp_encode_sideband_reply(&reply, msg);
	return 0;
}

static int drm_dp_send_up_ack_reply(struct drm_dp_mst_topology_mgr *mgr,
				    struct drm_dp_mst_branch *mstb,
				    int req_type, bool broadcast)
{
	struct drm_dp_sideband_msg_tx *txmsg;

	txmsg = kzalloc(sizeof(*txmsg), GFP_KERNEL);
	if (!txmsg)
		return -ENOMEM;

	txmsg->dst = mstb;
	drm_dp_encode_up_ack_reply(txmsg, req_type);

	mutex_lock(&mgr->qlock);
	 
	process_single_tx_qlock(mgr, txmsg, true);
	mutex_unlock(&mgr->qlock);

	kfree(txmsg);
	return 0;
}

 
int drm_dp_get_vc_payload_bw(const struct drm_dp_mst_topology_mgr *mgr,
			     int link_rate, int link_lane_count)
{
	if (link_rate == 0 || link_lane_count == 0)
		drm_dbg_kms(mgr->dev, "invalid link rate/lane count: (%d / %d)\n",
			    link_rate, link_lane_count);

	 
	return link_rate * link_lane_count / 54000;
}
EXPORT_SYMBOL(drm_dp_get_vc_payload_bw);

 
bool drm_dp_read_mst_cap(struct drm_dp_aux *aux,
			 const u8 dpcd[DP_RECEIVER_CAP_SIZE])
{
	u8 mstm_cap;

	if (dpcd[DP_DPCD_REV] < DP_DPCD_REV_12)
		return false;

	if (drm_dp_dpcd_readb(aux, DP_MSTM_CAP, &mstm_cap) != 1)
		return false;

	return mstm_cap & DP_MST_CAP;
}
EXPORT_SYMBOL(drm_dp_read_mst_cap);

 
int drm_dp_mst_topology_mgr_set_mst(struct drm_dp_mst_topology_mgr *mgr, bool mst_state)
{
	int ret = 0;
	struct drm_dp_mst_branch *mstb = NULL;

	mutex_lock(&mgr->lock);
	if (mst_state == mgr->mst_state)
		goto out_unlock;

	mgr->mst_state = mst_state;
	 
	if (mst_state) {
		WARN_ON(mgr->mst_primary);

		 
		ret = drm_dp_read_dpcd_caps(mgr->aux, mgr->dpcd);
		if (ret < 0) {
			drm_dbg_kms(mgr->dev, "%s: failed to read DPCD, ret %d\n",
				    mgr->aux->name, ret);
			goto out_unlock;
		}

		 
		mstb = drm_dp_add_mst_branch_device(1, NULL);
		if (mstb == NULL) {
			ret = -ENOMEM;
			goto out_unlock;
		}
		mstb->mgr = mgr;

		 
		mgr->mst_primary = mstb;
		drm_dp_mst_topology_get_mstb(mgr->mst_primary);

		ret = drm_dp_dpcd_writeb(mgr->aux, DP_MSTM_CTRL,
					 DP_MST_EN |
					 DP_UP_REQ_EN |
					 DP_UPSTREAM_IS_SRC);
		if (ret < 0)
			goto out_unlock;

		 
		drm_dp_dpcd_write_payload(mgr, 0, 0, 0x3f);

		queue_work(system_long_wq, &mgr->work);

		ret = 0;
	} else {
		 
		mstb = mgr->mst_primary;
		mgr->mst_primary = NULL;
		 
		drm_dp_dpcd_writeb(mgr->aux, DP_MSTM_CTRL, 0);
		ret = 0;
		mgr->payload_id_table_cleared = false;

		memset(&mgr->down_rep_recv, 0, sizeof(mgr->down_rep_recv));
		memset(&mgr->up_req_recv, 0, sizeof(mgr->up_req_recv));
	}

out_unlock:
	mutex_unlock(&mgr->lock);
	if (mstb)
		drm_dp_mst_topology_put_mstb(mstb);
	return ret;

}
EXPORT_SYMBOL(drm_dp_mst_topology_mgr_set_mst);

static void
drm_dp_mst_topology_mgr_invalidate_mstb(struct drm_dp_mst_branch *mstb)
{
	struct drm_dp_mst_port *port;

	 
	mstb->link_address_sent = false;

	list_for_each_entry(port, &mstb->ports, next)
		if (port->mstb)
			drm_dp_mst_topology_mgr_invalidate_mstb(port->mstb);
}

 
void drm_dp_mst_topology_mgr_suspend(struct drm_dp_mst_topology_mgr *mgr)
{
	mutex_lock(&mgr->lock);
	drm_dp_dpcd_writeb(mgr->aux, DP_MSTM_CTRL,
			   DP_MST_EN | DP_UPSTREAM_IS_SRC);
	mutex_unlock(&mgr->lock);
	flush_work(&mgr->up_req_work);
	flush_work(&mgr->work);
	flush_work(&mgr->delayed_destroy_work);

	mutex_lock(&mgr->lock);
	if (mgr->mst_state && mgr->mst_primary)
		drm_dp_mst_topology_mgr_invalidate_mstb(mgr->mst_primary);
	mutex_unlock(&mgr->lock);
}
EXPORT_SYMBOL(drm_dp_mst_topology_mgr_suspend);

 
int drm_dp_mst_topology_mgr_resume(struct drm_dp_mst_topology_mgr *mgr,
				   bool sync)
{
	int ret;
	u8 guid[16];

	mutex_lock(&mgr->lock);
	if (!mgr->mst_primary)
		goto out_fail;

	if (drm_dp_read_dpcd_caps(mgr->aux, mgr->dpcd) < 0) {
		drm_dbg_kms(mgr->dev, "dpcd read failed - undocked during suspend?\n");
		goto out_fail;
	}

	ret = drm_dp_dpcd_writeb(mgr->aux, DP_MSTM_CTRL,
				 DP_MST_EN |
				 DP_UP_REQ_EN |
				 DP_UPSTREAM_IS_SRC);
	if (ret < 0) {
		drm_dbg_kms(mgr->dev, "mst write failed - undocked during suspend?\n");
		goto out_fail;
	}

	 
	ret = drm_dp_dpcd_read(mgr->aux, DP_GUID, guid, 16);
	if (ret != 16) {
		drm_dbg_kms(mgr->dev, "dpcd read failed - undocked during suspend?\n");
		goto out_fail;
	}

	ret = drm_dp_check_mstb_guid(mgr->mst_primary, guid);
	if (ret) {
		drm_dbg_kms(mgr->dev, "check mstb failed - undocked during suspend?\n");
		goto out_fail;
	}

	 
	queue_work(system_long_wq, &mgr->work);
	mutex_unlock(&mgr->lock);

	if (sync) {
		drm_dbg_kms(mgr->dev,
			    "Waiting for link probe work to finish re-syncing topology...\n");
		flush_work(&mgr->work);
	}

	return 0;

out_fail:
	mutex_unlock(&mgr->lock);
	return -1;
}
EXPORT_SYMBOL(drm_dp_mst_topology_mgr_resume);

static bool
drm_dp_get_one_sb_msg(struct drm_dp_mst_topology_mgr *mgr, bool up,
		      struct drm_dp_mst_branch **mstb)
{
	int len;
	u8 replyblock[32];
	int replylen, curreply;
	int ret;
	u8 hdrlen;
	struct drm_dp_sideband_msg_hdr hdr;
	struct drm_dp_sideband_msg_rx *msg =
		up ? &mgr->up_req_recv : &mgr->down_rep_recv;
	int basereg = up ? DP_SIDEBAND_MSG_UP_REQ_BASE :
			   DP_SIDEBAND_MSG_DOWN_REP_BASE;

	if (!up)
		*mstb = NULL;

	len = min(mgr->max_dpcd_transaction_bytes, 16);
	ret = drm_dp_dpcd_read(mgr->aux, basereg, replyblock, len);
	if (ret != len) {
		drm_dbg_kms(mgr->dev, "failed to read DPCD down rep %d %d\n", len, ret);
		return false;
	}

	ret = drm_dp_decode_sideband_msg_hdr(mgr, &hdr, replyblock, len, &hdrlen);
	if (ret == false) {
		print_hex_dump(KERN_DEBUG, "failed hdr", DUMP_PREFIX_NONE, 16,
			       1, replyblock, len, false);
		drm_dbg_kms(mgr->dev, "ERROR: failed header\n");
		return false;
	}

	if (!up) {
		 
		*mstb = drm_dp_get_mst_branch_device(mgr, hdr.lct, hdr.rad);
		if (!*mstb) {
			drm_dbg_kms(mgr->dev, "Got MST reply from unknown device %d\n", hdr.lct);
			return false;
		}
	}

	if (!drm_dp_sideband_msg_set_header(msg, &hdr, hdrlen)) {
		drm_dbg_kms(mgr->dev, "sideband msg set header failed %d\n", replyblock[0]);
		return false;
	}

	replylen = min(msg->curchunk_len, (u8)(len - hdrlen));
	ret = drm_dp_sideband_append_payload(msg, replyblock + hdrlen, replylen);
	if (!ret) {
		drm_dbg_kms(mgr->dev, "sideband msg build failed %d\n", replyblock[0]);
		return false;
	}

	replylen = msg->curchunk_len + msg->curchunk_hdrlen - len;
	curreply = len;
	while (replylen > 0) {
		len = min3(replylen, mgr->max_dpcd_transaction_bytes, 16);
		ret = drm_dp_dpcd_read(mgr->aux, basereg + curreply,
				    replyblock, len);
		if (ret != len) {
			drm_dbg_kms(mgr->dev, "failed to read a chunk (len %d, ret %d)\n",
				    len, ret);
			return false;
		}

		ret = drm_dp_sideband_append_payload(msg, replyblock, len);
		if (!ret) {
			drm_dbg_kms(mgr->dev, "failed to build sideband msg\n");
			return false;
		}

		curreply += len;
		replylen -= len;
	}
	return true;
}

static int drm_dp_mst_handle_down_rep(struct drm_dp_mst_topology_mgr *mgr)
{
	struct drm_dp_sideband_msg_tx *txmsg;
	struct drm_dp_mst_branch *mstb = NULL;
	struct drm_dp_sideband_msg_rx *msg = &mgr->down_rep_recv;

	if (!drm_dp_get_one_sb_msg(mgr, false, &mstb))
		goto out_clear_reply;

	 
	if (!msg->have_eomt)
		goto out;

	 
	mutex_lock(&mgr->qlock);
	txmsg = list_first_entry_or_null(&mgr->tx_msg_downq,
					 struct drm_dp_sideband_msg_tx, next);
	mutex_unlock(&mgr->qlock);

	 
	if (!txmsg || txmsg->dst != mstb) {
		struct drm_dp_sideband_msg_hdr *hdr;

		hdr = &msg->initial_hdr;
		drm_dbg_kms(mgr->dev, "Got MST reply with no msg %p %d %d %02x %02x\n",
			    mstb, hdr->seqno, hdr->lct, hdr->rad[0], msg->msg[0]);
		goto out_clear_reply;
	}

	drm_dp_sideband_parse_reply(mgr, msg, &txmsg->reply);

	if (txmsg->reply.reply_type == DP_SIDEBAND_REPLY_NAK) {
		drm_dbg_kms(mgr->dev,
			    "Got NAK reply: req 0x%02x (%s), reason 0x%02x (%s), nak data 0x%02x\n",
			    txmsg->reply.req_type,
			    drm_dp_mst_req_type_str(txmsg->reply.req_type),
			    txmsg->reply.u.nak.reason,
			    drm_dp_mst_nak_reason_str(txmsg->reply.u.nak.reason),
			    txmsg->reply.u.nak.nak_data);
	}

	memset(msg, 0, sizeof(struct drm_dp_sideband_msg_rx));
	drm_dp_mst_topology_put_mstb(mstb);

	mutex_lock(&mgr->qlock);
	txmsg->state = DRM_DP_SIDEBAND_TX_RX;
	list_del(&txmsg->next);
	mutex_unlock(&mgr->qlock);

	wake_up_all(&mgr->tx_waitq);

	return 0;

out_clear_reply:
	memset(msg, 0, sizeof(struct drm_dp_sideband_msg_rx));
out:
	if (mstb)
		drm_dp_mst_topology_put_mstb(mstb);

	return 0;
}

static inline bool
drm_dp_mst_process_up_req(struct drm_dp_mst_topology_mgr *mgr,
			  struct drm_dp_pending_up_req *up_req)
{
	struct drm_dp_mst_branch *mstb = NULL;
	struct drm_dp_sideband_msg_req_body *msg = &up_req->msg;
	struct drm_dp_sideband_msg_hdr *hdr = &up_req->hdr;
	bool hotplug = false, dowork = false;

	if (hdr->broadcast) {
		const u8 *guid = NULL;

		if (msg->req_type == DP_CONNECTION_STATUS_NOTIFY)
			guid = msg->u.conn_stat.guid;
		else if (msg->req_type == DP_RESOURCE_STATUS_NOTIFY)
			guid = msg->u.resource_stat.guid;

		if (guid)
			mstb = drm_dp_get_mst_branch_device_by_guid(mgr, guid);
	} else {
		mstb = drm_dp_get_mst_branch_device(mgr, hdr->lct, hdr->rad);
	}

	if (!mstb) {
		drm_dbg_kms(mgr->dev, "Got MST reply from unknown device %d\n", hdr->lct);
		return false;
	}

	 
	if (msg->req_type == DP_CONNECTION_STATUS_NOTIFY) {
		dowork = drm_dp_mst_handle_conn_stat(mstb, &msg->u.conn_stat);
		hotplug = true;
	}

	drm_dp_mst_topology_put_mstb(mstb);

	if (dowork)
		queue_work(system_long_wq, &mgr->work);
	return hotplug;
}

static void drm_dp_mst_up_req_work(struct work_struct *work)
{
	struct drm_dp_mst_topology_mgr *mgr =
		container_of(work, struct drm_dp_mst_topology_mgr,
			     up_req_work);
	struct drm_dp_pending_up_req *up_req;
	bool send_hotplug = false;

	mutex_lock(&mgr->probe_lock);
	while (true) {
		mutex_lock(&mgr->up_req_lock);
		up_req = list_first_entry_or_null(&mgr->up_req_list,
						  struct drm_dp_pending_up_req,
						  next);
		if (up_req)
			list_del(&up_req->next);
		mutex_unlock(&mgr->up_req_lock);

		if (!up_req)
			break;

		send_hotplug |= drm_dp_mst_process_up_req(mgr, up_req);
		kfree(up_req);
	}
	mutex_unlock(&mgr->probe_lock);

	if (send_hotplug)
		drm_kms_helper_hotplug_event(mgr->dev);
}

static int drm_dp_mst_handle_up_req(struct drm_dp_mst_topology_mgr *mgr)
{
	struct drm_dp_pending_up_req *up_req;

	if (!drm_dp_get_one_sb_msg(mgr, true, NULL))
		goto out;

	if (!mgr->up_req_recv.have_eomt)
		return 0;

	up_req = kzalloc(sizeof(*up_req), GFP_KERNEL);
	if (!up_req)
		return -ENOMEM;

	INIT_LIST_HEAD(&up_req->next);

	drm_dp_sideband_parse_req(mgr, &mgr->up_req_recv, &up_req->msg);

	if (up_req->msg.req_type != DP_CONNECTION_STATUS_NOTIFY &&
	    up_req->msg.req_type != DP_RESOURCE_STATUS_NOTIFY) {
		drm_dbg_kms(mgr->dev, "Received unknown up req type, ignoring: %x\n",
			    up_req->msg.req_type);
		kfree(up_req);
		goto out;
	}

	drm_dp_send_up_ack_reply(mgr, mgr->mst_primary, up_req->msg.req_type,
				 false);

	if (up_req->msg.req_type == DP_CONNECTION_STATUS_NOTIFY) {
		const struct drm_dp_connection_status_notify *conn_stat =
			&up_req->msg.u.conn_stat;

		drm_dbg_kms(mgr->dev, "Got CSN: pn: %d ldps:%d ddps: %d mcs: %d ip: %d pdt: %d\n",
			    conn_stat->port_number,
			    conn_stat->legacy_device_plug_status,
			    conn_stat->displayport_device_plug_status,
			    conn_stat->message_capability_status,
			    conn_stat->input_port,
			    conn_stat->peer_device_type);
	} else if (up_req->msg.req_type == DP_RESOURCE_STATUS_NOTIFY) {
		const struct drm_dp_resource_status_notify *res_stat =
			&up_req->msg.u.resource_stat;

		drm_dbg_kms(mgr->dev, "Got RSN: pn: %d avail_pbn %d\n",
			    res_stat->port_number,
			    res_stat->available_pbn);
	}

	up_req->hdr = mgr->up_req_recv.initial_hdr;
	mutex_lock(&mgr->up_req_lock);
	list_add_tail(&up_req->next, &mgr->up_req_list);
	mutex_unlock(&mgr->up_req_lock);
	queue_work(system_long_wq, &mgr->up_req_work);

out:
	memset(&mgr->up_req_recv, 0, sizeof(struct drm_dp_sideband_msg_rx));
	return 0;
}

 
int drm_dp_mst_hpd_irq_handle_event(struct drm_dp_mst_topology_mgr *mgr, const u8 *esi,
				    u8 *ack, bool *handled)
{
	int ret = 0;
	int sc;
	*handled = false;
	sc = DP_GET_SINK_COUNT(esi[0]);

	if (sc != mgr->sink_count) {
		mgr->sink_count = sc;
		*handled = true;
	}

	if (esi[1] & DP_DOWN_REP_MSG_RDY) {
		ret = drm_dp_mst_handle_down_rep(mgr);
		*handled = true;
		ack[1] |= DP_DOWN_REP_MSG_RDY;
	}

	if (esi[1] & DP_UP_REQ_MSG_RDY) {
		ret |= drm_dp_mst_handle_up_req(mgr);
		*handled = true;
		ack[1] |= DP_UP_REQ_MSG_RDY;
	}

	return ret;
}
EXPORT_SYMBOL(drm_dp_mst_hpd_irq_handle_event);

 
void drm_dp_mst_hpd_irq_send_new_request(struct drm_dp_mst_topology_mgr *mgr)
{
	struct drm_dp_sideband_msg_tx *txmsg;
	bool kick = true;

	mutex_lock(&mgr->qlock);
	txmsg = list_first_entry_or_null(&mgr->tx_msg_downq,
					 struct drm_dp_sideband_msg_tx, next);
	 
	if (!txmsg ||
	    txmsg->state == DRM_DP_SIDEBAND_TX_START_SEND ||
	    txmsg->state == DRM_DP_SIDEBAND_TX_SENT)
		kick = false;
	mutex_unlock(&mgr->qlock);

	if (kick)
		drm_dp_mst_kick_tx(mgr);
}
EXPORT_SYMBOL(drm_dp_mst_hpd_irq_send_new_request);
 
int
drm_dp_mst_detect_port(struct drm_connector *connector,
		       struct drm_modeset_acquire_ctx *ctx,
		       struct drm_dp_mst_topology_mgr *mgr,
		       struct drm_dp_mst_port *port)
{
	int ret;

	 
	port = drm_dp_mst_topology_get_port_validated(mgr, port);
	if (!port)
		return connector_status_disconnected;

	ret = drm_modeset_lock(&mgr->base.lock, ctx);
	if (ret)
		goto out;

	ret = connector_status_disconnected;

	if (!port->ddps)
		goto out;

	switch (port->pdt) {
	case DP_PEER_DEVICE_NONE:
		break;
	case DP_PEER_DEVICE_MST_BRANCHING:
		if (!port->mcs)
			ret = connector_status_connected;
		break;

	case DP_PEER_DEVICE_SST_SINK:
		ret = connector_status_connected;
		 
		if (port->port_num >= DP_MST_LOGICAL_PORT_0 && !port->cached_edid)
			port->cached_edid = drm_edid_read_ddc(connector, &port->aux.ddc);
		break;
	case DP_PEER_DEVICE_DP_LEGACY_CONV:
		if (port->ldps)
			ret = connector_status_connected;
		break;
	}
out:
	drm_dp_mst_topology_put_port(port);
	return ret;
}
EXPORT_SYMBOL(drm_dp_mst_detect_port);

 
const struct drm_edid *drm_dp_mst_edid_read(struct drm_connector *connector,
					    struct drm_dp_mst_topology_mgr *mgr,
					    struct drm_dp_mst_port *port)
{
	const struct drm_edid *drm_edid;

	 
	port = drm_dp_mst_topology_get_port_validated(mgr, port);
	if (!port)
		return NULL;

	if (port->cached_edid)
		drm_edid = drm_edid_dup(port->cached_edid);
	else
		drm_edid = drm_edid_read_ddc(connector, &port->aux.ddc);

	drm_dp_mst_topology_put_port(port);

	return drm_edid;
}
EXPORT_SYMBOL(drm_dp_mst_edid_read);

 
struct edid *drm_dp_mst_get_edid(struct drm_connector *connector,
				 struct drm_dp_mst_topology_mgr *mgr,
				 struct drm_dp_mst_port *port)
{
	const struct drm_edid *drm_edid;
	struct edid *edid;

	drm_edid = drm_dp_mst_edid_read(connector, mgr, port);

	edid = drm_edid_duplicate(drm_edid_raw(drm_edid));

	drm_edid_free(drm_edid);

	return edid;
}
EXPORT_SYMBOL(drm_dp_mst_get_edid);

 
int drm_dp_atomic_find_time_slots(struct drm_atomic_state *state,
				  struct drm_dp_mst_topology_mgr *mgr,
				  struct drm_dp_mst_port *port, int pbn)
{
	struct drm_dp_mst_topology_state *topology_state;
	struct drm_dp_mst_atomic_payload *payload = NULL;
	struct drm_connector_state *conn_state;
	int prev_slots = 0, prev_bw = 0, req_slots;

	topology_state = drm_atomic_get_mst_topology_state(state, mgr);
	if (IS_ERR(topology_state))
		return PTR_ERR(topology_state);

	conn_state = drm_atomic_get_new_connector_state(state, port->connector);
	topology_state->pending_crtc_mask |= drm_crtc_mask(conn_state->crtc);

	 
	payload = drm_atomic_get_mst_payload_state(topology_state, port);
	if (payload) {
		prev_slots = payload->time_slots;
		prev_bw = payload->pbn;

		 
		if (drm_WARN_ON(mgr->dev, payload->delete)) {
			drm_err(mgr->dev,
				"cannot allocate and release time slots on [MST PORT:%p] in the same state\n",
				port);
			return -EINVAL;
		}
	}

	req_slots = DIV_ROUND_UP(pbn, topology_state->pbn_div);

	drm_dbg_atomic(mgr->dev, "[CONNECTOR:%d:%s] [MST PORT:%p] TU %d -> %d\n",
		       port->connector->base.id, port->connector->name,
		       port, prev_slots, req_slots);
	drm_dbg_atomic(mgr->dev, "[CONNECTOR:%d:%s] [MST PORT:%p] PBN %d -> %d\n",
		       port->connector->base.id, port->connector->name,
		       port, prev_bw, pbn);

	 
	if (!payload) {
		payload = kzalloc(sizeof(*payload), GFP_KERNEL);
		if (!payload)
			return -ENOMEM;

		drm_dp_mst_get_port_malloc(port);
		payload->port = port;
		payload->vc_start_slot = -1;
		list_add(&payload->next, &topology_state->payloads);
	}
	payload->time_slots = req_slots;
	payload->pbn = pbn;

	return req_slots;
}
EXPORT_SYMBOL(drm_dp_atomic_find_time_slots);

 
int drm_dp_atomic_release_time_slots(struct drm_atomic_state *state,
				     struct drm_dp_mst_topology_mgr *mgr,
				     struct drm_dp_mst_port *port)
{
	struct drm_dp_mst_topology_state *topology_state;
	struct drm_dp_mst_atomic_payload *payload;
	struct drm_connector_state *old_conn_state, *new_conn_state;
	bool update_payload = true;

	old_conn_state = drm_atomic_get_old_connector_state(state, port->connector);
	if (!old_conn_state->crtc)
		return 0;

	 
	new_conn_state = drm_atomic_get_new_connector_state(state, port->connector);
	if (new_conn_state->crtc) {
		struct drm_crtc_state *crtc_state =
			drm_atomic_get_new_crtc_state(state, new_conn_state->crtc);

		 
		if (!crtc_state || !drm_atomic_crtc_needs_modeset(crtc_state))
			return 0;

		if (!crtc_state->mode_changed && !crtc_state->connectors_changed)
			update_payload = false;
	}

	topology_state = drm_atomic_get_mst_topology_state(state, mgr);
	if (IS_ERR(topology_state))
		return PTR_ERR(topology_state);

	topology_state->pending_crtc_mask |= drm_crtc_mask(old_conn_state->crtc);
	if (!update_payload)
		return 0;

	payload = drm_atomic_get_mst_payload_state(topology_state, port);
	if (WARN_ON(!payload)) {
		drm_err(mgr->dev, "No payload for [MST PORT:%p] found in mst state %p\n",
			port, &topology_state->base);
		return -EINVAL;
	}

	if (new_conn_state->crtc)
		return 0;

	drm_dbg_atomic(mgr->dev, "[MST PORT:%p] TU %d -> 0\n", port, payload->time_slots);
	if (!payload->delete) {
		payload->pbn = 0;
		payload->delete = true;
		topology_state->payload_mask &= ~BIT(payload->vcpi - 1);
	}

	return 0;
}
EXPORT_SYMBOL(drm_dp_atomic_release_time_slots);

 
int drm_dp_mst_atomic_setup_commit(struct drm_atomic_state *state)
{
	struct drm_dp_mst_topology_mgr *mgr;
	struct drm_dp_mst_topology_state *mst_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	int i, j, commit_idx, num_commit_deps;

	for_each_new_mst_mgr_in_state(state, mgr, mst_state, i) {
		if (!mst_state->pending_crtc_mask)
			continue;

		num_commit_deps = hweight32(mst_state->pending_crtc_mask);
		mst_state->commit_deps = kmalloc_array(num_commit_deps,
						       sizeof(*mst_state->commit_deps), GFP_KERNEL);
		if (!mst_state->commit_deps)
			return -ENOMEM;
		mst_state->num_commit_deps = num_commit_deps;

		commit_idx = 0;
		for_each_new_crtc_in_state(state, crtc, crtc_state, j) {
			if (mst_state->pending_crtc_mask & drm_crtc_mask(crtc)) {
				mst_state->commit_deps[commit_idx++] =
					drm_crtc_commit_get(crtc_state->commit);
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL(drm_dp_mst_atomic_setup_commit);

 
void drm_dp_mst_atomic_wait_for_dependencies(struct drm_atomic_state *state)
{
	struct drm_dp_mst_topology_state *old_mst_state, *new_mst_state;
	struct drm_dp_mst_topology_mgr *mgr;
	struct drm_dp_mst_atomic_payload *old_payload, *new_payload;
	int i, j, ret;

	for_each_oldnew_mst_mgr_in_state(state, mgr, old_mst_state, new_mst_state, i) {
		for (j = 0; j < old_mst_state->num_commit_deps; j++) {
			ret = drm_crtc_commit_wait(old_mst_state->commit_deps[j]);
			if (ret < 0)
				drm_err(state->dev, "Failed to wait for %s: %d\n",
					old_mst_state->commit_deps[j]->crtc->name, ret);
		}

		 
		list_for_each_entry(old_payload, &old_mst_state->payloads, next) {
			if (old_payload->delete)
				continue;

			new_payload = drm_atomic_get_mst_payload_state(new_mst_state,
								       old_payload->port);
			new_payload->vc_start_slot = old_payload->vc_start_slot;
		}
	}
}
EXPORT_SYMBOL(drm_dp_mst_atomic_wait_for_dependencies);

 
int drm_dp_mst_root_conn_atomic_check(struct drm_connector_state *new_conn_state,
				      struct drm_dp_mst_topology_mgr *mgr)
{
	struct drm_atomic_state *state = new_conn_state->state;
	struct drm_connector_state *old_conn_state =
		drm_atomic_get_old_connector_state(state, new_conn_state->connector);
	struct drm_crtc_state *crtc_state;
	struct drm_dp_mst_topology_state *mst_state = NULL;

	if (new_conn_state->crtc) {
		crtc_state = drm_atomic_get_new_crtc_state(state, new_conn_state->crtc);
		if (crtc_state && drm_atomic_crtc_needs_modeset(crtc_state)) {
			mst_state = drm_atomic_get_mst_topology_state(state, mgr);
			if (IS_ERR(mst_state))
				return PTR_ERR(mst_state);

			mst_state->pending_crtc_mask |= drm_crtc_mask(new_conn_state->crtc);
		}
	}

	if (old_conn_state->crtc) {
		crtc_state = drm_atomic_get_new_crtc_state(state, old_conn_state->crtc);
		if (crtc_state && drm_atomic_crtc_needs_modeset(crtc_state)) {
			if (!mst_state) {
				mst_state = drm_atomic_get_mst_topology_state(state, mgr);
				if (IS_ERR(mst_state))
					return PTR_ERR(mst_state);
			}

			mst_state->pending_crtc_mask |= drm_crtc_mask(old_conn_state->crtc);
		}
	}

	return 0;
}
EXPORT_SYMBOL(drm_dp_mst_root_conn_atomic_check);

 
void drm_dp_mst_update_slots(struct drm_dp_mst_topology_state *mst_state, uint8_t link_encoding_cap)
{
	if (link_encoding_cap == DP_CAP_ANSI_128B132B) {
		mst_state->total_avail_slots = 64;
		mst_state->start_slot = 0;
	} else {
		mst_state->total_avail_slots = 63;
		mst_state->start_slot = 1;
	}

	DRM_DEBUG_KMS("%s encoding format on mst_state 0x%p\n",
		      (link_encoding_cap == DP_CAP_ANSI_128B132B) ? "128b/132b":"8b/10b",
		      mst_state);
}
EXPORT_SYMBOL(drm_dp_mst_update_slots);

static int drm_dp_dpcd_write_payload(struct drm_dp_mst_topology_mgr *mgr,
				     int id, u8 start_slot, u8 num_slots)
{
	u8 payload_alloc[3], status;
	int ret;
	int retries = 0;

	drm_dp_dpcd_writeb(mgr->aux, DP_PAYLOAD_TABLE_UPDATE_STATUS,
			   DP_PAYLOAD_TABLE_UPDATED);

	payload_alloc[0] = id;
	payload_alloc[1] = start_slot;
	payload_alloc[2] = num_slots;

	ret = drm_dp_dpcd_write(mgr->aux, DP_PAYLOAD_ALLOCATE_SET, payload_alloc, 3);
	if (ret != 3) {
		drm_dbg_kms(mgr->dev, "failed to write payload allocation %d\n", ret);
		goto fail;
	}

retry:
	ret = drm_dp_dpcd_readb(mgr->aux, DP_PAYLOAD_TABLE_UPDATE_STATUS, &status);
	if (ret < 0) {
		drm_dbg_kms(mgr->dev, "failed to read payload table status %d\n", ret);
		goto fail;
	}

	if (!(status & DP_PAYLOAD_TABLE_UPDATED)) {
		retries++;
		if (retries < 20) {
			usleep_range(10000, 20000);
			goto retry;
		}
		drm_dbg_kms(mgr->dev, "status not set after read payload table status %d\n",
			    status);
		ret = -EINVAL;
		goto fail;
	}
	ret = 0;
fail:
	return ret;
}

static int do_get_act_status(struct drm_dp_aux *aux)
{
	int ret;
	u8 status;

	ret = drm_dp_dpcd_readb(aux, DP_PAYLOAD_TABLE_UPDATE_STATUS, &status);
	if (ret < 0)
		return ret;

	return status;
}

 
int drm_dp_check_act_status(struct drm_dp_mst_topology_mgr *mgr)
{
	 
	const int timeout_ms = 3000;
	int ret, status;

	ret = readx_poll_timeout(do_get_act_status, mgr->aux, status,
				 status & DP_PAYLOAD_ACT_HANDLED || status < 0,
				 200, timeout_ms * USEC_PER_MSEC);
	if (ret < 0 && status >= 0) {
		drm_err(mgr->dev, "Failed to get ACT after %dms, last status: %02x\n",
			timeout_ms, status);
		return -EINVAL;
	} else if (status < 0) {
		 
		drm_dbg_kms(mgr->dev, "Failed to read payload table status: %d\n", status);
		return status;
	}

	return 0;
}
EXPORT_SYMBOL(drm_dp_check_act_status);

 
int drm_dp_calc_pbn_mode(int clock, int bpp)
{
	 
	return DIV_ROUND_UP_ULL(mul_u32_u32(clock * bpp, 64 * 1006 >> 4),
				1000 * 8 * 54 * 1000);
}
EXPORT_SYMBOL(drm_dp_calc_pbn_mode);

 
static void drm_dp_mst_kick_tx(struct drm_dp_mst_topology_mgr *mgr)
{
	queue_work(system_long_wq, &mgr->tx_work);
}

 
static const char *pdt_to_string(u8 pdt)
{
	switch (pdt) {
	case DP_PEER_DEVICE_NONE:
		return "NONE";
	case DP_PEER_DEVICE_SOURCE_OR_SST:
		return "SOURCE OR SST";
	case DP_PEER_DEVICE_MST_BRANCHING:
		return "MST BRANCHING";
	case DP_PEER_DEVICE_SST_SINK:
		return "SST SINK";
	case DP_PEER_DEVICE_DP_LEGACY_CONV:
		return "DP LEGACY CONV";
	default:
		return "ERR";
	}
}

static void drm_dp_mst_dump_mstb(struct seq_file *m,
				 struct drm_dp_mst_branch *mstb)
{
	struct drm_dp_mst_port *port;
	int tabs = mstb->lct;
	char prefix[10];
	int i;

	for (i = 0; i < tabs; i++)
		prefix[i] = '\t';
	prefix[i] = '\0';

	seq_printf(m, "%smstb - [%p]: num_ports: %d\n", prefix, mstb, mstb->num_ports);
	list_for_each_entry(port, &mstb->ports, next) {
		seq_printf(m, "%sport %d - [%p] (%s - %s): ddps: %d, ldps: %d, sdp: %d/%d, fec: %s, conn: %p\n",
			   prefix,
			   port->port_num,
			   port,
			   port->input ? "input" : "output",
			   pdt_to_string(port->pdt),
			   port->ddps,
			   port->ldps,
			   port->num_sdp_streams,
			   port->num_sdp_stream_sinks,
			   port->fec_capable ? "true" : "false",
			   port->connector);
		if (port->mstb)
			drm_dp_mst_dump_mstb(m, port->mstb);
	}
}

#define DP_PAYLOAD_TABLE_SIZE		64

static bool dump_dp_payload_table(struct drm_dp_mst_topology_mgr *mgr,
				  char *buf)
{
	int i;

	for (i = 0; i < DP_PAYLOAD_TABLE_SIZE; i += 16) {
		if (drm_dp_dpcd_read(mgr->aux,
				     DP_PAYLOAD_TABLE_UPDATE_STATUS + i,
				     &buf[i], 16) != 16)
			return false;
	}
	return true;
}

static void fetch_monitor_name(struct drm_dp_mst_topology_mgr *mgr,
			       struct drm_dp_mst_port *port, char *name,
			       int namelen)
{
	struct edid *mst_edid;

	mst_edid = drm_dp_mst_get_edid(port->connector, mgr, port);
	drm_edid_get_monitor_name(mst_edid, name, namelen);
	kfree(mst_edid);
}

 
void drm_dp_mst_dump_topology(struct seq_file *m,
			      struct drm_dp_mst_topology_mgr *mgr)
{
	struct drm_dp_mst_topology_state *state;
	struct drm_dp_mst_atomic_payload *payload;
	int i, ret;

	mutex_lock(&mgr->lock);
	if (mgr->mst_primary)
		drm_dp_mst_dump_mstb(m, mgr->mst_primary);

	 
	mutex_unlock(&mgr->lock);

	ret = drm_modeset_lock_single_interruptible(&mgr->base.lock);
	if (ret < 0)
		return;

	state = to_drm_dp_mst_topology_state(mgr->base.state);
	seq_printf(m, "\n*** Atomic state info ***\n");
	seq_printf(m, "payload_mask: %x, max_payloads: %d, start_slot: %u, pbn_div: %d\n",
		   state->payload_mask, mgr->max_payloads, state->start_slot, state->pbn_div);

	seq_printf(m, "\n| idx | port | vcpi | slots | pbn | dsc |     sink name     |\n");
	for (i = 0; i < mgr->max_payloads; i++) {
		list_for_each_entry(payload, &state->payloads, next) {
			char name[14];

			if (payload->vcpi != i || payload->delete)
				continue;

			fetch_monitor_name(mgr, payload->port, name, sizeof(name));
			seq_printf(m, " %5d %6d %6d %02d - %02d %5d %5s %19s\n",
				   i,
				   payload->port->port_num,
				   payload->vcpi,
				   payload->vc_start_slot,
				   payload->vc_start_slot + payload->time_slots - 1,
				   payload->pbn,
				   payload->dsc_enabled ? "Y" : "N",
				   (*name != 0) ? name : "Unknown");
		}
	}

	seq_printf(m, "\n*** DPCD Info ***\n");
	mutex_lock(&mgr->lock);
	if (mgr->mst_primary) {
		u8 buf[DP_PAYLOAD_TABLE_SIZE];
		int ret;

		if (drm_dp_read_dpcd_caps(mgr->aux, buf) < 0) {
			seq_printf(m, "dpcd read failed\n");
			goto out;
		}
		seq_printf(m, "dpcd: %*ph\n", DP_RECEIVER_CAP_SIZE, buf);

		ret = drm_dp_dpcd_read(mgr->aux, DP_FAUX_CAP, buf, 2);
		if (ret != 2) {
			seq_printf(m, "faux/mst read failed\n");
			goto out;
		}
		seq_printf(m, "faux/mst: %*ph\n", 2, buf);

		ret = drm_dp_dpcd_read(mgr->aux, DP_MSTM_CTRL, buf, 1);
		if (ret != 1) {
			seq_printf(m, "mst ctrl read failed\n");
			goto out;
		}
		seq_printf(m, "mst ctrl: %*ph\n", 1, buf);

		 
		ret = drm_dp_dpcd_read(mgr->aux, DP_BRANCH_OUI, buf, DP_BRANCH_OUI_HEADER_SIZE);
		if (ret != DP_BRANCH_OUI_HEADER_SIZE) {
			seq_printf(m, "branch oui read failed\n");
			goto out;
		}
		seq_printf(m, "branch oui: %*phN devid: ", 3, buf);

		for (i = 0x3; i < 0x8 && buf[i]; i++)
			seq_printf(m, "%c", buf[i]);
		seq_printf(m, " revision: hw: %x.%x sw: %x.%x\n",
			   buf[0x9] >> 4, buf[0x9] & 0xf, buf[0xa], buf[0xb]);
		if (dump_dp_payload_table(mgr, buf))
			seq_printf(m, "payload table: %*ph\n", DP_PAYLOAD_TABLE_SIZE, buf);
	}

out:
	mutex_unlock(&mgr->lock);
	drm_modeset_unlock(&mgr->base.lock);
}
EXPORT_SYMBOL(drm_dp_mst_dump_topology);

static void drm_dp_tx_work(struct work_struct *work)
{
	struct drm_dp_mst_topology_mgr *mgr = container_of(work, struct drm_dp_mst_topology_mgr, tx_work);

	mutex_lock(&mgr->qlock);
	if (!list_empty(&mgr->tx_msg_downq))
		process_single_down_tx_qlock(mgr);
	mutex_unlock(&mgr->qlock);
}

static inline void
drm_dp_delayed_destroy_port(struct drm_dp_mst_port *port)
{
	drm_dp_port_set_pdt(port, DP_PEER_DEVICE_NONE, port->mcs);

	if (port->connector) {
		drm_connector_unregister(port->connector);
		drm_connector_put(port->connector);
	}

	drm_dp_mst_put_port_malloc(port);
}

static inline void
drm_dp_delayed_destroy_mstb(struct drm_dp_mst_branch *mstb)
{
	struct drm_dp_mst_topology_mgr *mgr = mstb->mgr;
	struct drm_dp_mst_port *port, *port_tmp;
	struct drm_dp_sideband_msg_tx *txmsg, *txmsg_tmp;
	bool wake_tx = false;

	mutex_lock(&mgr->lock);
	list_for_each_entry_safe(port, port_tmp, &mstb->ports, next) {
		list_del(&port->next);
		drm_dp_mst_topology_put_port(port);
	}
	mutex_unlock(&mgr->lock);

	 
	mutex_lock(&mstb->mgr->qlock);
	list_for_each_entry_safe(txmsg, txmsg_tmp, &mgr->tx_msg_downq, next) {
		if (txmsg->dst != mstb)
			continue;

		txmsg->state = DRM_DP_SIDEBAND_TX_TIMEOUT;
		list_del(&txmsg->next);
		wake_tx = true;
	}
	mutex_unlock(&mstb->mgr->qlock);

	if (wake_tx)
		wake_up_all(&mstb->mgr->tx_waitq);

	drm_dp_mst_put_mstb_malloc(mstb);
}

static void drm_dp_delayed_destroy_work(struct work_struct *work)
{
	struct drm_dp_mst_topology_mgr *mgr =
		container_of(work, struct drm_dp_mst_topology_mgr,
			     delayed_destroy_work);
	bool send_hotplug = false, go_again;

	 
	do {
		go_again = false;

		for (;;) {
			struct drm_dp_mst_branch *mstb;

			mutex_lock(&mgr->delayed_destroy_lock);
			mstb = list_first_entry_or_null(&mgr->destroy_branch_device_list,
							struct drm_dp_mst_branch,
							destroy_next);
			if (mstb)
				list_del(&mstb->destroy_next);
			mutex_unlock(&mgr->delayed_destroy_lock);

			if (!mstb)
				break;

			drm_dp_delayed_destroy_mstb(mstb);
			go_again = true;
		}

		for (;;) {
			struct drm_dp_mst_port *port;

			mutex_lock(&mgr->delayed_destroy_lock);
			port = list_first_entry_or_null(&mgr->destroy_port_list,
							struct drm_dp_mst_port,
							next);
			if (port)
				list_del(&port->next);
			mutex_unlock(&mgr->delayed_destroy_lock);

			if (!port)
				break;

			drm_dp_delayed_destroy_port(port);
			send_hotplug = true;
			go_again = true;
		}
	} while (go_again);

	if (send_hotplug)
		drm_kms_helper_hotplug_event(mgr->dev);
}

static struct drm_private_state *
drm_dp_mst_duplicate_state(struct drm_private_obj *obj)
{
	struct drm_dp_mst_topology_state *state, *old_state =
		to_dp_mst_topology_state(obj->state);
	struct drm_dp_mst_atomic_payload *pos, *payload;

	state = kmemdup(old_state, sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	__drm_atomic_helper_private_obj_duplicate_state(obj, &state->base);

	INIT_LIST_HEAD(&state->payloads);
	state->commit_deps = NULL;
	state->num_commit_deps = 0;
	state->pending_crtc_mask = 0;

	list_for_each_entry(pos, &old_state->payloads, next) {
		 
		if (pos->delete)
			continue;

		payload = kmemdup(pos, sizeof(*payload), GFP_KERNEL);
		if (!payload)
			goto fail;

		drm_dp_mst_get_port_malloc(payload->port);
		list_add(&payload->next, &state->payloads);
	}

	return &state->base;

fail:
	list_for_each_entry_safe(pos, payload, &state->payloads, next) {
		drm_dp_mst_put_port_malloc(pos->port);
		kfree(pos);
	}
	kfree(state);

	return NULL;
}

static void drm_dp_mst_destroy_state(struct drm_private_obj *obj,
				     struct drm_private_state *state)
{
	struct drm_dp_mst_topology_state *mst_state =
		to_dp_mst_topology_state(state);
	struct drm_dp_mst_atomic_payload *pos, *tmp;
	int i;

	list_for_each_entry_safe(pos, tmp, &mst_state->payloads, next) {
		 
		if (!pos->delete)
			drm_dp_mst_put_port_malloc(pos->port);
		kfree(pos);
	}

	for (i = 0; i < mst_state->num_commit_deps; i++)
		drm_crtc_commit_put(mst_state->commit_deps[i]);

	kfree(mst_state->commit_deps);
	kfree(mst_state);
}

static bool drm_dp_mst_port_downstream_of_branch(struct drm_dp_mst_port *port,
						 struct drm_dp_mst_branch *branch)
{
	while (port->parent) {
		if (port->parent == branch)
			return true;

		if (port->parent->port_parent)
			port = port->parent->port_parent;
		else
			break;
	}
	return false;
}

static int
drm_dp_mst_atomic_check_port_bw_limit(struct drm_dp_mst_port *port,
				      struct drm_dp_mst_topology_state *state);

static int
drm_dp_mst_atomic_check_mstb_bw_limit(struct drm_dp_mst_branch *mstb,
				      struct drm_dp_mst_topology_state *state)
{
	struct drm_dp_mst_atomic_payload *payload;
	struct drm_dp_mst_port *port;
	int pbn_used = 0, ret;
	bool found = false;

	 
	list_for_each_entry(payload, &state->payloads, next) {
		if (!payload->pbn ||
		    !drm_dp_mst_port_downstream_of_branch(payload->port, mstb))
			continue;

		found = true;
		break;
	}
	if (!found)
		return 0;

	if (mstb->port_parent)
		drm_dbg_atomic(mstb->mgr->dev,
			       "[MSTB:%p] [MST PORT:%p] Checking bandwidth limits on [MSTB:%p]\n",
			       mstb->port_parent->parent, mstb->port_parent, mstb);
	else
		drm_dbg_atomic(mstb->mgr->dev, "[MSTB:%p] Checking bandwidth limits\n", mstb);

	list_for_each_entry(port, &mstb->ports, next) {
		ret = drm_dp_mst_atomic_check_port_bw_limit(port, state);
		if (ret < 0)
			return ret;

		pbn_used += ret;
	}

	return pbn_used;
}

static int
drm_dp_mst_atomic_check_port_bw_limit(struct drm_dp_mst_port *port,
				      struct drm_dp_mst_topology_state *state)
{
	struct drm_dp_mst_atomic_payload *payload;
	int pbn_used = 0;

	if (port->pdt == DP_PEER_DEVICE_NONE)
		return 0;

	if (drm_dp_mst_is_end_device(port->pdt, port->mcs)) {
		payload = drm_atomic_get_mst_payload_state(state, port);
		if (!payload)
			return 0;

		 
		if (!port->full_pbn) {
			drm_dbg_atomic(port->mgr->dev,
				       "[MSTB:%p] [MST PORT:%p] no BW available for the port\n",
				       port->parent, port);
			return -EINVAL;
		}

		pbn_used = payload->pbn;
	} else {
		pbn_used = drm_dp_mst_atomic_check_mstb_bw_limit(port->mstb,
								 state);
		if (pbn_used <= 0)
			return pbn_used;
	}

	if (pbn_used > port->full_pbn) {
		drm_dbg_atomic(port->mgr->dev,
			       "[MSTB:%p] [MST PORT:%p] required PBN of %d exceeds port limit of %d\n",
			       port->parent, port, pbn_used, port->full_pbn);
		return -ENOSPC;
	}

	drm_dbg_atomic(port->mgr->dev, "[MSTB:%p] [MST PORT:%p] uses %d out of %d PBN\n",
		       port->parent, port, pbn_used, port->full_pbn);

	return pbn_used;
}

static inline int
drm_dp_mst_atomic_check_payload_alloc_limits(struct drm_dp_mst_topology_mgr *mgr,
					     struct drm_dp_mst_topology_state *mst_state)
{
	struct drm_dp_mst_atomic_payload *payload;
	int avail_slots = mst_state->total_avail_slots, payload_count = 0;

	list_for_each_entry(payload, &mst_state->payloads, next) {
		 
		if (payload->delete) {
			drm_dbg_atomic(mgr->dev, "[MST PORT:%p] releases all time slots\n",
				       payload->port);
			continue;
		}

		drm_dbg_atomic(mgr->dev, "[MST PORT:%p] requires %d time slots\n",
			       payload->port, payload->time_slots);

		avail_slots -= payload->time_slots;
		if (avail_slots < 0) {
			drm_dbg_atomic(mgr->dev,
				       "[MST PORT:%p] not enough time slots in mst state %p (avail=%d)\n",
				       payload->port, mst_state, avail_slots + payload->time_slots);
			return -ENOSPC;
		}

		if (++payload_count > mgr->max_payloads) {
			drm_dbg_atomic(mgr->dev,
				       "[MST MGR:%p] state %p has too many payloads (max=%d)\n",
				       mgr, mst_state, mgr->max_payloads);
			return -EINVAL;
		}

		 
		if (!payload->vcpi) {
			payload->vcpi = ffz(mst_state->payload_mask) + 1;
			drm_dbg_atomic(mgr->dev, "[MST PORT:%p] assigned VCPI #%d\n",
				       payload->port, payload->vcpi);
			mst_state->payload_mask |= BIT(payload->vcpi - 1);
		}
	}

	if (!payload_count)
		mst_state->pbn_div = 0;

	drm_dbg_atomic(mgr->dev, "[MST MGR:%p] mst state %p TU pbn_div=%d avail=%d used=%d\n",
		       mgr, mst_state, mst_state->pbn_div, avail_slots,
		       mst_state->total_avail_slots - avail_slots);

	return 0;
}

 
int drm_dp_mst_add_affected_dsc_crtcs(struct drm_atomic_state *state, struct drm_dp_mst_topology_mgr *mgr)
{
	struct drm_dp_mst_topology_state *mst_state;
	struct drm_dp_mst_atomic_payload *pos;
	struct drm_connector *connector;
	struct drm_connector_state *conn_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;

	mst_state = drm_atomic_get_mst_topology_state(state, mgr);

	if (IS_ERR(mst_state))
		return PTR_ERR(mst_state);

	list_for_each_entry(pos, &mst_state->payloads, next) {

		connector = pos->port->connector;

		if (!connector)
			return -EINVAL;

		conn_state = drm_atomic_get_connector_state(state, connector);

		if (IS_ERR(conn_state))
			return PTR_ERR(conn_state);

		crtc = conn_state->crtc;

		if (!crtc)
			continue;

		if (!drm_dp_mst_dsc_aux_for_port(pos->port))
			continue;

		crtc_state = drm_atomic_get_crtc_state(mst_state->base.state, crtc);

		if (IS_ERR(crtc_state))
			return PTR_ERR(crtc_state);

		drm_dbg_atomic(mgr->dev, "[MST MGR:%p] Setting mode_changed flag on CRTC %p\n",
			       mgr, crtc);

		crtc_state->mode_changed = true;
	}
	return 0;
}
EXPORT_SYMBOL(drm_dp_mst_add_affected_dsc_crtcs);

 
int drm_dp_mst_atomic_enable_dsc(struct drm_atomic_state *state,
				 struct drm_dp_mst_port *port,
				 int pbn, bool enable)
{
	struct drm_dp_mst_topology_state *mst_state;
	struct drm_dp_mst_atomic_payload *payload;
	int time_slots = 0;

	mst_state = drm_atomic_get_mst_topology_state(state, port->mgr);
	if (IS_ERR(mst_state))
		return PTR_ERR(mst_state);

	payload = drm_atomic_get_mst_payload_state(mst_state, port);
	if (!payload) {
		drm_dbg_atomic(state->dev,
			       "[MST PORT:%p] Couldn't find payload in mst state %p\n",
			       port, mst_state);
		return -EINVAL;
	}

	if (payload->dsc_enabled == enable) {
		drm_dbg_atomic(state->dev,
			       "[MST PORT:%p] DSC flag is already set to %d, returning %d time slots\n",
			       port, enable, payload->time_slots);
		time_slots = payload->time_slots;
	}

	if (enable) {
		time_slots = drm_dp_atomic_find_time_slots(state, port->mgr, port, pbn);
		drm_dbg_atomic(state->dev,
			       "[MST PORT:%p] Enabling DSC flag, reallocating %d time slots on the port\n",
			       port, time_slots);
		if (time_slots < 0)
			return -EINVAL;
	}

	payload->dsc_enabled = enable;

	return time_slots;
}
EXPORT_SYMBOL(drm_dp_mst_atomic_enable_dsc);

 
int drm_dp_mst_atomic_check(struct drm_atomic_state *state)
{
	struct drm_dp_mst_topology_mgr *mgr;
	struct drm_dp_mst_topology_state *mst_state;
	int i, ret = 0;

	for_each_new_mst_mgr_in_state(state, mgr, mst_state, i) {
		if (!mgr->mst_state)
			continue;

		ret = drm_dp_mst_atomic_check_payload_alloc_limits(mgr, mst_state);
		if (ret)
			break;

		mutex_lock(&mgr->lock);
		ret = drm_dp_mst_atomic_check_mstb_bw_limit(mgr->mst_primary,
							    mst_state);
		mutex_unlock(&mgr->lock);
		if (ret < 0)
			break;
		else
			ret = 0;
	}

	return ret;
}
EXPORT_SYMBOL(drm_dp_mst_atomic_check);

const struct drm_private_state_funcs drm_dp_mst_topology_state_funcs = {
	.atomic_duplicate_state = drm_dp_mst_duplicate_state,
	.atomic_destroy_state = drm_dp_mst_destroy_state,
};
EXPORT_SYMBOL(drm_dp_mst_topology_state_funcs);

 
struct drm_dp_mst_topology_state *drm_atomic_get_mst_topology_state(struct drm_atomic_state *state,
								    struct drm_dp_mst_topology_mgr *mgr)
{
	return to_dp_mst_topology_state(drm_atomic_get_private_obj_state(state, &mgr->base));
}
EXPORT_SYMBOL(drm_atomic_get_mst_topology_state);

 
struct drm_dp_mst_topology_state *
drm_atomic_get_old_mst_topology_state(struct drm_atomic_state *state,
				      struct drm_dp_mst_topology_mgr *mgr)
{
	struct drm_private_state *old_priv_state =
		drm_atomic_get_old_private_obj_state(state, &mgr->base);

	return old_priv_state ? to_dp_mst_topology_state(old_priv_state) : NULL;
}
EXPORT_SYMBOL(drm_atomic_get_old_mst_topology_state);

 
struct drm_dp_mst_topology_state *
drm_atomic_get_new_mst_topology_state(struct drm_atomic_state *state,
				      struct drm_dp_mst_topology_mgr *mgr)
{
	struct drm_private_state *new_priv_state =
		drm_atomic_get_new_private_obj_state(state, &mgr->base);

	return new_priv_state ? to_dp_mst_topology_state(new_priv_state) : NULL;
}
EXPORT_SYMBOL(drm_atomic_get_new_mst_topology_state);

 
int drm_dp_mst_topology_mgr_init(struct drm_dp_mst_topology_mgr *mgr,
				 struct drm_device *dev, struct drm_dp_aux *aux,
				 int max_dpcd_transaction_bytes, int max_payloads,
				 int conn_base_id)
{
	struct drm_dp_mst_topology_state *mst_state;

	mutex_init(&mgr->lock);
	mutex_init(&mgr->qlock);
	mutex_init(&mgr->delayed_destroy_lock);
	mutex_init(&mgr->up_req_lock);
	mutex_init(&mgr->probe_lock);
#if IS_ENABLED(CONFIG_DRM_DEBUG_DP_MST_TOPOLOGY_REFS)
	mutex_init(&mgr->topology_ref_history_lock);
	stack_depot_init();
#endif
	INIT_LIST_HEAD(&mgr->tx_msg_downq);
	INIT_LIST_HEAD(&mgr->destroy_port_list);
	INIT_LIST_HEAD(&mgr->destroy_branch_device_list);
	INIT_LIST_HEAD(&mgr->up_req_list);

	 
	mgr->delayed_destroy_wq = alloc_ordered_workqueue("drm_dp_mst_wq", 0);
	if (mgr->delayed_destroy_wq == NULL)
		return -ENOMEM;

	INIT_WORK(&mgr->work, drm_dp_mst_link_probe_work);
	INIT_WORK(&mgr->tx_work, drm_dp_tx_work);
	INIT_WORK(&mgr->delayed_destroy_work, drm_dp_delayed_destroy_work);
	INIT_WORK(&mgr->up_req_work, drm_dp_mst_up_req_work);
	init_waitqueue_head(&mgr->tx_waitq);
	mgr->dev = dev;
	mgr->aux = aux;
	mgr->max_dpcd_transaction_bytes = max_dpcd_transaction_bytes;
	mgr->max_payloads = max_payloads;
	mgr->conn_base_id = conn_base_id;

	mst_state = kzalloc(sizeof(*mst_state), GFP_KERNEL);
	if (mst_state == NULL)
		return -ENOMEM;

	mst_state->total_avail_slots = 63;
	mst_state->start_slot = 1;

	mst_state->mgr = mgr;
	INIT_LIST_HEAD(&mst_state->payloads);

	drm_atomic_private_obj_init(dev, &mgr->base,
				    &mst_state->base,
				    &drm_dp_mst_topology_state_funcs);

	return 0;
}
EXPORT_SYMBOL(drm_dp_mst_topology_mgr_init);

 
void drm_dp_mst_topology_mgr_destroy(struct drm_dp_mst_topology_mgr *mgr)
{
	drm_dp_mst_topology_mgr_set_mst(mgr, false);
	flush_work(&mgr->work);
	 
	if (mgr->delayed_destroy_wq) {
		destroy_workqueue(mgr->delayed_destroy_wq);
		mgr->delayed_destroy_wq = NULL;
	}
	mgr->dev = NULL;
	mgr->aux = NULL;
	drm_atomic_private_obj_fini(&mgr->base);
	mgr->funcs = NULL;

	mutex_destroy(&mgr->delayed_destroy_lock);
	mutex_destroy(&mgr->qlock);
	mutex_destroy(&mgr->lock);
	mutex_destroy(&mgr->up_req_lock);
	mutex_destroy(&mgr->probe_lock);
#if IS_ENABLED(CONFIG_DRM_DEBUG_DP_MST_TOPOLOGY_REFS)
	mutex_destroy(&mgr->topology_ref_history_lock);
#endif
}
EXPORT_SYMBOL(drm_dp_mst_topology_mgr_destroy);

static bool remote_i2c_read_ok(const struct i2c_msg msgs[], int num)
{
	int i;

	if (num - 1 > DP_REMOTE_I2C_READ_MAX_TRANSACTIONS)
		return false;

	for (i = 0; i < num - 1; i++) {
		if (msgs[i].flags & I2C_M_RD ||
		    msgs[i].len > 0xff)
			return false;
	}

	return msgs[num - 1].flags & I2C_M_RD &&
		msgs[num - 1].len <= 0xff;
}

static bool remote_i2c_write_ok(const struct i2c_msg msgs[], int num)
{
	int i;

	for (i = 0; i < num - 1; i++) {
		if (msgs[i].flags & I2C_M_RD || !(msgs[i].flags & I2C_M_STOP) ||
		    msgs[i].len > 0xff)
			return false;
	}

	return !(msgs[num - 1].flags & I2C_M_RD) && msgs[num - 1].len <= 0xff;
}

static int drm_dp_mst_i2c_read(struct drm_dp_mst_branch *mstb,
			       struct drm_dp_mst_port *port,
			       struct i2c_msg *msgs, int num)
{
	struct drm_dp_mst_topology_mgr *mgr = port->mgr;
	unsigned int i;
	struct drm_dp_sideband_msg_req_body msg;
	struct drm_dp_sideband_msg_tx *txmsg = NULL;
	int ret;

	memset(&msg, 0, sizeof(msg));
	msg.req_type = DP_REMOTE_I2C_READ;
	msg.u.i2c_read.num_transactions = num - 1;
	msg.u.i2c_read.port_number = port->port_num;
	for (i = 0; i < num - 1; i++) {
		msg.u.i2c_read.transactions[i].i2c_dev_id = msgs[i].addr;
		msg.u.i2c_read.transactions[i].num_bytes = msgs[i].len;
		msg.u.i2c_read.transactions[i].bytes = msgs[i].buf;
		msg.u.i2c_read.transactions[i].no_stop_bit = !(msgs[i].flags & I2C_M_STOP);
	}
	msg.u.i2c_read.read_i2c_device_id = msgs[num - 1].addr;
	msg.u.i2c_read.num_bytes_read = msgs[num - 1].len;

	txmsg = kzalloc(sizeof(*txmsg), GFP_KERNEL);
	if (!txmsg) {
		ret = -ENOMEM;
		goto out;
	}

	txmsg->dst = mstb;
	drm_dp_encode_sideband_req(&msg, txmsg);

	drm_dp_queue_down_tx(mgr, txmsg);

	ret = drm_dp_mst_wait_tx_reply(mstb, txmsg);
	if (ret > 0) {

		if (txmsg->reply.reply_type == DP_SIDEBAND_REPLY_NAK) {
			ret = -EREMOTEIO;
			goto out;
		}
		if (txmsg->reply.u.remote_i2c_read_ack.num_bytes != msgs[num - 1].len) {
			ret = -EIO;
			goto out;
		}
		memcpy(msgs[num - 1].buf, txmsg->reply.u.remote_i2c_read_ack.bytes, msgs[num - 1].len);
		ret = num;
	}
out:
	kfree(txmsg);
	return ret;
}

static int drm_dp_mst_i2c_write(struct drm_dp_mst_branch *mstb,
				struct drm_dp_mst_port *port,
				struct i2c_msg *msgs, int num)
{
	struct drm_dp_mst_topology_mgr *mgr = port->mgr;
	unsigned int i;
	struct drm_dp_sideband_msg_req_body msg;
	struct drm_dp_sideband_msg_tx *txmsg = NULL;
	int ret;

	txmsg = kzalloc(sizeof(*txmsg), GFP_KERNEL);
	if (!txmsg) {
		ret = -ENOMEM;
		goto out;
	}
	for (i = 0; i < num; i++) {
		memset(&msg, 0, sizeof(msg));
		msg.req_type = DP_REMOTE_I2C_WRITE;
		msg.u.i2c_write.port_number = port->port_num;
		msg.u.i2c_write.write_i2c_device_id = msgs[i].addr;
		msg.u.i2c_write.num_bytes = msgs[i].len;
		msg.u.i2c_write.bytes = msgs[i].buf;

		memset(txmsg, 0, sizeof(*txmsg));
		txmsg->dst = mstb;

		drm_dp_encode_sideband_req(&msg, txmsg);
		drm_dp_queue_down_tx(mgr, txmsg);

		ret = drm_dp_mst_wait_tx_reply(mstb, txmsg);
		if (ret > 0) {
			if (txmsg->reply.reply_type == DP_SIDEBAND_REPLY_NAK) {
				ret = -EREMOTEIO;
				goto out;
			}
		} else {
			goto out;
		}
	}
	ret = num;
out:
	kfree(txmsg);
	return ret;
}

 
static int drm_dp_mst_i2c_xfer(struct i2c_adapter *adapter,
			       struct i2c_msg *msgs, int num)
{
	struct drm_dp_aux *aux = adapter->algo_data;
	struct drm_dp_mst_port *port =
		container_of(aux, struct drm_dp_mst_port, aux);
	struct drm_dp_mst_branch *mstb;
	struct drm_dp_mst_topology_mgr *mgr = port->mgr;
	int ret;

	mstb = drm_dp_mst_topology_get_mstb_validated(mgr, port->parent);
	if (!mstb)
		return -EREMOTEIO;

	if (remote_i2c_read_ok(msgs, num)) {
		ret = drm_dp_mst_i2c_read(mstb, port, msgs, num);
	} else if (remote_i2c_write_ok(msgs, num)) {
		ret = drm_dp_mst_i2c_write(mstb, port, msgs, num);
	} else {
		drm_dbg_kms(mgr->dev, "Unsupported I2C transaction for MST device\n");
		ret = -EIO;
	}

	drm_dp_mst_topology_put_mstb(mstb);
	return ret;
}

static u32 drm_dp_mst_i2c_functionality(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL |
	       I2C_FUNC_SMBUS_READ_BLOCK_DATA |
	       I2C_FUNC_SMBUS_BLOCK_PROC_CALL |
	       I2C_FUNC_10BIT_ADDR;
}

static const struct i2c_algorithm drm_dp_mst_i2c_algo = {
	.functionality = drm_dp_mst_i2c_functionality,
	.master_xfer = drm_dp_mst_i2c_xfer,
};

 
static int drm_dp_mst_register_i2c_bus(struct drm_dp_mst_port *port)
{
	struct drm_dp_aux *aux = &port->aux;
	struct device *parent_dev = port->mgr->dev->dev;

	aux->ddc.algo = &drm_dp_mst_i2c_algo;
	aux->ddc.algo_data = aux;
	aux->ddc.retries = 3;

	aux->ddc.class = I2C_CLASS_DDC;
	aux->ddc.owner = THIS_MODULE;
	 
	aux->ddc.dev.parent = parent_dev;
	aux->ddc.dev.of_node = parent_dev->of_node;

	strscpy(aux->ddc.name, aux->name ? aux->name : dev_name(parent_dev),
		sizeof(aux->ddc.name));

	return i2c_add_adapter(&aux->ddc);
}

 
static void drm_dp_mst_unregister_i2c_bus(struct drm_dp_mst_port *port)
{
	i2c_del_adapter(&port->aux.ddc);
}

 
static bool drm_dp_mst_is_virtual_dpcd(struct drm_dp_mst_port *port)
{
	struct drm_dp_mst_port *downstream_port;

	if (!port || port->dpcd_rev < DP_DPCD_REV_14)
		return false;

	 
	if (port->port_num >= 8)
		return true;

	 
	if (port->pdt == DP_PEER_DEVICE_DP_LEGACY_CONV &&
	    !port->mcs &&
	    port->ldps)
		return true;

	 
	mutex_lock(&port->mgr->lock);
	if (port->pdt == DP_PEER_DEVICE_MST_BRANCHING &&
	    port->mstb &&
	    port->mstb->num_ports == 2) {
		list_for_each_entry(downstream_port, &port->mstb->ports, next) {
			if (downstream_port->pdt == DP_PEER_DEVICE_SST_SINK &&
			    !downstream_port->input) {
				mutex_unlock(&port->mgr->lock);
				return true;
			}
		}
	}
	mutex_unlock(&port->mgr->lock);

	return false;
}

 
struct drm_dp_aux *drm_dp_mst_dsc_aux_for_port(struct drm_dp_mst_port *port)
{
	struct drm_dp_mst_port *immediate_upstream_port;
	struct drm_dp_mst_port *fec_port;
	struct drm_dp_desc desc = {};
	u8 endpoint_fec;
	u8 endpoint_dsc;

	if (!port)
		return NULL;

	if (port->parent->port_parent)
		immediate_upstream_port = port->parent->port_parent;
	else
		immediate_upstream_port = NULL;

	fec_port = immediate_upstream_port;
	while (fec_port) {
		 
		if (!drm_dp_mst_is_virtual_dpcd(fec_port) &&
		    !fec_port->fec_capable)
			return NULL;

		fec_port = fec_port->parent->port_parent;
	}

	 
	if (drm_dp_mst_is_virtual_dpcd(immediate_upstream_port)) {
		u8 upstream_dsc;

		if (drm_dp_dpcd_read(&port->aux,
				     DP_DSC_SUPPORT, &endpoint_dsc, 1) != 1)
			return NULL;
		if (drm_dp_dpcd_read(&port->aux,
				     DP_FEC_CAPABILITY, &endpoint_fec, 1) != 1)
			return NULL;
		if (drm_dp_dpcd_read(&immediate_upstream_port->aux,
				     DP_DSC_SUPPORT, &upstream_dsc, 1) != 1)
			return NULL;

		 
		if ((endpoint_dsc & DP_DSC_DECOMPRESSION_IS_SUPPORTED) &&
		    (endpoint_fec & DP_FEC_CAPABLE) &&
		    (upstream_dsc & DP_DSC_PASSTHROUGH_IS_SUPPORTED)) {
			port->passthrough_aux = &immediate_upstream_port->aux;
			return &port->aux;
		}

		 
		return &immediate_upstream_port->aux;
	}

	 
	if (drm_dp_mst_is_virtual_dpcd(port))
		return &port->aux;

	 
	if (drm_dp_read_desc(port->mgr->aux, &desc, true))
		return NULL;

	if (drm_dp_has_quirk(&desc, DP_DPCD_QUIRK_DSC_WITHOUT_VIRTUAL_DPCD) &&
	    port->mgr->dpcd[DP_DPCD_REV] >= DP_DPCD_REV_14 &&
	    port->parent == port->mgr->mst_primary) {
		u8 dpcd_ext[DP_RECEIVER_CAP_SIZE];

		if (drm_dp_read_dpcd_caps(port->mgr->aux, dpcd_ext) < 0)
			return NULL;

		if ((dpcd_ext[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_PRESENT) &&
		    ((dpcd_ext[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_TYPE_MASK)
		     != DP_DWN_STRM_PORT_TYPE_ANALOG))
			return port->mgr->aux;
	}

	 
	if (drm_dp_dpcd_read(&port->aux,
	   DP_DSC_SUPPORT, &endpoint_dsc, 1) != 1)
		return NULL;
	if (drm_dp_dpcd_read(&port->aux,
	   DP_FEC_CAPABILITY, &endpoint_fec, 1) != 1)
		return NULL;
	if ((endpoint_dsc & DP_DSC_DECOMPRESSION_IS_SUPPORTED) &&
	   (endpoint_fec & DP_FEC_CAPABLE))
		return &port->aux;

	return NULL;
}
EXPORT_SYMBOL(drm_dp_mst_dsc_aux_for_port);
