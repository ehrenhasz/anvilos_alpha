 

 

#ifndef _RELOCATABLE_H
#define _RELOCATABLE_H

 
#if ENABLE_RELOCATABLE

 
#if defined _MSC_VER && BUILDING_DLL
# define RELOCATABLE_DLL_EXPORTED __declspec(dllexport)
#else
# define RELOCATABLE_DLL_EXPORTED
#endif

 
extern RELOCATABLE_DLL_EXPORTED void
       set_relocation_prefix (const char *orig_prefix,
			      const char *curr_prefix);

 
extern const char * relocate (const char *pathname);

 

 
extern const char * compute_curr_prefix (const char *orig_installprefix,
					 const char *orig_installdir,
					 const char *curr_pathname);

#else

 
#define relocate(pathname) (pathname)

#endif

#endif  
