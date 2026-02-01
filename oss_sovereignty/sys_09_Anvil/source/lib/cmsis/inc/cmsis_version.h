 
 

#if   defined ( __ICCARM__ )
  #pragma system_include          
#elif defined (__clang__)
  #pragma clang system_header    
#endif

#ifndef __CMSIS_VERSION_H
#define __CMSIS_VERSION_H

 
#define __CM_CMSIS_VERSION_MAIN  ( 5U)                                       
#define __CM_CMSIS_VERSION_SUB   ( 6U)                                       
#define __CM_CMSIS_VERSION       ((__CM_CMSIS_VERSION_MAIN << 16U) | \
                                   __CM_CMSIS_VERSION_SUB           )        
#endif
