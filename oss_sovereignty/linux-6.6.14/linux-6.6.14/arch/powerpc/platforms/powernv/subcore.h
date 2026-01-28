#define SYNC_STEP_INITIAL	0
#define SYNC_STEP_UNSPLIT	1	 
#define SYNC_STEP_REAL_MODE	2	 
#define SYNC_STEP_FINISHED	3	 
#ifndef __ASSEMBLY__
#ifdef CONFIG_SMP
void split_core_secondary_loop(u8 *state);
extern void update_subcore_sibling_mask(void);
#else
static inline void update_subcore_sibling_mask(void) { }
#endif  
#endif  
