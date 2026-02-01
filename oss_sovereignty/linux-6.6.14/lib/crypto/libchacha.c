
 

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/module.h>

#include <crypto/algapi.h> 
#include <crypto/chacha.h>

void chacha_crypt_generic(u32 *state, u8 *dst, const u8 *src,
			  unsigned int bytes, int nrounds)
{
	 
	u8 stream[CHACHA_BLOCK_SIZE] __aligned(sizeof(long));

	while (bytes >= CHACHA_BLOCK_SIZE) {
		chacha_block_generic(state, stream, nrounds);
		crypto_xor_cpy(dst, src, stream, CHACHA_BLOCK_SIZE);
		bytes -= CHACHA_BLOCK_SIZE;
		dst += CHACHA_BLOCK_SIZE;
		src += CHACHA_BLOCK_SIZE;
	}
	if (bytes) {
		chacha_block_generic(state, stream, nrounds);
		crypto_xor_cpy(dst, src, stream, bytes);
	}
}
EXPORT_SYMBOL(chacha_crypt_generic);

MODULE_LICENSE("GPL");
