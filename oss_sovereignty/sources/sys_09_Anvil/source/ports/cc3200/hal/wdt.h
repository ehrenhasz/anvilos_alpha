




































#ifndef __WATCHDOG_H__
#define __WATCHDOG_H__







#ifdef __cplusplus
extern "C"
{
#endif







extern tBoolean WatchdogRunning(unsigned long ulBase);
extern void WatchdogEnable(unsigned long ulBase);
extern void WatchdogLock(unsigned long ulBase);
extern void WatchdogUnlock(unsigned long ulBase);
extern tBoolean WatchdogLockState(unsigned long ulBase);
extern void WatchdogReloadSet(unsigned long ulBase, unsigned long ulLoadVal);
extern unsigned long WatchdogReloadGet(unsigned long ulBase);
extern unsigned long WatchdogValueGet(unsigned long ulBase);
extern void WatchdogIntRegister(unsigned long ulBase, void(*pfnHandler)(void));
extern void WatchdogIntUnregister(unsigned long ulBase);
extern unsigned long WatchdogIntStatus(unsigned long ulBase, tBoolean bMasked);
extern void WatchdogIntClear(unsigned long ulBase);
extern void WatchdogStallEnable(unsigned long ulBase);
extern void WatchdogStallDisable(unsigned long ulBase);






#ifdef __cplusplus
}
#endif

#endif 
