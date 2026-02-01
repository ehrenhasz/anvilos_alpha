
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_bit.h"
#include "xfs_sb.h"
#include "xfs_mount.h"
#include "xfs_defer.h"
#include "xfs_inode.h"
#include "xfs_trans.h"
#include "xfs_log.h"
#include "xfs_log_priv.h"
#include "xfs_log_recover.h"
#include "xfs_trans_priv.h"
#include "xfs_alloc.h"
#include "xfs_ialloc.h"
#include "xfs_trace.h"
#include "xfs_icache.h"
#include "xfs_error.h"
#include "xfs_buf_item.h"
#include "xfs_ag.h"
#include "xfs_quota.h"
#include "xfs_reflink.h"

#define BLK_AVG(blk1, blk2)	((blk1+blk2) >> 1)

STATIC int
xlog_find_zeroed(
	struct xlog	*,
	xfs_daddr_t	*);
STATIC int
xlog_clear_stale_blocks(
	struct xlog	*,
	xfs_lsn_t);
STATIC int
xlog_do_recovery_pass(
        struct xlog *, xfs_daddr_t, xfs_daddr_t, int, xfs_daddr_t *);

 

 
static inline bool
xlog_verify_bno(
	struct xlog	*log,
	xfs_daddr_t	blk_no,
	int		bbcount)
{
	if (blk_no < 0 || blk_no >= log->l_logBBsize)
		return false;
	if (bbcount <= 0 || (blk_no + bbcount) > log->l_logBBsize)
		return false;
	return true;
}

 
static char *
xlog_alloc_buffer(
	struct xlog	*log,
	int		nbblks)
{
	 
	if (XFS_IS_CORRUPT(log->l_mp, !xlog_verify_bno(log, 0, nbblks))) {
		xfs_warn(log->l_mp, "Invalid block length (0x%x) for buffer",
			nbblks);
		return NULL;
	}

	 
	if (nbblks > 1 && log->l_sectBBsize > 1)
		nbblks += log->l_sectBBsize;
	nbblks = round_up(nbblks, log->l_sectBBsize);
	return kvzalloc(BBTOB(nbblks), GFP_KERNEL | __GFP_RETRY_MAYFAIL);
}

 
static inline unsigned int
xlog_align(
	struct xlog	*log,
	xfs_daddr_t	blk_no)
{
	return BBTOB(blk_no & ((xfs_daddr_t)log->l_sectBBsize - 1));
}

static int
xlog_do_io(
	struct xlog		*log,
	xfs_daddr_t		blk_no,
	unsigned int		nbblks,
	char			*data,
	enum req_op		op)
{
	int			error;

	if (XFS_IS_CORRUPT(log->l_mp, !xlog_verify_bno(log, blk_no, nbblks))) {
		xfs_warn(log->l_mp,
			 "Invalid log block/length (0x%llx, 0x%x) for buffer",
			 blk_no, nbblks);
		return -EFSCORRUPTED;
	}

	blk_no = round_down(blk_no, log->l_sectBBsize);
	nbblks = round_up(nbblks, log->l_sectBBsize);
	ASSERT(nbblks > 0);

	error = xfs_rw_bdev(log->l_targ->bt_bdev, log->l_logBBstart + blk_no,
			BBTOB(nbblks), data, op);
	if (error && !xlog_is_shutdown(log)) {
		xfs_alert(log->l_mp,
			  "log recovery %s I/O error at daddr 0x%llx len %d error %d",
			  op == REQ_OP_WRITE ? "write" : "read",
			  blk_no, nbblks, error);
	}
	return error;
}

STATIC int
xlog_bread_noalign(
	struct xlog	*log,
	xfs_daddr_t	blk_no,
	int		nbblks,
	char		*data)
{
	return xlog_do_io(log, blk_no, nbblks, data, REQ_OP_READ);
}

STATIC int
xlog_bread(
	struct xlog	*log,
	xfs_daddr_t	blk_no,
	int		nbblks,
	char		*data,
	char		**offset)
{
	int		error;

	error = xlog_do_io(log, blk_no, nbblks, data, REQ_OP_READ);
	if (!error)
		*offset = data + xlog_align(log, blk_no);
	return error;
}

STATIC int
xlog_bwrite(
	struct xlog	*log,
	xfs_daddr_t	blk_no,
	int		nbblks,
	char		*data)
{
	return xlog_do_io(log, blk_no, nbblks, data, REQ_OP_WRITE);
}

#ifdef DEBUG
 
STATIC void
xlog_header_check_dump(
	xfs_mount_t		*mp,
	xlog_rec_header_t	*head)
{
	xfs_debug(mp, "%s:  SB : uuid = %pU, fmt = %d",
		__func__, &mp->m_sb.sb_uuid, XLOG_FMT);
	xfs_debug(mp, "    log : uuid = %pU, fmt = %d",
		&head->h_fs_uuid, be32_to_cpu(head->h_fmt));
}
#else
#define xlog_header_check_dump(mp, head)
#endif

 
STATIC int
xlog_header_check_recover(
	xfs_mount_t		*mp,
	xlog_rec_header_t	*head)
{
	ASSERT(head->h_magicno == cpu_to_be32(XLOG_HEADER_MAGIC_NUM));

	 
	if (XFS_IS_CORRUPT(mp, head->h_fmt != cpu_to_be32(XLOG_FMT))) {
		xfs_warn(mp,
	"dirty log written in incompatible format - can't recover");
		xlog_header_check_dump(mp, head);
		return -EFSCORRUPTED;
	}
	if (XFS_IS_CORRUPT(mp, !uuid_equal(&mp->m_sb.sb_uuid,
					   &head->h_fs_uuid))) {
		xfs_warn(mp,
	"dirty log entry has mismatched uuid - can't recover");
		xlog_header_check_dump(mp, head);
		return -EFSCORRUPTED;
	}
	return 0;
}

 
STATIC int
xlog_header_check_mount(
	xfs_mount_t		*mp,
	xlog_rec_header_t	*head)
{
	ASSERT(head->h_magicno == cpu_to_be32(XLOG_HEADER_MAGIC_NUM));

	if (uuid_is_null(&head->h_fs_uuid)) {
		 
		xfs_warn(mp, "null uuid in log - IRIX style log");
	} else if (XFS_IS_CORRUPT(mp, !uuid_equal(&mp->m_sb.sb_uuid,
						  &head->h_fs_uuid))) {
		xfs_warn(mp, "log has mismatched uuid - can't recover");
		xlog_header_check_dump(mp, head);
		return -EFSCORRUPTED;
	}
	return 0;
}

 
STATIC int
xlog_find_cycle_start(
	struct xlog	*log,
	char		*buffer,
	xfs_daddr_t	first_blk,
	xfs_daddr_t	*last_blk,
	uint		cycle)
{
	char		*offset;
	xfs_daddr_t	mid_blk;
	xfs_daddr_t	end_blk;
	uint		mid_cycle;
	int		error;

	end_blk = *last_blk;
	mid_blk = BLK_AVG(first_blk, end_blk);
	while (mid_blk != first_blk && mid_blk != end_blk) {
		error = xlog_bread(log, mid_blk, 1, buffer, &offset);
		if (error)
			return error;
		mid_cycle = xlog_get_cycle(offset);
		if (mid_cycle == cycle)
			end_blk = mid_blk;    
		else
			first_blk = mid_blk;  
		mid_blk = BLK_AVG(first_blk, end_blk);
	}
	ASSERT((mid_blk == first_blk && mid_blk+1 == end_blk) ||
	       (mid_blk == end_blk && mid_blk-1 == first_blk));

	*last_blk = end_blk;

	return 0;
}

 
STATIC int
xlog_find_verify_cycle(
	struct xlog	*log,
	xfs_daddr_t	start_blk,
	int		nbblks,
	uint		stop_on_cycle_no,
	xfs_daddr_t	*new_blk)
{
	xfs_daddr_t	i, j;
	uint		cycle;
	char		*buffer;
	xfs_daddr_t	bufblks;
	char		*buf = NULL;
	int		error = 0;

	 
	bufblks = roundup_pow_of_two(nbblks);
	while (bufblks > log->l_logBBsize)
		bufblks >>= 1;
	while (!(buffer = xlog_alloc_buffer(log, bufblks))) {
		bufblks >>= 1;
		if (bufblks < log->l_sectBBsize)
			return -ENOMEM;
	}

	for (i = start_blk; i < start_blk + nbblks; i += bufblks) {
		int	bcount;

		bcount = min(bufblks, (start_blk + nbblks - i));

		error = xlog_bread(log, i, bcount, buffer, &buf);
		if (error)
			goto out;

		for (j = 0; j < bcount; j++) {
			cycle = xlog_get_cycle(buf);
			if (cycle == stop_on_cycle_no) {
				*new_blk = i+j;
				goto out;
			}

			buf += BBSIZE;
		}
	}

	*new_blk = -1;

out:
	kmem_free(buffer);
	return error;
}

static inline int
xlog_logrec_hblks(struct xlog *log, struct xlog_rec_header *rh)
{
	if (xfs_has_logv2(log->l_mp)) {
		int	h_size = be32_to_cpu(rh->h_size);

		if ((be32_to_cpu(rh->h_version) & XLOG_VERSION_2) &&
		    h_size > XLOG_HEADER_CYCLE_SIZE)
			return DIV_ROUND_UP(h_size, XLOG_HEADER_CYCLE_SIZE);
	}
	return 1;
}

 
STATIC int
xlog_find_verify_log_record(
	struct xlog		*log,
	xfs_daddr_t		start_blk,
	xfs_daddr_t		*last_blk,
	int			extra_bblks)
{
	xfs_daddr_t		i;
	char			*buffer;
	char			*offset = NULL;
	xlog_rec_header_t	*head = NULL;
	int			error = 0;
	int			smallmem = 0;
	int			num_blks = *last_blk - start_blk;
	int			xhdrs;

	ASSERT(start_blk != 0 || *last_blk != start_blk);

	buffer = xlog_alloc_buffer(log, num_blks);
	if (!buffer) {
		buffer = xlog_alloc_buffer(log, 1);
		if (!buffer)
			return -ENOMEM;
		smallmem = 1;
	} else {
		error = xlog_bread(log, start_blk, num_blks, buffer, &offset);
		if (error)
			goto out;
		offset += ((num_blks - 1) << BBSHIFT);
	}

	for (i = (*last_blk) - 1; i >= 0; i--) {
		if (i < start_blk) {
			 
			xfs_warn(log->l_mp,
		"Log inconsistent (didn't find previous header)");
			ASSERT(0);
			error = -EFSCORRUPTED;
			goto out;
		}

		if (smallmem) {
			error = xlog_bread(log, i, 1, buffer, &offset);
			if (error)
				goto out;
		}

		head = (xlog_rec_header_t *)offset;

		if (head->h_magicno == cpu_to_be32(XLOG_HEADER_MAGIC_NUM))
			break;

		if (!smallmem)
			offset -= BBSIZE;
	}

	 
	if (i == -1) {
		error = 1;
		goto out;
	}

	 
	if ((error = xlog_header_check_mount(log->l_mp, head)))
		goto out;

	 
	xhdrs = xlog_logrec_hblks(log, head);

	if (*last_blk - i + extra_bblks !=
	    BTOBB(be32_to_cpu(head->h_len)) + xhdrs)
		*last_blk = i;

out:
	kmem_free(buffer);
	return error;
}

 
STATIC int
xlog_find_head(
	struct xlog	*log,
	xfs_daddr_t	*return_head_blk)
{
	char		*buffer;
	char		*offset;
	xfs_daddr_t	new_blk, first_blk, start_blk, last_blk, head_blk;
	int		num_scan_bblks;
	uint		first_half_cycle, last_half_cycle;
	uint		stop_on_cycle;
	int		error, log_bbnum = log->l_logBBsize;

	 
	error = xlog_find_zeroed(log, &first_blk);
	if (error < 0) {
		xfs_warn(log->l_mp, "empty log check failed");
		return error;
	}
	if (error == 1) {
		*return_head_blk = first_blk;

		 
		if (!first_blk) {
			 
			xfs_warn(log->l_mp, "totally zeroed log");
		}

		return 0;
	}

	first_blk = 0;			 
	buffer = xlog_alloc_buffer(log, 1);
	if (!buffer)
		return -ENOMEM;

	error = xlog_bread(log, 0, 1, buffer, &offset);
	if (error)
		goto out_free_buffer;

	first_half_cycle = xlog_get_cycle(offset);

	last_blk = head_blk = log_bbnum - 1;	 
	error = xlog_bread(log, last_blk, 1, buffer, &offset);
	if (error)
		goto out_free_buffer;

	last_half_cycle = xlog_get_cycle(offset);
	ASSERT(last_half_cycle != 0);

	 
	if (first_half_cycle == last_half_cycle) {
		 
		head_blk = log_bbnum;
		stop_on_cycle = last_half_cycle - 1;
	} else {
		 
		stop_on_cycle = last_half_cycle;
		error = xlog_find_cycle_start(log, buffer, first_blk, &head_blk,
				last_half_cycle);
		if (error)
			goto out_free_buffer;
	}

	 
	num_scan_bblks = min_t(int, log_bbnum, XLOG_TOTAL_REC_SHIFT(log));
	if (head_blk >= num_scan_bblks) {
		 
		start_blk = head_blk - num_scan_bblks;
		if ((error = xlog_find_verify_cycle(log,
						start_blk, num_scan_bblks,
						stop_on_cycle, &new_blk)))
			goto out_free_buffer;
		if (new_blk != -1)
			head_blk = new_blk;
	} else {		 
		 
		ASSERT(head_blk <= INT_MAX &&
			(xfs_daddr_t) num_scan_bblks >= head_blk);
		start_blk = log_bbnum - (num_scan_bblks - head_blk);
		if ((error = xlog_find_verify_cycle(log, start_blk,
					num_scan_bblks - (int)head_blk,
					(stop_on_cycle - 1), &new_blk)))
			goto out_free_buffer;
		if (new_blk != -1) {
			head_blk = new_blk;
			goto validate_head;
		}

		 
		start_blk = 0;
		ASSERT(head_blk <= INT_MAX);
		if ((error = xlog_find_verify_cycle(log,
					start_blk, (int)head_blk,
					stop_on_cycle, &new_blk)))
			goto out_free_buffer;
		if (new_blk != -1)
			head_blk = new_blk;
	}

validate_head:
	 
	num_scan_bblks = XLOG_REC_SHIFT(log);
	if (head_blk >= num_scan_bblks) {
		start_blk = head_blk - num_scan_bblks;  

		 
		error = xlog_find_verify_log_record(log, start_blk, &head_blk, 0);
		if (error == 1)
			error = -EIO;
		if (error)
			goto out_free_buffer;
	} else {
		start_blk = 0;
		ASSERT(head_blk <= INT_MAX);
		error = xlog_find_verify_log_record(log, start_blk, &head_blk, 0);
		if (error < 0)
			goto out_free_buffer;
		if (error == 1) {
			 
			start_blk = log_bbnum - (num_scan_bblks - head_blk);
			new_blk = log_bbnum;
			ASSERT(start_blk <= INT_MAX &&
				(xfs_daddr_t) log_bbnum-start_blk >= 0);
			ASSERT(head_blk <= INT_MAX);
			error = xlog_find_verify_log_record(log, start_blk,
							&new_blk, (int)head_blk);
			if (error == 1)
				error = -EIO;
			if (error)
				goto out_free_buffer;
			if (new_blk != log_bbnum)
				head_blk = new_blk;
		} else if (error)
			goto out_free_buffer;
	}

	kmem_free(buffer);
	if (head_blk == log_bbnum)
		*return_head_blk = 0;
	else
		*return_head_blk = head_blk;
	 
	return 0;

out_free_buffer:
	kmem_free(buffer);
	if (error)
		xfs_warn(log->l_mp, "failed to find log head");
	return error;
}

 
STATIC int
xlog_rseek_logrec_hdr(
	struct xlog		*log,
	xfs_daddr_t		head_blk,
	xfs_daddr_t		tail_blk,
	int			count,
	char			*buffer,
	xfs_daddr_t		*rblk,
	struct xlog_rec_header	**rhead,
	bool			*wrapped)
{
	int			i;
	int			error;
	int			found = 0;
	char			*offset = NULL;
	xfs_daddr_t		end_blk;

	*wrapped = false;

	 
	end_blk = head_blk > tail_blk ? tail_blk : 0;
	for (i = (int) head_blk - 1; i >= end_blk; i--) {
		error = xlog_bread(log, i, 1, buffer, &offset);
		if (error)
			goto out_error;

		if (*(__be32 *) offset == cpu_to_be32(XLOG_HEADER_MAGIC_NUM)) {
			*rblk = i;
			*rhead = (struct xlog_rec_header *) offset;
			if (++found == count)
				break;
		}
	}

	 
	if (tail_blk >= head_blk && found != count) {
		for (i = log->l_logBBsize - 1; i >= (int) tail_blk; i--) {
			error = xlog_bread(log, i, 1, buffer, &offset);
			if (error)
				goto out_error;

			if (*(__be32 *)offset ==
			    cpu_to_be32(XLOG_HEADER_MAGIC_NUM)) {
				*wrapped = true;
				*rblk = i;
				*rhead = (struct xlog_rec_header *) offset;
				if (++found == count)
					break;
			}
		}
	}

	return found;

out_error:
	return error;
}

 
STATIC int
xlog_seek_logrec_hdr(
	struct xlog		*log,
	xfs_daddr_t		head_blk,
	xfs_daddr_t		tail_blk,
	int			count,
	char			*buffer,
	xfs_daddr_t		*rblk,
	struct xlog_rec_header	**rhead,
	bool			*wrapped)
{
	int			i;
	int			error;
	int			found = 0;
	char			*offset = NULL;
	xfs_daddr_t		end_blk;

	*wrapped = false;

	 
	end_blk = head_blk > tail_blk ? head_blk : log->l_logBBsize - 1;
	for (i = (int) tail_blk; i <= end_blk; i++) {
		error = xlog_bread(log, i, 1, buffer, &offset);
		if (error)
			goto out_error;

		if (*(__be32 *) offset == cpu_to_be32(XLOG_HEADER_MAGIC_NUM)) {
			*rblk = i;
			*rhead = (struct xlog_rec_header *) offset;
			if (++found == count)
				break;
		}
	}

	 
	if (tail_blk > head_blk && found != count) {
		for (i = 0; i < (int) head_blk; i++) {
			error = xlog_bread(log, i, 1, buffer, &offset);
			if (error)
				goto out_error;

			if (*(__be32 *)offset ==
			    cpu_to_be32(XLOG_HEADER_MAGIC_NUM)) {
				*wrapped = true;
				*rblk = i;
				*rhead = (struct xlog_rec_header *) offset;
				if (++found == count)
					break;
			}
		}
	}

	return found;

out_error:
	return error;
}

 
static inline int
xlog_tail_distance(
	struct xlog	*log,
	xfs_daddr_t	head_blk,
	xfs_daddr_t	tail_blk)
{
	if (head_blk < tail_blk)
		return tail_blk - head_blk;

	return tail_blk + (log->l_logBBsize - head_blk);
}

 
STATIC int
xlog_verify_tail(
	struct xlog		*log,
	xfs_daddr_t		head_blk,
	xfs_daddr_t		*tail_blk,
	int			hsize)
{
	struct xlog_rec_header	*thead;
	char			*buffer;
	xfs_daddr_t		first_bad;
	int			error = 0;
	bool			wrapped;
	xfs_daddr_t		tmp_tail;
	xfs_daddr_t		orig_tail = *tail_blk;

	buffer = xlog_alloc_buffer(log, 1);
	if (!buffer)
		return -ENOMEM;

	 
	error = xlog_seek_logrec_hdr(log, head_blk, *tail_blk, 1, buffer,
			&tmp_tail, &thead, &wrapped);
	if (error < 0)
		goto out;
	if (*tail_blk != tmp_tail)
		*tail_blk = tmp_tail;

	 
	first_bad = 0;
	error = xlog_do_recovery_pass(log, head_blk, *tail_blk,
				      XLOG_RECOVER_CRCPASS, &first_bad);
	while ((error == -EFSBADCRC || error == -EFSCORRUPTED) && first_bad) {
		int	tail_distance;

		 
		tail_distance = xlog_tail_distance(log, head_blk, first_bad);
		if (tail_distance > BTOBB(XLOG_MAX_ICLOGS * hsize))
			break;

		 
		error = xlog_seek_logrec_hdr(log, head_blk, first_bad, 2,
				buffer, &tmp_tail, &thead, &wrapped);
		if (error < 0)
			goto out;

		*tail_blk = tmp_tail;
		first_bad = 0;
		error = xlog_do_recovery_pass(log, head_blk, *tail_blk,
					      XLOG_RECOVER_CRCPASS, &first_bad);
	}

	if (!error && *tail_blk != orig_tail)
		xfs_warn(log->l_mp,
		"Tail block (0x%llx) overwrite detected. Updated to 0x%llx",
			 orig_tail, *tail_blk);
out:
	kmem_free(buffer);
	return error;
}

 
STATIC int
xlog_verify_head(
	struct xlog		*log,
	xfs_daddr_t		*head_blk,	 
	xfs_daddr_t		*tail_blk,	 
	char			*buffer,
	xfs_daddr_t		*rhead_blk,	 
	struct xlog_rec_header	**rhead,	 
	bool			*wrapped)	 
{
	struct xlog_rec_header	*tmp_rhead;
	char			*tmp_buffer;
	xfs_daddr_t		first_bad;
	xfs_daddr_t		tmp_rhead_blk;
	int			found;
	int			error;
	bool			tmp_wrapped;

	 
	tmp_buffer = xlog_alloc_buffer(log, 1);
	if (!tmp_buffer)
		return -ENOMEM;
	error = xlog_rseek_logrec_hdr(log, *head_blk, *tail_blk,
				      XLOG_MAX_ICLOGS, tmp_buffer,
				      &tmp_rhead_blk, &tmp_rhead, &tmp_wrapped);
	kmem_free(tmp_buffer);
	if (error < 0)
		return error;

	 
	error = xlog_do_recovery_pass(log, *head_blk, tmp_rhead_blk,
				      XLOG_RECOVER_CRCPASS, &first_bad);
	if ((error == -EFSBADCRC || error == -EFSCORRUPTED) && first_bad) {
		 
		error = 0;
		xfs_warn(log->l_mp,
"Torn write (CRC failure) detected at log block 0x%llx. Truncating head block from 0x%llx.",
			 first_bad, *head_blk);

		 
		found = xlog_rseek_logrec_hdr(log, first_bad, *tail_blk, 1,
				buffer, rhead_blk, rhead, wrapped);
		if (found < 0)
			return found;
		if (found == 0)		 
			return -EIO;

		 
		*head_blk = first_bad;
		*tail_blk = BLOCK_LSN(be64_to_cpu((*rhead)->h_tail_lsn));
		if (*head_blk == *tail_blk) {
			ASSERT(0);
			return 0;
		}
	}
	if (error)
		return error;

	return xlog_verify_tail(log, *head_blk, tail_blk,
				be32_to_cpu((*rhead)->h_size));
}

 
static inline xfs_daddr_t
xlog_wrap_logbno(
	struct xlog		*log,
	xfs_daddr_t		bno)
{
	int			mod;

	div_s64_rem(bno, log->l_logBBsize, &mod);
	return mod;
}

 
static int
xlog_check_unmount_rec(
	struct xlog		*log,
	xfs_daddr_t		*head_blk,
	xfs_daddr_t		*tail_blk,
	struct xlog_rec_header	*rhead,
	xfs_daddr_t		rhead_blk,
	char			*buffer,
	bool			*clean)
{
	struct xlog_op_header	*op_head;
	xfs_daddr_t		umount_data_blk;
	xfs_daddr_t		after_umount_blk;
	int			hblks;
	int			error;
	char			*offset;

	*clean = false;

	 
	hblks = xlog_logrec_hblks(log, rhead);
	after_umount_blk = xlog_wrap_logbno(log,
			rhead_blk + hblks + BTOBB(be32_to_cpu(rhead->h_len)));

	if (*head_blk == after_umount_blk &&
	    be32_to_cpu(rhead->h_num_logops) == 1) {
		umount_data_blk = xlog_wrap_logbno(log, rhead_blk + hblks);
		error = xlog_bread(log, umount_data_blk, 1, buffer, &offset);
		if (error)
			return error;

		op_head = (struct xlog_op_header *)offset;
		if (op_head->oh_flags & XLOG_UNMOUNT_TRANS) {
			 
			xlog_assign_atomic_lsn(&log->l_tail_lsn,
					log->l_curr_cycle, after_umount_blk);
			xlog_assign_atomic_lsn(&log->l_last_sync_lsn,
					log->l_curr_cycle, after_umount_blk);
			*tail_blk = after_umount_blk;

			*clean = true;
		}
	}

	return 0;
}

static void
xlog_set_state(
	struct xlog		*log,
	xfs_daddr_t		head_blk,
	struct xlog_rec_header	*rhead,
	xfs_daddr_t		rhead_blk,
	bool			bump_cycle)
{
	 
	log->l_prev_block = rhead_blk;
	log->l_curr_block = (int)head_blk;
	log->l_curr_cycle = be32_to_cpu(rhead->h_cycle);
	if (bump_cycle)
		log->l_curr_cycle++;
	atomic64_set(&log->l_tail_lsn, be64_to_cpu(rhead->h_tail_lsn));
	atomic64_set(&log->l_last_sync_lsn, be64_to_cpu(rhead->h_lsn));
	xlog_assign_grant_head(&log->l_reserve_head.grant, log->l_curr_cycle,
					BBTOB(log->l_curr_block));
	xlog_assign_grant_head(&log->l_write_head.grant, log->l_curr_cycle,
					BBTOB(log->l_curr_block));
}

 
STATIC int
xlog_find_tail(
	struct xlog		*log,
	xfs_daddr_t		*head_blk,
	xfs_daddr_t		*tail_blk)
{
	xlog_rec_header_t	*rhead;
	char			*offset = NULL;
	char			*buffer;
	int			error;
	xfs_daddr_t		rhead_blk;
	xfs_lsn_t		tail_lsn;
	bool			wrapped = false;
	bool			clean = false;

	 
	if ((error = xlog_find_head(log, head_blk)))
		return error;
	ASSERT(*head_blk < INT_MAX);

	buffer = xlog_alloc_buffer(log, 1);
	if (!buffer)
		return -ENOMEM;
	if (*head_blk == 0) {				 
		error = xlog_bread(log, 0, 1, buffer, &offset);
		if (error)
			goto done;

		if (xlog_get_cycle(offset) == 0) {
			*tail_blk = 0;
			 
			goto done;
		}
	}

	 
	error = xlog_rseek_logrec_hdr(log, *head_blk, *head_blk, 1, buffer,
				      &rhead_blk, &rhead, &wrapped);
	if (error < 0)
		goto done;
	if (!error) {
		xfs_warn(log->l_mp, "%s: couldn't find sync record", __func__);
		error = -EFSCORRUPTED;
		goto done;
	}
	*tail_blk = BLOCK_LSN(be64_to_cpu(rhead->h_tail_lsn));

	 
	xlog_set_state(log, *head_blk, rhead, rhead_blk, wrapped);
	tail_lsn = atomic64_read(&log->l_tail_lsn);

	 
	error = xlog_check_unmount_rec(log, head_blk, tail_blk, rhead,
				       rhead_blk, buffer, &clean);
	if (error)
		goto done;

	 
	if (!clean) {
		xfs_daddr_t	orig_head = *head_blk;

		error = xlog_verify_head(log, head_blk, tail_blk, buffer,
					 &rhead_blk, &rhead, &wrapped);
		if (error)
			goto done;

		 
		if (*head_blk != orig_head) {
			xlog_set_state(log, *head_blk, rhead, rhead_blk,
				       wrapped);
			tail_lsn = atomic64_read(&log->l_tail_lsn);
			error = xlog_check_unmount_rec(log, head_blk, tail_blk,
						       rhead, rhead_blk, buffer,
						       &clean);
			if (error)
				goto done;
		}
	}

	 
	if (clean)
		set_bit(XFS_OPSTATE_CLEAN, &log->l_mp->m_opstate);

	 
	if (!xfs_readonly_buftarg(log->l_targ))
		error = xlog_clear_stale_blocks(log, tail_lsn);

done:
	kmem_free(buffer);

	if (error)
		xfs_warn(log->l_mp, "failed to locate log tail");
	return error;
}

 
STATIC int
xlog_find_zeroed(
	struct xlog	*log,
	xfs_daddr_t	*blk_no)
{
	char		*buffer;
	char		*offset;
	uint	        first_cycle, last_cycle;
	xfs_daddr_t	new_blk, last_blk, start_blk;
	xfs_daddr_t     num_scan_bblks;
	int	        error, log_bbnum = log->l_logBBsize;

	*blk_no = 0;

	 
	buffer = xlog_alloc_buffer(log, 1);
	if (!buffer)
		return -ENOMEM;
	error = xlog_bread(log, 0, 1, buffer, &offset);
	if (error)
		goto out_free_buffer;

	first_cycle = xlog_get_cycle(offset);
	if (first_cycle == 0) {		 
		*blk_no = 0;
		kmem_free(buffer);
		return 1;
	}

	 
	error = xlog_bread(log, log_bbnum-1, 1, buffer, &offset);
	if (error)
		goto out_free_buffer;

	last_cycle = xlog_get_cycle(offset);
	if (last_cycle != 0) {		 
		kmem_free(buffer);
		return 0;
	}

	 
	last_blk = log_bbnum-1;
	error = xlog_find_cycle_start(log, buffer, 0, &last_blk, 0);
	if (error)
		goto out_free_buffer;

	 
	num_scan_bblks = XLOG_TOTAL_REC_SHIFT(log);
	ASSERT(num_scan_bblks <= INT_MAX);

	if (last_blk < num_scan_bblks)
		num_scan_bblks = last_blk;
	start_blk = last_blk - num_scan_bblks;

	 
	if ((error = xlog_find_verify_cycle(log, start_blk,
					 (int)num_scan_bblks, 0, &new_blk)))
		goto out_free_buffer;
	if (new_blk != -1)
		last_blk = new_blk;

	 
	error = xlog_find_verify_log_record(log, start_blk, &last_blk, 0);
	if (error == 1)
		error = -EIO;
	if (error)
		goto out_free_buffer;

	*blk_no = last_blk;
out_free_buffer:
	kmem_free(buffer);
	if (error)
		return error;
	return 1;
}

 
STATIC void
xlog_add_record(
	struct xlog		*log,
	char			*buf,
	int			cycle,
	int			block,
	int			tail_cycle,
	int			tail_block)
{
	xlog_rec_header_t	*recp = (xlog_rec_header_t *)buf;

	memset(buf, 0, BBSIZE);
	recp->h_magicno = cpu_to_be32(XLOG_HEADER_MAGIC_NUM);
	recp->h_cycle = cpu_to_be32(cycle);
	recp->h_version = cpu_to_be32(
			xfs_has_logv2(log->l_mp) ? 2 : 1);
	recp->h_lsn = cpu_to_be64(xlog_assign_lsn(cycle, block));
	recp->h_tail_lsn = cpu_to_be64(xlog_assign_lsn(tail_cycle, tail_block));
	recp->h_fmt = cpu_to_be32(XLOG_FMT);
	memcpy(&recp->h_fs_uuid, &log->l_mp->m_sb.sb_uuid, sizeof(uuid_t));
}

STATIC int
xlog_write_log_records(
	struct xlog	*log,
	int		cycle,
	int		start_block,
	int		blocks,
	int		tail_cycle,
	int		tail_block)
{
	char		*offset;
	char		*buffer;
	int		balign, ealign;
	int		sectbb = log->l_sectBBsize;
	int		end_block = start_block + blocks;
	int		bufblks;
	int		error = 0;
	int		i, j = 0;

	 
	bufblks = roundup_pow_of_two(blocks);
	while (bufblks > log->l_logBBsize)
		bufblks >>= 1;
	while (!(buffer = xlog_alloc_buffer(log, bufblks))) {
		bufblks >>= 1;
		if (bufblks < sectbb)
			return -ENOMEM;
	}

	 
	balign = round_down(start_block, sectbb);
	if (balign != start_block) {
		error = xlog_bread_noalign(log, start_block, 1, buffer);
		if (error)
			goto out_free_buffer;

		j = start_block - balign;
	}

	for (i = start_block; i < end_block; i += bufblks) {
		int		bcount, endcount;

		bcount = min(bufblks, end_block - start_block);
		endcount = bcount - j;

		 
		ealign = round_down(end_block, sectbb);
		if (j == 0 && (start_block + endcount > ealign)) {
			error = xlog_bread_noalign(log, ealign, sectbb,
					buffer + BBTOB(ealign - start_block));
			if (error)
				break;

		}

		offset = buffer + xlog_align(log, start_block);
		for (; j < endcount; j++) {
			xlog_add_record(log, offset, cycle, i+j,
					tail_cycle, tail_block);
			offset += BBSIZE;
		}
		error = xlog_bwrite(log, start_block, endcount, buffer);
		if (error)
			break;
		start_block += endcount;
		j = 0;
	}

out_free_buffer:
	kmem_free(buffer);
	return error;
}

 
STATIC int
xlog_clear_stale_blocks(
	struct xlog	*log,
	xfs_lsn_t	tail_lsn)
{
	int		tail_cycle, head_cycle;
	int		tail_block, head_block;
	int		tail_distance, max_distance;
	int		distance;
	int		error;

	tail_cycle = CYCLE_LSN(tail_lsn);
	tail_block = BLOCK_LSN(tail_lsn);
	head_cycle = log->l_curr_cycle;
	head_block = log->l_curr_block;

	 
	if (head_cycle == tail_cycle) {
		 
		if (XFS_IS_CORRUPT(log->l_mp,
				   head_block < tail_block ||
				   head_block >= log->l_logBBsize))
			return -EFSCORRUPTED;
		tail_distance = tail_block + (log->l_logBBsize - head_block);
	} else {
		 
		if (XFS_IS_CORRUPT(log->l_mp,
				   head_block >= tail_block ||
				   head_cycle != tail_cycle + 1))
			return -EFSCORRUPTED;
		tail_distance = tail_block - head_block;
	}

	 
	if (tail_distance <= 0) {
		ASSERT(tail_distance == 0);
		return 0;
	}

	max_distance = XLOG_TOTAL_REC_SHIFT(log);
	 
	max_distance = min(max_distance, tail_distance);

	if ((head_block + max_distance) <= log->l_logBBsize) {
		 
		error = xlog_write_log_records(log, (head_cycle - 1),
				head_block, max_distance, tail_cycle,
				tail_block);
		if (error)
			return error;
	} else {
		 
		distance = log->l_logBBsize - head_block;
		error = xlog_write_log_records(log, (head_cycle - 1),
				head_block, distance, tail_cycle,
				tail_block);

		if (error)
			return error;

		 
		distance = max_distance - (log->l_logBBsize - head_block);
		error = xlog_write_log_records(log, head_cycle, 0, distance,
				tail_cycle, tail_block);
		if (error)
			return error;
	}

	return 0;
}

 
void
xlog_recover_release_intent(
	struct xlog		*log,
	unsigned short		intent_type,
	uint64_t		intent_id)
{
	struct xfs_ail_cursor	cur;
	struct xfs_log_item	*lip;
	struct xfs_ail		*ailp = log->l_ailp;

	spin_lock(&ailp->ail_lock);
	for (lip = xfs_trans_ail_cursor_first(ailp, &cur, 0); lip != NULL;
	     lip = xfs_trans_ail_cursor_next(ailp, &cur)) {
		if (lip->li_type != intent_type)
			continue;
		if (!lip->li_ops->iop_match(lip, intent_id))
			continue;

		spin_unlock(&ailp->ail_lock);
		lip->li_ops->iop_release(lip);
		spin_lock(&ailp->ail_lock);
		break;
	}

	xfs_trans_ail_cursor_done(&cur);
	spin_unlock(&ailp->ail_lock);
}

int
xlog_recover_iget(
	struct xfs_mount	*mp,
	xfs_ino_t		ino,
	struct xfs_inode	**ipp)
{
	int			error;

	error = xfs_iget(mp, NULL, ino, 0, 0, ipp);
	if (error)
		return error;

	error = xfs_qm_dqattach(*ipp);
	if (error) {
		xfs_irele(*ipp);
		return error;
	}

	if (VFS_I(*ipp)->i_nlink == 0)
		xfs_iflags_set(*ipp, XFS_IRECOVERY);

	return 0;
}

 
static const struct xlog_recover_item_ops *xlog_recover_item_ops[] = {
	&xlog_buf_item_ops,
	&xlog_inode_item_ops,
	&xlog_dquot_item_ops,
	&xlog_quotaoff_item_ops,
	&xlog_icreate_item_ops,
	&xlog_efi_item_ops,
	&xlog_efd_item_ops,
	&xlog_rui_item_ops,
	&xlog_rud_item_ops,
	&xlog_cui_item_ops,
	&xlog_cud_item_ops,
	&xlog_bui_item_ops,
	&xlog_bud_item_ops,
	&xlog_attri_item_ops,
	&xlog_attrd_item_ops,
};

static const struct xlog_recover_item_ops *
xlog_find_item_ops(
	struct xlog_recover_item		*item)
{
	unsigned int				i;

	for (i = 0; i < ARRAY_SIZE(xlog_recover_item_ops); i++)
		if (ITEM_TYPE(item) == xlog_recover_item_ops[i]->item_type)
			return xlog_recover_item_ops[i];

	return NULL;
}

 
STATIC int
xlog_recover_reorder_trans(
	struct xlog		*log,
	struct xlog_recover	*trans,
	int			pass)
{
	struct xlog_recover_item *item, *n;
	int			error = 0;
	LIST_HEAD(sort_list);
	LIST_HEAD(cancel_list);
	LIST_HEAD(buffer_list);
	LIST_HEAD(inode_buffer_list);
	LIST_HEAD(item_list);

	list_splice_init(&trans->r_itemq, &sort_list);
	list_for_each_entry_safe(item, n, &sort_list, ri_list) {
		enum xlog_recover_reorder	fate = XLOG_REORDER_ITEM_LIST;

		item->ri_ops = xlog_find_item_ops(item);
		if (!item->ri_ops) {
			xfs_warn(log->l_mp,
				"%s: unrecognized type of log operation (%d)",
				__func__, ITEM_TYPE(item));
			ASSERT(0);
			 
			if (!list_empty(&sort_list))
				list_splice_init(&sort_list, &trans->r_itemq);
			error = -EFSCORRUPTED;
			break;
		}

		if (item->ri_ops->reorder)
			fate = item->ri_ops->reorder(item);

		switch (fate) {
		case XLOG_REORDER_BUFFER_LIST:
			list_move_tail(&item->ri_list, &buffer_list);
			break;
		case XLOG_REORDER_CANCEL_LIST:
			trace_xfs_log_recover_item_reorder_head(log,
					trans, item, pass);
			list_move(&item->ri_list, &cancel_list);
			break;
		case XLOG_REORDER_INODE_BUFFER_LIST:
			list_move(&item->ri_list, &inode_buffer_list);
			break;
		case XLOG_REORDER_ITEM_LIST:
			trace_xfs_log_recover_item_reorder_tail(log,
							trans, item, pass);
			list_move_tail(&item->ri_list, &item_list);
			break;
		}
	}

	ASSERT(list_empty(&sort_list));
	if (!list_empty(&buffer_list))
		list_splice(&buffer_list, &trans->r_itemq);
	if (!list_empty(&item_list))
		list_splice_tail(&item_list, &trans->r_itemq);
	if (!list_empty(&inode_buffer_list))
		list_splice_tail(&inode_buffer_list, &trans->r_itemq);
	if (!list_empty(&cancel_list))
		list_splice_tail(&cancel_list, &trans->r_itemq);
	return error;
}

void
xlog_buf_readahead(
	struct xlog		*log,
	xfs_daddr_t		blkno,
	uint			len,
	const struct xfs_buf_ops *ops)
{
	if (!xlog_is_buffer_cancelled(log, blkno, len))
		xfs_buf_readahead(log->l_mp->m_ddev_targp, blkno, len, ops);
}

STATIC int
xlog_recover_items_pass2(
	struct xlog                     *log,
	struct xlog_recover             *trans,
	struct list_head                *buffer_list,
	struct list_head                *item_list)
{
	struct xlog_recover_item	*item;
	int				error = 0;

	list_for_each_entry(item, item_list, ri_list) {
		trace_xfs_log_recover_item_recover(log, trans, item,
				XLOG_RECOVER_PASS2);

		if (item->ri_ops->commit_pass2)
			error = item->ri_ops->commit_pass2(log, buffer_list,
					item, trans->r_lsn);
		if (error)
			return error;
	}

	return error;
}

 
STATIC int
xlog_recover_commit_trans(
	struct xlog		*log,
	struct xlog_recover	*trans,
	int			pass,
	struct list_head	*buffer_list)
{
	int				error = 0;
	int				items_queued = 0;
	struct xlog_recover_item	*item;
	struct xlog_recover_item	*next;
	LIST_HEAD			(ra_list);
	LIST_HEAD			(done_list);

	#define XLOG_RECOVER_COMMIT_QUEUE_MAX 100

	hlist_del_init(&trans->r_list);

	error = xlog_recover_reorder_trans(log, trans, pass);
	if (error)
		return error;

	list_for_each_entry_safe(item, next, &trans->r_itemq, ri_list) {
		trace_xfs_log_recover_item_recover(log, trans, item, pass);

		switch (pass) {
		case XLOG_RECOVER_PASS1:
			if (item->ri_ops->commit_pass1)
				error = item->ri_ops->commit_pass1(log, item);
			break;
		case XLOG_RECOVER_PASS2:
			if (item->ri_ops->ra_pass2)
				item->ri_ops->ra_pass2(log, item);
			list_move_tail(&item->ri_list, &ra_list);
			items_queued++;
			if (items_queued >= XLOG_RECOVER_COMMIT_QUEUE_MAX) {
				error = xlog_recover_items_pass2(log, trans,
						buffer_list, &ra_list);
				list_splice_tail_init(&ra_list, &done_list);
				items_queued = 0;
			}

			break;
		default:
			ASSERT(0);
		}

		if (error)
			goto out;
	}

out:
	if (!list_empty(&ra_list)) {
		if (!error)
			error = xlog_recover_items_pass2(log, trans,
					buffer_list, &ra_list);
		list_splice_tail_init(&ra_list, &done_list);
	}

	if (!list_empty(&done_list))
		list_splice_init(&done_list, &trans->r_itemq);

	return error;
}

STATIC void
xlog_recover_add_item(
	struct list_head	*head)
{
	struct xlog_recover_item *item;

	item = kmem_zalloc(sizeof(struct xlog_recover_item), 0);
	INIT_LIST_HEAD(&item->ri_list);
	list_add_tail(&item->ri_list, head);
}

STATIC int
xlog_recover_add_to_cont_trans(
	struct xlog		*log,
	struct xlog_recover	*trans,
	char			*dp,
	int			len)
{
	struct xlog_recover_item *item;
	char			*ptr, *old_ptr;
	int			old_len;

	 
	if (list_empty(&trans->r_itemq)) {
		ASSERT(len <= sizeof(struct xfs_trans_header));
		if (len > sizeof(struct xfs_trans_header)) {
			xfs_warn(log->l_mp, "%s: bad header length", __func__);
			return -EFSCORRUPTED;
		}

		xlog_recover_add_item(&trans->r_itemq);
		ptr = (char *)&trans->r_theader +
				sizeof(struct xfs_trans_header) - len;
		memcpy(ptr, dp, len);
		return 0;
	}

	 
	item = list_entry(trans->r_itemq.prev, struct xlog_recover_item,
			  ri_list);

	old_ptr = item->ri_buf[item->ri_cnt-1].i_addr;
	old_len = item->ri_buf[item->ri_cnt-1].i_len;

	ptr = kvrealloc(old_ptr, old_len, len + old_len, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;
	memcpy(&ptr[old_len], dp, len);
	item->ri_buf[item->ri_cnt-1].i_len += len;
	item->ri_buf[item->ri_cnt-1].i_addr = ptr;
	trace_xfs_log_recover_item_add_cont(log, trans, item, 0);
	return 0;
}

 
STATIC int
xlog_recover_add_to_trans(
	struct xlog		*log,
	struct xlog_recover	*trans,
	char			*dp,
	int			len)
{
	struct xfs_inode_log_format	*in_f;			 
	struct xlog_recover_item *item;
	char			*ptr;

	if (!len)
		return 0;
	if (list_empty(&trans->r_itemq)) {
		 
		if (*(uint *)dp != XFS_TRANS_HEADER_MAGIC) {
			xfs_warn(log->l_mp, "%s: bad header magic number",
				__func__);
			ASSERT(0);
			return -EFSCORRUPTED;
		}

		if (len > sizeof(struct xfs_trans_header)) {
			xfs_warn(log->l_mp, "%s: bad header length", __func__);
			ASSERT(0);
			return -EFSCORRUPTED;
		}

		 
		if (len == sizeof(struct xfs_trans_header))
			xlog_recover_add_item(&trans->r_itemq);
		memcpy(&trans->r_theader, dp, len);
		return 0;
	}

	ptr = kmem_alloc(len, 0);
	memcpy(ptr, dp, len);
	in_f = (struct xfs_inode_log_format *)ptr;

	 
	item = list_entry(trans->r_itemq.prev, struct xlog_recover_item,
			  ri_list);
	if (item->ri_total != 0 &&
	     item->ri_total == item->ri_cnt) {
		 
		xlog_recover_add_item(&trans->r_itemq);
		item = list_entry(trans->r_itemq.prev,
					struct xlog_recover_item, ri_list);
	}

	if (item->ri_total == 0) {		 
		if (in_f->ilf_size == 0 ||
		    in_f->ilf_size > XLOG_MAX_REGIONS_IN_ITEM) {
			xfs_warn(log->l_mp,
		"bad number of regions (%d) in inode log format",
				  in_f->ilf_size);
			ASSERT(0);
			kmem_free(ptr);
			return -EFSCORRUPTED;
		}

		item->ri_total = in_f->ilf_size;
		item->ri_buf =
			kmem_zalloc(item->ri_total * sizeof(xfs_log_iovec_t),
				    0);
	}

	if (item->ri_total <= item->ri_cnt) {
		xfs_warn(log->l_mp,
	"log item region count (%d) overflowed size (%d)",
				item->ri_cnt, item->ri_total);
		ASSERT(0);
		kmem_free(ptr);
		return -EFSCORRUPTED;
	}

	 
	item->ri_buf[item->ri_cnt].i_addr = ptr;
	item->ri_buf[item->ri_cnt].i_len  = len;
	item->ri_cnt++;
	trace_xfs_log_recover_item_add(log, trans, item, 0);
	return 0;
}

 
STATIC void
xlog_recover_free_trans(
	struct xlog_recover	*trans)
{
	struct xlog_recover_item *item, *n;
	int			i;

	hlist_del_init(&trans->r_list);

	list_for_each_entry_safe(item, n, &trans->r_itemq, ri_list) {
		 
		list_del(&item->ri_list);
		for (i = 0; i < item->ri_cnt; i++)
			kmem_free(item->ri_buf[i].i_addr);
		 
		kmem_free(item->ri_buf);
		kmem_free(item);
	}
	 
	kmem_free(trans);
}

 
STATIC int
xlog_recovery_process_trans(
	struct xlog		*log,
	struct xlog_recover	*trans,
	char			*dp,
	unsigned int		len,
	unsigned int		flags,
	int			pass,
	struct list_head	*buffer_list)
{
	int			error = 0;
	bool			freeit = false;

	 
	flags &= ~XLOG_END_TRANS;
	if (flags & XLOG_WAS_CONT_TRANS)
		flags &= ~XLOG_CONTINUE_TRANS;

	 
	switch (flags) {
	 
	case 0:
	case XLOG_CONTINUE_TRANS:
		error = xlog_recover_add_to_trans(log, trans, dp, len);
		break;
	case XLOG_WAS_CONT_TRANS:
		error = xlog_recover_add_to_cont_trans(log, trans, dp, len);
		break;
	case XLOG_COMMIT_TRANS:
		error = xlog_recover_commit_trans(log, trans, pass,
						  buffer_list);
		 
		freeit = true;
		break;

	 
	case XLOG_UNMOUNT_TRANS:
		 
		xfs_warn(log->l_mp, "%s: Unmount LR", __func__);
		freeit = true;
		break;
	case XLOG_START_TRANS:
	default:
		xfs_warn(log->l_mp, "%s: bad flag 0x%x", __func__, flags);
		ASSERT(0);
		error = -EFSCORRUPTED;
		break;
	}
	if (error || freeit)
		xlog_recover_free_trans(trans);
	return error;
}

 
STATIC struct xlog_recover *
xlog_recover_ophdr_to_trans(
	struct hlist_head	rhash[],
	struct xlog_rec_header	*rhead,
	struct xlog_op_header	*ohead)
{
	struct xlog_recover	*trans;
	xlog_tid_t		tid;
	struct hlist_head	*rhp;

	tid = be32_to_cpu(ohead->oh_tid);
	rhp = &rhash[XLOG_RHASH(tid)];
	hlist_for_each_entry(trans, rhp, r_list) {
		if (trans->r_log_tid == tid)
			return trans;
	}

	 
	if (!(ohead->oh_flags & XLOG_START_TRANS))
		return NULL;

	ASSERT(be32_to_cpu(ohead->oh_len) == 0);

	 
	trans = kmem_zalloc(sizeof(struct xlog_recover), 0);
	trans->r_log_tid = tid;
	trans->r_lsn = be64_to_cpu(rhead->h_lsn);
	INIT_LIST_HEAD(&trans->r_itemq);
	INIT_HLIST_NODE(&trans->r_list);
	hlist_add_head(&trans->r_list, rhp);

	 
	return NULL;
}

STATIC int
xlog_recover_process_ophdr(
	struct xlog		*log,
	struct hlist_head	rhash[],
	struct xlog_rec_header	*rhead,
	struct xlog_op_header	*ohead,
	char			*dp,
	char			*end,
	int			pass,
	struct list_head	*buffer_list)
{
	struct xlog_recover	*trans;
	unsigned int		len;
	int			error;

	 
	if (ohead->oh_clientid != XFS_TRANSACTION &&
	    ohead->oh_clientid != XFS_LOG) {
		xfs_warn(log->l_mp, "%s: bad clientid 0x%x",
			__func__, ohead->oh_clientid);
		ASSERT(0);
		return -EFSCORRUPTED;
	}

	 
	len = be32_to_cpu(ohead->oh_len);
	if (dp + len > end) {
		xfs_warn(log->l_mp, "%s: bad length 0x%x", __func__, len);
		WARN_ON(1);
		return -EFSCORRUPTED;
	}

	trans = xlog_recover_ophdr_to_trans(rhash, rhead, ohead);
	if (!trans) {
		 
		return 0;
	}

	 
	if (log->l_recovery_lsn != trans->r_lsn &&
	    ohead->oh_flags & XLOG_COMMIT_TRANS) {
		error = xfs_buf_delwri_submit(buffer_list);
		if (error)
			return error;
		log->l_recovery_lsn = trans->r_lsn;
	}

	return xlog_recovery_process_trans(log, trans, dp, len,
					   ohead->oh_flags, pass, buffer_list);
}

 
STATIC int
xlog_recover_process_data(
	struct xlog		*log,
	struct hlist_head	rhash[],
	struct xlog_rec_header	*rhead,
	char			*dp,
	int			pass,
	struct list_head	*buffer_list)
{
	struct xlog_op_header	*ohead;
	char			*end;
	int			num_logops;
	int			error;

	end = dp + be32_to_cpu(rhead->h_len);
	num_logops = be32_to_cpu(rhead->h_num_logops);

	 
	if (xlog_header_check_recover(log->l_mp, rhead))
		return -EIO;

	trace_xfs_log_recover_record(log, rhead, pass);
	while ((dp < end) && num_logops) {

		ohead = (struct xlog_op_header *)dp;
		dp += sizeof(*ohead);
		ASSERT(dp <= end);

		 
		error = xlog_recover_process_ophdr(log, rhash, rhead, ohead,
						   dp, end, pass, buffer_list);
		if (error)
			return error;

		dp += be32_to_cpu(ohead->oh_len);
		num_logops--;
	}
	return 0;
}

 
static int
xlog_finish_defer_ops(
	struct xfs_mount	*mp,
	struct list_head	*capture_list)
{
	struct xfs_defer_capture *dfc, *next;
	struct xfs_trans	*tp;
	int			error = 0;

	list_for_each_entry_safe(dfc, next, capture_list, dfc_list) {
		struct xfs_trans_res	resv;
		struct xfs_defer_resources dres;

		 
		resv.tr_logres = dfc->dfc_logres;
		resv.tr_logcount = 1;
		resv.tr_logflags = XFS_TRANS_PERM_LOG_RES;

		error = xfs_trans_alloc(mp, &resv, dfc->dfc_blkres,
				dfc->dfc_rtxres, XFS_TRANS_RESERVE, &tp);
		if (error) {
			xlog_force_shutdown(mp->m_log, SHUTDOWN_LOG_IO_ERROR);
			return error;
		}

		 
		list_del_init(&dfc->dfc_list);
		xfs_defer_ops_continue(dfc, tp, &dres);
		error = xfs_trans_commit(tp);
		xfs_defer_resources_rele(&dres);
		if (error)
			return error;
	}

	ASSERT(list_empty(capture_list));
	return 0;
}

 
static void
xlog_abort_defer_ops(
	struct xfs_mount		*mp,
	struct list_head		*capture_list)
{
	struct xfs_defer_capture	*dfc;
	struct xfs_defer_capture	*next;

	list_for_each_entry_safe(dfc, next, capture_list, dfc_list) {
		list_del_init(&dfc->dfc_list);
		xfs_defer_ops_capture_free(mp, dfc);
	}
}

 
STATIC int
xlog_recover_process_intents(
	struct xlog		*log)
{
	LIST_HEAD(capture_list);
	struct xfs_ail_cursor	cur;
	struct xfs_log_item	*lip;
	struct xfs_ail		*ailp;
	int			error = 0;
#if defined(DEBUG) || defined(XFS_WARN)
	xfs_lsn_t		last_lsn;
#endif

	ailp = log->l_ailp;
	spin_lock(&ailp->ail_lock);
#if defined(DEBUG) || defined(XFS_WARN)
	last_lsn = xlog_assign_lsn(log->l_curr_cycle, log->l_curr_block);
#endif
	for (lip = xfs_trans_ail_cursor_first(ailp, &cur, 0);
	     lip != NULL;
	     lip = xfs_trans_ail_cursor_next(ailp, &cur)) {
		const struct xfs_item_ops	*ops;

		if (!xlog_item_is_intent(lip))
			break;

		 
		ASSERT(XFS_LSN_CMP(last_lsn, lip->li_lsn) >= 0);

		 
		spin_unlock(&ailp->ail_lock);
		ops = lip->li_ops;
		error = ops->iop_recover(lip, &capture_list);
		spin_lock(&ailp->ail_lock);
		if (error) {
			trace_xlog_intent_recovery_failed(log->l_mp, error,
					ops->iop_recover);
			break;
		}
	}

	xfs_trans_ail_cursor_done(&cur);
	spin_unlock(&ailp->ail_lock);
	if (error)
		goto err;

	error = xlog_finish_defer_ops(log->l_mp, &capture_list);
	if (error)
		goto err;

	return 0;
err:
	xlog_abort_defer_ops(log->l_mp, &capture_list);
	return error;
}

 
STATIC void
xlog_recover_cancel_intents(
	struct xlog		*log)
{
	struct xfs_log_item	*lip;
	struct xfs_ail_cursor	cur;
	struct xfs_ail		*ailp;

	ailp = log->l_ailp;
	spin_lock(&ailp->ail_lock);
	lip = xfs_trans_ail_cursor_first(ailp, &cur, 0);
	while (lip != NULL) {
		if (!xlog_item_is_intent(lip))
			break;

		spin_unlock(&ailp->ail_lock);
		lip->li_ops->iop_release(lip);
		spin_lock(&ailp->ail_lock);
		lip = xfs_trans_ail_cursor_next(ailp, &cur);
	}

	xfs_trans_ail_cursor_done(&cur);
	spin_unlock(&ailp->ail_lock);
}

 
STATIC void
xlog_recover_clear_agi_bucket(
	struct xfs_perag	*pag,
	int			bucket)
{
	struct xfs_mount	*mp = pag->pag_mount;
	struct xfs_trans	*tp;
	struct xfs_agi		*agi;
	struct xfs_buf		*agibp;
	int			offset;
	int			error;

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_clearagi, 0, 0, 0, &tp);
	if (error)
		goto out_error;

	error = xfs_read_agi(pag, tp, &agibp);
	if (error)
		goto out_abort;

	agi = agibp->b_addr;
	agi->agi_unlinked[bucket] = cpu_to_be32(NULLAGINO);
	offset = offsetof(xfs_agi_t, agi_unlinked) +
		 (sizeof(xfs_agino_t) * bucket);
	xfs_trans_log_buf(tp, agibp, offset,
			  (offset + sizeof(xfs_agino_t) - 1));

	error = xfs_trans_commit(tp);
	if (error)
		goto out_error;
	return;

out_abort:
	xfs_trans_cancel(tp);
out_error:
	xfs_warn(mp, "%s: failed to clear agi %d. Continuing.", __func__,
			pag->pag_agno);
	return;
}

static int
xlog_recover_iunlink_bucket(
	struct xfs_perag	*pag,
	struct xfs_agi		*agi,
	int			bucket)
{
	struct xfs_mount	*mp = pag->pag_mount;
	struct xfs_inode	*prev_ip = NULL;
	struct xfs_inode	*ip;
	xfs_agino_t		prev_agino, agino;
	int			error = 0;

	agino = be32_to_cpu(agi->agi_unlinked[bucket]);
	while (agino != NULLAGINO) {
		error = xfs_iget(mp, NULL,
				XFS_AGINO_TO_INO(mp, pag->pag_agno, agino),
				0, 0, &ip);
		if (error)
			break;

		ASSERT(VFS_I(ip)->i_nlink == 0);
		ASSERT(VFS_I(ip)->i_mode != 0);
		xfs_iflags_clear(ip, XFS_IRECOVERY);
		agino = ip->i_next_unlinked;

		if (prev_ip) {
			ip->i_prev_unlinked = prev_agino;
			xfs_irele(prev_ip);

			 
			error = xfs_inodegc_flush(mp);
			if (error)
				break;
		}

		prev_agino = agino;
		prev_ip = ip;
	}

	if (prev_ip) {
		int	error2;

		ip->i_prev_unlinked = prev_agino;
		xfs_irele(prev_ip);

		error2 = xfs_inodegc_flush(mp);
		if (error2 && !error)
			return error2;
	}
	return error;
}

 
static void
xlog_recover_iunlink_ag(
	struct xfs_perag	*pag)
{
	struct xfs_agi		*agi;
	struct xfs_buf		*agibp;
	int			bucket;
	int			error;

	error = xfs_read_agi(pag, NULL, &agibp);
	if (error) {
		 
		return;
	}

	 
	agi = agibp->b_addr;
	xfs_buf_unlock(agibp);

	for (bucket = 0; bucket < XFS_AGI_UNLINKED_BUCKETS; bucket++) {
		error = xlog_recover_iunlink_bucket(pag, agi, bucket);
		if (error) {
			 
			xlog_recover_clear_agi_bucket(pag, bucket);
		}
	}

	xfs_buf_rele(agibp);
}

static void
xlog_recover_process_iunlinks(
	struct xlog	*log)
{
	struct xfs_perag	*pag;
	xfs_agnumber_t		agno;

	for_each_perag(log->l_mp, agno, pag)
		xlog_recover_iunlink_ag(pag);
}

STATIC void
xlog_unpack_data(
	struct xlog_rec_header	*rhead,
	char			*dp,
	struct xlog		*log)
{
	int			i, j, k;

	for (i = 0; i < BTOBB(be32_to_cpu(rhead->h_len)) &&
		  i < (XLOG_HEADER_CYCLE_SIZE / BBSIZE); i++) {
		*(__be32 *)dp = *(__be32 *)&rhead->h_cycle_data[i];
		dp += BBSIZE;
	}

	if (xfs_has_logv2(log->l_mp)) {
		xlog_in_core_2_t *xhdr = (xlog_in_core_2_t *)rhead;
		for ( ; i < BTOBB(be32_to_cpu(rhead->h_len)); i++) {
			j = i / (XLOG_HEADER_CYCLE_SIZE / BBSIZE);
			k = i % (XLOG_HEADER_CYCLE_SIZE / BBSIZE);
			*(__be32 *)dp = xhdr[j].hic_xheader.xh_cycle_data[k];
			dp += BBSIZE;
		}
	}
}

 
STATIC int
xlog_recover_process(
	struct xlog		*log,
	struct hlist_head	rhash[],
	struct xlog_rec_header	*rhead,
	char			*dp,
	int			pass,
	struct list_head	*buffer_list)
{
	__le32			old_crc = rhead->h_crc;
	__le32			crc;

	crc = xlog_cksum(log, rhead, dp, be32_to_cpu(rhead->h_len));

	 
	if (pass == XLOG_RECOVER_CRCPASS) {
		if (old_crc && crc != old_crc)
			return -EFSBADCRC;
		return 0;
	}

	 
	if (crc != old_crc) {
		if (old_crc || xfs_has_crc(log->l_mp)) {
			xfs_alert(log->l_mp,
		"log record CRC mismatch: found 0x%x, expected 0x%x.",
					le32_to_cpu(old_crc),
					le32_to_cpu(crc));
			xfs_hex_dump(dp, 32);
		}

		 
		if (xfs_has_crc(log->l_mp)) {
			XFS_ERROR_REPORT(__func__, XFS_ERRLEVEL_LOW, log->l_mp);
			return -EFSCORRUPTED;
		}
	}

	xlog_unpack_data(rhead, dp, log);

	return xlog_recover_process_data(log, rhash, rhead, dp, pass,
					 buffer_list);
}

STATIC int
xlog_valid_rec_header(
	struct xlog		*log,
	struct xlog_rec_header	*rhead,
	xfs_daddr_t		blkno,
	int			bufsize)
{
	int			hlen;

	if (XFS_IS_CORRUPT(log->l_mp,
			   rhead->h_magicno != cpu_to_be32(XLOG_HEADER_MAGIC_NUM)))
		return -EFSCORRUPTED;
	if (XFS_IS_CORRUPT(log->l_mp,
			   (!rhead->h_version ||
			   (be32_to_cpu(rhead->h_version) &
			    (~XLOG_VERSION_OKBITS))))) {
		xfs_warn(log->l_mp, "%s: unrecognised log version (%d).",
			__func__, be32_to_cpu(rhead->h_version));
		return -EFSCORRUPTED;
	}

	 
	hlen = be32_to_cpu(rhead->h_len);
	if (XFS_IS_CORRUPT(log->l_mp, hlen <= 0 || hlen > bufsize))
		return -EFSCORRUPTED;

	if (XFS_IS_CORRUPT(log->l_mp,
			   blkno > log->l_logBBsize || blkno > INT_MAX))
		return -EFSCORRUPTED;
	return 0;
}

 
STATIC int
xlog_do_recovery_pass(
	struct xlog		*log,
	xfs_daddr_t		head_blk,
	xfs_daddr_t		tail_blk,
	int			pass,
	xfs_daddr_t		*first_bad)	 
{
	xlog_rec_header_t	*rhead;
	xfs_daddr_t		blk_no, rblk_no;
	xfs_daddr_t		rhead_blk;
	char			*offset;
	char			*hbp, *dbp;
	int			error = 0, h_size, h_len;
	int			error2 = 0;
	int			bblks, split_bblks;
	int			hblks, split_hblks, wrapped_hblks;
	int			i;
	struct hlist_head	rhash[XLOG_RHASH_SIZE];
	LIST_HEAD		(buffer_list);

	ASSERT(head_blk != tail_blk);
	blk_no = rhead_blk = tail_blk;

	for (i = 0; i < XLOG_RHASH_SIZE; i++)
		INIT_HLIST_HEAD(&rhash[i]);

	 
	if (xfs_has_logv2(log->l_mp)) {
		 
		hbp = xlog_alloc_buffer(log, 1);
		if (!hbp)
			return -ENOMEM;

		error = xlog_bread(log, tail_blk, 1, hbp, &offset);
		if (error)
			goto bread_err1;

		rhead = (xlog_rec_header_t *)offset;

		 
		h_size = be32_to_cpu(rhead->h_size);
		h_len = be32_to_cpu(rhead->h_len);
		if (h_len > h_size && h_len <= log->l_mp->m_logbsize &&
		    rhead->h_num_logops == cpu_to_be32(1)) {
			xfs_warn(log->l_mp,
		"invalid iclog size (%d bytes), using lsunit (%d bytes)",
				 h_size, log->l_mp->m_logbsize);
			h_size = log->l_mp->m_logbsize;
		}

		error = xlog_valid_rec_header(log, rhead, tail_blk, h_size);
		if (error)
			goto bread_err1;

		hblks = xlog_logrec_hblks(log, rhead);
		if (hblks != 1) {
			kmem_free(hbp);
			hbp = xlog_alloc_buffer(log, hblks);
		}
	} else {
		ASSERT(log->l_sectBBsize == 1);
		hblks = 1;
		hbp = xlog_alloc_buffer(log, 1);
		h_size = XLOG_BIG_RECORD_BSIZE;
	}

	if (!hbp)
		return -ENOMEM;
	dbp = xlog_alloc_buffer(log, BTOBB(h_size));
	if (!dbp) {
		kmem_free(hbp);
		return -ENOMEM;
	}

	memset(rhash, 0, sizeof(rhash));
	if (tail_blk > head_blk) {
		 
		while (blk_no < log->l_logBBsize) {
			 
			offset = hbp;
			split_hblks = 0;
			wrapped_hblks = 0;
			if (blk_no + hblks <= log->l_logBBsize) {
				 
				error = xlog_bread(log, blk_no, hblks, hbp,
						   &offset);
				if (error)
					goto bread_err2;
			} else {
				 
				if (blk_no != log->l_logBBsize) {
					 
					ASSERT(blk_no <= INT_MAX);
					split_hblks = log->l_logBBsize - (int)blk_no;
					ASSERT(split_hblks > 0);
					error = xlog_bread(log, blk_no,
							   split_hblks, hbp,
							   &offset);
					if (error)
						goto bread_err2;
				}

				 
				wrapped_hblks = hblks - split_hblks;
				error = xlog_bread_noalign(log, 0,
						wrapped_hblks,
						offset + BBTOB(split_hblks));
				if (error)
					goto bread_err2;
			}
			rhead = (xlog_rec_header_t *)offset;
			error = xlog_valid_rec_header(log, rhead,
					split_hblks ? blk_no : 0, h_size);
			if (error)
				goto bread_err2;

			bblks = (int)BTOBB(be32_to_cpu(rhead->h_len));
			blk_no += hblks;

			 
			if (blk_no + bblks <= log->l_logBBsize ||
			    blk_no >= log->l_logBBsize) {
				rblk_no = xlog_wrap_logbno(log, blk_no);
				error = xlog_bread(log, rblk_no, bblks, dbp,
						   &offset);
				if (error)
					goto bread_err2;
			} else {
				 
				offset = dbp;
				split_bblks = 0;
				if (blk_no != log->l_logBBsize) {
					 
					ASSERT(!wrapped_hblks);
					ASSERT(blk_no <= INT_MAX);
					split_bblks =
						log->l_logBBsize - (int)blk_no;
					ASSERT(split_bblks > 0);
					error = xlog_bread(log, blk_no,
							split_bblks, dbp,
							&offset);
					if (error)
						goto bread_err2;
				}

				 
				error = xlog_bread_noalign(log, 0,
						bblks - split_bblks,
						offset + BBTOB(split_bblks));
				if (error)
					goto bread_err2;
			}

			error = xlog_recover_process(log, rhash, rhead, offset,
						     pass, &buffer_list);
			if (error)
				goto bread_err2;

			blk_no += bblks;
			rhead_blk = blk_no;
		}

		ASSERT(blk_no >= log->l_logBBsize);
		blk_no -= log->l_logBBsize;
		rhead_blk = blk_no;
	}

	 
	while (blk_no < head_blk) {
		error = xlog_bread(log, blk_no, hblks, hbp, &offset);
		if (error)
			goto bread_err2;

		rhead = (xlog_rec_header_t *)offset;
		error = xlog_valid_rec_header(log, rhead, blk_no, h_size);
		if (error)
			goto bread_err2;

		 
		bblks = (int)BTOBB(be32_to_cpu(rhead->h_len));
		error = xlog_bread(log, blk_no+hblks, bblks, dbp,
				   &offset);
		if (error)
			goto bread_err2;

		error = xlog_recover_process(log, rhash, rhead, offset, pass,
					     &buffer_list);
		if (error)
			goto bread_err2;

		blk_no += bblks + hblks;
		rhead_blk = blk_no;
	}

 bread_err2:
	kmem_free(dbp);
 bread_err1:
	kmem_free(hbp);

	 
	if (!list_empty(&buffer_list))
		error2 = xfs_buf_delwri_submit(&buffer_list);

	if (error && first_bad)
		*first_bad = rhead_blk;

	 
	for (i = 0; i < XLOG_RHASH_SIZE; i++) {
		struct hlist_node	*tmp;
		struct xlog_recover	*trans;

		hlist_for_each_entry_safe(trans, tmp, &rhash[i], r_list)
			xlog_recover_free_trans(trans);
	}

	return error ? error : error2;
}

 
STATIC int
xlog_do_log_recovery(
	struct xlog	*log,
	xfs_daddr_t	head_blk,
	xfs_daddr_t	tail_blk)
{
	int		error;

	ASSERT(head_blk != tail_blk);

	 
	error = xlog_alloc_buf_cancel_table(log);
	if (error)
		return error;

	error = xlog_do_recovery_pass(log, head_blk, tail_blk,
				      XLOG_RECOVER_PASS1, NULL);
	if (error != 0)
		goto out_cancel;

	 
	error = xlog_do_recovery_pass(log, head_blk, tail_blk,
				      XLOG_RECOVER_PASS2, NULL);
	if (!error)
		xlog_check_buf_cancel_table(log);
out_cancel:
	xlog_free_buf_cancel_table(log);
	return error;
}

 
STATIC int
xlog_do_recover(
	struct xlog		*log,
	xfs_daddr_t		head_blk,
	xfs_daddr_t		tail_blk)
{
	struct xfs_mount	*mp = log->l_mp;
	struct xfs_buf		*bp = mp->m_sb_bp;
	struct xfs_sb		*sbp = &mp->m_sb;
	int			error;

	trace_xfs_log_recover(log, head_blk, tail_blk);

	 
	error = xlog_do_log_recovery(log, head_blk, tail_blk);
	if (error)
		return error;

	if (xlog_is_shutdown(log))
		return -EIO;

	 
	xlog_assign_tail_lsn(mp);

	 
	xfs_buf_lock(bp);
	xfs_buf_hold(bp);
	error = _xfs_buf_read(bp, XBF_READ);
	if (error) {
		if (!xlog_is_shutdown(log)) {
			xfs_buf_ioerror_alert(bp, __this_address);
			ASSERT(0);
		}
		xfs_buf_relse(bp);
		return error;
	}

	 
	xfs_sb_from_disk(sbp, bp->b_addr);
	xfs_buf_relse(bp);

	 
	mp->m_features |= xfs_sb_version_to_features(sbp);
	xfs_reinit_percpu_counters(mp);
	error = xfs_initialize_perag(mp, sbp->sb_agcount, sbp->sb_dblocks,
			&mp->m_maxagi);
	if (error) {
		xfs_warn(mp, "Failed post-recovery per-ag init: %d", error);
		return error;
	}
	mp->m_alloc_set_aside = xfs_alloc_set_aside(mp);

	 
	clear_bit(XLOG_ACTIVE_RECOVERY, &log->l_opstate);
	return 0;
}

 
int
xlog_recover(
	struct xlog	*log)
{
	xfs_daddr_t	head_blk, tail_blk;
	int		error;

	 
	error = xlog_find_tail(log, &head_blk, &tail_blk);
	if (error)
		return error;

	 
	if (xfs_has_crc(log->l_mp) &&
	    !xfs_log_check_lsn(log->l_mp, log->l_mp->m_sb.sb_lsn))
		return -EINVAL;

	if (tail_blk != head_blk) {
		 
		if ((error = xfs_dev_is_read_only(log->l_mp, "recovery"))) {
			return error;
		}

		 
		if (xfs_sb_is_v5(&log->l_mp->m_sb) &&
		    xfs_sb_has_incompat_log_feature(&log->l_mp->m_sb,
					XFS_SB_FEAT_INCOMPAT_LOG_UNKNOWN)) {
			xfs_warn(log->l_mp,
"Superblock has unknown incompatible log features (0x%x) enabled.",
				(log->l_mp->m_sb.sb_features_log_incompat &
					XFS_SB_FEAT_INCOMPAT_LOG_UNKNOWN));
			xfs_warn(log->l_mp,
"The log can not be fully and/or safely recovered by this kernel.");
			xfs_warn(log->l_mp,
"Please recover the log on a kernel that supports the unknown features.");
			return -EINVAL;
		}

		 
		if (xfs_globals.log_recovery_delay) {
			xfs_notice(log->l_mp,
				"Delaying log recovery for %d seconds.",
				xfs_globals.log_recovery_delay);
			msleep(xfs_globals.log_recovery_delay * 1000);
		}

		xfs_notice(log->l_mp, "Starting recovery (logdev: %s)",
				log->l_mp->m_logname ? log->l_mp->m_logname
						     : "internal");

		error = xlog_do_recover(log, head_blk, tail_blk);
		set_bit(XLOG_RECOVERY_NEEDED, &log->l_opstate);
	}
	return error;
}

 
int
xlog_recover_finish(
	struct xlog	*log)
{
	int	error;

	error = xlog_recover_process_intents(log);
	if (error) {
		 
		xlog_recover_cancel_intents(log);
		xfs_alert(log->l_mp, "Failed to recover intents");
		xlog_force_shutdown(log, SHUTDOWN_LOG_IO_ERROR);
		return error;
	}

	 
	xfs_log_force(log->l_mp, XFS_LOG_SYNC);

	 
	if (xfs_clear_incompat_log_features(log->l_mp)) {
		error = xfs_sync_sb(log->l_mp, false);
		if (error < 0) {
			xfs_alert(log->l_mp,
	"Failed to clear log incompat features on recovery");
			return error;
		}
	}

	xlog_recover_process_iunlinks(log);

	 
	error = xfs_reflink_recover_cow(log->l_mp);
	if (error) {
		xfs_alert(log->l_mp,
	"Failed to recover leftover CoW staging extents, err %d.",
				error);
		 
		xlog_force_shutdown(log, SHUTDOWN_LOG_IO_ERROR);
	}

	return 0;
}

void
xlog_recover_cancel(
	struct xlog	*log)
{
	if (xlog_recovery_needed(log))
		xlog_recover_cancel_intents(log);
}

