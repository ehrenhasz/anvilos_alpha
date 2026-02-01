 
 

#include <sys/zfs_context.h>
#include <modes/modes.h>
#include <sys/crypto/common.h>
#include <sys/crypto/impl.h>

#ifdef HAVE_EFFICIENT_UNALIGNED_ACCESS
#include <sys/byteorder.h>
#define	UNALIGNED_POINTERS_PERMITTED
#endif

 
int
ccm_mode_encrypt_contiguous_blocks(ccm_ctx_t *ctx, char *data, size_t length,
    crypto_data_t *out, size_t block_size,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*copy_block)(uint8_t *, uint8_t *),
    void (*xor_block)(uint8_t *, uint8_t *))
{
	size_t remainder = length;
	size_t need = 0;
	uint8_t *datap = (uint8_t *)data;
	uint8_t *blockp;
	uint8_t *lastp;
	void *iov_or_mp;
	offset_t offset;
	uint8_t *out_data_1;
	uint8_t *out_data_2;
	size_t out_data_1_len;
	uint64_t counter;
	uint8_t *mac_buf;

	if (length + ctx->ccm_remainder_len < block_size) {
		 
		memcpy((uint8_t *)ctx->ccm_remainder + ctx->ccm_remainder_len,
		    datap,
		    length);
		ctx->ccm_remainder_len += length;
		ctx->ccm_copy_to = datap;
		return (CRYPTO_SUCCESS);
	}

	crypto_init_ptrs(out, &iov_or_mp, &offset);

	mac_buf = (uint8_t *)ctx->ccm_mac_buf;

	do {
		 
		if (ctx->ccm_remainder_len > 0) {
			need = block_size - ctx->ccm_remainder_len;

			if (need > remainder)
				return (CRYPTO_DATA_LEN_RANGE);

			memcpy(&((uint8_t *)ctx->ccm_remainder)
			    [ctx->ccm_remainder_len], datap, need);

			blockp = (uint8_t *)ctx->ccm_remainder;
		} else {
			blockp = datap;
		}

		 
		xor_block(blockp, mac_buf);
		encrypt_block(ctx->ccm_keysched, mac_buf, mac_buf);

		 
		encrypt_block(ctx->ccm_keysched, (uint8_t *)ctx->ccm_cb,
		    (uint8_t *)ctx->ccm_tmp);

		lastp = (uint8_t *)ctx->ccm_tmp;

		 
#ifdef _ZFS_LITTLE_ENDIAN
		counter = ntohll(ctx->ccm_cb[1] & ctx->ccm_counter_mask);
		counter = htonll(counter + 1);
#else
		counter = ctx->ccm_cb[1] & ctx->ccm_counter_mask;
		counter++;
#endif	 
		counter &= ctx->ccm_counter_mask;
		ctx->ccm_cb[1] =
		    (ctx->ccm_cb[1] & ~(ctx->ccm_counter_mask)) | counter;

		 
		xor_block(blockp, lastp);

		ctx->ccm_processed_data_len += block_size;

		crypto_get_ptrs(out, &iov_or_mp, &offset, &out_data_1,
		    &out_data_1_len, &out_data_2, block_size);

		 
		if (out_data_1_len == block_size) {
			copy_block(lastp, out_data_1);
		} else {
			memcpy(out_data_1, lastp, out_data_1_len);
			if (out_data_2 != NULL) {
				memcpy(out_data_2,
				    lastp + out_data_1_len,
				    block_size - out_data_1_len);
			}
		}
		 
		out->cd_offset += block_size;

		 
		if (ctx->ccm_remainder_len != 0) {
			datap += need;
			ctx->ccm_remainder_len = 0;
		} else {
			datap += block_size;
		}

		remainder = (size_t)&data[length] - (size_t)datap;

		 
		if (remainder > 0 && remainder < block_size) {
			memcpy(ctx->ccm_remainder, datap, remainder);
			ctx->ccm_remainder_len = remainder;
			ctx->ccm_copy_to = datap;
			goto out;
		}
		ctx->ccm_copy_to = NULL;

	} while (remainder > 0);

out:
	return (CRYPTO_SUCCESS);
}

void
calculate_ccm_mac(ccm_ctx_t *ctx, uint8_t *ccm_mac,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *))
{
	uint64_t counter;
	uint8_t *counterp, *mac_buf;
	int i;

	mac_buf = (uint8_t *)ctx->ccm_mac_buf;

	 
	counter = 0;
	ctx->ccm_cb[1] = (ctx->ccm_cb[1] & ~(ctx->ccm_counter_mask)) | counter;

	counterp = (uint8_t *)ctx->ccm_tmp;
	encrypt_block(ctx->ccm_keysched, (uint8_t *)ctx->ccm_cb, counterp);

	 
	for (i = 0; i < ctx->ccm_mac_len; i++) {
		ccm_mac[i] = mac_buf[i] ^ counterp[i];
	}
}

int
ccm_encrypt_final(ccm_ctx_t *ctx, crypto_data_t *out, size_t block_size,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*xor_block)(uint8_t *, uint8_t *))
{
	uint8_t *lastp, *mac_buf, *ccm_mac_p, *macp = NULL;
	void *iov_or_mp;
	offset_t offset;
	uint8_t *out_data_1;
	uint8_t *out_data_2;
	size_t out_data_1_len;
	int i;

	if (out->cd_length < (ctx->ccm_remainder_len + ctx->ccm_mac_len)) {
		return (CRYPTO_DATA_LEN_RANGE);
	}

	 
	if ((ctx->ccm_processed_data_len + ctx->ccm_remainder_len)
	    != (ctx->ccm_data_len)) {
		return (CRYPTO_DATA_LEN_RANGE);
	}

	mac_buf = (uint8_t *)ctx->ccm_mac_buf;

	if (ctx->ccm_remainder_len > 0) {

		 
		macp = (uint8_t *)ctx->ccm_mac_input_buf;
		memset(macp, 0, block_size);

		 
		memcpy(macp, ctx->ccm_remainder, ctx->ccm_remainder_len);

		 
		xor_block(macp, mac_buf);
		encrypt_block(ctx->ccm_keysched, mac_buf, mac_buf);

		 
		lastp = (uint8_t *)ctx->ccm_tmp;
		encrypt_block(ctx->ccm_keysched, (uint8_t *)ctx->ccm_cb, lastp);

		 
		for (i = 0; i < ctx->ccm_remainder_len; i++) {
			macp[i] ^= lastp[i];
		}
		ctx->ccm_processed_data_len += ctx->ccm_remainder_len;
	}

	 
	ccm_mac_p = (uint8_t *)ctx->ccm_tmp;
	calculate_ccm_mac(ctx, ccm_mac_p, encrypt_block);

	crypto_init_ptrs(out, &iov_or_mp, &offset);
	crypto_get_ptrs(out, &iov_or_mp, &offset, &out_data_1,
	    &out_data_1_len, &out_data_2,
	    ctx->ccm_remainder_len + ctx->ccm_mac_len);

	if (ctx->ccm_remainder_len > 0) {
		 
		if (out_data_2 == NULL) {
			 
			memcpy(out_data_1, macp, ctx->ccm_remainder_len);
			memcpy(out_data_1 + ctx->ccm_remainder_len, ccm_mac_p,
			    ctx->ccm_mac_len);
		} else {
			if (out_data_1_len < ctx->ccm_remainder_len) {
				size_t data_2_len_used;

				memcpy(out_data_1, macp, out_data_1_len);

				data_2_len_used = ctx->ccm_remainder_len
				    - out_data_1_len;

				memcpy(out_data_2,
				    (uint8_t *)macp + out_data_1_len,
				    data_2_len_used);
				memcpy(out_data_2 + data_2_len_used,
				    ccm_mac_p,
				    ctx->ccm_mac_len);
			} else {
				memcpy(out_data_1, macp, out_data_1_len);
				if (out_data_1_len == ctx->ccm_remainder_len) {
					 
					memcpy(out_data_2, ccm_mac_p,
					    ctx->ccm_mac_len);
				} else {
					size_t len_not_used = out_data_1_len -
					    ctx->ccm_remainder_len;
					 
					memcpy(out_data_1 +
					    ctx->ccm_remainder_len,
					    ccm_mac_p, len_not_used);
					memcpy(out_data_2,
					    ccm_mac_p + len_not_used,
					    ctx->ccm_mac_len - len_not_used);

				}
			}
		}
	} else {
		 
		memcpy(out_data_1, ccm_mac_p, out_data_1_len);
		if (out_data_2 != NULL) {
			memcpy(out_data_2, ccm_mac_p + out_data_1_len,
			    block_size - out_data_1_len);
		}
	}
	out->cd_offset += ctx->ccm_remainder_len + ctx->ccm_mac_len;
	ctx->ccm_remainder_len = 0;
	return (CRYPTO_SUCCESS);
}

 
static void
ccm_decrypt_incomplete_block(ccm_ctx_t *ctx,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *))
{
	uint8_t *datap, *outp, *counterp;
	int i;

	datap = (uint8_t *)ctx->ccm_remainder;
	outp = &((ctx->ccm_pt_buf)[ctx->ccm_processed_data_len]);

	counterp = (uint8_t *)ctx->ccm_tmp;
	encrypt_block(ctx->ccm_keysched, (uint8_t *)ctx->ccm_cb, counterp);

	 
	for (i = 0; i < ctx->ccm_remainder_len; i++) {
		outp[i] = datap[i] ^ counterp[i];
	}
}

 
int
ccm_mode_decrypt_contiguous_blocks(ccm_ctx_t *ctx, char *data, size_t length,
    crypto_data_t *out, size_t block_size,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*copy_block)(uint8_t *, uint8_t *),
    void (*xor_block)(uint8_t *, uint8_t *))
{
	(void) out;
	size_t remainder = length;
	size_t need = 0;
	uint8_t *datap = (uint8_t *)data;
	uint8_t *blockp;
	uint8_t *cbp;
	uint64_t counter;
	size_t pt_len, total_decrypted_len, mac_len, pm_len, pd_len;
	uint8_t *resultp;


	pm_len = ctx->ccm_processed_mac_len;

	if (pm_len > 0) {
		uint8_t *tmp;
		 
		if ((pm_len + length) > ctx->ccm_mac_len) {
			return (CRYPTO_ENCRYPTED_DATA_LEN_RANGE);
		}
		tmp = (uint8_t *)ctx->ccm_mac_input_buf;

		memcpy(tmp + pm_len, datap, length);

		ctx->ccm_processed_mac_len += length;
		return (CRYPTO_SUCCESS);
	}

	 
	pd_len = ctx->ccm_processed_data_len;
	total_decrypted_len = pd_len + length + ctx->ccm_remainder_len;

	if (total_decrypted_len >
	    (ctx->ccm_data_len + ctx->ccm_mac_len)) {
		return (CRYPTO_ENCRYPTED_DATA_LEN_RANGE);
	}

	pt_len = ctx->ccm_data_len;

	if (total_decrypted_len > pt_len) {
		 
		size_t pt_part = pt_len - pd_len - ctx->ccm_remainder_len;

		mac_len = length - pt_part;

		ctx->ccm_processed_mac_len = mac_len;
		memcpy(ctx->ccm_mac_input_buf, data + pt_part, mac_len);

		if (pt_part + ctx->ccm_remainder_len < block_size) {
			 
			memcpy(&((uint8_t *)ctx->ccm_remainder)
			    [ctx->ccm_remainder_len], datap, pt_part);
			ctx->ccm_remainder_len += pt_part;
			ccm_decrypt_incomplete_block(ctx, encrypt_block);
			ctx->ccm_processed_data_len += ctx->ccm_remainder_len;
			ctx->ccm_remainder_len = 0;
			return (CRYPTO_SUCCESS);
		} else {
			 
			length = pt_part;
		}
	} else if (length + ctx->ccm_remainder_len < block_size) {
		 
		memcpy((uint8_t *)ctx->ccm_remainder + ctx->ccm_remainder_len,
		    datap,
		    length);
		ctx->ccm_remainder_len += length;
		ctx->ccm_copy_to = datap;
		return (CRYPTO_SUCCESS);
	}

	do {
		 
		if (ctx->ccm_remainder_len > 0) {
			need = block_size - ctx->ccm_remainder_len;

			if (need > remainder)
				return (CRYPTO_ENCRYPTED_DATA_LEN_RANGE);

			memcpy(&((uint8_t *)ctx->ccm_remainder)
			    [ctx->ccm_remainder_len], datap, need);

			blockp = (uint8_t *)ctx->ccm_remainder;
		} else {
			blockp = datap;
		}

		 
		cbp = (uint8_t *)ctx->ccm_tmp;
		encrypt_block(ctx->ccm_keysched, (uint8_t *)ctx->ccm_cb, cbp);

		 
#ifdef _ZFS_LITTLE_ENDIAN
		counter = ntohll(ctx->ccm_cb[1] & ctx->ccm_counter_mask);
		counter = htonll(counter + 1);
#else
		counter = ctx->ccm_cb[1] & ctx->ccm_counter_mask;
		counter++;
#endif	 
		counter &= ctx->ccm_counter_mask;
		ctx->ccm_cb[1] =
		    (ctx->ccm_cb[1] & ~(ctx->ccm_counter_mask)) | counter;

		 
		xor_block(blockp, cbp);

		 
		resultp = (uint8_t *)ctx->ccm_pt_buf +
		    ctx->ccm_processed_data_len;
		copy_block(cbp, resultp);

		ctx->ccm_processed_data_len += block_size;

		ctx->ccm_lastp = blockp;

		 
		if (ctx->ccm_remainder_len != 0) {
			datap += need;
			ctx->ccm_remainder_len = 0;
		} else {
			datap += block_size;
		}

		remainder = (size_t)&data[length] - (size_t)datap;

		 
		if (remainder > 0 && remainder < block_size) {
			memcpy(ctx->ccm_remainder, datap, remainder);
			ctx->ccm_remainder_len = remainder;
			ctx->ccm_copy_to = datap;
			if (ctx->ccm_processed_mac_len > 0) {
				 
				ccm_decrypt_incomplete_block(ctx,
				    encrypt_block);
				ctx->ccm_processed_data_len += remainder;
				ctx->ccm_remainder_len = 0;
			}
			goto out;
		}
		ctx->ccm_copy_to = NULL;

	} while (remainder > 0);

out:
	return (CRYPTO_SUCCESS);
}

int
ccm_decrypt_final(ccm_ctx_t *ctx, crypto_data_t *out, size_t block_size,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*copy_block)(uint8_t *, uint8_t *),
    void (*xor_block)(uint8_t *, uint8_t *))
{
	size_t mac_remain, pt_len;
	uint8_t *pt, *mac_buf, *macp, *ccm_mac_p;
	int rv;

	pt_len = ctx->ccm_data_len;

	 
	if (out->cd_length < pt_len) {
		return (CRYPTO_DATA_LEN_RANGE);
	}

	pt = ctx->ccm_pt_buf;
	mac_remain = ctx->ccm_processed_data_len;
	mac_buf = (uint8_t *)ctx->ccm_mac_buf;

	macp = (uint8_t *)ctx->ccm_tmp;

	while (mac_remain > 0) {
		if (mac_remain < block_size) {
			memset(macp, 0, block_size);
			memcpy(macp, pt, mac_remain);
			mac_remain = 0;
		} else {
			copy_block(pt, macp);
			mac_remain -= block_size;
			pt += block_size;
		}

		 
		xor_block(macp, mac_buf);
		encrypt_block(ctx->ccm_keysched, mac_buf, mac_buf);
	}

	 
	ccm_mac_p = (uint8_t *)ctx->ccm_tmp;
	calculate_ccm_mac((ccm_ctx_t *)ctx, ccm_mac_p, encrypt_block);

	 
	if (memcmp(ctx->ccm_mac_input_buf, ccm_mac_p, ctx->ccm_mac_len)) {
		 
		return (CRYPTO_INVALID_MAC);
	} else {
		rv = crypto_put_output_data(ctx->ccm_pt_buf, out, pt_len);
		if (rv != CRYPTO_SUCCESS)
			return (rv);
		out->cd_offset += pt_len;
	}
	return (CRYPTO_SUCCESS);
}

static int
ccm_validate_args(CK_AES_CCM_PARAMS *ccm_param, boolean_t is_encrypt_init)
{
	size_t macSize, nonceSize;
	uint8_t q;
	uint64_t maxValue;

	 
	macSize = ccm_param->ulMACSize;
	if ((macSize < 4) || (macSize > 16) || ((macSize % 2) != 0)) {
		return (CRYPTO_MECHANISM_PARAM_INVALID);
	}

	 
	nonceSize = ccm_param->ulNonceSize;
	if ((nonceSize < 7) || (nonceSize > 13)) {
		return (CRYPTO_MECHANISM_PARAM_INVALID);
	}

	 
	q = (uint8_t)((15 - nonceSize) & 0xFF);


	 
	if ((!is_encrypt_init) && (ccm_param->ulDataSize < macSize)) {
		return (CRYPTO_MECHANISM_PARAM_INVALID);
	}

	 
	if (q < 8) {
		maxValue = (1ULL << (q * 8)) - 1;
	} else {
		maxValue = ULONG_MAX;
	}

	if (ccm_param->ulDataSize > maxValue) {
		return (CRYPTO_MECHANISM_PARAM_INVALID);
	}
	return (CRYPTO_SUCCESS);
}

 
static void
ccm_format_initial_blocks(uchar_t *nonce, ulong_t nonceSize,
    ulong_t authDataSize, uint8_t *b0, ccm_ctx_t *aes_ctx)
{
	uint64_t payloadSize;
	uint8_t t, q, have_adata = 0;
	size_t limit;
	int i, j, k;
	uint64_t mask = 0;
	uint8_t *cb;

	q = (uint8_t)((15 - nonceSize) & 0xFF);
	t = (uint8_t)((aes_ctx->ccm_mac_len) & 0xFF);

	 
	if (authDataSize > 0) {
		have_adata = 1;
	}
	b0[0] = (have_adata << 6) | (((t - 2)  / 2) << 3) | (q - 1);

	 
	memcpy(&(b0[1]), nonce, nonceSize);

	 
	memset(&(b0[1+nonceSize]), 0, q);

	payloadSize = aes_ctx->ccm_data_len;
	limit = MIN(8, q);

	for (i = 0, j = 0, k = 15; i < limit; i++, j += 8, k--) {
		b0[k] = (uint8_t)((payloadSize >> j) & 0xFF);
	}

	 

	cb = (uint8_t *)aes_ctx->ccm_cb;

	cb[0] = 0x07 & (q-1);  

	 
	memcpy(&(cb[1]), nonce, nonceSize);

	memset(&(cb[1+nonceSize]), 0, q);

	 
	q <<= 3;
	while (q-- > 0) {
		mask |= (1ULL << q);
	}

#ifdef _ZFS_LITTLE_ENDIAN
	mask = htonll(mask);
#endif
	aes_ctx->ccm_counter_mask = mask;

	 
	cb[15] = 0x01;
}

 
static void
encode_adata_len(ulong_t auth_data_len, uint8_t *encoded, size_t *encoded_len)
{
#ifdef UNALIGNED_POINTERS_PERMITTED
	uint32_t	*lencoded_ptr;
#ifdef _LP64
	uint64_t	*llencoded_ptr;
#endif
#endif	 

	if (auth_data_len < ((1ULL<<16) - (1ULL<<8))) {
		 
		*encoded_len = 2;
		encoded[0] = (auth_data_len & 0xff00) >> 8;
		encoded[1] = auth_data_len & 0xff;

	} else if ((auth_data_len >= ((1ULL<<16) - (1ULL<<8))) &&
	    (auth_data_len < (1ULL << 31))) {
		 
		*encoded_len = 6;
		encoded[0] = 0xff;
		encoded[1] = 0xfe;
#ifdef UNALIGNED_POINTERS_PERMITTED
		lencoded_ptr = (uint32_t *)&encoded[2];
		*lencoded_ptr = htonl(auth_data_len);
#else
		encoded[2] = (auth_data_len & 0xff000000) >> 24;
		encoded[3] = (auth_data_len & 0xff0000) >> 16;
		encoded[4] = (auth_data_len & 0xff00) >> 8;
		encoded[5] = auth_data_len & 0xff;
#endif	 

#ifdef _LP64
	} else {
		 
		*encoded_len = 10;
		encoded[0] = 0xff;
		encoded[1] = 0xff;
#ifdef UNALIGNED_POINTERS_PERMITTED
		llencoded_ptr = (uint64_t *)&encoded[2];
		*llencoded_ptr = htonl(auth_data_len);
#else
		encoded[2] = (auth_data_len & 0xff00000000000000) >> 56;
		encoded[3] = (auth_data_len & 0xff000000000000) >> 48;
		encoded[4] = (auth_data_len & 0xff0000000000) >> 40;
		encoded[5] = (auth_data_len & 0xff00000000) >> 32;
		encoded[6] = (auth_data_len & 0xff000000) >> 24;
		encoded[7] = (auth_data_len & 0xff0000) >> 16;
		encoded[8] = (auth_data_len & 0xff00) >> 8;
		encoded[9] = auth_data_len & 0xff;
#endif	 
#endif	 
	}
}

static int
ccm_init(ccm_ctx_t *ctx, unsigned char *nonce, size_t nonce_len,
    unsigned char *auth_data, size_t auth_data_len, size_t block_size,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*xor_block)(uint8_t *, uint8_t *))
{
	uint8_t *mac_buf, *datap, *ivp, *authp;
	size_t remainder, processed;
	uint8_t encoded_a[10];  
	size_t encoded_a_len = 0;

	mac_buf = (uint8_t *)&(ctx->ccm_mac_buf);

	 
	ccm_format_initial_blocks(nonce, nonce_len,
	    auth_data_len, mac_buf, ctx);

	 
	ivp = (uint8_t *)ctx->ccm_tmp;
	memset(ivp, 0, block_size);

	xor_block(ivp, mac_buf);

	 
	encrypt_block(ctx->ccm_keysched, mac_buf, mac_buf);

	 
	if (auth_data_len == 0) {
		return (CRYPTO_SUCCESS);
	}

	encode_adata_len(auth_data_len, encoded_a, &encoded_a_len);

	remainder = auth_data_len;

	 
	authp = (uint8_t *)ctx->ccm_tmp;
	memset(authp, 0, block_size);
	memcpy(authp, encoded_a, encoded_a_len);
	processed = block_size - encoded_a_len;
	if (processed > auth_data_len) {
		 
		processed = auth_data_len;
	}
	memcpy(authp+encoded_a_len, auth_data, processed);
	 
	xor_block(authp, mac_buf);
	encrypt_block(ctx->ccm_keysched, mac_buf, mac_buf);
	remainder -= processed;
	if (remainder == 0) {
		 
		return (CRYPTO_SUCCESS);
	}

	do {
		if (remainder < block_size) {
			 
			memset(authp, 0, block_size);
			memcpy(authp, &(auth_data[processed]), remainder);
			datap = (uint8_t *)authp;
			remainder = 0;
		} else {
			datap = (uint8_t *)(&(auth_data[processed]));
			processed += block_size;
			remainder -= block_size;
		}

		xor_block(datap, mac_buf);
		encrypt_block(ctx->ccm_keysched, mac_buf, mac_buf);

	} while (remainder > 0);

	return (CRYPTO_SUCCESS);
}

 
int
ccm_init_ctx(ccm_ctx_t *ccm_ctx, char *param, int kmflag,
    boolean_t is_encrypt_init, size_t block_size,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*xor_block)(uint8_t *, uint8_t *))
{
	int rv;
	CK_AES_CCM_PARAMS *ccm_param;

	if (param != NULL) {
		ccm_param = (CK_AES_CCM_PARAMS *)param;

		if ((rv = ccm_validate_args(ccm_param,
		    is_encrypt_init)) != 0) {
			return (rv);
		}

		ccm_ctx->ccm_mac_len = ccm_param->ulMACSize;
		if (is_encrypt_init) {
			ccm_ctx->ccm_data_len = ccm_param->ulDataSize;
		} else {
			ccm_ctx->ccm_data_len =
			    ccm_param->ulDataSize - ccm_ctx->ccm_mac_len;
			ccm_ctx->ccm_processed_mac_len = 0;
		}
		ccm_ctx->ccm_processed_data_len = 0;

		ccm_ctx->ccm_flags |= CCM_MODE;
	} else {
		return (CRYPTO_MECHANISM_PARAM_INVALID);
	}

	if (ccm_init(ccm_ctx, ccm_param->nonce, ccm_param->ulNonceSize,
	    ccm_param->authData, ccm_param->ulAuthDataSize, block_size,
	    encrypt_block, xor_block) != 0) {
		return (CRYPTO_MECHANISM_PARAM_INVALID);
	}
	if (!is_encrypt_init) {
		 
		ccm_ctx->ccm_pt_buf = vmem_alloc(ccm_ctx->ccm_data_len,
		    kmflag);
		if (ccm_ctx->ccm_pt_buf == NULL) {
			rv = CRYPTO_HOST_MEMORY;
		}
	}
	return (rv);
}

void *
ccm_alloc_ctx(int kmflag)
{
	ccm_ctx_t *ccm_ctx;

	if ((ccm_ctx = kmem_zalloc(sizeof (ccm_ctx_t), kmflag)) == NULL)
		return (NULL);

	ccm_ctx->ccm_flags = CCM_MODE;
	return (ccm_ctx);
}
