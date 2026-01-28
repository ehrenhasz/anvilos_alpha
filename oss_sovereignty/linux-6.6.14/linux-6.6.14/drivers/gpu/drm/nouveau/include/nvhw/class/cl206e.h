#ifndef _cl206e_h_
#define _cl206e_h_
#define NV206E_DMA_OPCODE2                                         1:0
#define NV206E_DMA_OPCODE2_NONE                                    (0x00000000)
#define NV206E_DMA_OPCODE2_JUMP_LONG                               (0x00000001)
#define NV206E_DMA_JUMP_LONG_OFFSET                                31:2
#define NV206E_DMA_OPCODE2_CALL                                    (0x00000002)
#define NV206E_DMA_CALL_OFFSET                                     31:2
#endif  
