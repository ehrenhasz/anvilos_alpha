
#ifndef MICROPY_INCLUDED_RENESAS_RA_IT_H
#define MICROPY_INCLUDED_RENESAS_RA_IT_H

extern int pyb_hard_fault_debug;

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

#endif 
