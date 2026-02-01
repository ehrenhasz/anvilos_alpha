 
 
 

#ifndef _SS_MLS_TYPES_H_
#define _SS_MLS_TYPES_H_

#include "security.h"
#include "ebitmap.h"

struct mls_level {
	u32 sens;		 
	struct ebitmap cat;	 
};

struct mls_range {
	struct mls_level level[2];  
};

static inline int mls_level_eq(const struct mls_level *l1, const struct mls_level *l2)
{
	return ((l1->sens == l2->sens) &&
		ebitmap_cmp(&l1->cat, &l2->cat));
}

static inline int mls_level_dom(const struct mls_level *l1, const struct mls_level *l2)
{
	return ((l1->sens >= l2->sens) &&
		ebitmap_contains(&l1->cat, &l2->cat, 0));
}

#define mls_level_incomp(l1, l2) \
(!mls_level_dom((l1), (l2)) && !mls_level_dom((l2), (l1)))

#define mls_level_between(l1, l2, l3) \
(mls_level_dom((l1), (l2)) && mls_level_dom((l3), (l1)))

#define mls_range_contains(r1, r2) \
(mls_level_dom(&(r2).level[0], &(r1).level[0]) && \
 mls_level_dom(&(r1).level[1], &(r2).level[1]))

#endif	 
