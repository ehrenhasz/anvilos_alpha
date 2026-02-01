 
 
#ifndef _DMU_REDACT_H_
#define	_DMU_REDACT_H_

#include <sys/spa.h>
#include <sys/dsl_bookmark.h>

#define	REDACT_BLOCK_MAX_COUNT (1ULL << 48)

static inline uint64_t
redact_block_get_size(redact_block_phys_t *rbp)
{
	return (BF64_GET_SB((rbp)->rbp_size_count, 48, 16, SPA_MINBLOCKSHIFT,
	    0));
}

static inline void
redact_block_set_size(redact_block_phys_t *rbp, uint64_t size)
{
	 
	BF64_SET_SB((rbp)->rbp_size_count, 48, 16, SPA_MINBLOCKSHIFT, 0, size);
}

static inline uint64_t
redact_block_get_count(redact_block_phys_t *rbp)
{
	return (BF64_GET_SB((rbp)->rbp_size_count, 0, 48, 0, 1));
}

static inline void
redact_block_set_count(redact_block_phys_t *rbp, uint64_t count)
{
	 
	BF64_SET_SB((rbp)->rbp_size_count, 0, 48, 0, 1, count);
}

int dmu_redact_snap(const char *, nvlist_t *, const char *);
#endif  
