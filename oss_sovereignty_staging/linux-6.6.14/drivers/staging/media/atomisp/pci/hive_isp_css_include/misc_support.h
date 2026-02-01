 
 

#ifndef __MISC_SUPPORT_H_INCLUDED__
#define __MISC_SUPPORT_H_INCLUDED__

 
#ifndef NOT_USED
#define NOT_USED(a) ((void)(a))
#endif

 
#define tot_bytes_for_pow2_align(pow2, cur_bytes)	((cur_bytes + (pow2 - 1)) & ~(pow2 - 1))

#endif  
