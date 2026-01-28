#ifndef _cl0039_h_
#define _cl0039_h_
#define NV039_SET_OBJECT                                           (0x00000000)
#define NV039_NO_OPERATION                                         (0x00000100)
#define NV039_SET_CONTEXT_DMA_NOTIFIES                             (0x00000180)
#define NV039_SET_CONTEXT_DMA_BUFFER_IN                            (0x00000184)
#define NV039_SET_CONTEXT_DMA_BUFFER_OUT                           (0x00000188)
#define NV039_OFFSET_IN                                            (0x0000030C)
#define NV039_OFFSET_OUT                                           (0x00000310)
#define NV039_PITCH_IN                                             (0x00000314)
#define NV039_PITCH_OUT                                            (0x00000318)
#define NV039_LINE_LENGTH_IN                                       (0x0000031C)
#define NV039_LINE_COUNT                                           (0x00000320)
#define NV039_FORMAT                                               (0x00000324)
#define NV039_FORMAT_IN                                            7:0
#define NV039_FORMAT_OUT                                           31:8
#define NV039_BUFFER_NOTIFY                                        (0x00000328)
#define NV039_BUFFER_NOTIFY_WRITE_ONLY                             (0x00000000)
#define NV039_BUFFER_NOTIFY_WRITE_THEN_AWAKEN                      (0x00000001)
#endif  
