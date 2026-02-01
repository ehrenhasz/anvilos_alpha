
 

#include <linux/debugfs.h>

#include "cipher.h"
#include "util.h"

 
#define SPU_OFIFO_CTRL      0x40
#define SPU_FIFO_WATERMARK  0x1FF

 
int spu_sg_at_offset(struct scatterlist *sg, unsigned int skip,
		     struct scatterlist **sge, unsigned int *sge_offset)
{
	 
	unsigned int index = 0;
	 
	unsigned int next_index;

	next_index = sg->length;
	while (next_index <= skip) {
		sg = sg_next(sg);
		index = next_index;
		if (!sg)
			return -EINVAL;
		next_index += sg->length;
	}

	*sge_offset = skip - index;
	*sge = sg;
	return 0;
}

 
void sg_copy_part_to_buf(struct scatterlist *src, u8 *dest,
			 unsigned int len, unsigned int skip)
{
	size_t copied;
	unsigned int nents = sg_nents(src);

	copied = sg_pcopy_to_buffer(src, nents, dest, len, skip);
	if (copied != len) {
		flow_log("%s copied %u bytes of %u requested. ",
			 __func__, (u32)copied, len);
		flow_log("sg with %u entries and skip %u\n", nents, skip);
	}
}

 
void sg_copy_part_from_buf(struct scatterlist *dest, u8 *src,
			   unsigned int len, unsigned int skip)
{
	size_t copied;
	unsigned int nents = sg_nents(dest);

	copied = sg_pcopy_from_buffer(dest, nents, src, len, skip);
	if (copied != len) {
		flow_log("%s copied %u bytes of %u requested. ",
			 __func__, (u32)copied, len);
		flow_log("sg with %u entries and skip %u\n", nents, skip);
	}
}

 
int spu_sg_count(struct scatterlist *sg_list, unsigned int skip, int nbytes)
{
	struct scatterlist *sg;
	int sg_nents = 0;
	unsigned int offset;

	if (!sg_list)
		return 0;

	if (spu_sg_at_offset(sg_list, skip, &sg, &offset) < 0)
		return 0;

	while (sg && (nbytes > 0)) {
		sg_nents++;
		nbytes -= (sg->length - offset);
		offset = 0;
		sg = sg_next(sg);
	}
	return sg_nents;
}

 
u32 spu_msg_sg_add(struct scatterlist **to_sg,
		   struct scatterlist **from_sg, u32 *from_skip,
		   u8 from_nents, u32 length)
{
	struct scatterlist *sg;	 
	struct scatterlist *to = *to_sg;
	struct scatterlist *from = *from_sg;
	u32 skip = *from_skip;
	u32 offset;
	int i;
	u32 entry_len = 0;
	u32 frag_len = 0;	 
	u32 copied = 0;		 

	if (length == 0)
		return 0;

	for_each_sg(from, sg, from_nents, i) {
		 
		entry_len = sg->length - skip;
		frag_len = min(entry_len, length - copied);
		offset = sg->offset + skip;
		if (frag_len)
			sg_set_page(to++, sg_page(sg), frag_len, offset);
		copied += frag_len;
		if (copied == entry_len) {
			 
			skip = 0;	 
		}
		if (copied == length)
			break;
	}
	*to_sg = to;
	*from_sg = sg;
	if (frag_len < entry_len)
		*from_skip = skip + frag_len;
	else
		*from_skip = 0;

	return copied;
}

void add_to_ctr(u8 *ctr_pos, unsigned int increment)
{
	__be64 *high_be = (__be64 *)ctr_pos;
	__be64 *low_be = high_be + 1;
	u64 orig_low = __be64_to_cpu(*low_be);
	u64 new_low = orig_low + (u64)increment;

	*low_be = __cpu_to_be64(new_low);
	if (new_low < orig_low)
		 
		*high_be = __cpu_to_be64(__be64_to_cpu(*high_be) + 1);
}

struct sdesc {
	struct shash_desc shash;
	char ctx[];
};

 
int do_shash(unsigned char *name, unsigned char *result,
	     const u8 *data1, unsigned int data1_len,
	     const u8 *data2, unsigned int data2_len,
	     const u8 *key, unsigned int key_len)
{
	int rc;
	unsigned int size;
	struct crypto_shash *hash;
	struct sdesc *sdesc;

	hash = crypto_alloc_shash(name, 0, 0);
	if (IS_ERR(hash)) {
		rc = PTR_ERR(hash);
		pr_err("%s: Crypto %s allocation error %d\n", __func__, name, rc);
		return rc;
	}

	size = sizeof(struct shash_desc) + crypto_shash_descsize(hash);
	sdesc = kmalloc(size, GFP_KERNEL);
	if (!sdesc) {
		rc = -ENOMEM;
		goto do_shash_err;
	}
	sdesc->shash.tfm = hash;

	if (key_len > 0) {
		rc = crypto_shash_setkey(hash, key, key_len);
		if (rc) {
			pr_err("%s: Could not setkey %s shash\n", __func__, name);
			goto do_shash_err;
		}
	}

	rc = crypto_shash_init(&sdesc->shash);
	if (rc) {
		pr_err("%s: Could not init %s shash\n", __func__, name);
		goto do_shash_err;
	}
	rc = crypto_shash_update(&sdesc->shash, data1, data1_len);
	if (rc) {
		pr_err("%s: Could not update1\n", __func__);
		goto do_shash_err;
	}
	if (data2 && data2_len) {
		rc = crypto_shash_update(&sdesc->shash, data2, data2_len);
		if (rc) {
			pr_err("%s: Could not update2\n", __func__);
			goto do_shash_err;
		}
	}
	rc = crypto_shash_final(&sdesc->shash, result);
	if (rc)
		pr_err("%s: Could not generate %s hash\n", __func__, name);

do_shash_err:
	crypto_free_shash(hash);
	kfree(sdesc);

	return rc;
}

#ifdef DEBUG
 
void __dump_sg(struct scatterlist *sg, unsigned int skip, unsigned int len)
{
	u8 dbuf[16];
	unsigned int idx = skip;
	unsigned int num_out = 0;	 
	unsigned int count;

	if (packet_debug_logging) {
		while (num_out < len) {
			count = (len - num_out > 16) ? 16 : len - num_out;
			sg_copy_part_to_buf(sg, dbuf, count, idx);
			num_out += count;
			print_hex_dump(KERN_ALERT, "  sg: ", DUMP_PREFIX_NONE,
				       4, 1, dbuf, count, false);
			idx += 16;
		}
	}
	if (debug_logging_sleep)
		msleep(debug_logging_sleep);
}
#endif

 
char *spu_alg_name(enum spu_cipher_alg alg, enum spu_cipher_mode mode)
{
	switch (alg) {
	case CIPHER_ALG_RC4:
		return "rc4";
	case CIPHER_ALG_AES:
		switch (mode) {
		case CIPHER_MODE_CBC:
			return "cbc(aes)";
		case CIPHER_MODE_ECB:
			return "ecb(aes)";
		case CIPHER_MODE_OFB:
			return "ofb(aes)";
		case CIPHER_MODE_CFB:
			return "cfb(aes)";
		case CIPHER_MODE_CTR:
			return "ctr(aes)";
		case CIPHER_MODE_XTS:
			return "xts(aes)";
		case CIPHER_MODE_GCM:
			return "gcm(aes)";
		default:
			return "aes";
		}
		break;
	case CIPHER_ALG_DES:
		switch (mode) {
		case CIPHER_MODE_CBC:
			return "cbc(des)";
		case CIPHER_MODE_ECB:
			return "ecb(des)";
		case CIPHER_MODE_CTR:
			return "ctr(des)";
		default:
			return "des";
		}
		break;
	case CIPHER_ALG_3DES:
		switch (mode) {
		case CIPHER_MODE_CBC:
			return "cbc(des3_ede)";
		case CIPHER_MODE_ECB:
			return "ecb(des3_ede)";
		case CIPHER_MODE_CTR:
			return "ctr(des3_ede)";
		default:
			return "3des";
		}
		break;
	default:
		return "other";
	}
}

static ssize_t spu_debugfs_read(struct file *filp, char __user *ubuf,
				size_t count, loff_t *offp)
{
	struct bcm_device_private *ipriv;
	char *buf;
	ssize_t ret, out_offset, out_count;
	int i;
	u32 fifo_len;
	u32 spu_ofifo_ctrl;
	u32 alg;
	u32 mode;
	u32 op_cnt;

	out_count = 2048;

	buf = kmalloc(out_count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ipriv = filp->private_data;
	out_offset = 0;
	out_offset += scnprintf(buf + out_offset, out_count - out_offset,
			       "Number of SPUs.........%u\n",
			       ipriv->spu.num_spu);
	out_offset += scnprintf(buf + out_offset, out_count - out_offset,
			       "Current sessions.......%u\n",
			       atomic_read(&ipriv->session_count));
	out_offset += scnprintf(buf + out_offset, out_count - out_offset,
			       "Session count..........%u\n",
			       atomic_read(&ipriv->stream_count));
	out_offset += scnprintf(buf + out_offset, out_count - out_offset,
			       "Cipher setkey..........%u\n",
			       atomic_read(&ipriv->setkey_cnt[SPU_OP_CIPHER]));
	out_offset += scnprintf(buf + out_offset, out_count - out_offset,
			       "Cipher Ops.............%u\n",
			       atomic_read(&ipriv->op_counts[SPU_OP_CIPHER]));
	for (alg = 0; alg < CIPHER_ALG_LAST; alg++) {
		for (mode = 0; mode < CIPHER_MODE_LAST; mode++) {
			op_cnt = atomic_read(&ipriv->cipher_cnt[alg][mode]);
			if (op_cnt) {
				out_offset += scnprintf(buf + out_offset,
						       out_count - out_offset,
			       "  %-13s%11u\n",
			       spu_alg_name(alg, mode), op_cnt);
			}
		}
	}
	out_offset += scnprintf(buf + out_offset, out_count - out_offset,
			       "Hash Ops...............%u\n",
			       atomic_read(&ipriv->op_counts[SPU_OP_HASH]));
	for (alg = 0; alg < HASH_ALG_LAST; alg++) {
		op_cnt = atomic_read(&ipriv->hash_cnt[alg]);
		if (op_cnt) {
			out_offset += scnprintf(buf + out_offset,
					       out_count - out_offset,
		       "  %-13s%11u\n",
		       hash_alg_name[alg], op_cnt);
		}
	}
	out_offset += scnprintf(buf + out_offset, out_count - out_offset,
			       "HMAC setkey............%u\n",
			       atomic_read(&ipriv->setkey_cnt[SPU_OP_HMAC]));
	out_offset += scnprintf(buf + out_offset, out_count - out_offset,
			       "HMAC Ops...............%u\n",
			       atomic_read(&ipriv->op_counts[SPU_OP_HMAC]));
	for (alg = 0; alg < HASH_ALG_LAST; alg++) {
		op_cnt = atomic_read(&ipriv->hmac_cnt[alg]);
		if (op_cnt) {
			out_offset += scnprintf(buf + out_offset,
					       out_count - out_offset,
		       "  %-13s%11u\n",
		       hash_alg_name[alg], op_cnt);
		}
	}
	out_offset += scnprintf(buf + out_offset, out_count - out_offset,
			       "AEAD setkey............%u\n",
			       atomic_read(&ipriv->setkey_cnt[SPU_OP_AEAD]));

	out_offset += scnprintf(buf + out_offset, out_count - out_offset,
			       "AEAD Ops...............%u\n",
			       atomic_read(&ipriv->op_counts[SPU_OP_AEAD]));
	for (alg = 0; alg < AEAD_TYPE_LAST; alg++) {
		op_cnt = atomic_read(&ipriv->aead_cnt[alg]);
		if (op_cnt) {
			out_offset += scnprintf(buf + out_offset,
					       out_count - out_offset,
		       "  %-13s%11u\n",
		       aead_alg_name[alg], op_cnt);
		}
	}
	out_offset += scnprintf(buf + out_offset, out_count - out_offset,
			       "Bytes of req data......%llu\n",
			       (u64)atomic64_read(&ipriv->bytes_out));
	out_offset += scnprintf(buf + out_offset, out_count - out_offset,
			       "Bytes of resp data.....%llu\n",
			       (u64)atomic64_read(&ipriv->bytes_in));
	out_offset += scnprintf(buf + out_offset, out_count - out_offset,
			       "Mailbox full...........%u\n",
			       atomic_read(&ipriv->mb_no_spc));
	out_offset += scnprintf(buf + out_offset, out_count - out_offset,
			       "Mailbox send failures..%u\n",
			       atomic_read(&ipriv->mb_send_fail));
	out_offset += scnprintf(buf + out_offset, out_count - out_offset,
			       "Check ICV errors.......%u\n",
			       atomic_read(&ipriv->bad_icv));
	if (ipriv->spu.spu_type == SPU_TYPE_SPUM)
		for (i = 0; i < ipriv->spu.num_spu; i++) {
			spu_ofifo_ctrl = ioread32(ipriv->spu.reg_vbase[i] +
						  SPU_OFIFO_CTRL);
			fifo_len = spu_ofifo_ctrl & SPU_FIFO_WATERMARK;
			out_offset += scnprintf(buf + out_offset,
					       out_count - out_offset,
				       "SPU %d output FIFO high water.....%u\n",
				       i, fifo_len);
		}

	if (out_offset > out_count)
		out_offset = out_count;

	ret = simple_read_from_buffer(ubuf, count, offp, buf, out_offset);
	kfree(buf);
	return ret;
}

static const struct file_operations spu_debugfs_stats = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = spu_debugfs_read,
};

 
void spu_setup_debugfs(void)
{
	if (!debugfs_initialized())
		return;

	if (!iproc_priv.debugfs_dir)
		iproc_priv.debugfs_dir = debugfs_create_dir(KBUILD_MODNAME,
							    NULL);

	if (!iproc_priv.debugfs_stats)
		 
		debugfs_create_file("stats", 0400, iproc_priv.debugfs_dir,
				    &iproc_priv, &spu_debugfs_stats);
}

void spu_free_debugfs(void)
{
	debugfs_remove_recursive(iproc_priv.debugfs_dir);
	iproc_priv.debugfs_dir = NULL;
}

 
void format_value_ccm(unsigned int val, u8 *buf, u8 len)
{
	int i;

	 
	memset(buf, 0, len);

	 
	for (i = 0; i < len; i++) {
		buf[len - i - 1] = (val >> (8 * i)) & 0xff;
		if (i >= 3)
			break;   
	}
}
