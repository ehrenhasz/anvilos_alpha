
 
#include <linux/ctype.h>
#include "cifsglob.h"
#include "cifsproto.h"
#include "smb2proto.h"
#include "cifs_debug.h"
#include "cifs_unicode.h"
#include "smb2status.h"
#include "smb2glob.h"
#include "nterr.h"
#include "cached_dir.h"

static int
check_smb2_hdr(struct smb2_hdr *shdr, __u64 mid)
{
	__u64 wire_mid = le64_to_cpu(shdr->MessageId);

	 
	if ((shdr->ProtocolId == SMB2_PROTO_NUMBER) &&
	    (mid == wire_mid)) {
		if (shdr->Flags & SMB2_FLAGS_SERVER_TO_REDIR)
			return 0;
		else {
			 
			if (shdr->Command == SMB2_OPLOCK_BREAK)
				return 0;
			else
				cifs_dbg(VFS, "Received Request not response\n");
		}
	} else {  
		if (shdr->ProtocolId != SMB2_PROTO_NUMBER)
			cifs_dbg(VFS, "Bad protocol string signature header %x\n",
				 le32_to_cpu(shdr->ProtocolId));
		if (mid != wire_mid)
			cifs_dbg(VFS, "Mids do not match: %llu and %llu\n",
				 mid, wire_mid);
	}
	cifs_dbg(VFS, "Bad SMB detected. The Mid=%llu\n", wire_mid);
	return 1;
}

 
static const __le16 smb2_rsp_struct_sizes[NUMBER_OF_SMB2_COMMANDS] = {
	  cpu_to_le16(65),
	  cpu_to_le16(9),
	  cpu_to_le16(4),
	  cpu_to_le16(16),
	  cpu_to_le16(4),
	  cpu_to_le16(89),
	  cpu_to_le16(60),
	  cpu_to_le16(4),
	  cpu_to_le16(17),
	  cpu_to_le16(17),
	  cpu_to_le16(4),
	  cpu_to_le16(49),
	 
	  cpu_to_le16(0),
	  cpu_to_le16(4),
	  cpu_to_le16(9),
	  cpu_to_le16(9),
	  cpu_to_le16(9),
	  cpu_to_le16(2),
	 
	  cpu_to_le16(24)
};

#define SMB311_NEGPROT_BASE_SIZE (sizeof(struct smb2_hdr) + sizeof(struct smb2_negotiate_rsp))

static __u32 get_neg_ctxt_len(struct smb2_hdr *hdr, __u32 len,
			      __u32 non_ctxlen)
{
	__u16 neg_count;
	__u32 nc_offset, size_of_pad_before_neg_ctxts;
	struct smb2_negotiate_rsp *pneg_rsp = (struct smb2_negotiate_rsp *)hdr;

	 
	neg_count = le16_to_cpu(pneg_rsp->NegotiateContextCount);
	if ((neg_count == 0) ||
	   (pneg_rsp->DialectRevision != cpu_to_le16(SMB311_PROT_ID)))
		return 0;

	 
	nc_offset = le32_to_cpu(pneg_rsp->NegotiateContextOffset);
	 
	if (nc_offset + 1 < non_ctxlen) {
		pr_warn_once("Invalid negotiate context offset %d\n", nc_offset);
		return 0;
	} else if (nc_offset + 1 == non_ctxlen) {
		cifs_dbg(FYI, "no SPNEGO security blob in negprot rsp\n");
		size_of_pad_before_neg_ctxts = 0;
	} else if (non_ctxlen == SMB311_NEGPROT_BASE_SIZE + 1)
		 
		size_of_pad_before_neg_ctxts = nc_offset - non_ctxlen + 1;
	else
		size_of_pad_before_neg_ctxts = nc_offset - non_ctxlen;

	 
	if (len < nc_offset + (neg_count * sizeof(struct smb2_neg_context))) {
		pr_warn_once("negotiate context goes beyond end\n");
		return 0;
	}

	cifs_dbg(FYI, "length of negcontexts %d pad %d\n",
		len - nc_offset, size_of_pad_before_neg_ctxts);

	 
	return (len - nc_offset) + size_of_pad_before_neg_ctxts;
}

int
smb2_check_message(char *buf, unsigned int len, struct TCP_Server_Info *server)
{
	struct TCP_Server_Info *pserver;
	struct smb2_hdr *shdr = (struct smb2_hdr *)buf;
	struct smb2_pdu *pdu = (struct smb2_pdu *)shdr;
	int hdr_size = sizeof(struct smb2_hdr);
	int pdu_size = sizeof(struct smb2_pdu);
	int command;
	__u32 calc_len;  
	__u64 mid;

	 
	pserver = SERVER_IS_CHAN(server) ? server->primary_server : server;

	 
	if (shdr->ProtocolId == SMB2_TRANSFORM_PROTO_NUM) {
		struct smb2_transform_hdr *thdr =
			(struct smb2_transform_hdr *)buf;
		struct cifs_ses *ses = NULL;
		struct cifs_ses *iter;

		 
		spin_lock(&cifs_tcp_ses_lock);
		list_for_each_entry(iter, &pserver->smb_ses_list, smb_ses_list) {
			if (iter->Suid == le64_to_cpu(thdr->SessionId)) {
				ses = iter;
				break;
			}
		}
		spin_unlock(&cifs_tcp_ses_lock);
		if (!ses) {
			cifs_dbg(VFS, "no decryption - session id not found\n");
			return 1;
		}
	}

	mid = le64_to_cpu(shdr->MessageId);
	if (check_smb2_hdr(shdr, mid))
		return 1;

	if (shdr->StructureSize != SMB2_HEADER_STRUCTURE_SIZE) {
		cifs_dbg(VFS, "Invalid structure size %u\n",
			 le16_to_cpu(shdr->StructureSize));
		return 1;
	}

	command = le16_to_cpu(shdr->Command);
	if (command >= NUMBER_OF_SMB2_COMMANDS) {
		cifs_dbg(VFS, "Invalid SMB2 command %d\n", command);
		return 1;
	}

	if (len < pdu_size) {
		if ((len >= hdr_size)
		    && (shdr->Status != 0)) {
			pdu->StructureSize2 = 0;
			 
			return 0;
		} else {
			cifs_dbg(VFS, "Length less than SMB header size\n");
		}
		return 1;
	}
	if (len > CIFSMaxBufSize + MAX_SMB2_HDR_SIZE) {
		cifs_dbg(VFS, "SMB length greater than maximum, mid=%llu\n",
			 mid);
		return 1;
	}

	if (smb2_rsp_struct_sizes[command] != pdu->StructureSize2) {
		if (command != SMB2_OPLOCK_BREAK_HE && (shdr->Status == 0 ||
		    pdu->StructureSize2 != SMB2_ERROR_STRUCTURE_SIZE2_LE)) {
			 
			cifs_dbg(VFS, "Invalid response size %u for command %d\n",
				 le16_to_cpu(pdu->StructureSize2), command);
			return 1;
		} else if (command == SMB2_OPLOCK_BREAK_HE
			   && (shdr->Status == 0)
			   && (le16_to_cpu(pdu->StructureSize2) != 44)
			   && (le16_to_cpu(pdu->StructureSize2) != 36)) {
			 
			cifs_dbg(VFS, "Invalid response size %d for oplock break\n",
				 le16_to_cpu(pdu->StructureSize2));
			return 1;
		}
	}

	calc_len = smb2_calc_size(buf);

	 
	if (command == SMB2_IOCTL_HE && calc_len == 0)
		return 0;

	if (command == SMB2_NEGOTIATE_HE)
		calc_len += get_neg_ctxt_len(shdr, len, calc_len);

	if (len != calc_len) {
		 
		if (command == SMB2_CREATE_HE &&
		    shdr->Status == STATUS_STOPPED_ON_SYMLINK)
			return 0;
		 
		if (calc_len + 24 == len && command == SMB2_OPLOCK_BREAK_HE)
			return 0;
		 
		if (calc_len == len + 1)
			return 0;

		 
		if (ALIGN(calc_len, 8) == len)
			return 0;

		 
		if (calc_len < len)
			return 0;

		 
		if (unlikely(cifsFYI))
			cifs_dbg(FYI, "Server response too short: calculated "
				 "length %u doesn't match read length %u (cmd=%d, mid=%llu)\n",
				 calc_len, len, command, mid);
		else
			pr_warn("Server response too short: calculated length "
				"%u doesn't match read length %u (cmd=%d, mid=%llu)\n",
				calc_len, len, command, mid);

		return 1;
	}
	return 0;
}

 
static const bool has_smb2_data_area[NUMBER_OF_SMB2_COMMANDS] = {
	  true,
	  true,
	  false,
	 	false,
	  false,
	  true,
	  false,
	  false,
	 	true,
	  false,
	 	false,
	  true,
	  false,  
	  false,
	  true,
	  true,
	  true,
	  false,
	  false
};

 
char *
smb2_get_data_area_len(int *off, int *len, struct smb2_hdr *shdr)
{
	const int max_off = 4096;
	const int max_len = 128 * 1024;

	*off = 0;
	*len = 0;

	 
	if (shdr->Status && shdr->Status != STATUS_MORE_PROCESSING_REQUIRED &&
	    (((struct smb2_err_rsp *)shdr)->StructureSize) ==
						SMB2_ERROR_STRUCTURE_SIZE2_LE)
		return NULL;

	 
	switch (shdr->Command) {
	case SMB2_NEGOTIATE:
		*off = le16_to_cpu(
		  ((struct smb2_negotiate_rsp *)shdr)->SecurityBufferOffset);
		*len = le16_to_cpu(
		  ((struct smb2_negotiate_rsp *)shdr)->SecurityBufferLength);
		break;
	case SMB2_SESSION_SETUP:
		*off = le16_to_cpu(
		  ((struct smb2_sess_setup_rsp *)shdr)->SecurityBufferOffset);
		*len = le16_to_cpu(
		  ((struct smb2_sess_setup_rsp *)shdr)->SecurityBufferLength);
		break;
	case SMB2_CREATE:
		*off = le32_to_cpu(
		    ((struct smb2_create_rsp *)shdr)->CreateContextsOffset);
		*len = le32_to_cpu(
		    ((struct smb2_create_rsp *)shdr)->CreateContextsLength);
		break;
	case SMB2_QUERY_INFO:
		*off = le16_to_cpu(
		    ((struct smb2_query_info_rsp *)shdr)->OutputBufferOffset);
		*len = le32_to_cpu(
		    ((struct smb2_query_info_rsp *)shdr)->OutputBufferLength);
		break;
	case SMB2_READ:
		 
		*off = ((struct smb2_read_rsp *)shdr)->DataOffset;
		*len = le32_to_cpu(((struct smb2_read_rsp *)shdr)->DataLength);
		break;
	case SMB2_QUERY_DIRECTORY:
		*off = le16_to_cpu(
		  ((struct smb2_query_directory_rsp *)shdr)->OutputBufferOffset);
		*len = le32_to_cpu(
		  ((struct smb2_query_directory_rsp *)shdr)->OutputBufferLength);
		break;
	case SMB2_IOCTL:
		*off = le32_to_cpu(
		  ((struct smb2_ioctl_rsp *)shdr)->OutputOffset);
		*len = le32_to_cpu(
		  ((struct smb2_ioctl_rsp *)shdr)->OutputCount);
		break;
	case SMB2_CHANGE_NOTIFY:
		*off = le16_to_cpu(
		  ((struct smb2_change_notify_rsp *)shdr)->OutputBufferOffset);
		*len = le32_to_cpu(
		  ((struct smb2_change_notify_rsp *)shdr)->OutputBufferLength);
		break;
	default:
		cifs_dbg(VFS, "no length check for command %d\n", le16_to_cpu(shdr->Command));
		break;
	}

	 
	if (unlikely(*off < 0 || *off > max_off ||
		     *len < 0 || *len > max_len)) {
		cifs_dbg(VFS, "%s: invalid data area (off=%d len=%d)\n",
			 __func__, *off, *len);
		*off = 0;
		*len = 0;
	} else if (*off == 0) {
		*len = 0;
	}

	 
	if (*off > 0 && *len > 0)
		return (char *)shdr + *off;
	return NULL;
}

 
unsigned int
smb2_calc_size(void *buf)
{
	struct smb2_pdu *pdu = buf;
	struct smb2_hdr *shdr = &pdu->hdr;
	int offset;  
	int data_length;  
	 
	int len = le16_to_cpu(shdr->StructureSize);

	 
	len += le16_to_cpu(pdu->StructureSize2);

	if (has_smb2_data_area[le16_to_cpu(shdr->Command)] == false)
		goto calc_size_exit;

	smb2_get_data_area_len(&offset, &data_length, shdr);
	cifs_dbg(FYI, "SMB2 data length %d offset %d\n", data_length, offset);

	if (data_length > 0) {
		 
		if (offset + 1 < len) {
			cifs_dbg(VFS, "data area offset %d overlaps SMB2 header %d\n",
				 offset + 1, len);
			data_length = 0;
		} else {
			len = offset + data_length;
		}
	}
calc_size_exit:
	cifs_dbg(FYI, "SMB2 len %d\n", len);
	return len;
}

 
__le16 *
cifs_convert_path_to_utf16(const char *from, struct cifs_sb_info *cifs_sb)
{
	int len;
	const char *start_of_path;
	__le16 *to;
	int map_type;

	if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_MAP_SFM_CHR)
		map_type = SFM_MAP_UNI_RSVD;
	else if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_MAP_SPECIAL_CHR)
		map_type = SFU_MAP_UNI_RSVD;
	else
		map_type = NO_MAP_UNI_RSVD;

	 
	if (from[0] == '\\')
		start_of_path = from + 1;

	 
	else if (cifs_sb_master_tlink(cifs_sb) &&
		 cifs_sb_master_tcon(cifs_sb)->posix_extensions &&
		 (from[0] == '/')) {
		start_of_path = from + 1;
	} else
		start_of_path = from;

	to = cifs_strndup_to_utf16(start_of_path, PATH_MAX, &len,
				   cifs_sb->local_nls, map_type);
	return to;
}

__le32
smb2_get_lease_state(struct cifsInodeInfo *cinode)
{
	__le32 lease = 0;

	if (CIFS_CACHE_WRITE(cinode))
		lease |= SMB2_LEASE_WRITE_CACHING_LE;
	if (CIFS_CACHE_HANDLE(cinode))
		lease |= SMB2_LEASE_HANDLE_CACHING_LE;
	if (CIFS_CACHE_READ(cinode))
		lease |= SMB2_LEASE_READ_CACHING_LE;
	return lease;
}

struct smb2_lease_break_work {
	struct work_struct lease_break;
	struct tcon_link *tlink;
	__u8 lease_key[16];
	__le32 lease_state;
};

static void
cifs_ses_oplock_break(struct work_struct *work)
{
	struct smb2_lease_break_work *lw = container_of(work,
				struct smb2_lease_break_work, lease_break);
	int rc = 0;

	rc = SMB2_lease_break(0, tlink_tcon(lw->tlink), lw->lease_key,
			      lw->lease_state);

	cifs_dbg(FYI, "Lease release rc %d\n", rc);
	cifs_put_tlink(lw->tlink);
	kfree(lw);
}

static void
smb2_queue_pending_open_break(struct tcon_link *tlink, __u8 *lease_key,
			      __le32 new_lease_state)
{
	struct smb2_lease_break_work *lw;

	lw = kmalloc(sizeof(struct smb2_lease_break_work), GFP_KERNEL);
	if (!lw) {
		cifs_put_tlink(tlink);
		return;
	}

	INIT_WORK(&lw->lease_break, cifs_ses_oplock_break);
	lw->tlink = tlink;
	lw->lease_state = new_lease_state;
	memcpy(lw->lease_key, lease_key, SMB2_LEASE_KEY_SIZE);
	queue_work(cifsiod_wq, &lw->lease_break);
}

static bool
smb2_tcon_has_lease(struct cifs_tcon *tcon, struct smb2_lease_break *rsp)
{
	__u8 lease_state;
	struct cifsFileInfo *cfile;
	struct cifsInodeInfo *cinode;
	int ack_req = le32_to_cpu(rsp->Flags &
				  SMB2_NOTIFY_BREAK_LEASE_FLAG_ACK_REQUIRED);

	lease_state = le32_to_cpu(rsp->NewLeaseState);

	list_for_each_entry(cfile, &tcon->openFileList, tlist) {
		cinode = CIFS_I(d_inode(cfile->dentry));

		if (memcmp(cinode->lease_key, rsp->LeaseKey,
							SMB2_LEASE_KEY_SIZE))
			continue;

		cifs_dbg(FYI, "found in the open list\n");
		cifs_dbg(FYI, "lease key match, lease break 0x%x\n",
			 lease_state);

		if (ack_req)
			cfile->oplock_break_cancelled = false;
		else
			cfile->oplock_break_cancelled = true;

		set_bit(CIFS_INODE_PENDING_OPLOCK_BREAK, &cinode->flags);

		cfile->oplock_epoch = le16_to_cpu(rsp->Epoch);
		cfile->oplock_level = lease_state;

		cifs_queue_oplock_break(cfile);
		return true;
	}

	return false;
}

static struct cifs_pending_open *
smb2_tcon_find_pending_open_lease(struct cifs_tcon *tcon,
				  struct smb2_lease_break *rsp)
{
	__u8 lease_state = le32_to_cpu(rsp->NewLeaseState);
	int ack_req = le32_to_cpu(rsp->Flags &
				  SMB2_NOTIFY_BREAK_LEASE_FLAG_ACK_REQUIRED);
	struct cifs_pending_open *open;
	struct cifs_pending_open *found = NULL;

	list_for_each_entry(open, &tcon->pending_opens, olist) {
		if (memcmp(open->lease_key, rsp->LeaseKey,
			   SMB2_LEASE_KEY_SIZE))
			continue;

		if (!found && ack_req) {
			found = open;
		}

		cifs_dbg(FYI, "found in the pending open list\n");
		cifs_dbg(FYI, "lease key match, lease break 0x%x\n",
			 lease_state);

		open->oplock = lease_state;
	}

	return found;
}

static bool
smb2_is_valid_lease_break(char *buffer, struct TCP_Server_Info *server)
{
	struct smb2_lease_break *rsp = (struct smb2_lease_break *)buffer;
	struct TCP_Server_Info *pserver;
	struct cifs_ses *ses;
	struct cifs_tcon *tcon;
	struct cifs_pending_open *open;

	cifs_dbg(FYI, "Checking for lease break\n");

	 
	pserver = SERVER_IS_CHAN(server) ? server->primary_server : server;

	 
	spin_lock(&cifs_tcp_ses_lock);
	list_for_each_entry(ses, &pserver->smb_ses_list, smb_ses_list) {
		list_for_each_entry(tcon, &ses->tcon_list, tcon_list) {
			spin_lock(&tcon->open_file_lock);
			cifs_stats_inc(
				       &tcon->stats.cifs_stats.num_oplock_brks);
			if (smb2_tcon_has_lease(tcon, rsp)) {
				spin_unlock(&tcon->open_file_lock);
				spin_unlock(&cifs_tcp_ses_lock);
				return true;
			}
			open = smb2_tcon_find_pending_open_lease(tcon,
								 rsp);
			if (open) {
				__u8 lease_key[SMB2_LEASE_KEY_SIZE];
				struct tcon_link *tlink;

				tlink = cifs_get_tlink(open->tlink);
				memcpy(lease_key, open->lease_key,
				       SMB2_LEASE_KEY_SIZE);
				spin_unlock(&tcon->open_file_lock);
				spin_unlock(&cifs_tcp_ses_lock);
				smb2_queue_pending_open_break(tlink,
							      lease_key,
							      rsp->NewLeaseState);
				return true;
			}
			spin_unlock(&tcon->open_file_lock);

			if (cached_dir_lease_break(tcon, rsp->LeaseKey)) {
				spin_unlock(&cifs_tcp_ses_lock);
				return true;
			}
		}
	}
	spin_unlock(&cifs_tcp_ses_lock);
	cifs_dbg(FYI, "Can not process lease break - no lease matched\n");
	trace_smb3_lease_not_found(le32_to_cpu(rsp->CurrentLeaseState),
				   le32_to_cpu(rsp->hdr.Id.SyncId.TreeId),
				   le64_to_cpu(rsp->hdr.SessionId),
				   *((u64 *)rsp->LeaseKey),
				   *((u64 *)&rsp->LeaseKey[8]));

	return false;
}

bool
smb2_is_valid_oplock_break(char *buffer, struct TCP_Server_Info *server)
{
	struct smb2_oplock_break *rsp = (struct smb2_oplock_break *)buffer;
	struct TCP_Server_Info *pserver;
	struct cifs_ses *ses;
	struct cifs_tcon *tcon;
	struct cifsInodeInfo *cinode;
	struct cifsFileInfo *cfile;

	cifs_dbg(FYI, "Checking for oplock break\n");

	if (rsp->hdr.Command != SMB2_OPLOCK_BREAK)
		return false;

	if (rsp->StructureSize !=
				smb2_rsp_struct_sizes[SMB2_OPLOCK_BREAK_HE]) {
		if (le16_to_cpu(rsp->StructureSize) == 44)
			return smb2_is_valid_lease_break(buffer, server);
		else
			return false;
	}

	cifs_dbg(FYI, "oplock level 0x%x\n", rsp->OplockLevel);

	 
	pserver = SERVER_IS_CHAN(server) ? server->primary_server : server;

	 
	spin_lock(&cifs_tcp_ses_lock);
	list_for_each_entry(ses, &pserver->smb_ses_list, smb_ses_list) {
		list_for_each_entry(tcon, &ses->tcon_list, tcon_list) {

			spin_lock(&tcon->open_file_lock);
			list_for_each_entry(cfile, &tcon->openFileList, tlist) {
				if (rsp->PersistentFid !=
				    cfile->fid.persistent_fid ||
				    rsp->VolatileFid !=
				    cfile->fid.volatile_fid)
					continue;

				cifs_dbg(FYI, "file id match, oplock break\n");
				cifs_stats_inc(
				    &tcon->stats.cifs_stats.num_oplock_brks);
				cinode = CIFS_I(d_inode(cfile->dentry));
				spin_lock(&cfile->file_info_lock);
				if (!CIFS_CACHE_WRITE(cinode) &&
				    rsp->OplockLevel == SMB2_OPLOCK_LEVEL_NONE)
					cfile->oplock_break_cancelled = true;
				else
					cfile->oplock_break_cancelled = false;

				set_bit(CIFS_INODE_PENDING_OPLOCK_BREAK,
					&cinode->flags);

				cfile->oplock_epoch = 0;
				cfile->oplock_level = rsp->OplockLevel;

				spin_unlock(&cfile->file_info_lock);

				cifs_queue_oplock_break(cfile);

				spin_unlock(&tcon->open_file_lock);
				spin_unlock(&cifs_tcp_ses_lock);
				return true;
			}
			spin_unlock(&tcon->open_file_lock);
		}
	}
	spin_unlock(&cifs_tcp_ses_lock);
	cifs_dbg(FYI, "No file id matched, oplock break ignored\n");
	trace_smb3_oplock_not_found(0  , rsp->PersistentFid,
				  le32_to_cpu(rsp->hdr.Id.SyncId.TreeId),
				  le64_to_cpu(rsp->hdr.SessionId));

	return true;
}

void
smb2_cancelled_close_fid(struct work_struct *work)
{
	struct close_cancelled_open *cancelled = container_of(work,
					struct close_cancelled_open, work);
	struct cifs_tcon *tcon = cancelled->tcon;
	int rc;

	if (cancelled->mid)
		cifs_tcon_dbg(VFS, "Close unmatched open for MID:%llu\n",
			      cancelled->mid);
	else
		cifs_tcon_dbg(VFS, "Close interrupted close\n");

	rc = SMB2_close(0, tcon, cancelled->fid.persistent_fid,
			cancelled->fid.volatile_fid);
	if (rc)
		cifs_tcon_dbg(VFS, "Close cancelled mid failed rc:%d\n", rc);

	cifs_put_tcon(tcon);
	kfree(cancelled);
}

 
static int
__smb2_handle_cancelled_cmd(struct cifs_tcon *tcon, __u16 cmd, __u64 mid,
			    __u64 persistent_fid, __u64 volatile_fid)
{
	struct close_cancelled_open *cancelled;

	cancelled = kzalloc(sizeof(*cancelled), GFP_KERNEL);
	if (!cancelled)
		return -ENOMEM;

	cancelled->fid.persistent_fid = persistent_fid;
	cancelled->fid.volatile_fid = volatile_fid;
	cancelled->tcon = tcon;
	cancelled->cmd = cmd;
	cancelled->mid = mid;
	INIT_WORK(&cancelled->work, smb2_cancelled_close_fid);
	WARN_ON(queue_work(cifsiod_wq, &cancelled->work) == false);

	return 0;
}

int
smb2_handle_cancelled_close(struct cifs_tcon *tcon, __u64 persistent_fid,
			    __u64 volatile_fid)
{
	int rc;

	cifs_dbg(FYI, "%s: tc_count=%d\n", __func__, tcon->tc_count);
	spin_lock(&cifs_tcp_ses_lock);
	if (tcon->tc_count <= 0) {
		struct TCP_Server_Info *server = NULL;

		WARN_ONCE(tcon->tc_count < 0, "tcon refcount is negative");
		spin_unlock(&cifs_tcp_ses_lock);

		if (tcon->ses)
			server = tcon->ses->server;

		cifs_server_dbg(FYI, "tid=0x%x: tcon is closing, skipping async close retry of fid %llu %llu\n",
				tcon->tid, persistent_fid, volatile_fid);

		return 0;
	}
	tcon->tc_count++;
	spin_unlock(&cifs_tcp_ses_lock);

	rc = __smb2_handle_cancelled_cmd(tcon, SMB2_CLOSE_HE, 0,
					 persistent_fid, volatile_fid);
	if (rc)
		cifs_put_tcon(tcon);

	return rc;
}

int
smb2_handle_cancelled_mid(struct mid_q_entry *mid, struct TCP_Server_Info *server)
{
	struct smb2_hdr *hdr = mid->resp_buf;
	struct smb2_create_rsp *rsp = mid->resp_buf;
	struct cifs_tcon *tcon;
	int rc;

	if ((mid->optype & CIFS_CP_CREATE_CLOSE_OP) || hdr->Command != SMB2_CREATE ||
	    hdr->Status != STATUS_SUCCESS)
		return 0;

	tcon = smb2_find_smb_tcon(server, le64_to_cpu(hdr->SessionId),
				  le32_to_cpu(hdr->Id.SyncId.TreeId));
	if (!tcon)
		return -ENOENT;

	rc = __smb2_handle_cancelled_cmd(tcon,
					 le16_to_cpu(hdr->Command),
					 le64_to_cpu(hdr->MessageId),
					 rsp->PersistentFileId,
					 rsp->VolatileFileId);
	if (rc)
		cifs_put_tcon(tcon);

	return rc;
}

 
int
smb311_update_preauth_hash(struct cifs_ses *ses, struct TCP_Server_Info *server,
			   struct kvec *iov, int nvec)
{
	int i, rc;
	struct smb2_hdr *hdr;
	struct shash_desc *sha512 = NULL;

	hdr = (struct smb2_hdr *)iov[0].iov_base;
	 
	if (hdr->Command == SMB2_NEGOTIATE)
		goto ok;

	 
	if (server->dialect != SMB311_PROT_ID)
		return 0;

	if (hdr->Command != SMB2_SESSION_SETUP)
		return 0;

	 
	if ((hdr->Flags & SMB2_FLAGS_SERVER_TO_REDIR)
	    && (hdr->Status == NT_STATUS_OK
		|| (hdr->Status !=
		    cpu_to_le32(NT_STATUS_MORE_PROCESSING_REQUIRED))))
		return 0;

ok:
	rc = smb311_crypto_shash_allocate(server);
	if (rc)
		return rc;

	sha512 = server->secmech.sha512;
	rc = crypto_shash_init(sha512);
	if (rc) {
		cifs_dbg(VFS, "%s: Could not init sha512 shash\n", __func__);
		return rc;
	}

	rc = crypto_shash_update(sha512, ses->preauth_sha_hash,
				 SMB2_PREAUTH_HASH_SIZE);
	if (rc) {
		cifs_dbg(VFS, "%s: Could not update sha512 shash\n", __func__);
		return rc;
	}

	for (i = 0; i < nvec; i++) {
		rc = crypto_shash_update(sha512, iov[i].iov_base, iov[i].iov_len);
		if (rc) {
			cifs_dbg(VFS, "%s: Could not update sha512 shash\n",
				 __func__);
			return rc;
		}
	}

	rc = crypto_shash_final(sha512, ses->preauth_sha_hash);
	if (rc) {
		cifs_dbg(VFS, "%s: Could not finalize sha512 shash\n",
			 __func__);
		return rc;
	}

	return 0;
}
