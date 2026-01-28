






































#ifndef __PRCM_H__
#define __PRCM_H__







#ifdef __cplusplus
extern "C"
{
#endif






typedef struct _PRCM_PeripheralRegs_
{

unsigned char ulClkReg;
unsigned char ulRstReg;

}PRCM_PeriphRegs_t;





#define PRCM_RUN_MODE_CLK         0x00000001
#define PRCM_SLP_MODE_CLK         0x00000100
#define PRCM_DSLP_MODE_CLK        0x00010000





#define PRCM_SRAM_COL_1           0x00000001
#define PRCM_SRAM_COL_2           0x00000002
#define PRCM_SRAM_COL_3           0x00000004
#define PRCM_SRAM_COL_4           0x00000008





#define PRCM_SRAM_DSLP_RET        0x00000001
#define PRCM_SRAM_LPDS_RET        0x00000002





#define PRCM_LPDS_HOST_IRQ        0x00000080
#define PRCM_LPDS_GPIO            0x00000010
#define PRCM_LPDS_TIMER           0x00000001




#define PRCM_LPDS_LOW_LEVEL       0x00000002
#define PRCM_LPDS_HIGH_LEVEL      0x00000000
#define PRCM_LPDS_FALL_EDGE       0x00000001
#define PRCM_LPDS_RISE_EDGE       0x00000003




#define PRCM_LPDS_GPIO2           0x00000000
#define PRCM_LPDS_GPIO4           0x00000001
#define PRCM_LPDS_GPIO13          0x00000002
#define PRCM_LPDS_GPIO17          0x00000003
#define PRCM_LPDS_GPIO11          0x00000004
#define PRCM_LPDS_GPIO24          0x00000005
#define PRCM_LPDS_GPIO26          0x00000006





#define PRCM_HIB_SLOW_CLK_CTR     0x00000001




#define PRCM_HIB_LOW_LEVEL        0x00000000
#define PRCM_HIB_HIGH_LEVEL       0x00000001
#define PRCM_HIB_FALL_EDGE        0x00000002
#define PRCM_HIB_RISE_EDGE        0x00000003





#define PRCM_HIB_GPIO2            0x00010000
#define PRCM_HIB_GPIO4            0x00020000
#define PRCM_HIB_GPIO13           0x00040000
#define PRCM_HIB_GPIO17           0x00080000
#define PRCM_HIB_GPIO11           0x00100000
#define PRCM_HIB_GPIO24           0x00200000
#define PRCM_HIB_GPIO26           0x00400000




#define PRCM_POWER_ON             0x00000000
#define PRCM_LPDS_EXIT            0x00000001
#define PRCM_CORE_RESET           0x00000003
#define PRCM_MCU_RESET            0x00000004
#define PRCM_WDT_RESET            0x00000005
#define PRCM_SOC_RESET            0x00000006
#define PRCM_HIB_EXIT             0x00000007




#define PRCM_HIB_WAKEUP_CAUSE_SLOW_CLOCK  0x00000002
#define PRCM_HIB_WAKEUP_CAUSE_GPIO        0x00000004




#define PRCM_INT_SLOW_CLK_CTR     0x00004000





#define PRCM_CAMERA               0x00000000
#define PRCM_I2S                  0x00000001
#define PRCM_SDHOST               0x00000002
#define PRCM_GSPI                 0x00000003
#define PRCM_LSPI                 0x00000004
#define PRCM_UDMA                 0x00000005
#define PRCM_GPIOA0               0x00000006
#define PRCM_GPIOA1               0x00000007
#define PRCM_GPIOA2               0x00000008
#define PRCM_GPIOA3               0x00000009
#define PRCM_GPIOA4               0x0000000A
#define PRCM_WDT                  0x0000000B
#define PRCM_UARTA0               0x0000000C
#define PRCM_UARTA1               0x0000000D
#define PRCM_TIMERA0              0x0000000E
#define PRCM_TIMERA1              0x0000000F
#define PRCM_TIMERA2              0x00000010
#define PRCM_TIMERA3              0x00000011
#define PRCM_DTHE                 0x00000012
#define PRCM_SSPI                 0x00000013
#define PRCM_I2CA0                0x00000014


#define PRCM_ADC                  0x000000FF




#define PRCM_SAFE_BOOT_BIT              30
#define PRCM_WDT_RESET_BIT              29
#define PRCM_FIRST_BOOT_BIT             28






extern void PRCMSetSpecialBit(unsigned char bit);
extern void PRCMClearSpecialBit(unsigned char bit);
extern tBoolean PRCMGetSpecialBit(unsigned char bit);
extern void PRCMSOCReset(void);
extern void PRCMMCUReset(tBoolean bIncludeSubsystem);
extern unsigned long PRCMSysResetCauseGet(void);

extern void PRCMPeripheralClkEnable(unsigned long ulPeripheral,
                                    unsigned long ulClkFlags);
extern void PRCMPeripheralClkDisable(unsigned long ulPeripheral,
                                     unsigned long ulClkFlags);
extern void PRCMPeripheralReset(unsigned long ulPeripheral);
extern tBoolean PRCMPeripheralStatusGet(unsigned long ulPeripheral);

extern void PRCMI2SClockFreqSet(unsigned long ulI2CClkFreq);
extern unsigned long PRCMPeripheralClockGet(unsigned long ulPeripheral);

extern void PRCMSleepEnter(void);
extern void PRCMDeepSleepEnter(void);

extern void PRCMSRAMRetentionEnable(unsigned long ulSramColSel,
                                    unsigned long ulFlags);
extern void PRCMSRAMRetentionDisable(unsigned long ulSramColSel,
                                     unsigned long ulFlags);
extern void PRCMLPDSRestoreInfoSet(unsigned long ulRestoreSP,
                                   unsigned long ulRestorePC);
extern void PRCMLPDSEnter(void);
extern void PRCMLPDSIntervalSet(unsigned long ulTicks);
extern void PRCMLPDSWakeupSourceEnable(unsigned long ulLpdsWakeupSrc);
extern unsigned long PRCMLPDSWakeupCauseGet(void);
extern void PRCMLPDSWakeUpGPIOSelect(unsigned long ulGPIOPin,
                                     unsigned long ulType);
extern void PRCMLPDSWakeupSourceDisable(unsigned long ulLpdsWakeupSrc);

extern void PRCMHibernateEnter(void);
extern void PRCMHibernateWakeupSourceEnable(unsigned long ulHIBWakupSrc);
extern unsigned long PRCMHibernateWakeupCauseGet(void);
extern void PRCMHibernateWakeUpGPIOSelect(unsigned long ulMultiGPIOBitMap,
                                          unsigned long ulType);
extern void PRCMHibernateWakeupSourceDisable(unsigned long ulHIBWakupSrc);
extern void PRCMHibernateIntervalSet(unsigned long long ullTicks);

extern unsigned long long PRCMSlowClkCtrGet(void);
extern unsigned long long PRCMSlowClkCtrFastGet(void);
extern void PRCMSlowClkCtrMatchSet(unsigned long long ullTicks);
extern unsigned long long PRCMSlowClkCtrMatchGet(void);

extern void PRCMOCRRegisterWrite(unsigned char ucIndex,
                                 unsigned long ulRegValue);
extern unsigned long PRCMOCRRegisterRead(unsigned char ucIndex);

extern void PRCMIntRegister(void (*pfnHandler)(void));
extern void PRCMIntUnregister(void);
extern void PRCMIntEnable(unsigned long ulIntFlags);
extern void PRCMIntDisable(unsigned long ulIntFlags);
extern unsigned long PRCMIntStatus(void);
extern void PRCMRTCInUseSet(void);
extern void PRCMRTCInUseClear(void);
extern tBoolean PRCMRTCInUseGet(void);
extern void PRCMRTCSet(unsigned long ulSecs, unsigned short usMsec);
extern void PRCMRTCGet(unsigned long *ulSecs, unsigned short *usMsec);
extern void PRCMRTCMatchSet(unsigned long ulSecs, unsigned short usMsec);
extern void PRCMRTCMatchGet(unsigned long *ulSecs, unsigned short *usMsec);
extern void PRCMCC3200MCUInit(void);
extern unsigned long PRCMHIBRegRead(unsigned long ulRegAddr);
extern void PRCMHIBRegWrite(unsigned long ulRegAddr, unsigned long ulValue);
extern unsigned long PRCMCameraFreqSet(unsigned char ulDivider, unsigned char ulWidth);







#ifdef __cplusplus
}
#endif

#endif 
