






































#ifndef __CPU_H__
#define __CPU_H__







#ifdef __cplusplus
extern "C"
{
#endif






extern unsigned long CPUcpsid(void);
extern unsigned long CPUcpsie(void);
extern unsigned long CPUprimask(void);
extern void CPUwfi(void);
extern unsigned long CPUbasepriGet(void);
extern void CPUbasepriSet(unsigned long ulNewBasepri);






#ifdef __cplusplus
}
#endif

#endif 
