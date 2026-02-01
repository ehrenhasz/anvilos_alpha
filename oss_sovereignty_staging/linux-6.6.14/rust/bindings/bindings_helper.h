 
 

#include <kunit/test.h>
#include <linux/errname.h>
#include <linux/slab.h>
#include <linux/refcount.h>
#include <linux/wait.h>
#include <linux/sched.h>

 
const size_t BINDINGS_ARCH_SLAB_MINALIGN = ARCH_SLAB_MINALIGN;
const gfp_t BINDINGS_GFP_KERNEL = GFP_KERNEL;
const gfp_t BINDINGS___GFP_ZERO = __GFP_ZERO;
