 

#include <crypto/skcipher.h>
#include <linux/types.h>
#include <linux/jiffies.h>
#include <linux/sunrpc/gss_krb5.h>
#include <linux/pagemap.h>

#include "gss_krb5_internal.h"

#if IS_ENABLED(CONFIG_SUNRPC_DEBUG)
# define RPCDBG_FACILITY	RPCDBG_AUTH
#endif

 
#define LOCAL_BUF_LEN 32u

static void rotate_buf_a_little(struct xdr_buf *buf, unsigned int shift)
{
	char head[LOCAL_BUF_LEN];
	char tmp[LOCAL_BUF_LEN];
	unsigned int this_len, i;

	BUG_ON(shift > LOCAL_BUF_LEN);

	read_bytes_from_xdr_buf(buf, 0, head, shift);
	for (i = 0; i + shift < buf->len; i += LOCAL_BUF_LEN) {
		this_len = min(LOCAL_BUF_LEN, buf->len - (i + shift));
		read_bytes_from_xdr_buf(buf, i+shift, tmp, this_len);
		write_bytes_to_xdr_buf(buf, i, tmp, this_len);
	}
	write_bytes_to_xdr_buf(buf, buf->len - shift, head, shift);
}

static void _rotate_left(struct xdr_buf *buf, unsigned int shift)
{
	int shifted = 0;
	int this_shift;

	shift %= buf->len;
	while (shifted < shift) {
		this_shift = min(shift - shifted, LOCAL_BUF_LEN);
		rotate_buf_a_little(buf, this_shift);
		shifted += this_shift;
	}
}

static void rotate_left(u32 base, struct xdr_buf *buf, unsigned int shift)
{
	struct xdr_buf subbuf;

	xdr_buf_subsegment(buf, &subbuf, base, buf->len - base);
	_rotate_left(&subbuf, shift);
}

u32
gss_krb5_wrap_v2(struct krb5_ctx *kctx, int offset,
		 struct xdr_buf *buf, struct page **pages)
{
	u8		*ptr;
	time64_t	now;
	u8		flags = 0x00;
	__be16		*be16ptr;
	__be64		*be64ptr;
	u32		err;

	dprintk("RPC:       %s\n", __func__);

	 
	if (xdr_extend_head(buf, offset, GSS_KRB5_TOK_HDR_LEN))
		return GSS_S_FAILURE;

	 
	ptr = buf->head[0].iov_base + offset;
	*ptr++ = (unsigned char) ((KG2_TOK_WRAP>>8) & 0xff);
	*ptr++ = (unsigned char) (KG2_TOK_WRAP & 0xff);

	if ((kctx->flags & KRB5_CTX_FLAG_INITIATOR) == 0)
		flags |= KG2_TOKEN_FLAG_SENTBYACCEPTOR;
	if ((kctx->flags & KRB5_CTX_FLAG_ACCEPTOR_SUBKEY) != 0)
		flags |= KG2_TOKEN_FLAG_ACCEPTORSUBKEY;
	 
	flags |= KG2_TOKEN_FLAG_SEALED;

	*ptr++ = flags;
	*ptr++ = 0xff;
	be16ptr = (__be16 *)ptr;

	*be16ptr++ = 0;
	 
	*be16ptr++ = 0;

	be64ptr = (__be64 *)be16ptr;
	*be64ptr = cpu_to_be64(atomic64_fetch_inc(&kctx->seq_send64));

	err = (*kctx->gk5e->encrypt)(kctx, offset, buf, pages);
	if (err)
		return err;

	now = ktime_get_real_seconds();
	return (kctx->endtime < now) ? GSS_S_CONTEXT_EXPIRED : GSS_S_COMPLETE;
}

u32
gss_krb5_unwrap_v2(struct krb5_ctx *kctx, int offset, int len,
		   struct xdr_buf *buf, unsigned int *slack,
		   unsigned int *align)
{
	time64_t	now;
	u8		*ptr;
	u8		flags = 0x00;
	u16		ec, rrc;
	int		err;
	u32		headskip, tailskip;
	u8		decrypted_hdr[GSS_KRB5_TOK_HDR_LEN];
	unsigned int	movelen;


	dprintk("RPC:       %s\n", __func__);

	ptr = buf->head[0].iov_base + offset;

	if (be16_to_cpu(*((__be16 *)ptr)) != KG2_TOK_WRAP)
		return GSS_S_DEFECTIVE_TOKEN;

	flags = ptr[2];
	if ((!kctx->initiate && (flags & KG2_TOKEN_FLAG_SENTBYACCEPTOR)) ||
	    (kctx->initiate && !(flags & KG2_TOKEN_FLAG_SENTBYACCEPTOR)))
		return GSS_S_BAD_SIG;

	if ((flags & KG2_TOKEN_FLAG_SEALED) == 0) {
		dprintk("%s: token missing expected sealed flag\n", __func__);
		return GSS_S_DEFECTIVE_TOKEN;
	}

	if (ptr[3] != 0xff)
		return GSS_S_DEFECTIVE_TOKEN;

	ec = be16_to_cpup((__be16 *)(ptr + 4));
	rrc = be16_to_cpup((__be16 *)(ptr + 6));

	 

	if (rrc != 0)
		rotate_left(offset + 16, buf, rrc);

	err = (*kctx->gk5e->decrypt)(kctx, offset, len, buf,
				     &headskip, &tailskip);
	if (err)
		return GSS_S_FAILURE;

	 
	err = read_bytes_from_xdr_buf(buf,
				len - GSS_KRB5_TOK_HDR_LEN - tailskip,
				decrypted_hdr, GSS_KRB5_TOK_HDR_LEN);
	if (err) {
		dprintk("%s: error %u getting decrypted_hdr\n", __func__, err);
		return GSS_S_FAILURE;
	}
	if (memcmp(ptr, decrypted_hdr, 6)
				|| memcmp(ptr + 8, decrypted_hdr + 8, 8)) {
		dprintk("%s: token hdr, plaintext hdr mismatch!\n", __func__);
		return GSS_S_FAILURE;
	}

	 

	 
	now = ktime_get_real_seconds();
	if (now > kctx->endtime)
		return GSS_S_CONTEXT_EXPIRED;

	 
	movelen = min_t(unsigned int, buf->head[0].iov_len, len);
	movelen -= offset + GSS_KRB5_TOK_HDR_LEN + headskip;
	BUG_ON(offset + GSS_KRB5_TOK_HDR_LEN + headskip + movelen >
							buf->head[0].iov_len);
	memmove(ptr, ptr + GSS_KRB5_TOK_HDR_LEN + headskip, movelen);
	buf->head[0].iov_len -= GSS_KRB5_TOK_HDR_LEN + headskip;
	buf->len = len - (GSS_KRB5_TOK_HDR_LEN + headskip);

	 
	xdr_buf_trim(buf, ec + GSS_KRB5_TOK_HDR_LEN + tailskip);

	*align = XDR_QUADLEN(GSS_KRB5_TOK_HDR_LEN + headskip);
	*slack = *align + XDR_QUADLEN(ec + GSS_KRB5_TOK_HDR_LEN + tailskip);
	return GSS_S_COMPLETE;
}
