 

#ifndef SMU_13_0_1_PPSMC_H
#define SMU_13_0_1_PPSMC_H

 
#define PPS_PMFW_IF_VER "1.0" 

 
#define PPSMC_Result_OK                    0x1  
#define PPSMC_Result_Failed                0xFF 
#define PPSMC_Result_UnknownCmd            0xFE 
#define PPSMC_Result_CmdRejectedPrereq     0xFD 
#define PPSMC_Result_CmdRejectedBusy       0xFC 
 

 
#define PPSMC_MSG_TestMessage                   0x01 
#define PPSMC_MSG_GetSmuVersion                 0x02 
#define PPSMC_MSG_GetDriverIfVersion            0x03 
#define PPSMC_MSG_EnableGfxOff                  0x04 
#define PPSMC_MSG_DisableGfxOff                 0x05 
#define PPSMC_MSG_PowerDownVcn                  0x06 
#define PPSMC_MSG_PowerUpVcn                    0x07 
#define PPSMC_MSG_SetHardMinVcn                 0x08 
#define PPSMC_MSG_SetSoftMinGfxclk              0x09 
#define PPSMC_MSG_ActiveProcessNotify           0x0A 
#define PPSMC_MSG_ForcePowerDownGfx             0x0B 
#define PPSMC_MSG_PrepareMp1ForUnload           0x0C 
#define PPSMC_MSG_SetDriverDramAddrHigh         0x0D 
#define PPSMC_MSG_SetDriverDramAddrLow          0x0E 
#define PPSMC_MSG_TransferTableSmu2Dram         0x0F 
#define PPSMC_MSG_TransferTableDram2Smu         0x10 
#define PPSMC_MSG_GfxDeviceDriverReset          0x11 
#define PPSMC_MSG_GetEnabledSmuFeatures         0x12 
#define PPSMC_MSG_SetHardMinSocclkByFreq        0x13 
#define PPSMC_MSG_SetSoftMinFclk                0x14 
#define PPSMC_MSG_SetSoftMinVcn                 0x15 
#define PPSMC_MSG_SPARE                         0x16 
#define PPSMC_MSG_GetGfxclkFrequency            0x17 
#define PPSMC_MSG_GetFclkFrequency              0x18 
#define PPSMC_MSG_AllowGfxOff                   0x19 
#define PPSMC_MSG_DisallowGfxOff                0x1A 
#define PPSMC_MSG_SetSoftMaxGfxClk              0x1B 
#define PPSMC_MSG_SetHardMinGfxClk              0x1C 
#define PPSMC_MSG_SetSoftMaxSocclkByFreq        0x1D 
#define PPSMC_MSG_SetSoftMaxFclkByFreq          0x1E 
#define PPSMC_MSG_SetSoftMaxVcn                 0x1F 
#define PPSMC_MSG_SetPowerLimitPercentage       0x20 
#define PPSMC_MSG_PowerDownJpeg                 0x21 
#define PPSMC_MSG_PowerUpJpeg                   0x22 
#define PPSMC_MSG_SetHardMinFclkByFreq          0x23 
#define PPSMC_MSG_SetSoftMinSocclkByFreq        0x24 
#define PPSMC_MSG_AllowZstates                  0x25 
#define PPSMC_MSG_DisallowZstates               0x26 
#define PPSMC_MSG_RequestActiveWgp              0x27 
#define PPSMC_MSG_QueryActiveWgp                0x28 
#define PPSMC_Message_Count                     0x29 
 
 
  
typedef enum {
  MODE1_RESET = 1,  
  MODE2_RESET = 2   
} Mode_Reset_e;    
 

#endif
