#ifndef _ASM_SPARC_VDSO_H
#define _ASM_SPARC_VDSO_H
struct vdso_image {
	void *data;
	unsigned long size;    
	long sym_vvar_start;   
};
#ifdef CONFIG_SPARC64
extern const struct vdso_image vdso_image_64_builtin;
#endif
#ifdef CONFIG_COMPAT
extern const struct vdso_image vdso_image_32_builtin;
#endif
#endif  
