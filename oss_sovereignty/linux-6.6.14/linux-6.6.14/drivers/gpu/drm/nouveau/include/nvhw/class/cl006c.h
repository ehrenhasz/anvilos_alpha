#ifndef _cl006c_h_
#define _cl006c_h_
#define NV06C_PUT                                                  (0x00000040)
#define NV06C_PUT_PTR                                              31:2
#define NV06C_GET                                                  (0x00000044)
#define NV06C_GET_PTR                                              31:2
#define NV06C_METHOD_ADDRESS                                       12:2
#define NV06C_METHOD_SUBCHANNEL                                    15:13
#define NV06C_METHOD_COUNT                                         28:18
#define NV06C_OPCODE                                               31:29
#define NV06C_OPCODE_METHOD                                        (0x00000000)
#define NV06C_OPCODE_NONINC_METHOD                                 (0x00000002)
#define NV06C_DATA                                                 31:0
#define NV06C_OPCODE_JUMP                                          (0x00000001)
#define NV06C_JUMP_OFFSET                                          28:2
#endif  
