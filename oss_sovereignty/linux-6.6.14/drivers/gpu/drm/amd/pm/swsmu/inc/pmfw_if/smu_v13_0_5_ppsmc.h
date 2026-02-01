 

#ifndef __SMU_V13_0_5_PPSMC_H__
#define __SMU_V13_0_5_PPSMC_H__


#define PPSMC_Result_OK                    0x1
#define PPSMC_Result_Failed                0xFF
#define PPSMC_Result_UnknownCmd            0xFE
#define PPSMC_Result_CmdRejectedPrereq     0xFD
#define PPSMC_Result_CmdRejectedBusy       0xFC



#define PPSMC_MSG_TestMessage               1
#define PPSMC_MSG_GetSmuVersion             2
#define PPSMC_MSG_EnableGfxOff              3  
#define PPSMC_MSG_DisableGfxOff             4  
#define PPSMC_MSG_PowerDownVcn              5  
#define PPSMC_MSG_PowerUpVcn                6  
#define PPSMC_MSG_SetHardMinVcn             7  
#define PPSMC_MSG_SetSoftMinGfxclk          8  
#define PPSMC_MSG_Spare0                    9  
#define PPSMC_MSG_GfxDeviceDriverReset      10 
#define PPSMC_MSG_SetDriverDramAddrHigh     11 
#define PPSMC_MSG_SetDriverDramAddrLow      12 
#define PPSMC_MSG_TransferTableSmu2Dram     13 
#define PPSMC_MSG_TransferTableDram2Smu     14 
#define PPSMC_MSG_GetGfxclkFrequency        15 
#define PPSMC_MSG_GetEnabledSmuFeatures     16 
#define PPSMC_MSG_SetSoftMaxVcn             17 
#define PPSMC_MSG_PowerDownJpeg             18 
#define PPSMC_MSG_PowerUpJpeg               19 
#define PPSMC_MSG_SetSoftMaxGfxClk          20
#define PPSMC_MSG_SetHardMinGfxClk          21 
#define PPSMC_MSG_AllowGfxOff               22 
#define PPSMC_MSG_DisallowGfxOff            23 
#define PPSMC_MSG_SetSoftMinVcn             24 
#define PPSMC_MSG_GetDriverIfVersion        25 
#define PPSMC_MSG_PrepareMp1ForUnload        26 
#define PPSMC_Message_Count                 27

 
typedef enum {
  MODE1_RESET = 1,  
  MODE2_RESET = 2   
} Mode_Reset_e;
 

#endif

