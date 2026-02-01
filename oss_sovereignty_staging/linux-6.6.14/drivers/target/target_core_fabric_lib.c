
 

 

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/spinlock.h>
#include <linux/export.h>
#include <asm/unaligned.h>

#include <scsi/scsi_proto.h>

#include <target/target_core_base.h>
#include <target/target_core_fabric.h>

#include "target_core_internal.h"
#include "target_core_pr.h"


static int sas_get_pr_transport_id(
	struct se_node_acl *nacl,
	int *format_code,
	unsigned char *buf)
{
	int ret;

	 
	ret = hex2bin(&buf[4], &nacl->initiatorname[4], 8);
	if (ret) {
		pr_debug("%s: invalid hex string\n", __func__);
		return ret;
	}

	return 24;
}

static int fc_get_pr_transport_id(
	struct se_node_acl *se_nacl,
	int *format_code,
	unsigned char *buf)
{
	unsigned char *ptr;
	int i, ret;
	u32 off = 8;

	 
	ptr = &se_nacl->initiatorname[0];
	for (i = 0; i < 23; ) {
		if (!strncmp(&ptr[i], ":", 1)) {
			i++;
			continue;
		}
		ret = hex2bin(&buf[off++], &ptr[i], 1);
		if (ret < 0) {
			pr_debug("%s: invalid hex string\n", __func__);
			return ret;
		}
		i += 2;
	}
	 
	return 24;
}

static int sbp_get_pr_transport_id(
	struct se_node_acl *nacl,
	int *format_code,
	unsigned char *buf)
{
	int ret;

	ret = hex2bin(&buf[8], nacl->initiatorname, 8);
	if (ret) {
		pr_debug("%s: invalid hex string\n", __func__);
		return ret;
	}

	return 24;
}

static int srp_get_pr_transport_id(
	struct se_node_acl *nacl,
	int *format_code,
	unsigned char *buf)
{
	const char *p;
	unsigned len, count, leading_zero_bytes;
	int rc;

	p = nacl->initiatorname;
	if (strncasecmp(p, "0x", 2) == 0)
		p += 2;
	len = strlen(p);
	if (len % 2)
		return -EINVAL;

	count = min(len / 2, 16U);
	leading_zero_bytes = 16 - count;
	memset(buf + 8, 0, leading_zero_bytes);
	rc = hex2bin(buf + 8 + leading_zero_bytes, p, count);
	if (rc < 0) {
		pr_debug("hex2bin failed for %s: %d\n", p, rc);
		return rc;
	}

	return 24;
}

static int iscsi_get_pr_transport_id(
	struct se_node_acl *se_nacl,
	struct t10_pr_registration *pr_reg,
	int *format_code,
	unsigned char *buf)
{
	u32 off = 4, padding = 0;
	int isid_len;
	u16 len = 0;

	spin_lock_irq(&se_nacl->nacl_sess_lock);
	 
	len = sprintf(&buf[off], "%s", se_nacl->initiatorname);
	off += len;
	if ((*format_code == 1) && (pr_reg->isid_present_at_reg)) {
		 
		buf[0] |= 0x40;
		 
		buf[off++] = 0x2c;  
		buf[off++] = 0x69;  
		buf[off++] = 0x2c;  
		buf[off++] = 0x30;  
		buf[off++] = 0x78;  
		len += 5;

		isid_len = sprintf(buf + off, "%s", pr_reg->pr_reg_isid);
		off += isid_len;
		len += isid_len;
	}
	buf[off] = '\0';
	len += 1;
	spin_unlock_irq(&se_nacl->nacl_sess_lock);
	 
	padding = ((-len) & 3);
	if (padding != 0)
		len += padding;

	put_unaligned_be16(len, &buf[2]);
	 
	len += 4;

	return len;
}

static int iscsi_get_pr_transport_id_len(
	struct se_node_acl *se_nacl,
	struct t10_pr_registration *pr_reg,
	int *format_code)
{
	u32 len = 0, padding = 0;

	spin_lock_irq(&se_nacl->nacl_sess_lock);
	len = strlen(se_nacl->initiatorname);
	 
	len++;
	 
	if (pr_reg->isid_present_at_reg) {
		len += 5;  
		len += strlen(pr_reg->pr_reg_isid);
		*format_code = 1;
	} else
		*format_code = 0;
	spin_unlock_irq(&se_nacl->nacl_sess_lock);
	 
	padding = ((-len) & 3);
	if (padding != 0)
		len += padding;
	 
	len += 4;

	return len;
}

static char *iscsi_parse_pr_out_transport_id(
	struct se_portal_group *se_tpg,
	char *buf,
	u32 *out_tid_len,
	char **port_nexus_ptr)
{
	char *p;
	int i;
	u8 format_code = (buf[0] & 0xc0);
	 
	if ((format_code != 0x00) && (format_code != 0x40)) {
		pr_err("Illegal format code: 0x%02x for iSCSI"
			" Initiator Transport ID\n", format_code);
		return NULL;
	}
	 
	if (out_tid_len) {
		 
		*out_tid_len = get_unaligned_be16(&buf[2]);
		 
		*out_tid_len += 4;
	}

	 
	if (format_code == 0x40) {
		p = strstr(&buf[4], ",i,0x");
		if (!p) {
			pr_err("Unable to locate \",i,0x\" separator"
				" for Initiator port identifier: %s\n",
				&buf[4]);
			return NULL;
		}
		*p = '\0';  
		p += 5;  

		*port_nexus_ptr = p;
		 
		for (i = 0; i < 12; i++) {
			 
			if (*p == '\0')
				break;

			if (isdigit(*p)) {
				p++;
				continue;
			}
			*p = tolower(*p);
			p++;
		}
	} else
		*port_nexus_ptr = NULL;

	return &buf[4];
}

int target_get_pr_transport_id_len(struct se_node_acl *nacl,
		struct t10_pr_registration *pr_reg, int *format_code)
{
	switch (nacl->se_tpg->proto_id) {
	case SCSI_PROTOCOL_FCP:
	case SCSI_PROTOCOL_SBP:
	case SCSI_PROTOCOL_SRP:
	case SCSI_PROTOCOL_SAS:
		break;
	case SCSI_PROTOCOL_ISCSI:
		return iscsi_get_pr_transport_id_len(nacl, pr_reg, format_code);
	default:
		pr_err("Unknown proto_id: 0x%02x\n", nacl->se_tpg->proto_id);
		return -EINVAL;
	}

	 
	*format_code = 0;
	return 24;
}

int target_get_pr_transport_id(struct se_node_acl *nacl,
		struct t10_pr_registration *pr_reg, int *format_code,
		unsigned char *buf)
{
	switch (nacl->se_tpg->proto_id) {
	case SCSI_PROTOCOL_SAS:
		return sas_get_pr_transport_id(nacl, format_code, buf);
	case SCSI_PROTOCOL_SBP:
		return sbp_get_pr_transport_id(nacl, format_code, buf);
	case SCSI_PROTOCOL_SRP:
		return srp_get_pr_transport_id(nacl, format_code, buf);
	case SCSI_PROTOCOL_FCP:
		return fc_get_pr_transport_id(nacl, format_code, buf);
	case SCSI_PROTOCOL_ISCSI:
		return iscsi_get_pr_transport_id(nacl, pr_reg, format_code,
				buf);
	default:
		pr_err("Unknown proto_id: 0x%02x\n", nacl->se_tpg->proto_id);
		return -EINVAL;
	}
}

const char *target_parse_pr_out_transport_id(struct se_portal_group *tpg,
		char *buf, u32 *out_tid_len, char **port_nexus_ptr)
{
	u32 offset;

	switch (tpg->proto_id) {
	case SCSI_PROTOCOL_SAS:
		 
		offset = 4;
		break;
	case SCSI_PROTOCOL_SBP:
	case SCSI_PROTOCOL_SRP:
	case SCSI_PROTOCOL_FCP:
		offset = 8;
		break;
	case SCSI_PROTOCOL_ISCSI:
		return iscsi_parse_pr_out_transport_id(tpg, buf, out_tid_len,
					port_nexus_ptr);
	default:
		pr_err("Unknown proto_id: 0x%02x\n", tpg->proto_id);
		return NULL;
	}

	*port_nexus_ptr = NULL;
	*out_tid_len = 24;
	return buf + offset;
}
