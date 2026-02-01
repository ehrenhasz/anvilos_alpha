 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


 
#define VMA_PROT_READ    (1<<0)
#define VMA_PROT_WRITE   (1<<1)
#define VMA_PROT_EXECUTE (1<<2)

typedef int (*vma_iterate_callback_fn) (void *data,
                                        uintptr_t start, uintptr_t end,
                                        unsigned int flags);

 
extern int vma_iterate (vma_iterate_callback_fn callback, void *data);

 
#if defined __linux__ || defined __ANDROID__ || defined __GNU__ || defined __FreeBSD_kernel__ || defined __FreeBSD__ || defined __DragonFly__ || defined __NetBSD__ || defined _AIX || defined __sgi || defined __osf__ || defined __sun || HAVE_PSTAT_GETPROCVM || (defined __APPLE__ && defined __MACH__) || defined _WIN32 || defined __CYGWIN__ || defined __BEOS__ || defined __HAIKU__ || defined __minix || HAVE_MQUERY
# define VMA_ITERATE_SUPPORTED 1
#endif


#ifdef __cplusplus
}
#endif

#endif  
