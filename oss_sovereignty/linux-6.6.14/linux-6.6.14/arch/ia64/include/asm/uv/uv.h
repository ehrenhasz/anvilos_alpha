#ifndef _ASM_IA64_UV_UV_H
#define _ASM_IA64_UV_UV_H
#ifdef CONFIG_IA64_SGI_UV
extern bool ia64_is_uv;
static inline int is_uv_system(void)
{
	return ia64_is_uv;
}
void __init uv_probe_system_type(void);
void __init uv_setup(char **cmdline_p);
#else  
static inline int is_uv_system(void)
{
	return false;
}
static inline void __init uv_probe_system_type(void)
{
}
static inline void __init uv_setup(char **cmdline_p)
{
}
#endif  
#endif	 
