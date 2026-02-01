 

#ifndef SMU_11_8_0_PPSMC_H
#define SMU_11_8_0_PPSMC_H


#define PPSMC_Result_OK                    0x1
#define PPSMC_Result_Failed                0xFF
#define PPSMC_Result_UnknownCmd            0xFE
#define PPSMC_Result_CmdRejectedPrereq     0xFD
#define PPSMC_Result_CmdRejectedBusy       0xFC


#define PPSMC_MSG_TestMessage                           0x1
#define PPSMC_MSG_GetSmuVersion                         0x2
#define PPSMC_MSG_GetDriverIfVersion                    0x3
#define PPSMC_MSG_SetDriverTableDramAddrHigh            0x4
#define PPSMC_MSG_SetDriverTableDramAddrLow             0x5
#define PPSMC_MSG_TransferTableSmu2Dram                 0x6
#define PPSMC_MSG_TransferTableDram2Smu                 0x7
#define PPSMC_MSG_Rsvd1                                 0xA
#define PPSMC_MSG_RequestCorePstate                     0xB
#define PPSMC_MSG_QueryCorePstate                       0xC
#define PPSMC_MSG_Rsvd2                                 0xD
#define PPSMC_MSG_RequestGfxclk                         0xE
#define PPSMC_MSG_QueryGfxclk                           0xF
#define PPSMC_MSG_QueryVddcrSocClock                    0x11
#define PPSMC_MSG_QueryDfPstate                         0x13
#define PPSMC_MSG_Rsvd3                                 0x14
#define PPSMC_MSG_ConfigureS3PwrOffRegisterAddressHigh  0x16
#define PPSMC_MSG_ConfigureS3PwrOffRegisterAddressLow   0x17
#define PPSMC_MSG_RequestActiveWgp                      0x18
#define PPSMC_MSG_SetMinDeepSleepGfxclkFreq             0x19
#define PPSMC_MSG_SetMaxDeepSleepDfllGfxDiv             0x1A
#define PPSMC_MSG_StartTelemetryReporting               0x1B
#define PPSMC_MSG_StopTelemetryReporting                0x1C
#define PPSMC_MSG_ClearTelemetryMax                     0x1D
#define PPSMC_MSG_QueryActiveWgp                        0x1E
#define PPSMC_MSG_SetCoreEnableMask                     0x2C
#define PPSMC_MSG_InitiateGcRsmuSoftReset               0x2E
#define PPSMC_MSG_GfxCacWeightOperation                 0x2F
#define PPSMC_MSG_L3CacWeightOperation                  0x30
#define PPSMC_MSG_PackCoreCacWeight                     0x31
#define PPSMC_MSG_SetDriverTableVMID                    0x34
#define PPSMC_MSG_SetSoftMinCclk                        0x35
#define PPSMC_MSG_SetSoftMaxCclk                        0x36
#define PPSMC_MSG_GetGfxFrequency                       0x37
#define PPSMC_MSG_GetGfxVid                             0x38
#define PPSMC_MSG_ForceGfxFreq                          0x39
#define PPSMC_MSG_UnForceGfxFreq                        0x3A
#define PPSMC_MSG_ForceGfxVid                           0x3B
#define PPSMC_MSG_UnforceGfxVid                         0x3C
#define PPSMC_MSG_GetEnabledSmuFeatures                 0x3D
#define PPSMC_Message_Count                             0x3E

#endif
