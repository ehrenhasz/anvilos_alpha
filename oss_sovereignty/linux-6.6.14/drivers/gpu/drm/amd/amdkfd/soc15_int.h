 

#ifndef HSA_SOC15_INT_H_INCLUDED
#define HSA_SOC15_INT_H_INCLUDED

#include "soc15_ih_clientid.h"

#define SOC15_INTSRC_CP_END_OF_PIPE	181
#define SOC15_INTSRC_CP_BAD_OPCODE	183
#define SOC15_INTSRC_SQ_INTERRUPT_MSG	239
#define SOC15_INTSRC_VMC_FAULT		0
#define SOC15_INTSRC_SDMA_TRAP		224
#define SOC15_INTSRC_SDMA_ECC		220
#define SOC21_INTSRC_SDMA_TRAP		49
#define SOC21_INTSRC_SDMA_ECC		62

#define SOC15_CLIENT_ID_FROM_IH_ENTRY(entry) (le32_to_cpu(entry[0]) & 0xff)
#define SOC15_SOURCE_ID_FROM_IH_ENTRY(entry) (le32_to_cpu(entry[0]) >> 8 & 0xff)
#define SOC15_RING_ID_FROM_IH_ENTRY(entry) (le32_to_cpu(entry[0]) >> 16 & 0xff)
#define SOC15_VMID_FROM_IH_ENTRY(entry) (le32_to_cpu(entry[0]) >> 24 & 0xf)
#define SOC15_VMID_TYPE_FROM_IH_ENTRY(entry) (le32_to_cpu(entry[0]) >> 31 & 0x1)
#define SOC15_PASID_FROM_IH_ENTRY(entry) (le32_to_cpu(entry[3]) & 0xffff)
#define SOC15_NODEID_FROM_IH_ENTRY(entry) (le32_to_cpu(entry[3]) >> 16 & 0xff)
#define SOC15_CONTEXT_ID0_FROM_IH_ENTRY(entry) (le32_to_cpu(entry[4]))
#define SOC15_CONTEXT_ID1_FROM_IH_ENTRY(entry) (le32_to_cpu(entry[5]))
#define SOC15_CONTEXT_ID2_FROM_IH_ENTRY(entry) (le32_to_cpu(entry[6]))
#define SOC15_CONTEXT_ID3_FROM_IH_ENTRY(entry) (le32_to_cpu(entry[7]))

#endif

