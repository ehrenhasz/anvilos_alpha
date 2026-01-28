



#ifndef	_SYS_BLAKE3_H
#define	_SYS_BLAKE3_H

#ifdef  _KERNEL
#include <sys/types.h>
#else
#include <stdint.h>
#include <stdlib.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define	BLAKE3_KEY_LEN		32
#define	BLAKE3_OUT_LEN		32
#define	BLAKE3_MAX_DEPTH	54
#define	BLAKE3_BLOCK_LEN	64
#define	BLAKE3_CHUNK_LEN	1024


typedef struct {
	uint32_t cv[8];
	uint64_t chunk_counter;
	uint8_t buf[BLAKE3_BLOCK_LEN];
	uint8_t buf_len;
	uint8_t blocks_compressed;
	uint8_t flags;
} blake3_chunk_state_t;

typedef struct {
	uint32_t key[8];
	blake3_chunk_state_t chunk;
	uint8_t cv_stack_len;

	
	uint8_t cv_stack[(BLAKE3_MAX_DEPTH + 1) * BLAKE3_OUT_LEN];

	
	const void *ops;
} BLAKE3_CTX;


void Blake3_Init(BLAKE3_CTX *ctx);


void Blake3_InitKeyed(BLAKE3_CTX *ctx, const uint8_t key[BLAKE3_KEY_LEN]);


void Blake3_Update(BLAKE3_CTX *ctx, const void *input, size_t input_len);


void Blake3_Final(const BLAKE3_CTX *ctx, uint8_t *out);


void Blake3_FinalSeek(const BLAKE3_CTX *ctx, uint64_t seek, uint8_t *out,
    size_t out_len);


extern void **blake3_per_cpu_ctx;
extern void blake3_per_cpu_ctx_init(void);
extern void blake3_per_cpu_ctx_fini(void);

#ifdef __cplusplus
}
#endif

#endif	
