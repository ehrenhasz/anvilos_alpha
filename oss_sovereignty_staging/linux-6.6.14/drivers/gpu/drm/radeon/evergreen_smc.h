 
#ifndef __EVERGREEN_SMC_H__
#define __EVERGREEN_SMC_H__

#include "rv770_smc.h"

#pragma pack(push, 1)

#define SMC_EVERGREEN_MC_REGISTER_ARRAY_SIZE 16

struct SMC_Evergreen_MCRegisterAddress
{
    uint16_t s0;
    uint16_t s1;
};

typedef struct SMC_Evergreen_MCRegisterAddress SMC_Evergreen_MCRegisterAddress;


struct SMC_Evergreen_MCRegisterSet
{
    uint32_t value[SMC_EVERGREEN_MC_REGISTER_ARRAY_SIZE];
};

typedef struct SMC_Evergreen_MCRegisterSet SMC_Evergreen_MCRegisterSet;

struct SMC_Evergreen_MCRegisters
{
    uint8_t                             last;
    uint8_t                             reserved[3];
    SMC_Evergreen_MCRegisterAddress     address[SMC_EVERGREEN_MC_REGISTER_ARRAY_SIZE];
    SMC_Evergreen_MCRegisterSet         data[5];
};

typedef struct SMC_Evergreen_MCRegisters SMC_Evergreen_MCRegisters;

#define EVERGREEN_SMC_FIRMWARE_HEADER_LOCATION 0x100

#define EVERGREEN_SMC_FIRMWARE_HEADER_softRegisters   0x8
#define EVERGREEN_SMC_FIRMWARE_HEADER_stateTable      0xC
#define EVERGREEN_SMC_FIRMWARE_HEADER_mcRegisterTable 0x20


#pragma pack(pop)

#endif
