

#ifndef __NONOS_H__
#define	__NONOS_H__

#ifdef	__cplusplus
extern "C" {
#endif


#if (defined (SL_PLATFORM_MULTI_THREADED)) && (!defined (SL_PLATFORM_EXTERNAL_SPAWN))

extern void _SlInternalSpawnTaskEntry();
extern _i16 _SlInternalSpawn(_SlSpawnEntryFunc_t pEntry , void* pValue , _u32 flags);

#undef sl_Spawn
#define sl_Spawn(pEntry,pValue,flags)               _SlInternalSpawn(pEntry,pValue,flags)

#undef _SlTaskEntry
#define _SlTaskEntry                                _SlInternalSpawnTaskEntry


#endif

#ifdef  __cplusplus
}
#endif 

#endif
