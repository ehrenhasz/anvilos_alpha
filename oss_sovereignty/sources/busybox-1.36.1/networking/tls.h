

#include "libbb.h"



#define HAVE_NATIVE_INT64
#undef  USE_1024_KEY_SPEED_OPTIMIZATIONS
#undef  USE_2048_KEY_SPEED_OPTIMIZATIONS
#define USE_AES
#undef  USE_AES_CBC_EXTERNAL
#undef  USE_AES_CCM
#undef  USE_AES_GCM
#undef  USE_3DES
#undef  USE_ARC4
#undef  USE_IDEA
#undef  USE_RC2
#undef  USE_SEED

#undef  DISABLE_PSTM
#if defined(__GNUC__) && defined(__i386__)
  
# define PSTM_32BIT
# define PSTM_X86
#endif










#define PS_SUCCESS              0
#define PS_FAILURE              -1
#define PS_ARG_FAIL             -6      
#define PS_PLATFORM_FAIL        -7      
#define PS_MEM_FAIL             -8      
#define PS_LIMIT_FAIL           -9      

#define PS_TRUE         1
#define PS_FALSE        0

#if BB_BIG_ENDIAN
# define ENDIAN_BIG     1
# undef  ENDIAN_LITTLE


#else
# define ENDIAN_LITTLE  1
# undef  ENDIAN_BIG
#endif

typedef uint64_t uint64;
typedef  int64_t  int64;
typedef uint32_t uint32;
typedef  int32_t  int32;
typedef uint16_t uint16;
typedef  int16_t  int16;




#define PS_EXPTMOD_WINSIZE   3



#define PUBKEY_TYPE     0x01
#define PRIVKEY_TYPE    0x02

#define AES_BLOCK_SIZE  16

void tls_get_random(void *buf, unsigned len) FAST_FUNC;

void xorbuf(void* buf, const void* mask, unsigned count) FAST_FUNC;

#define ALIGNED_long ALIGNED(sizeof(long))
void xorbuf_aligned_AES_BLOCK_SIZE(void* buf, const void* mask) FAST_FUNC;

#define matrixCryptoGetPrngData(buf, len, userPtr) (tls_get_random(buf, len), PS_SUCCESS)

#define psFree(p, pool)    free(p)
#define psTraceCrypto(msg) bb_simple_error_msg_and_die(msg)


#define memset_s(A,B,C,D) memset((A),(C),(D))

#define memcmpct(s1, s2, len) memcmp((s1), (s2), (len))
#undef  min
#define min(x, y) ((x) < (y) ? (x) : (y))


#include "tls_pstm.h"
#include "tls_aes.h"
#include "tls_aesgcm.h"
#include "tls_rsa.h"

#define EC_CURVE_KEYSIZE   32
#define P256_KEYSIZE       32
#define CURVE25519_KEYSIZE 32

void curve_x25519_compute_pubkey_and_premaster(
		uint8_t *pubkey32, uint8_t *premaster32,
		const uint8_t *peerkey32) FAST_FUNC;

void curve_P256_compute_pubkey_and_premaster(
		uint8_t *pubkey2x32, uint8_t *premaster32,
		const uint8_t *peerkey2x32) FAST_FUNC;

void curve_P256_compute_pubkey_and_premaster_NEW(
		uint8_t *pubkey2x32, uint8_t *premaster32,
		const uint8_t *peerkey2x32) FAST_FUNC;
