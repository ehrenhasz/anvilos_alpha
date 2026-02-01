 


#ifndef _cl837d_h_
#define _cl837d_h_


#define NV837D_SOR_SET_CONTROL(a)                                               (0x00000600 + (a)*0x00000040)
#define NV837D_SOR_SET_CONTROL_OWNER                                            3:0
#define NV837D_SOR_SET_CONTROL_OWNER_NONE                                       (0x00000000)
#define NV837D_SOR_SET_CONTROL_OWNER_HEAD0                                      (0x00000001)
#define NV837D_SOR_SET_CONTROL_OWNER_HEAD1                                      (0x00000002)
#define NV837D_SOR_SET_CONTROL_SUB_OWNER                                        5:4
#define NV837D_SOR_SET_CONTROL_SUB_OWNER_NONE                                   (0x00000000)
#define NV837D_SOR_SET_CONTROL_SUB_OWNER_SUBHEAD0                               (0x00000001)
#define NV837D_SOR_SET_CONTROL_SUB_OWNER_SUBHEAD1                               (0x00000002)
#define NV837D_SOR_SET_CONTROL_SUB_OWNER_BOTH                                   (0x00000003)
#define NV837D_SOR_SET_CONTROL_PROTOCOL                                         11:8
#define NV837D_SOR_SET_CONTROL_PROTOCOL_LVDS_CUSTOM                             (0x00000000)
#define NV837D_SOR_SET_CONTROL_PROTOCOL_SINGLE_TMDS_A                           (0x00000001)
#define NV837D_SOR_SET_CONTROL_PROTOCOL_SINGLE_TMDS_B                           (0x00000002)
#define NV837D_SOR_SET_CONTROL_PROTOCOL_SINGLE_TMDS_AB                          (0x00000003)
#define NV837D_SOR_SET_CONTROL_PROTOCOL_DUAL_SINGLE_TMDS                        (0x00000004)
#define NV837D_SOR_SET_CONTROL_PROTOCOL_DUAL_TMDS                               (0x00000005)
#define NV837D_SOR_SET_CONTROL_PROTOCOL_DDI_OUT                                 (0x00000007)
#define NV837D_SOR_SET_CONTROL_PROTOCOL_CUSTOM                                  (0x0000000F)
#define NV837D_SOR_SET_CONTROL_HSYNC_POLARITY                                   12:12
#define NV837D_SOR_SET_CONTROL_HSYNC_POLARITY_POSITIVE_TRUE                     (0x00000000)
#define NV837D_SOR_SET_CONTROL_HSYNC_POLARITY_NEGATIVE_TRUE                     (0x00000001)
#define NV837D_SOR_SET_CONTROL_VSYNC_POLARITY                                   13:13
#define NV837D_SOR_SET_CONTROL_VSYNC_POLARITY_POSITIVE_TRUE                     (0x00000000)
#define NV837D_SOR_SET_CONTROL_VSYNC_POLARITY_NEGATIVE_TRUE                     (0x00000001)
#define NV837D_SOR_SET_CONTROL_DE_SYNC_POLARITY                                 14:14
#define NV837D_SOR_SET_CONTROL_DE_SYNC_POLARITY_POSITIVE_TRUE                   (0x00000000)
#define NV837D_SOR_SET_CONTROL_DE_SYNC_POLARITY_NEGATIVE_TRUE                   (0x00000001)
#define NV837D_SOR_SET_CONTROL_PIXEL_DEPTH                                      19:16
#define NV837D_SOR_SET_CONTROL_PIXEL_DEPTH_DEFAULT                              (0x00000000)
#define NV837D_SOR_SET_CONTROL_PIXEL_DEPTH_BPP_16_422                           (0x00000001)
#define NV837D_SOR_SET_CONTROL_PIXEL_DEPTH_BPP_18_444                           (0x00000002)
#define NV837D_SOR_SET_CONTROL_PIXEL_DEPTH_BPP_20_422                           (0x00000003)
#define NV837D_SOR_SET_CONTROL_PIXEL_DEPTH_BPP_24_422                           (0x00000004)
#define NV837D_SOR_SET_CONTROL_PIXEL_DEPTH_BPP_24_444                           (0x00000005)
#define NV837D_SOR_SET_CONTROL_PIXEL_DEPTH_BPP_30_444                           (0x00000006)
#define NV837D_SOR_SET_CONTROL_PIXEL_DEPTH_BPP_32_422                           (0x00000007)
#define NV837D_SOR_SET_CONTROL_PIXEL_DEPTH_BPP_36_444                           (0x00000008)
#define NV837D_SOR_SET_CONTROL_PIXEL_DEPTH_BPP_48_444                           (0x00000009)

#define NV837D_PIOR_SET_CONTROL(a)                                              (0x00000700 + (a)*0x00000040)
#define NV837D_PIOR_SET_CONTROL_OWNER                                           3:0
#define NV837D_PIOR_SET_CONTROL_OWNER_NONE                                      (0x00000000)
#define NV837D_PIOR_SET_CONTROL_OWNER_HEAD0                                     (0x00000001)
#define NV837D_PIOR_SET_CONTROL_OWNER_HEAD1                                     (0x00000002)
#define NV837D_PIOR_SET_CONTROL_SUB_OWNER                                       5:4
#define NV837D_PIOR_SET_CONTROL_SUB_OWNER_NONE                                  (0x00000000)
#define NV837D_PIOR_SET_CONTROL_SUB_OWNER_SUBHEAD0                              (0x00000001)
#define NV837D_PIOR_SET_CONTROL_SUB_OWNER_SUBHEAD1                              (0x00000002)
#define NV837D_PIOR_SET_CONTROL_SUB_OWNER_BOTH                                  (0x00000003)
#define NV837D_PIOR_SET_CONTROL_PROTOCOL                                        11:8
#define NV837D_PIOR_SET_CONTROL_PROTOCOL_EXT_TMDS_ENC                           (0x00000000)
#define NV837D_PIOR_SET_CONTROL_PROTOCOL_EXT_TV_ENC                             (0x00000001)
#define NV837D_PIOR_SET_CONTROL_HSYNC_POLARITY                                  12:12
#define NV837D_PIOR_SET_CONTROL_HSYNC_POLARITY_POSITIVE_TRUE                    (0x00000000)
#define NV837D_PIOR_SET_CONTROL_HSYNC_POLARITY_NEGATIVE_TRUE                    (0x00000001)
#define NV837D_PIOR_SET_CONTROL_VSYNC_POLARITY                                  13:13
#define NV837D_PIOR_SET_CONTROL_VSYNC_POLARITY_POSITIVE_TRUE                    (0x00000000)
#define NV837D_PIOR_SET_CONTROL_VSYNC_POLARITY_NEGATIVE_TRUE                    (0x00000001)
#define NV837D_PIOR_SET_CONTROL_DE_SYNC_POLARITY                                14:14
#define NV837D_PIOR_SET_CONTROL_DE_SYNC_POLARITY_POSITIVE_TRUE                  (0x00000000)
#define NV837D_PIOR_SET_CONTROL_DE_SYNC_POLARITY_NEGATIVE_TRUE                  (0x00000001)
#define NV837D_PIOR_SET_CONTROL_PIXEL_DEPTH                                     19:16
#define NV837D_PIOR_SET_CONTROL_PIXEL_DEPTH_DEFAULT                             (0x00000000)
#define NV837D_PIOR_SET_CONTROL_PIXEL_DEPTH_BPP_16_422                          (0x00000001)
#define NV837D_PIOR_SET_CONTROL_PIXEL_DEPTH_BPP_18_444                          (0x00000002)
#define NV837D_PIOR_SET_CONTROL_PIXEL_DEPTH_BPP_20_422                          (0x00000003)
#define NV837D_PIOR_SET_CONTROL_PIXEL_DEPTH_BPP_24_422                          (0x00000004)
#define NV837D_PIOR_SET_CONTROL_PIXEL_DEPTH_BPP_24_444                          (0x00000005)
#define NV837D_PIOR_SET_CONTROL_PIXEL_DEPTH_BPP_30_444                          (0x00000006)
#define NV837D_PIOR_SET_CONTROL_PIXEL_DEPTH_BPP_32_422                          (0x00000007)
#define NV837D_PIOR_SET_CONTROL_PIXEL_DEPTH_BPP_36_444                          (0x00000008)
#define NV837D_PIOR_SET_CONTROL_PIXEL_DEPTH_BPP_48_444                          (0x00000009)
#endif 
