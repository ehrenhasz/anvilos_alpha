#ifndef __ASM_ARC_ASSERTS_H
#define __ASM_ARC_ASSERTS_H
void chk_opt_strict(char *opt_name, bool hw_exists, bool opt_ena);
void chk_opt_weak(char *opt_name, bool hw_exists, bool opt_ena);
#define CHK_OPT_STRICT(opt_name, hw_exists)				\
({									\
	chk_opt_strict(#opt_name, hw_exists, IS_ENABLED(opt_name));	\
})
#define CHK_OPT_WEAK(opt_name, hw_exists)				\
({									\
	chk_opt_weak(#opt_name, hw_exists, IS_ENABLED(opt_name));	\
})
#endif  
