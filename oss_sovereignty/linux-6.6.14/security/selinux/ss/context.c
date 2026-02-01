
 

#include <linux/jhash.h>

#include "context.h"
#include "mls.h"

u32 context_compute_hash(const struct context *c)
{
	u32 hash = 0;

	 
	if (c->len)
		return full_name_hash(NULL, c->str, c->len);

	hash = jhash_3words(c->user, c->role, c->type, hash);
	hash = mls_range_hash(&c->range, hash);
	return hash;
}
