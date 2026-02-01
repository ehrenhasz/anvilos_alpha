 
 

#ifndef _SECURITY_LANDLOCK_RULESET_H
#define _SECURITY_LANDLOCK_RULESET_H

#include <linux/bitops.h>
#include <linux/build_bug.h>
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/refcount.h>
#include <linux/workqueue.h>

#include "limits.h"
#include "object.h"

typedef u16 access_mask_t;
 
static_assert(BITS_PER_TYPE(access_mask_t) >= LANDLOCK_NUM_ACCESS_FS);
 
static_assert(sizeof(unsigned long) >= sizeof(access_mask_t));

typedef u16 layer_mask_t;
 
static_assert(BITS_PER_TYPE(layer_mask_t) >= LANDLOCK_MAX_NUM_LAYERS);

 
struct landlock_layer {
	 
	u16 level;
	 
	access_mask_t access;
};

 
struct landlock_rule {
	 
	struct rb_node node;
	 
	struct landlock_object *object;
	 
	u32 num_layers;
	 
	struct landlock_layer layers[] __counted_by(num_layers);
};

 
struct landlock_hierarchy {
	 
	struct landlock_hierarchy *parent;
	 
	refcount_t usage;
};

 
struct landlock_ruleset {
	 
	struct rb_root root;
	 
	struct landlock_hierarchy *hierarchy;
	union {
		 
		struct work_struct work_free;
		struct {
			 
			struct mutex lock;
			 
			refcount_t usage;
			 
			u32 num_rules;
			 
			u32 num_layers;
			 
			access_mask_t fs_access_masks[];
		};
	};
};

struct landlock_ruleset *
landlock_create_ruleset(const access_mask_t fs_access_mask);

void landlock_put_ruleset(struct landlock_ruleset *const ruleset);
void landlock_put_ruleset_deferred(struct landlock_ruleset *const ruleset);

int landlock_insert_rule(struct landlock_ruleset *const ruleset,
			 struct landlock_object *const object,
			 const access_mask_t access);

struct landlock_ruleset *
landlock_merge_ruleset(struct landlock_ruleset *const parent,
		       struct landlock_ruleset *const ruleset);

const struct landlock_rule *
landlock_find_rule(const struct landlock_ruleset *const ruleset,
		   const struct landlock_object *const object);

static inline void landlock_get_ruleset(struct landlock_ruleset *const ruleset)
{
	if (ruleset)
		refcount_inc(&ruleset->usage);
}

#endif  
