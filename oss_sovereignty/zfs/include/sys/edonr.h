#ifndef	_SYS_EDONR_H_
#define	_SYS_EDONR_H_
#ifdef	__cplusplus
extern "C" {
#endif
#ifdef  _KERNEL
#include <sys/types.h>
#else
#include <stdint.h>
#include <stdlib.h>
#endif
#define	EdonR512_DIGEST_SIZE	64
#define	EdonR512_BLOCK_SIZE	128
#define	EdonR512_BLOCK_BITSIZE	1024
typedef struct {
	uint64_t DoublePipe[16];
	uint8_t LastPart[EdonR512_BLOCK_SIZE * 2];
} EdonRData512;
typedef struct {
	uint64_t bits_processed;
	int unprocessed_bits;
	union {
		EdonRData512 p512[1];
	} pipe[1];
} EdonRState;
void EdonRInit(EdonRState *state);
void EdonRUpdate(EdonRState *state, const uint8_t *data, size_t databitlen);
void EdonRFinal(EdonRState *state, uint8_t *hashval);
void EdonRHash(const uint8_t *data, size_t databitlen, uint8_t *hashval);
#ifdef	__cplusplus
}
#endif
#endif	 
