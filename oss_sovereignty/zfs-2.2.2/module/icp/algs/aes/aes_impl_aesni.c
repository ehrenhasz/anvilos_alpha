 
 

#if defined(__x86_64) && defined(HAVE_AES)

#include <sys/simd.h>
#include <sys/types.h>
#include <sys/asm_linkage.h>

 
extern ASMABI int rijndael_key_setup_enc_intel(uint32_t rk[],
	const uint32_t cipherKey[], uint64_t keyBits);
extern ASMABI int rijndael_key_setup_dec_intel(uint32_t rk[],
	const uint32_t cipherKey[], uint64_t keyBits);
extern ASMABI void aes_encrypt_intel(const uint32_t rk[], int Nr,
	const uint32_t pt[4], uint32_t ct[4]);
extern ASMABI void aes_decrypt_intel(const uint32_t rk[], int Nr,
	const uint32_t ct[4], uint32_t pt[4]);


#include <aes/aes_impl.h>

 
static void
aes_aesni_generate(aes_key_t *key, const uint32_t *keyarr32, int keybits)
{
	kfpu_begin();
	key->nr = rijndael_key_setup_enc_intel(&(key->encr_ks.ks32[0]),
	    keyarr32, keybits);
	key->nr = rijndael_key_setup_dec_intel(&(key->decr_ks.ks32[0]),
	    keyarr32, keybits);
	kfpu_end();
}

 
static void
aes_aesni_encrypt(const uint32_t rk[], int Nr, const uint32_t pt[4],
    uint32_t ct[4])
{
	kfpu_begin();
	aes_encrypt_intel(rk, Nr, pt, ct);
	kfpu_end();
}

 
static void
aes_aesni_decrypt(const uint32_t rk[], int Nr, const uint32_t ct[4],
    uint32_t pt[4])
{
	kfpu_begin();
	aes_decrypt_intel(rk, Nr, ct, pt);
	kfpu_end();
}

static boolean_t
aes_aesni_will_work(void)
{
	return (kfpu_allowed() && zfs_aes_available());
}

const aes_impl_ops_t aes_aesni_impl = {
	.generate = &aes_aesni_generate,
	.encrypt = &aes_aesni_encrypt,
	.decrypt = &aes_aesni_decrypt,
	.is_supported = &aes_aesni_will_work,
	.needs_byteswap = B_FALSE,
	.name = "aesni"
};

#endif  
