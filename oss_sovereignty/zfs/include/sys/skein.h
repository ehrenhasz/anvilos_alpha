#ifndef	_SYS_SKEIN_H_
#define	_SYS_SKEIN_H_
#ifdef  _KERNEL
#include <sys/types.h>		 
#else
#include <stdint.h>
#include <stdlib.h>
#endif
#ifdef	__cplusplus
extern "C" {
#endif
enum {
	SKEIN_SUCCESS = 0,	 
	SKEIN_FAIL = 1,
	SKEIN_BAD_HASHLEN = 2
};
#define	SKEIN_MODIFIER_WORDS	(2)	 
#define	SKEIN_256_STATE_WORDS	(4)
#define	SKEIN_512_STATE_WORDS	(8)
#define	SKEIN1024_STATE_WORDS	(16)
#define	SKEIN_MAX_STATE_WORDS	(16)
#define	SKEIN_256_STATE_BYTES	(8 * SKEIN_256_STATE_WORDS)
#define	SKEIN_512_STATE_BYTES	(8 * SKEIN_512_STATE_WORDS)
#define	SKEIN1024_STATE_BYTES	(8 * SKEIN1024_STATE_WORDS)
#define	SKEIN_256_STATE_BITS	(64 * SKEIN_256_STATE_WORDS)
#define	SKEIN_512_STATE_BITS	(64 * SKEIN_512_STATE_WORDS)
#define	SKEIN1024_STATE_BITS	(64 * SKEIN1024_STATE_WORDS)
#define	SKEIN_256_BLOCK_BYTES	(8 * SKEIN_256_STATE_WORDS)
#define	SKEIN_512_BLOCK_BYTES	(8 * SKEIN_512_STATE_WORDS)
#define	SKEIN1024_BLOCK_BYTES	(8 * SKEIN1024_STATE_WORDS)
typedef struct {
	size_t hashBitLen;	 
	size_t bCnt;		 
	uint64_t T[SKEIN_MODIFIER_WORDS];
} Skein_Ctxt_Hdr_t;
typedef struct {		 
	Skein_Ctxt_Hdr_t h;	 
	uint64_t X[SKEIN_256_STATE_WORDS];	 
	uint8_t b[SKEIN_256_BLOCK_BYTES];
} Skein_256_Ctxt_t;
typedef struct {		 
	Skein_Ctxt_Hdr_t h;	 
	uint64_t X[SKEIN_512_STATE_WORDS];	 
	uint8_t b[SKEIN_512_BLOCK_BYTES];
} Skein_512_Ctxt_t;
typedef struct {		 
	Skein_Ctxt_Hdr_t h;	 
	uint64_t X[SKEIN1024_STATE_WORDS];	 
	uint8_t b[SKEIN1024_BLOCK_BYTES];
} Skein1024_Ctxt_t;
int Skein_256_Init(Skein_256_Ctxt_t *ctx, size_t hashBitLen);
int Skein_512_Init(Skein_512_Ctxt_t *ctx, size_t hashBitLen);
int Skein1024_Init(Skein1024_Ctxt_t *ctx, size_t hashBitLen);
int Skein_256_Update(Skein_256_Ctxt_t *ctx, const uint8_t *msg,
    size_t msgByteCnt);
int Skein_512_Update(Skein_512_Ctxt_t *ctx, const uint8_t *msg,
    size_t msgByteCnt);
int Skein1024_Update(Skein1024_Ctxt_t *ctx, const uint8_t *msg,
    size_t msgByteCnt);
int Skein_256_Final(Skein_256_Ctxt_t *ctx, uint8_t *hashVal);
int Skein_512_Final(Skein_512_Ctxt_t *ctx, uint8_t *hashVal);
int Skein1024_Final(Skein1024_Ctxt_t *ctx, uint8_t *hashVal);
int Skein_256_InitExt(Skein_256_Ctxt_t *ctx, size_t hashBitLen,
    uint64_t treeInfo, const uint8_t *key, size_t keyBytes);
int Skein_512_InitExt(Skein_512_Ctxt_t *ctx, size_t hashBitLen,
    uint64_t treeInfo, const uint8_t *key, size_t keyBytes);
int Skein1024_InitExt(Skein1024_Ctxt_t *ctx, size_t hashBitLen,
    uint64_t treeInfo, const uint8_t *key, size_t keyBytes);
int Skein_256_Final_Pad(Skein_256_Ctxt_t *ctx, uint8_t *hashVal);
int Skein_512_Final_Pad(Skein_512_Ctxt_t *ctx, uint8_t *hashVal);
int Skein1024_Final_Pad(Skein1024_Ctxt_t *ctx, uint8_t *hashVal);
#ifndef	SKEIN_TREE_HASH
#define	SKEIN_TREE_HASH (1)
#endif
#if	SKEIN_TREE_HASH
int Skein_256_Output(Skein_256_Ctxt_t *ctx, uint8_t *hashVal);
int Skein_512_Output(Skein_512_Ctxt_t *ctx, uint8_t *hashVal);
int Skein1024_Output(Skein1024_Ctxt_t *ctx, uint8_t *hashVal);
#endif
typedef struct skein_param {
	size_t	sp_digest_bitlen;		 
} skein_param_t;
#ifdef	SKEIN_MODULE_IMPL
#define	CKM_SKEIN_256				"CKM_SKEIN_256"
#define	CKM_SKEIN_512				"CKM_SKEIN_512"
#define	CKM_SKEIN1024				"CKM_SKEIN1024"
#define	CKM_SKEIN_256_MAC			"CKM_SKEIN_256_MAC"
#define	CKM_SKEIN_512_MAC			"CKM_SKEIN_512_MAC"
#define	CKM_SKEIN1024_MAC			"CKM_SKEIN1024_MAC"
typedef enum skein_mech_type {
	SKEIN_256_MECH_INFO_TYPE,
	SKEIN_512_MECH_INFO_TYPE,
	SKEIN1024_MECH_INFO_TYPE,
	SKEIN_256_MAC_MECH_INFO_TYPE,
	SKEIN_512_MAC_MECH_INFO_TYPE,
	SKEIN1024_MAC_MECH_INFO_TYPE
} skein_mech_type_t;
#define	VALID_SKEIN_DIGEST_MECH(__mech)				\
	((int)(__mech) >= SKEIN_256_MECH_INFO_TYPE &&		\
	(__mech) <= SKEIN1024_MECH_INFO_TYPE)
#define	VALID_SKEIN_MAC_MECH(__mech)				\
	((int)(__mech) >= SKEIN_256_MAC_MECH_INFO_TYPE &&	\
	(__mech) <= SKEIN1024_MAC_MECH_INFO_TYPE)
#endif	 
#ifdef	__cplusplus
}
#endif
#endif	 
