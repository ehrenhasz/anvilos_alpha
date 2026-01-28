#ifndef _clC37b_h_
#define _clC37b_h_
#define NVC37B_DMA
#define NVC37B_DMA_OPCODE                                                        31:29
#define NVC37B_DMA_OPCODE_METHOD                                            0x00000000
#define NVC37B_DMA_OPCODE_JUMP                                              0x00000001
#define NVC37B_DMA_OPCODE_NONINC_METHOD                                     0x00000002
#define NVC37B_DMA_OPCODE_SET_SUBDEVICE_MASK                                0x00000003
#define NVC37B_DMA_METHOD_COUNT                                                  27:18
#define NVC37B_DMA_METHOD_OFFSET                                                  13:2
#define NVC37B_DMA_DATA                                                           31:0
#define NVC37B_DMA_DATA_NOP                                                 0x00000000
#define NVC37B_DMA_JUMP_OFFSET                                                    11:2
#define NVC37B_DMA_SET_SUBDEVICE_MASK_VALUE                                       11:0
#define NVC37B_UPDATE                                                           (0x00000200)
#define NVC37B_UPDATE_INTERLOCK_WITH_WINDOW                                     1:1
#define NVC37B_UPDATE_INTERLOCK_WITH_WINDOW_DISABLE                             (0x00000000)
#define NVC37B_UPDATE_INTERLOCK_WITH_WINDOW_ENABLE                              (0x00000001)
#define NVC37B_SET_POINT_OUT(b)                                                 (0x00000208 + (b)*0x00000004)
#define NVC37B_SET_POINT_OUT_X                                                  15:0
#define NVC37B_SET_POINT_OUT_Y                                                  31:16
#endif  
