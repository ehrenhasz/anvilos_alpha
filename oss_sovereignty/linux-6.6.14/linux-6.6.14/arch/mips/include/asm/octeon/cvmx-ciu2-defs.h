#ifndef __CVMX_CIU2_DEFS_H__
#define __CVMX_CIU2_DEFS_H__
#define CVMX_CIU2_ACK_PPX_IP2(block_id) (CVMX_ADD_IO_SEG(0x00010701000C0000ull) + ((block_id) & 31) * 0x200000ull)
#define CVMX_CIU2_ACK_PPX_IP3(block_id) (CVMX_ADD_IO_SEG(0x00010701000C0200ull) + ((block_id) & 31) * 0x200000ull)
#define CVMX_CIU2_EN_PPX_IP2_RML(block_id) (CVMX_ADD_IO_SEG(0x0001070100092000ull) + ((block_id) & 31) * 0x200000ull)
#define CVMX_CIU2_EN_PPX_IP2_WDOG(block_id) (CVMX_ADD_IO_SEG(0x0001070100091000ull) + ((block_id) & 31) * 0x200000ull)
#define CVMX_CIU2_EN_PPX_IP2_WRKQ(block_id) (CVMX_ADD_IO_SEG(0x0001070100090000ull) + ((block_id) & 31) * 0x200000ull)
#define CVMX_CIU2_EN_PPX_IP2_WRKQ_W1C(block_id) (CVMX_ADD_IO_SEG(0x00010701000B0000ull) + ((block_id) & 31) * 0x200000ull)
#define CVMX_CIU2_EN_PPX_IP2_WRKQ_W1S(block_id) (CVMX_ADD_IO_SEG(0x00010701000A0000ull) + ((block_id) & 31) * 0x200000ull)
#define CVMX_CIU2_EN_PPX_IP3_MBOX_W1C(block_id) (CVMX_ADD_IO_SEG(0x00010701000B8200ull) + ((block_id) & 31) * 0x200000ull)
#define CVMX_CIU2_EN_PPX_IP3_MBOX_W1S(block_id) (CVMX_ADD_IO_SEG(0x00010701000A8200ull) + ((block_id) & 31) * 0x200000ull)
#define CVMX_CIU2_INTR_CIU_READY (CVMX_ADD_IO_SEG(0x0001070100102008ull))
#define CVMX_CIU2_RAW_PPX_IP2_WRKQ(block_id) (CVMX_ADD_IO_SEG(0x0001070100040000ull) + ((block_id) & 31) * 0x200000ull)
#define CVMX_CIU2_SRC_PPX_IP2_RML(block_id) (CVMX_ADD_IO_SEG(0x0001070100082000ull) + ((block_id) & 31) * 0x200000ull)
#define CVMX_CIU2_SRC_PPX_IP2_WDOG(block_id) (CVMX_ADD_IO_SEG(0x0001070100081000ull) + ((block_id) & 31) * 0x200000ull)
#define CVMX_CIU2_SRC_PPX_IP2_WRKQ(block_id) (CVMX_ADD_IO_SEG(0x0001070100080000ull) + ((block_id) & 31) * 0x200000ull)
#define CVMX_CIU2_SUM_PPX_IP2(offset) (CVMX_ADD_IO_SEG(0x0001070100000000ull) + ((offset) & 31) * 8)
#define CVMX_CIU2_SUM_PPX_IP3(offset) (CVMX_ADD_IO_SEG(0x0001070100000200ull) + ((offset) & 31) * 8)
#endif
