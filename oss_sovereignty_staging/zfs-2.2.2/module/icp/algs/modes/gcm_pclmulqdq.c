 
 

#if defined(__x86_64) && defined(HAVE_PCLMULQDQ)

#include <sys/types.h>
#include <sys/simd.h>
#include <sys/asm_linkage.h>

 
extern void ASMABI gcm_mul_pclmulqdq(uint64_t *, uint64_t *, uint64_t *);

#include <modes/gcm_impl.h>

 
static void
gcm_pclmulqdq_mul(uint64_t *x_in, uint64_t *y, uint64_t *res)
{
	kfpu_begin();
	gcm_mul_pclmulqdq(x_in, y, res);
	kfpu_end();
}

static boolean_t
gcm_pclmulqdq_will_work(void)
{
	return (kfpu_allowed() && zfs_pclmulqdq_available());
}

const gcm_impl_ops_t gcm_pclmulqdq_impl = {
	.mul = &gcm_pclmulqdq_mul,
	.is_supported = &gcm_pclmulqdq_will_work,
	.name = "pclmulqdq"
};

#endif  
