 

 

 

#include <linux/types.h>
#include <linux/jiffies.h>
#include <linux/sunrpc/gss_krb5.h>
#include <linux/random.h>
#include <linux/crypto.h>
#include <linux/atomic.h>

#include "gss_krb5_internal.h"

#if IS_ENABLED(CONFIG_SUNRPC_DEBUG)
# define RPCDBG_FACILITY        RPCDBG_AUTH
#endif

static void *
setup_token_v2(struct krb5_ctx *ctx, struct xdr_netobj *token)
{
	u16 *ptr;
	void *krb5_hdr;
	u8 *p, flags = 0x00;

	if ((ctx->flags & KRB5_CTX_FLAG_INITIATOR) == 0)
		flags |= 0x01;
	if (ctx->flags & KRB5_CTX_FLAG_ACCEPTOR_SUBKEY)
		flags |= 0x04;

	 
	krb5_hdr = (u16 *)token->data;
	ptr = krb5_hdr;

	*ptr++ = KG2_TOK_MIC;
	p = (u8 *)ptr;
	*p++ = flags;
	*p++ = 0xff;
	ptr = (u16 *)p;
	*ptr++ = 0xffff;
	*ptr = 0xffff;

	token->len = GSS_KRB5_TOK_HDR_LEN + ctx->gk5e->cksumlength;
	return krb5_hdr;
}

u32
gss_krb5_get_mic_v2(struct krb5_ctx *ctx, struct xdr_buf *text,
		    struct xdr_netobj *token)
{
	struct crypto_ahash *tfm = ctx->initiate ?
				   ctx->initiator_sign : ctx->acceptor_sign;
	struct xdr_netobj cksumobj = {
		.len =	ctx->gk5e->cksumlength,
	};
	__be64 seq_send_be64;
	void *krb5_hdr;
	time64_t now;

	dprintk("RPC:       %s\n", __func__);

	krb5_hdr = setup_token_v2(ctx, token);

	 
	seq_send_be64 = cpu_to_be64(atomic64_fetch_inc(&ctx->seq_send64));
	memcpy(krb5_hdr + 8, (char *) &seq_send_be64, 8);

	cksumobj.data = krb5_hdr + GSS_KRB5_TOK_HDR_LEN;
	if (gss_krb5_checksum(tfm, krb5_hdr, GSS_KRB5_TOK_HDR_LEN,
			      text, 0, &cksumobj))
		return GSS_S_FAILURE;

	now = ktime_get_real_seconds();
	return (ctx->endtime < now) ? GSS_S_CONTEXT_EXPIRED : GSS_S_COMPLETE;
}
