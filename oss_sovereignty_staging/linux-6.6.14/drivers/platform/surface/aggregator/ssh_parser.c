
 

#include <asm/unaligned.h>
#include <linux/compiler.h>
#include <linux/device.h>
#include <linux/types.h>

#include <linux/surface_aggregator/serial_hub.h>
#include "ssh_parser.h"

 
static bool sshp_validate_crc(const struct ssam_span *src, const u8 *crc)
{
	u16 actual = ssh_crc(src->ptr, src->len);
	u16 expected = get_unaligned_le16(crc);

	return actual == expected;
}

 
static bool sshp_starts_with_syn(const struct ssam_span *src)
{
	return src->len >= 2 && get_unaligned_le16(src->ptr) == SSH_MSG_SYN;
}

 
bool sshp_find_syn(const struct ssam_span *src, struct ssam_span *rem)
{
	size_t i;

	for (i = 0; i < src->len - 1; i++) {
		if (likely(get_unaligned_le16(src->ptr + i) == SSH_MSG_SYN)) {
			rem->ptr = src->ptr + i;
			rem->len = src->len - i;
			return true;
		}
	}

	if (unlikely(src->ptr[src->len - 1] == (SSH_MSG_SYN & 0xff))) {
		rem->ptr = src->ptr + src->len - 1;
		rem->len = 1;
		return false;
	}

	rem->ptr = src->ptr + src->len;
	rem->len = 0;
	return false;
}

 
int sshp_parse_frame(const struct device *dev, const struct ssam_span *source,
		     struct ssh_frame **frame, struct ssam_span *payload,
		     size_t maxlen)
{
	struct ssam_span sf;
	struct ssam_span sp;

	 
	*frame = NULL;
	payload->ptr = NULL;
	payload->len = 0;

	if (!sshp_starts_with_syn(source)) {
		dev_warn(dev, "rx: parser: invalid start of frame\n");
		return -ENOMSG;
	}

	 
	if (unlikely(source->len < SSH_MESSAGE_LENGTH(0))) {
		dev_dbg(dev, "rx: parser: not enough data for frame\n");
		return 0;
	}

	 
	sf.ptr = source->ptr + sizeof(u16);
	sf.len = sizeof(struct ssh_frame);

	 
	if (unlikely(!sshp_validate_crc(&sf, sf.ptr + sf.len))) {
		dev_warn(dev, "rx: parser: invalid frame CRC\n");
		return -EBADMSG;
	}

	 
	sp.len = get_unaligned_le16(&((struct ssh_frame *)sf.ptr)->len);
	if (unlikely(SSH_MESSAGE_LENGTH(sp.len) > maxlen)) {
		dev_warn(dev, "rx: parser: frame too large: %llu bytes\n",
			 SSH_MESSAGE_LENGTH(sp.len));
		return -EMSGSIZE;
	}

	 
	sp.ptr = sf.ptr + sf.len + sizeof(u16);

	 
	if (source->len < SSH_MESSAGE_LENGTH(sp.len)) {
		dev_dbg(dev, "rx: parser: not enough data for payload\n");
		return 0;
	}

	 
	if (unlikely(!sshp_validate_crc(&sp, sp.ptr + sp.len))) {
		dev_warn(dev, "rx: parser: invalid payload CRC\n");
		return -EBADMSG;
	}

	*frame = (struct ssh_frame *)sf.ptr;
	*payload = sp;

	dev_dbg(dev, "rx: parser: valid frame found (type: %#04x, len: %u)\n",
		(*frame)->type, (*frame)->len);

	return 0;
}

 
int sshp_parse_command(const struct device *dev, const struct ssam_span *source,
		       struct ssh_command **command,
		       struct ssam_span *command_data)
{
	 
	if (unlikely(source->len < sizeof(struct ssh_command))) {
		*command = NULL;
		command_data->ptr = NULL;
		command_data->len = 0;

		dev_err(dev, "rx: parser: command payload is too short\n");
		return -ENOMSG;
	}

	*command = (struct ssh_command *)source->ptr;
	command_data->ptr = source->ptr + sizeof(struct ssh_command);
	command_data->len = source->len - sizeof(struct ssh_command);

	dev_dbg(dev, "rx: parser: valid command found (tc: %#04x, cid: %#04x)\n",
		(*command)->tc, (*command)->cid);

	return 0;
}
