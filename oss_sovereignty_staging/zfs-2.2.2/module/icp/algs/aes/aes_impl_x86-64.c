 
 

#if defined(__x86_64)

#include <sys/simd.h>
#include <aes/aes_impl.h>

 
static void
aes_x86_64_generate(aes_key_t *key, const uint32_t *keyarr32, int keybits)
{
	key->nr = rijndael_key_setup_enc_amd64(&(key->encr_ks.ks32[0]),
	    keyarr32, keybits);
	key->nr = rijndael_key_setup_dec_amd64(&(key->decr_ks.ks32[0]),
	    keyarr32, keybits);
}

static boolean_t
aes_x86_64_will_work(void)
{
	return (B_TRUE);
}

const aes_impl_ops_t aes_x86_64_impl = {
	.generate = &aes_x86_64_generate,
	.encrypt = &aes_encrypt_amd64,
	.decrypt = &aes_decrypt_amd64,
	.is_supported = &aes_x86_64_will_work,
	.needs_byteswap = B_FALSE,
	.name = "x86_64"
};

#endif  
