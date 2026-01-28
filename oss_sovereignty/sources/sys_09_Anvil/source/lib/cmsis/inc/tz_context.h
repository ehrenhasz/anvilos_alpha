


#if   defined ( __ICCARM__ )
  #pragma system_include         
#elif defined (__clang__)
  #pragma clang system_header   
#endif

#ifndef TZ_CONTEXT_H
#define TZ_CONTEXT_H
 
#include <stdint.h>
 
#ifndef TZ_MODULEID_T
#define TZ_MODULEID_T

typedef uint32_t TZ_ModuleId_t;
#endif
 

typedef uint32_t TZ_MemoryId_t;
  


uint32_t TZ_InitContextSystem_S (void);
 




TZ_MemoryId_t TZ_AllocModuleContext_S (TZ_ModuleId_t module);
 



uint32_t TZ_FreeModuleContext_S (TZ_MemoryId_t id);
 



uint32_t TZ_LoadContext_S (TZ_MemoryId_t id);
 



uint32_t TZ_StoreContext_S (TZ_MemoryId_t id);
 
#endif  
