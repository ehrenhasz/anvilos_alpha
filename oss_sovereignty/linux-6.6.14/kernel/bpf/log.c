
 
#include <uapi/linux/btf.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/bpf.h>
#include <linux/bpf_verifier.h>
#include <linux/math64.h>

static bool bpf_verifier_log_attr_valid(const struct bpf_verifier_log *log)
{
	 
	if (!!log->ubuf != !!log->len_total)
		return false;
	 
	if (log->ubuf && log->level == 0)
		return false;
	if (log->level & ~BPF_LOG_MASK)
		return false;
	if (log->len_total > UINT_MAX >> 2)
		return false;
	return true;
}

int bpf_vlog_init(struct bpf_verifier_log *log, u32 log_level,
		  char __user *log_buf, u32 log_size)
{
	log->level = log_level;
	log->ubuf = log_buf;
	log->len_total = log_size;

	 
	if (!bpf_verifier_log_attr_valid(log))
		return -EINVAL;

	return 0;
}

static void bpf_vlog_update_len_max(struct bpf_verifier_log *log, u32 add_len)
{
	 
	u64 len = log->end_pos + add_len;

	 
	if (len > UINT_MAX)
		log->len_max = UINT_MAX;
	else if (len > log->len_max)
		log->len_max = len;
}

void bpf_verifier_vlog(struct bpf_verifier_log *log, const char *fmt,
		       va_list args)
{
	u64 cur_pos;
	u32 new_n, n;

	n = vscnprintf(log->kbuf, BPF_VERIFIER_TMP_LOG_SIZE, fmt, args);

	if (log->level == BPF_LOG_KERNEL) {
		bool newline = n > 0 && log->kbuf[n - 1] == '\n';

		pr_err("BPF: %s%s", log->kbuf, newline ? "" : "\n");
		return;
	}

	n += 1;  
	bpf_vlog_update_len_max(log, n);

	if (log->level & BPF_LOG_FIXED) {
		 
		new_n = 0;
		if (log->end_pos < log->len_total) {
			new_n = min_t(u32, log->len_total - log->end_pos, n);
			log->kbuf[new_n - 1] = '\0';
		}

		cur_pos = log->end_pos;
		log->end_pos += n - 1;  

		if (log->ubuf && new_n &&
		    copy_to_user(log->ubuf + cur_pos, log->kbuf, new_n))
			goto fail;
	} else {
		u64 new_end, new_start;
		u32 buf_start, buf_end, new_n;

		new_end = log->end_pos + n;
		if (new_end - log->start_pos >= log->len_total)
			new_start = new_end - log->len_total;
		else
			new_start = log->start_pos;

		log->start_pos = new_start;
		log->end_pos = new_end - 1;  

		if (!log->ubuf)
			return;

		new_n = min(n, log->len_total);
		cur_pos = new_end - new_n;
		div_u64_rem(cur_pos, log->len_total, &buf_start);
		div_u64_rem(new_end, log->len_total, &buf_end);
		 
		if (buf_end == 0)
			buf_end = log->len_total;

		 
		if (buf_start < buf_end) {
			 
			if (copy_to_user(log->ubuf + buf_start,
					 log->kbuf + n - new_n,
					 buf_end - buf_start))
				goto fail;
		} else {
			 
			if (copy_to_user(log->ubuf + buf_start,
					 log->kbuf + n - new_n,
					 log->len_total - buf_start))
				goto fail;
			if (copy_to_user(log->ubuf,
					 log->kbuf + n - buf_end,
					 buf_end))
				goto fail;
		}
	}

	return;
fail:
	log->ubuf = NULL;
}

void bpf_vlog_reset(struct bpf_verifier_log *log, u64 new_pos)
{
	char zero = 0;
	u32 pos;

	if (WARN_ON_ONCE(new_pos > log->end_pos))
		return;

	if (!bpf_verifier_log_needed(log) || log->level == BPF_LOG_KERNEL)
		return;

	 
	log->end_pos = new_pos;
	if (log->end_pos < log->start_pos)
		log->start_pos = log->end_pos;

	if (!log->ubuf)
		return;

	if (log->level & BPF_LOG_FIXED)
		pos = log->end_pos + 1;
	else
		div_u64_rem(new_pos, log->len_total, &pos);

	if (pos < log->len_total && put_user(zero, log->ubuf + pos))
		log->ubuf = NULL;
}

static void bpf_vlog_reverse_kbuf(char *buf, int len)
{
	int i, j;

	for (i = 0, j = len - 1; i < j; i++, j--)
		swap(buf[i], buf[j]);
}

static int bpf_vlog_reverse_ubuf(struct bpf_verifier_log *log, int start, int end)
{
	 
	int n = sizeof(log->kbuf) / 2, nn;
	char *lbuf = log->kbuf, *rbuf = log->kbuf + n;

	 
	while (end - start > 1) {
		nn = min(n, (end - start ) / 2);

		if (copy_from_user(lbuf, log->ubuf + start, nn))
			return -EFAULT;
		if (copy_from_user(rbuf, log->ubuf + end - nn, nn))
			return -EFAULT;

		bpf_vlog_reverse_kbuf(lbuf, nn);
		bpf_vlog_reverse_kbuf(rbuf, nn);

		 
		if (copy_to_user(log->ubuf + start, rbuf, nn))
			return -EFAULT;
		if (copy_to_user(log->ubuf + end - nn, lbuf, nn))
			return -EFAULT;

		start += nn;
		end -= nn;
	}

	return 0;
}

int bpf_vlog_finalize(struct bpf_verifier_log *log, u32 *log_size_actual)
{
	u32 sublen;
	int err;

	*log_size_actual = 0;
	if (!log || log->level == 0 || log->level == BPF_LOG_KERNEL)
		return 0;

	if (!log->ubuf)
		goto skip_log_rotate;
	 
	if (log->start_pos == 0)
		goto skip_log_rotate;

	 

	 
	div_u64_rem(log->start_pos, log->len_total, &sublen);
	sublen = log->len_total - sublen;

	err = bpf_vlog_reverse_ubuf(log, 0, log->len_total);
	err = err ?: bpf_vlog_reverse_ubuf(log, 0, sublen);
	err = err ?: bpf_vlog_reverse_ubuf(log, sublen, log->len_total);
	if (err)
		log->ubuf = NULL;

skip_log_rotate:
	*log_size_actual = log->len_max;

	 
	if (!!log->ubuf != !!log->len_total)
		return -EFAULT;

	 
	if (log->ubuf && log->len_max > log->len_total)
		return -ENOSPC;

	return 0;
}

 
__printf(2, 3) void bpf_verifier_log_write(struct bpf_verifier_env *env,
					   const char *fmt, ...)
{
	va_list args;

	if (!bpf_verifier_log_needed(&env->log))
		return;

	va_start(args, fmt);
	bpf_verifier_vlog(&env->log, fmt, args);
	va_end(args);
}
EXPORT_SYMBOL_GPL(bpf_verifier_log_write);

__printf(2, 3) void bpf_log(struct bpf_verifier_log *log,
			    const char *fmt, ...)
{
	va_list args;

	if (!bpf_verifier_log_needed(log))
		return;

	va_start(args, fmt);
	bpf_verifier_vlog(log, fmt, args);
	va_end(args);
}
EXPORT_SYMBOL_GPL(bpf_log);
