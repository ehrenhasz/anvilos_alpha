 

 

 

#include <crypto/algapi.h>
#include <linux/types.h>
#include <linux/jiffies.h>
#include <linux/sunrpc/gss_krb5.h>
#include <linux/crypto.h>

#include "gss_krb5_internal.h"

#if IS_ENABLED(CONFIG_SUNRPC_DEBUG)
# define RPCDBG_FACILITY        RPCDBG_AUTH
#endif

u32
gss_krb5_verify_mic_v2(struct krb5_ctx *ctx, struct xdr_buf *message_buffer,
		       struct xdr_netobj *read_token)
{
	struct crypto_ahash *tfm = ctx->initiate ?
				   ctx->acceptor_sign : ctx->initiator_sign;
	char cksumdata[GSS_KRB5_MAX_CKSUM_LEN];
	struct xdr_netobj cksumobj = {
		.len	= ctx->gk5e->cksumlength,
		.data	= cksumdata,
	};
	u8 *ptr = read_token->data;
	__be16 be16_ptr;
	time64_t now;
	u8 flags;
	int i;

	dprintk("RPC:       %s\n", __func__);

	memcpy(&be16_ptr, (char *) ptr, 2);
	if (be16_to_cpu(be16_ptr) != KG2_TOK_MIC)
		return GSS_S_DEFECTIVE_TOKEN;

	flags = ptr[2];
	if ((!ctx->initiate && (flags & KG2_TOKEN_FLAG_SENTBYACCEPTOR)) ||
	    (ctx->initiate && !(flags & KG2_TOKEN_FLAG_SENTBYACCEPTOR)))
		return GSS_S_BAD_SIG;

	if (flags & KG2_TOKEN_FLAG_SEALED) {
		dprintk("%s: token has unexpected sealed flag\n", __func__);
		return GSS_S_FAILURE;
	}

	for (i = 3; i < 8; i++)
		if (ptr[i] != 0xff)
			return GSS_S_DEFECTIVE_TOKEN;

	if (gss_krb5_checksum(tfm, ptr, GSS_KRB5_TOK_HDR_LEN,
			      message_buffer, 0, &cksumobj))
		return GSS_S_FAILURE;

	if (memcmp(cksumobj.data, ptr + GSS_KRB5_TOK_HDR_LEN,
				ctx->gk5e->cksumlength))
		return GSS_S_BAD_SIG;

	 
	now = ktime_get_real_seconds();
	if (now > ctx->endtime)
		return GSS_S_CONTEXT_EXPIRED;

	 

	return GSS_S_COMPLETE;
}
