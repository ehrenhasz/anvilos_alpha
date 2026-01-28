


#ifndef _DM_PERSISTENT_DATA_INTERNAL_H
#define _DM_PERSISTENT_DATA_INTERNAL_H

#include "dm-block-manager.h"

static inline unsigned int dm_hash_block(dm_block_t b, unsigned int hash_mask)
{
	const unsigned int BIG_PRIME = 4294967291UL;

	return (((unsigned int) b) * BIG_PRIME) & hash_mask;
}

#endif	
