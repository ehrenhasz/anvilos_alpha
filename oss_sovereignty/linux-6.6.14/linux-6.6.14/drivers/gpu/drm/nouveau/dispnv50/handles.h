#ifndef __NV50_KMS_HANDLES_H__
#define __NV50_KMS_HANDLES_H__
#define NV50_DISP_HANDLE_SYNCBUF                                        0xf0000000
#define NV50_DISP_HANDLE_VRAM                                           0xf0000001
#define NV50_DISP_HANDLE_WNDW_CTX(kind)                        (0xfb000000 | kind)
#define NV50_DISP_HANDLE_CRC_CTX(head, i) (0xfc000000 | head->base.index << 1 | i)
#endif  
