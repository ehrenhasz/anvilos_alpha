#include <sys/zfs_context.h>
#include <modes/modes.h>
#include <aes/aes_impl.h>
void
aes_copy_block(uint8_t *in, uint8_t *out)
{
	if (IS_P2ALIGNED2(in, out, sizeof (uint32_t))) {
		*(uint32_t *)&out[0] = *(uint32_t *)&in[0];
		*(uint32_t *)&out[4] = *(uint32_t *)&in[4];
		*(uint32_t *)&out[8] = *(uint32_t *)&in[8];
		*(uint32_t *)&out[12] = *(uint32_t *)&in[12];
	} else {
		AES_COPY_BLOCK(in, out);
	}
}
void
aes_xor_block(uint8_t *data, uint8_t *dst)
{
	if (IS_P2ALIGNED2(dst, data, sizeof (uint32_t))) {
		*(uint32_t *)&dst[0] ^= *(uint32_t *)&data[0];
		*(uint32_t *)&dst[4] ^= *(uint32_t *)&data[4];
		*(uint32_t *)&dst[8] ^= *(uint32_t *)&data[8];
		*(uint32_t *)&dst[12] ^= *(uint32_t *)&data[12];
	} else {
		AES_XOR_BLOCK(data, dst);
	}
}
int
aes_encrypt_contiguous_blocks(void *ctx, char *data, size_t length,
    crypto_data_t *out)
{
	aes_ctx_t *aes_ctx = ctx;
	int rv;
	if (aes_ctx->ac_flags & CTR_MODE) {
		rv = ctr_mode_contiguous_blocks(ctx, data, length, out,
		    AES_BLOCK_LEN, aes_encrypt_block, aes_xor_block);
	} else if (aes_ctx->ac_flags & CCM_MODE) {
		rv = ccm_mode_encrypt_contiguous_blocks(ctx, data, length,
		    out, AES_BLOCK_LEN, aes_encrypt_block, aes_copy_block,
		    aes_xor_block);
	} else if (aes_ctx->ac_flags & (GCM_MODE|GMAC_MODE)) {
		rv = gcm_mode_encrypt_contiguous_blocks(ctx, data, length,
		    out, AES_BLOCK_LEN, aes_encrypt_block, aes_copy_block,
		    aes_xor_block);
	} else if (aes_ctx->ac_flags & CBC_MODE) {
		rv = cbc_encrypt_contiguous_blocks(ctx,
		    data, length, out, AES_BLOCK_LEN, aes_encrypt_block,
		    aes_copy_block, aes_xor_block);
	} else {
		rv = ecb_cipher_contiguous_blocks(ctx, data, length, out,
		    AES_BLOCK_LEN, aes_encrypt_block);
	}
	return (rv);
}
int
aes_decrypt_contiguous_blocks(void *ctx, char *data, size_t length,
    crypto_data_t *out)
{
	aes_ctx_t *aes_ctx = ctx;
	int rv;
	if (aes_ctx->ac_flags & CTR_MODE) {
		rv = ctr_mode_contiguous_blocks(ctx, data, length, out,
		    AES_BLOCK_LEN, aes_encrypt_block, aes_xor_block);
		if (rv == CRYPTO_DATA_LEN_RANGE)
			rv = CRYPTO_ENCRYPTED_DATA_LEN_RANGE;
	} else if (aes_ctx->ac_flags & CCM_MODE) {
		rv = ccm_mode_decrypt_contiguous_blocks(ctx, data, length,
		    out, AES_BLOCK_LEN, aes_encrypt_block, aes_copy_block,
		    aes_xor_block);
	} else if (aes_ctx->ac_flags & (GCM_MODE|GMAC_MODE)) {
		rv = gcm_mode_decrypt_contiguous_blocks(ctx, data, length,
		    out, AES_BLOCK_LEN, aes_encrypt_block, aes_copy_block,
		    aes_xor_block);
	} else if (aes_ctx->ac_flags & CBC_MODE) {
		rv = cbc_decrypt_contiguous_blocks(ctx, data, length, out,
		    AES_BLOCK_LEN, aes_decrypt_block, aes_copy_block,
		    aes_xor_block);
	} else {
		rv = ecb_cipher_contiguous_blocks(ctx, data, length, out,
		    AES_BLOCK_LEN, aes_decrypt_block);
		if (rv == CRYPTO_DATA_LEN_RANGE)
			rv = CRYPTO_ENCRYPTED_DATA_LEN_RANGE;
	}
	return (rv);
}
