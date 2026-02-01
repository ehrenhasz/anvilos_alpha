 
 

#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/skein.h>		 
#include "skein_impl.h"		 

 
 
int
Skein_256_Init(Skein_256_Ctxt_t *ctx, size_t hashBitLen)
{
	union {
		uint8_t b[SKEIN_256_STATE_BYTES];
		uint64_t w[SKEIN_256_STATE_WORDS];
	} cfg;			 

	Skein_Assert(hashBitLen > 0, SKEIN_BAD_HASHLEN);
	ctx->h.hashBitLen = hashBitLen;	 

	switch (hashBitLen) {	 
#ifndef	SKEIN_NO_PRECOMP
	case 256:
		memcpy(ctx->X, SKEIN_256_IV_256, sizeof (ctx->X));
		break;
	case 224:
		memcpy(ctx->X, SKEIN_256_IV_224, sizeof (ctx->X));
		break;
	case 160:
		memcpy(ctx->X, SKEIN_256_IV_160, sizeof (ctx->X));
		break;
	case 128:
		memcpy(ctx->X, SKEIN_256_IV_128, sizeof (ctx->X));
		break;
#endif
	default:
		 
		 
		 
		Skein_Start_New_Type(ctx, CFG_FINAL);

		 
		cfg.w[0] = Skein_Swap64(SKEIN_SCHEMA_VER);
		 
		cfg.w[1] = Skein_Swap64(hashBitLen);
		cfg.w[2] = Skein_Swap64(SKEIN_CFG_TREE_INFO_SEQUENTIAL);
		 
		memset(&cfg.w[3], 0, sizeof (cfg) - 3 * sizeof (cfg.w[0]));

		 
		 
		memset(ctx->X, 0, sizeof (ctx->X));
		Skein_256_Process_Block(ctx, cfg.b, 1, SKEIN_CFG_STR_LEN);
		break;
	}
	 
	Skein_Start_New_Type(ctx, MSG);	 

	return (SKEIN_SUCCESS);
}

 
 
int
Skein_256_InitExt(Skein_256_Ctxt_t *ctx, size_t hashBitLen, uint64_t treeInfo,
    const uint8_t *key, size_t keyBytes)
{
	union {
		uint8_t b[SKEIN_256_STATE_BYTES];
		uint64_t w[SKEIN_256_STATE_WORDS];
	} cfg;			 

	Skein_Assert(hashBitLen > 0, SKEIN_BAD_HASHLEN);
	Skein_Assert(keyBytes == 0 || key != NULL, SKEIN_FAIL);

	 
	if (keyBytes == 0) {	 
		 
		memset(ctx->X, 0, sizeof (ctx->X));
	} else {		 

		Skein_assert(sizeof (cfg.b) >= sizeof (ctx->X));
		 
		 
		ctx->h.hashBitLen = 8 * sizeof (ctx->X);
		 
		Skein_Start_New_Type(ctx, KEY);
		 
		memset(ctx->X, 0, sizeof (ctx->X));
		 
		(void) Skein_256_Update(ctx, key, keyBytes);
		 
		(void) Skein_256_Final_Pad(ctx, cfg.b);
		 
		memcpy(ctx->X, cfg.b, sizeof (cfg.b));
#if	SKEIN_NEED_SWAP
		{
			uint_t i;
			 
			for (i = 0; i < SKEIN_256_STATE_WORDS; i++)
				ctx->X[i] = Skein_Swap64(ctx->X[i]);
		}
#endif
	}
	 
	ctx->h.hashBitLen = hashBitLen;	 
	Skein_Start_New_Type(ctx, CFG_FINAL);

	memset(&cfg.w, 0, sizeof (cfg.w));  
	cfg.w[0] = Skein_Swap64(SKEIN_SCHEMA_VER);
	cfg.w[1] = Skein_Swap64(hashBitLen);	 
	 
	cfg.w[2] = Skein_Swap64(treeInfo);

	Skein_Show_Key(256, &ctx->h, key, keyBytes);

	 
	Skein_256_Process_Block(ctx, cfg.b, 1, SKEIN_CFG_STR_LEN);

	 
	 
	ctx->h.bCnt = 0;	 
	Skein_Start_New_Type(ctx, MSG);

	return (SKEIN_SUCCESS);
}

 
int
Skein_256_Update(Skein_256_Ctxt_t *ctx, const uint8_t *msg, size_t msgByteCnt)
{
	size_t n;

	 
	Skein_Assert(ctx->h.bCnt <= SKEIN_256_BLOCK_BYTES, SKEIN_FAIL);

	 
	if (msgByteCnt + ctx->h.bCnt > SKEIN_256_BLOCK_BYTES) {
		 
		if (ctx->h.bCnt) {
			 
			n = SKEIN_256_BLOCK_BYTES - ctx->h.bCnt;
			if (n) {
				 
				Skein_assert(n < msgByteCnt);
				memcpy(&ctx->b[ctx->h.bCnt], msg, n);
				msgByteCnt -= n;
				msg += n;
				ctx->h.bCnt += n;
			}
			Skein_assert(ctx->h.bCnt == SKEIN_256_BLOCK_BYTES);
			Skein_256_Process_Block(ctx, ctx->b, 1,
			    SKEIN_256_BLOCK_BYTES);
			ctx->h.bCnt = 0;
		}
		 
		if (msgByteCnt > SKEIN_256_BLOCK_BYTES) {
			 
			n = (msgByteCnt - 1) / SKEIN_256_BLOCK_BYTES;
			Skein_256_Process_Block(ctx, msg, n,
			    SKEIN_256_BLOCK_BYTES);
			msgByteCnt -= n * SKEIN_256_BLOCK_BYTES;
			msg += n * SKEIN_256_BLOCK_BYTES;
		}
		Skein_assert(ctx->h.bCnt == 0);
	}

	 
	if (msgByteCnt) {
		Skein_assert(msgByteCnt + ctx->h.bCnt <= SKEIN_256_BLOCK_BYTES);
		memcpy(&ctx->b[ctx->h.bCnt], msg, msgByteCnt);
		ctx->h.bCnt += msgByteCnt;
	}

	return (SKEIN_SUCCESS);
}

 
int
Skein_256_Final(Skein_256_Ctxt_t *ctx, uint8_t *hashVal)
{
	size_t i, n, byteCnt;
	uint64_t X[SKEIN_256_STATE_WORDS];

	 
	Skein_Assert(ctx->h.bCnt <= SKEIN_256_BLOCK_BYTES, SKEIN_FAIL);

	ctx->h.T[1] |= SKEIN_T1_FLAG_FINAL;	 
	 
	if (ctx->h.bCnt < SKEIN_256_BLOCK_BYTES)
		memset(&ctx->b[ctx->h.bCnt], 0,
		    SKEIN_256_BLOCK_BYTES - ctx->h.bCnt);

	 
	Skein_256_Process_Block(ctx, ctx->b, 1, ctx->h.bCnt);

	 
	 
	byteCnt = (ctx->h.hashBitLen + 7) >> 3;

	 
	 
	memset(ctx->b, 0, sizeof (ctx->b));
	 
	memcpy(X, ctx->X, sizeof (X));
	for (i = 0; i * SKEIN_256_BLOCK_BYTES < byteCnt; i++) {
		 
		*(uint64_t *)ctx->b = Skein_Swap64((uint64_t)i);
		Skein_Start_New_Type(ctx, OUT_FINAL);
		 
		Skein_256_Process_Block(ctx, ctx->b, 1, sizeof (uint64_t));
		 
		n = byteCnt - i * SKEIN_256_BLOCK_BYTES;
		if (n >= SKEIN_256_BLOCK_BYTES)
			n = SKEIN_256_BLOCK_BYTES;
		Skein_Put64_LSB_First(hashVal + i * SKEIN_256_BLOCK_BYTES,
		    ctx->X, n);	 
		Skein_Show_Final(256, &ctx->h, n,
		    hashVal + i * SKEIN_256_BLOCK_BYTES);
		 
		memcpy(ctx->X, X, sizeof (X));
	}
	return (SKEIN_SUCCESS);
}

 

 
int
Skein_512_Init(Skein_512_Ctxt_t *ctx, size_t hashBitLen)
{
	union {
		uint8_t b[SKEIN_512_STATE_BYTES];
		uint64_t w[SKEIN_512_STATE_WORDS];
	} cfg;			 

	Skein_Assert(hashBitLen > 0, SKEIN_BAD_HASHLEN);
	ctx->h.hashBitLen = hashBitLen;	 

	switch (hashBitLen) {	 
#ifndef	SKEIN_NO_PRECOMP
	case 512:
		memcpy(ctx->X, SKEIN_512_IV_512, sizeof (ctx->X));
		break;
	case 384:
		memcpy(ctx->X, SKEIN_512_IV_384, sizeof (ctx->X));
		break;
	case 256:
		memcpy(ctx->X, SKEIN_512_IV_256, sizeof (ctx->X));
		break;
	case 224:
		memcpy(ctx->X, SKEIN_512_IV_224, sizeof (ctx->X));
		break;
#endif
	default:
		 
		 
		Skein_Start_New_Type(ctx, CFG_FINAL);

		 
		cfg.w[0] = Skein_Swap64(SKEIN_SCHEMA_VER);
		 
		cfg.w[1] = Skein_Swap64(hashBitLen);
		cfg.w[2] = Skein_Swap64(SKEIN_CFG_TREE_INFO_SEQUENTIAL);
		 
		memset(&cfg.w[3], 0, sizeof (cfg) - 3 * sizeof (cfg.w[0]));

		 
		 
		memset(ctx->X, 0, sizeof (ctx->X));
		Skein_512_Process_Block(ctx, cfg.b, 1, SKEIN_CFG_STR_LEN);
		break;
	}

	 
	Skein_Start_New_Type(ctx, MSG);	 

	return (SKEIN_SUCCESS);
}

 
 
int
Skein_512_InitExt(Skein_512_Ctxt_t *ctx, size_t hashBitLen, uint64_t treeInfo,
    const uint8_t *key, size_t keyBytes)
{
	union {
		uint8_t b[SKEIN_512_STATE_BYTES];
		uint64_t w[SKEIN_512_STATE_WORDS];
	} cfg;			 

	Skein_Assert(hashBitLen > 0, SKEIN_BAD_HASHLEN);
	Skein_Assert(keyBytes == 0 || key != NULL, SKEIN_FAIL);

	 
	if (keyBytes == 0) {	 
		 
		memset(ctx->X, 0, sizeof (ctx->X));
	} else {		 

		Skein_assert(sizeof (cfg.b) >= sizeof (ctx->X));
		 
		 
		ctx->h.hashBitLen = 8 * sizeof (ctx->X);
		 
		Skein_Start_New_Type(ctx, KEY);
		 
		memset(ctx->X, 0, sizeof (ctx->X));
		(void) Skein_512_Update(ctx, key, keyBytes);  
		 
		(void) Skein_512_Final_Pad(ctx, cfg.b);
		 
		memcpy(ctx->X, cfg.b, sizeof (cfg.b));
#if	SKEIN_NEED_SWAP
		{
			uint_t i;
			 
			for (i = 0; i < SKEIN_512_STATE_WORDS; i++)
				ctx->X[i] = Skein_Swap64(ctx->X[i]);
		}
#endif
	}
	 
	ctx->h.hashBitLen = hashBitLen;	 
	Skein_Start_New_Type(ctx, CFG_FINAL);

	memset(&cfg.w, 0, sizeof (cfg.w));  
	cfg.w[0] = Skein_Swap64(SKEIN_SCHEMA_VER);
	cfg.w[1] = Skein_Swap64(hashBitLen);	 
	 
	cfg.w[2] = Skein_Swap64(treeInfo);

	Skein_Show_Key(512, &ctx->h, key, keyBytes);

	 
	Skein_512_Process_Block(ctx, cfg.b, 1, SKEIN_CFG_STR_LEN);

	 
	 
	ctx->h.bCnt = 0;	 
	Skein_Start_New_Type(ctx, MSG);

	return (SKEIN_SUCCESS);
}

 
int
Skein_512_Update(Skein_512_Ctxt_t *ctx, const uint8_t *msg, size_t msgByteCnt)
{
	size_t n;

	 
	Skein_Assert(ctx->h.bCnt <= SKEIN_512_BLOCK_BYTES, SKEIN_FAIL);

	 
	if (msgByteCnt + ctx->h.bCnt > SKEIN_512_BLOCK_BYTES) {
		 
		if (ctx->h.bCnt) {
			 
			n = SKEIN_512_BLOCK_BYTES - ctx->h.bCnt;
			if (n) {
				 
				Skein_assert(n < msgByteCnt);
				memcpy(&ctx->b[ctx->h.bCnt], msg, n);
				msgByteCnt -= n;
				msg += n;
				ctx->h.bCnt += n;
			}
			Skein_assert(ctx->h.bCnt == SKEIN_512_BLOCK_BYTES);
			Skein_512_Process_Block(ctx, ctx->b, 1,
			    SKEIN_512_BLOCK_BYTES);
			ctx->h.bCnt = 0;
		}
		 
		if (msgByteCnt > SKEIN_512_BLOCK_BYTES) {
			 
			n = (msgByteCnt - 1) / SKEIN_512_BLOCK_BYTES;
			Skein_512_Process_Block(ctx, msg, n,
			    SKEIN_512_BLOCK_BYTES);
			msgByteCnt -= n * SKEIN_512_BLOCK_BYTES;
			msg += n * SKEIN_512_BLOCK_BYTES;
		}
		Skein_assert(ctx->h.bCnt == 0);
	}

	 
	if (msgByteCnt) {
		Skein_assert(msgByteCnt + ctx->h.bCnt <= SKEIN_512_BLOCK_BYTES);
		memcpy(&ctx->b[ctx->h.bCnt], msg, msgByteCnt);
		ctx->h.bCnt += msgByteCnt;
	}

	return (SKEIN_SUCCESS);
}

 
int
Skein_512_Final(Skein_512_Ctxt_t *ctx, uint8_t *hashVal)
{
	size_t i, n, byteCnt;
	uint64_t X[SKEIN_512_STATE_WORDS];

	 
	Skein_Assert(ctx->h.bCnt <= SKEIN_512_BLOCK_BYTES, SKEIN_FAIL);

	ctx->h.T[1] |= SKEIN_T1_FLAG_FINAL;	 
	 
	if (ctx->h.bCnt < SKEIN_512_BLOCK_BYTES)
		memset(&ctx->b[ctx->h.bCnt], 0,
		    SKEIN_512_BLOCK_BYTES - ctx->h.bCnt);

	 
	Skein_512_Process_Block(ctx, ctx->b, 1, ctx->h.bCnt);

	 
	 
	byteCnt = (ctx->h.hashBitLen + 7) >> 3;

	 
	 
	memset(ctx->b, 0, sizeof (ctx->b));
	 
	memcpy(X, ctx->X, sizeof (X));
	for (i = 0; i * SKEIN_512_BLOCK_BYTES < byteCnt; i++) {
		 
		*(uint64_t *)ctx->b = Skein_Swap64((uint64_t)i);
		Skein_Start_New_Type(ctx, OUT_FINAL);
		 
		Skein_512_Process_Block(ctx, ctx->b, 1, sizeof (uint64_t));
		 
		n = byteCnt - i * SKEIN_512_BLOCK_BYTES;
		if (n >= SKEIN_512_BLOCK_BYTES)
			n = SKEIN_512_BLOCK_BYTES;
		Skein_Put64_LSB_First(hashVal + i * SKEIN_512_BLOCK_BYTES,
		    ctx->X, n);	 
		Skein_Show_Final(512, &ctx->h, n,
		    hashVal + i * SKEIN_512_BLOCK_BYTES);
		 
		memcpy(ctx->X, X, sizeof (X));
	}
	return (SKEIN_SUCCESS);
}

 

 
int
Skein1024_Init(Skein1024_Ctxt_t *ctx, size_t hashBitLen)
{
	union {
		uint8_t b[SKEIN1024_STATE_BYTES];
		uint64_t w[SKEIN1024_STATE_WORDS];
	} cfg;			 

	Skein_Assert(hashBitLen > 0, SKEIN_BAD_HASHLEN);
	ctx->h.hashBitLen = hashBitLen;	 

	switch (hashBitLen) {	 
#ifndef	SKEIN_NO_PRECOMP
	case 512:
		memcpy(ctx->X, SKEIN1024_IV_512, sizeof (ctx->X));
		break;
	case 384:
		memcpy(ctx->X, SKEIN1024_IV_384, sizeof (ctx->X));
		break;
	case 1024:
		memcpy(ctx->X, SKEIN1024_IV_1024, sizeof (ctx->X));
		break;
#endif
	default:
		 
		 
		 
		Skein_Start_New_Type(ctx, CFG_FINAL);

		 
		cfg.w[0] = Skein_Swap64(SKEIN_SCHEMA_VER);
		 
		cfg.w[1] = Skein_Swap64(hashBitLen);
		cfg.w[2] = Skein_Swap64(SKEIN_CFG_TREE_INFO_SEQUENTIAL);
		 
		memset(&cfg.w[3], 0, sizeof (cfg) - 3 * sizeof (cfg.w[0]));

		 
		 
		memset(ctx->X, 0, sizeof (ctx->X));
		Skein1024_Process_Block(ctx, cfg.b, 1, SKEIN_CFG_STR_LEN);
		break;
	}

	 
	Skein_Start_New_Type(ctx, MSG);	 

	return (SKEIN_SUCCESS);
}

 
 
int
Skein1024_InitExt(Skein1024_Ctxt_t *ctx, size_t hashBitLen, uint64_t treeInfo,
    const uint8_t *key, size_t keyBytes)
{
	union {
		uint8_t b[SKEIN1024_STATE_BYTES];
		uint64_t w[SKEIN1024_STATE_WORDS];
	} cfg;			 

	Skein_Assert(hashBitLen > 0, SKEIN_BAD_HASHLEN);
	Skein_Assert(keyBytes == 0 || key != NULL, SKEIN_FAIL);

	 
	if (keyBytes == 0) {	 
		 
		memset(ctx->X, 0, sizeof (ctx->X));
	} else {		 
		Skein_assert(sizeof (cfg.b) >= sizeof (ctx->X));
		 
		 
		ctx->h.hashBitLen = 8 * sizeof (ctx->X);
		 
		Skein_Start_New_Type(ctx, KEY);
		 
		memset(ctx->X, 0, sizeof (ctx->X));
		(void) Skein1024_Update(ctx, key, keyBytes);  
		 
		(void) Skein1024_Final_Pad(ctx, cfg.b);
		 
		memcpy(ctx->X, cfg.b, sizeof (cfg.b));
#if	SKEIN_NEED_SWAP
		{
			uint_t i;
			 
			for (i = 0; i < SKEIN1024_STATE_WORDS; i++)
				ctx->X[i] = Skein_Swap64(ctx->X[i]);
		}
#endif
	}
	 
	ctx->h.hashBitLen = hashBitLen;	 
	Skein_Start_New_Type(ctx, CFG_FINAL);

	memset(&cfg.w, 0, sizeof (cfg.w));  
	cfg.w[0] = Skein_Swap64(SKEIN_SCHEMA_VER);
	 
	cfg.w[1] = Skein_Swap64(hashBitLen);
	 
	cfg.w[2] = Skein_Swap64(treeInfo);

	Skein_Show_Key(1024, &ctx->h, key, keyBytes);

	 
	Skein1024_Process_Block(ctx, cfg.b, 1, SKEIN_CFG_STR_LEN);

	 
	 
	ctx->h.bCnt = 0;	 
	Skein_Start_New_Type(ctx, MSG);

	return (SKEIN_SUCCESS);
}

 
int
Skein1024_Update(Skein1024_Ctxt_t *ctx, const uint8_t *msg, size_t msgByteCnt)
{
	size_t n;

	 
	Skein_Assert(ctx->h.bCnt <= SKEIN1024_BLOCK_BYTES, SKEIN_FAIL);

	 
	if (msgByteCnt + ctx->h.bCnt > SKEIN1024_BLOCK_BYTES) {
		 
		if (ctx->h.bCnt) {
			 
			n = SKEIN1024_BLOCK_BYTES - ctx->h.bCnt;
			if (n) {
				 
				Skein_assert(n < msgByteCnt);
				memcpy(&ctx->b[ctx->h.bCnt], msg, n);
				msgByteCnt -= n;
				msg += n;
				ctx->h.bCnt += n;
			}
			Skein_assert(ctx->h.bCnt == SKEIN1024_BLOCK_BYTES);
			Skein1024_Process_Block(ctx, ctx->b, 1,
			    SKEIN1024_BLOCK_BYTES);
			ctx->h.bCnt = 0;
		}
		 
		if (msgByteCnt > SKEIN1024_BLOCK_BYTES) {
			 
			n = (msgByteCnt - 1) / SKEIN1024_BLOCK_BYTES;
			Skein1024_Process_Block(ctx, msg, n,
			    SKEIN1024_BLOCK_BYTES);
			msgByteCnt -= n * SKEIN1024_BLOCK_BYTES;
			msg += n * SKEIN1024_BLOCK_BYTES;
		}
		Skein_assert(ctx->h.bCnt == 0);
	}

	 
	if (msgByteCnt) {
		Skein_assert(msgByteCnt + ctx->h.bCnt <= SKEIN1024_BLOCK_BYTES);
		memcpy(&ctx->b[ctx->h.bCnt], msg, msgByteCnt);
		ctx->h.bCnt += msgByteCnt;
	}

	return (SKEIN_SUCCESS);
}

 
int
Skein1024_Final(Skein1024_Ctxt_t *ctx, uint8_t *hashVal)
{
	size_t i, n, byteCnt;
	uint64_t X[SKEIN1024_STATE_WORDS];

	 
	Skein_Assert(ctx->h.bCnt <= SKEIN1024_BLOCK_BYTES, SKEIN_FAIL);

	ctx->h.T[1] |= SKEIN_T1_FLAG_FINAL;	 
	 
	if (ctx->h.bCnt < SKEIN1024_BLOCK_BYTES)
		memset(&ctx->b[ctx->h.bCnt], 0,
		    SKEIN1024_BLOCK_BYTES - ctx->h.bCnt);

	 
	Skein1024_Process_Block(ctx, ctx->b, 1, ctx->h.bCnt);

	 
	 
	byteCnt = (ctx->h.hashBitLen + 7) >> 3;

	 
	 
	memset(ctx->b, 0, sizeof (ctx->b));
	 
	memcpy(X, ctx->X, sizeof (X));
	for (i = 0; i * SKEIN1024_BLOCK_BYTES < byteCnt; i++) {
		 
		*(uint64_t *)ctx->b = Skein_Swap64((uint64_t)i);
		Skein_Start_New_Type(ctx, OUT_FINAL);
		 
		Skein1024_Process_Block(ctx, ctx->b, 1, sizeof (uint64_t));
		 
		n = byteCnt - i * SKEIN1024_BLOCK_BYTES;
		if (n >= SKEIN1024_BLOCK_BYTES)
			n = SKEIN1024_BLOCK_BYTES;
		Skein_Put64_LSB_First(hashVal + i * SKEIN1024_BLOCK_BYTES,
		    ctx->X, n);	 
		Skein_Show_Final(1024, &ctx->h, n,
		    hashVal + i * SKEIN1024_BLOCK_BYTES);
		 
		memcpy(ctx->X, X, sizeof (X));
	}
	return (SKEIN_SUCCESS);
}

 
 

 
int
Skein_256_Final_Pad(Skein_256_Ctxt_t *ctx, uint8_t *hashVal)
{
	 
	Skein_Assert(ctx->h.bCnt <= SKEIN_256_BLOCK_BYTES, SKEIN_FAIL);

	ctx->h.T[1] |= SKEIN_T1_FLAG_FINAL;	 
	 
	if (ctx->h.bCnt < SKEIN_256_BLOCK_BYTES)
		memset(&ctx->b[ctx->h.bCnt], 0,
		    SKEIN_256_BLOCK_BYTES - ctx->h.bCnt);
	 
	Skein_256_Process_Block(ctx, ctx->b, 1, ctx->h.bCnt);

	 
	Skein_Put64_LSB_First(hashVal, ctx->X, SKEIN_256_BLOCK_BYTES);

	return (SKEIN_SUCCESS);
}

 
int
Skein_512_Final_Pad(Skein_512_Ctxt_t *ctx, uint8_t *hashVal)
{
	 
	Skein_Assert(ctx->h.bCnt <= SKEIN_512_BLOCK_BYTES, SKEIN_FAIL);

	ctx->h.T[1] |= SKEIN_T1_FLAG_FINAL;	 
	 
	if (ctx->h.bCnt < SKEIN_512_BLOCK_BYTES)
		memset(&ctx->b[ctx->h.bCnt], 0,
		    SKEIN_512_BLOCK_BYTES - ctx->h.bCnt);
	 
	Skein_512_Process_Block(ctx, ctx->b, 1, ctx->h.bCnt);

	 
	Skein_Put64_LSB_First(hashVal, ctx->X, SKEIN_512_BLOCK_BYTES);

	return (SKEIN_SUCCESS);
}

 
int
Skein1024_Final_Pad(Skein1024_Ctxt_t *ctx, uint8_t *hashVal)
{
	 
	Skein_Assert(ctx->h.bCnt <= SKEIN1024_BLOCK_BYTES, SKEIN_FAIL);

	 
	ctx->h.T[1] |= SKEIN_T1_FLAG_FINAL;
	 
	if (ctx->h.bCnt < SKEIN1024_BLOCK_BYTES)
		memset(&ctx->b[ctx->h.bCnt], 0,
		    SKEIN1024_BLOCK_BYTES - ctx->h.bCnt);
	 
	Skein1024_Process_Block(ctx, ctx->b, 1, ctx->h.bCnt);

	 
	Skein_Put64_LSB_First(hashVal, ctx->X, SKEIN1024_BLOCK_BYTES);

	return (SKEIN_SUCCESS);
}

#if	SKEIN_TREE_HASH
 
int
Skein_256_Output(Skein_256_Ctxt_t *ctx, uint8_t *hashVal)
{
	size_t i, n, byteCnt;
	uint64_t X[SKEIN_256_STATE_WORDS];

	 
	Skein_Assert(ctx->h.bCnt <= SKEIN_256_BLOCK_BYTES, SKEIN_FAIL);

	 
	 
	byteCnt = (ctx->h.hashBitLen + 7) >> 3;

	 
	 
	memset(ctx->b, 0, sizeof (ctx->b));
	 
	memcpy(X, ctx->X, sizeof (X));
	for (i = 0; i * SKEIN_256_BLOCK_BYTES < byteCnt; i++) {
		 
		*(uint64_t *)ctx->b = Skein_Swap64((uint64_t)i);
		Skein_Start_New_Type(ctx, OUT_FINAL);
		 
		Skein_256_Process_Block(ctx, ctx->b, 1, sizeof (uint64_t));
		 
		n = byteCnt - i * SKEIN_256_BLOCK_BYTES;
		if (n >= SKEIN_256_BLOCK_BYTES)
			n = SKEIN_256_BLOCK_BYTES;
		Skein_Put64_LSB_First(hashVal + i * SKEIN_256_BLOCK_BYTES,
		    ctx->X, n);	 
		Skein_Show_Final(256, &ctx->h, n,
		    hashVal + i * SKEIN_256_BLOCK_BYTES);
		 
		memcpy(ctx->X, X, sizeof (X));
	}
	return (SKEIN_SUCCESS);
}

 
int
Skein_512_Output(Skein_512_Ctxt_t *ctx, uint8_t *hashVal)
{
	size_t i, n, byteCnt;
	uint64_t X[SKEIN_512_STATE_WORDS];

	 
	Skein_Assert(ctx->h.bCnt <= SKEIN_512_BLOCK_BYTES, SKEIN_FAIL);

	 
	 
	byteCnt = (ctx->h.hashBitLen + 7) >> 3;

	 
	 
	memset(ctx->b, 0, sizeof (ctx->b));
	 
	memcpy(X, ctx->X, sizeof (X));
	for (i = 0; i * SKEIN_512_BLOCK_BYTES < byteCnt; i++) {
		 
		*(uint64_t *)ctx->b = Skein_Swap64((uint64_t)i);
		Skein_Start_New_Type(ctx, OUT_FINAL);
		 
		Skein_512_Process_Block(ctx, ctx->b, 1, sizeof (uint64_t));
		 
		n = byteCnt - i * SKEIN_512_BLOCK_BYTES;
		if (n >= SKEIN_512_BLOCK_BYTES)
			n = SKEIN_512_BLOCK_BYTES;
		Skein_Put64_LSB_First(hashVal + i * SKEIN_512_BLOCK_BYTES,
		    ctx->X, n);	 
		Skein_Show_Final(256, &ctx->h, n,
		    hashVal + i * SKEIN_512_BLOCK_BYTES);
		 
		memcpy(ctx->X, X, sizeof (X));
	}
	return (SKEIN_SUCCESS);
}

 
int
Skein1024_Output(Skein1024_Ctxt_t *ctx, uint8_t *hashVal)
{
	size_t i, n, byteCnt;
	uint64_t X[SKEIN1024_STATE_WORDS];

	 
	Skein_Assert(ctx->h.bCnt <= SKEIN1024_BLOCK_BYTES, SKEIN_FAIL);

	 
	 
	byteCnt = (ctx->h.hashBitLen + 7) >> 3;

	 
	 
	memset(ctx->b, 0, sizeof (ctx->b));
	 
	memcpy(X, ctx->X, sizeof (X));
	for (i = 0; i * SKEIN1024_BLOCK_BYTES < byteCnt; i++) {
		 
		*(uint64_t *)ctx->b = Skein_Swap64((uint64_t)i);
		Skein_Start_New_Type(ctx, OUT_FINAL);
		 
		Skein1024_Process_Block(ctx, ctx->b, 1, sizeof (uint64_t));
		 
		n = byteCnt - i * SKEIN1024_BLOCK_BYTES;
		if (n >= SKEIN1024_BLOCK_BYTES)
			n = SKEIN1024_BLOCK_BYTES;
		Skein_Put64_LSB_First(hashVal + i * SKEIN1024_BLOCK_BYTES,
		    ctx->X, n);	 
		Skein_Show_Final(256, &ctx->h, n,
		    hashVal + i * SKEIN1024_BLOCK_BYTES);
		 
		memcpy(ctx->X, X, sizeof (X));
	}
	return (SKEIN_SUCCESS);
}
#endif

#ifdef _KERNEL
EXPORT_SYMBOL(Skein_512_Init);
EXPORT_SYMBOL(Skein_512_InitExt);
EXPORT_SYMBOL(Skein_512_Update);
EXPORT_SYMBOL(Skein_512_Final);
#endif
