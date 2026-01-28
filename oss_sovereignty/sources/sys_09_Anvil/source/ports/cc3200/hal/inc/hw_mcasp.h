


































#ifndef __HW_MCASP_H__
#define __HW_MCASP_H__






#define MCASP_O_PID             0x00000000
#define MCASP_O_ESYSCONFIG      0x00000004  
#define MCASP_O_PFUNC           0x00000010
#define MCASP_O_PDIR            0x00000014
#define MCASP_O_PDOUT           0x00000018
#define MCASP_O_PDSET           0x0000001C  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCASP_O_PDIN            0x0000001C  
                                            
                                            
                                            
                                            
                                            
#define MCASP_O_PDCLR           0x00000020  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define MCASP_O_TLGC            0x00000030  
#define MCASP_O_TLMR            0x00000034  
#define MCASP_O_TLEC            0x00000038  
#define MCASP_O_GBLCTL          0x00000044
#define MCASP_O_AMUTE           0x00000048
#define MCASP_O_LBCTL           0x0000004C
#define MCASP_O_TXDITCTL        0x00000050
#define MCASP_O_GBLCTLR         0x00000060
#define MCASP_O_RXMASK          0x00000064
#define MCASP_O_RXFMT           0x00000068
#define MCASP_O_RXFMCTL         0x0000006C
#define MCASP_O_ACLKRCTL        0x00000070
#define MCASP_O_AHCLKRCTL       0x00000074
#define MCASP_O_RXTDM           0x00000078
#define MCASP_O_EVTCTLR         0x0000007C
#define MCASP_O_RXSTAT          0x00000080
#define MCASP_O_RXTDMSLOT       0x00000084
#define MCASP_O_RXCLKCHK        0x00000088
#define MCASP_O_REVTCTL         0x0000008C
#define MCASP_O_GBLCTLX         0x000000A0
#define MCASP_O_TXMASK          0x000000A4
#define MCASP_O_TXFMT           0x000000A8
#define MCASP_O_TXFMCTL         0x000000AC
#define MCASP_O_ACLKXCTL        0x000000B0
#define MCASP_O_AHCLKXCTL       0x000000B4
#define MCASP_O_TXTDM           0x000000B8
#define MCASP_O_EVTCTLX         0x000000BC
#define MCASP_O_TXSTAT          0x000000C0
#define MCASP_O_TXTDMSLOT       0x000000C4
#define MCASP_O_TXCLKCHK        0x000000C8
#define MCASP_O_XEVTCTL         0x000000CC
#define MCASP_O_CLKADJEN        0x000000D0
#define MCASP_O_DITCSRA0        0x00000100
#define MCASP_O_DITCSRA1        0x00000104
#define MCASP_O_DITCSRA2        0x00000108
#define MCASP_O_DITCSRA3        0x0000010C
#define MCASP_O_DITCSRA4        0x00000110
#define MCASP_O_DITCSRA5        0x00000114
#define MCASP_O_DITCSRB0        0x00000118
#define MCASP_O_DITCSRB1        0x0000011C
#define MCASP_O_DITCSRB2        0x00000120
#define MCASP_O_DITCSRB3        0x00000124
#define MCASP_O_DITCSRB4        0x00000128
#define MCASP_O_DITCSRB5        0x0000012C
#define MCASP_O_DITUDRA0        0x00000130
#define MCASP_O_DITUDRA1        0x00000134
#define MCASP_O_DITUDRA2        0x00000138
#define MCASP_O_DITUDRA3        0x0000013C
#define MCASP_O_DITUDRA4        0x00000140
#define MCASP_O_DITUDRA5        0x00000144
#define MCASP_O_DITUDRB0        0x00000148
#define MCASP_O_DITUDRB1        0x0000014C
#define MCASP_O_DITUDRB2        0x00000150
#define MCASP_O_DITUDRB3        0x00000154
#define MCASP_O_DITUDRB4        0x00000158
#define MCASP_O_DITUDRB5        0x0000015C
#define MCASP_O_XRSRCTL0        0x00000180
#define MCASP_O_XRSRCTL1        0x00000184
#define MCASP_O_XRSRCTL2        0x00000188
#define MCASP_O_XRSRCTL3        0x0000018C
#define MCASP_O_XRSRCTL4        0x00000190
#define MCASP_O_XRSRCTL5        0x00000194
#define MCASP_O_XRSRCTL6        0x00000198
#define MCASP_O_XRSRCTL7        0x0000019C
#define MCASP_O_XRSRCTL8        0x000001A0
#define MCASP_O_XRSRCTL9        0x000001A4
#define MCASP_O_XRSRCTL10       0x000001A8
#define MCASP_O_XRSRCTL11       0x000001AC
#define MCASP_O_XRSRCTL12       0x000001B0
#define MCASP_O_XRSRCTL13       0x000001B4
#define MCASP_O_XRSRCTL14       0x000001B8
#define MCASP_O_XRSRCTL15       0x000001BC
#define MCASP_O_TXBUF0          0x00000200
#define MCASP_O_TXBUF1          0x00000204
#define MCASP_O_TXBUF2          0x00000208
#define MCASP_O_TXBUF3          0x0000020C
#define MCASP_O_TXBUF4          0x00000210
#define MCASP_O_TXBUF5          0x00000214
#define MCASP_O_TXBUF6          0x00000218
#define MCASP_O_TXBUF7          0x0000021C
#define MCASP_O_TXBUF8          0x00000220
#define MCASP_O_TXBUF9          0x00000224
#define MCASP_O_TXBUF10         0x00000228
#define MCASP_O_TXBUF11         0x0000022C
#define MCASP_O_TXBUF12         0x00000230
#define MCASP_O_TXBUF13         0x00000234
#define MCASP_O_TXBUF14         0x00000238
#define MCASP_O_TXBUF15         0x0000023C
#define MCASP_O_RXBUF0          0x00000280
#define MCASP_O_RXBUF1          0x00000284
#define MCASP_O_RXBUF2          0x00000288
#define MCASP_O_RXBUF3          0x0000028C
#define MCASP_O_RXBUF4          0x00000290
#define MCASP_O_RXBUF5          0x00000294
#define MCASP_O_RXBUF6          0x00000298
#define MCASP_O_RXBUF7          0x0000029C
#define MCASP_O_RXBUF8          0x000002A0
#define MCASP_O_RXBUF9          0x000002A4
#define MCASP_O_RXBUF10         0x000002A8
#define MCASP_O_RXBUF11         0x000002AC
#define MCASP_O_RXBUF12         0x000002B0
#define MCASP_O_RXBUF13         0x000002B4
#define MCASP_O_RXBUF14         0x000002B8
#define MCASP_O_RXBUF15         0x000002BC
#define	MCASP_0_WFIFOCTL	0x00001000
#define	MCASP_0_WFIFOSTS	0x00001004
#define	MCASP_0_RFIFOCTL	0x00001008
#define	MCASP_0_RFIFOSTS	0x0000100C







#define MCASP_PID_SCHEME_M      0xC0000000
#define MCASP_PID_SCHEME_S      30
#define MCASP_PID_RESV_M        0x30000000
#define MCASP_PID_RESV_S        28
#define MCASP_PID_FUNCTION_M    0x0FFF0000  
#define MCASP_PID_FUNCTION_S    16
#define MCASP_PID_RTL_M         0x0000F800
#define MCASP_PID_RTL_S         11
#define MCASP_PID_REVMAJOR_M    0x00000700
#define MCASP_PID_REVMAJOR_S    8
#define MCASP_PID_CUSTOM_M      0x000000C0  
#define MCASP_PID_CUSTOM_S      6
#define MCASP_PID_REVMINOR_M    0x0000003F
#define MCASP_PID_REVMINOR_S    0






#define MCASP_ESYSCONFIG_RSV_M  0xFFFFFFC0  
#define MCASP_ESYSCONFIG_RSV_S  6
#define MCASP_ESYSCONFIG_OTHER_M \
                                0x0000003C  

#define MCASP_ESYSCONFIG_OTHER_S 2
#define MCASP_ESYSCONFIG_IDLE_MODE_M \
                                0x00000003  

#define MCASP_ESYSCONFIG_IDLE_MODE_S 0





#define MCASP_PFUNC_AFSR        0x80000000  
#define MCASP_PFUNC_AHCLKR      0x40000000  
#define MCASP_PFUNC_ACLKR       0x20000000  
#define MCASP_PFUNC_AFSX        0x10000000  
#define MCASP_PFUNC_AHCLKX      0x08000000  
#define MCASP_PFUNC_ACLKX       0x04000000  
#define MCASP_PFUNC_AMUTE       0x02000000  
#define MCASP_PFUNC_RESV1_M     0x01FF0000  
#define MCASP_PFUNC_RESV1_S     16
#define MCASP_PFUNC_AXR15       0x00008000  
#define MCASP_PFUNC_AXR14       0x00004000  
#define MCASP_PFUNC_AXR13       0x00002000  
#define MCASP_PFUNC_AXR12       0x00001000  
#define MCASP_PFUNC_AXR11       0x00000800  
#define MCASP_PFUNC_AXR10       0x00000400  
#define MCASP_PFUNC_AXR9        0x00000200  
#define MCASP_PFUNC_AXR8        0x00000100  
#define MCASP_PFUNC_AXR7        0x00000080  
#define MCASP_PFUNC_AXR6        0x00000040  
#define MCASP_PFUNC_AXR5        0x00000020  
#define MCASP_PFUNC_AXR4        0x00000010  
#define MCASP_PFUNC_AXR3        0x00000008  
#define MCASP_PFUNC_AXR2        0x00000004  
#define MCASP_PFUNC_AXR1        0x00000002  
#define MCASP_PFUNC_AXR0        0x00000001  





#define MCASP_PDIR_AFSR         0x80000000  
#define MCASP_PDIR_AHCLKR       0x40000000  
#define MCASP_PDIR_ACLKR        0x20000000  
#define MCASP_PDIR_AFSX         0x10000000  
#define MCASP_PDIR_AHCLKX       0x08000000  
#define MCASP_PDIR_ACLKX        0x04000000  
#define MCASP_PDIR_AMUTE        0x02000000  
#define MCASP_PDIR_RESV_M       0x01FF0000  
#define MCASP_PDIR_RESV_S       16
#define MCASP_PDIR_AXR15        0x00008000  
#define MCASP_PDIR_AXR14        0x00004000  
#define MCASP_PDIR_AXR13        0x00002000  
#define MCASP_PDIR_AXR12        0x00001000  
#define MCASP_PDIR_AXR11        0x00000800  
#define MCASP_PDIR_AXR10        0x00000400  
#define MCASP_PDIR_AXR9         0x00000200  
#define MCASP_PDIR_AXR8         0x00000100  
#define MCASP_PDIR_AXR7         0x00000080  
#define MCASP_PDIR_AXR6         0x00000040  
#define MCASP_PDIR_AXR5         0x00000020  
#define MCASP_PDIR_AXR4         0x00000010  
#define MCASP_PDIR_AXR3         0x00000008  
#define MCASP_PDIR_AXR2         0x00000004  
#define MCASP_PDIR_AXR1         0x00000002  
#define MCASP_PDIR_AXR0         0x00000001  





#define MCASP_PDOUT_AFSR        0x80000000  
#define MCASP_PDOUT_AHCLKR      0x40000000  
#define MCASP_PDOUT_ACLKR       0x20000000  
#define MCASP_PDOUT_AFSX        0x10000000  
#define MCASP_PDOUT_AHCLKX      0x08000000  
#define MCASP_PDOUT_ACLKX       0x04000000  
#define MCASP_PDOUT_AMUTE       0x02000000  
#define MCASP_PDOUT_RESV_M      0x01FF0000  
#define MCASP_PDOUT_RESV_S      16
#define MCASP_PDOUT_AXR15       0x00008000  
#define MCASP_PDOUT_AXR14       0x00004000  
#define MCASP_PDOUT_AXR13       0x00002000  
#define MCASP_PDOUT_AXR12       0x00001000  
#define MCASP_PDOUT_AXR11       0x00000800  
#define MCASP_PDOUT_AXR10       0x00000400  
#define MCASP_PDOUT_AXR9        0x00000200  
#define MCASP_PDOUT_AXR8        0x00000100  
#define MCASP_PDOUT_AXR7        0x00000080  
#define MCASP_PDOUT_AXR6        0x00000040  
#define MCASP_PDOUT_AXR5        0x00000020  
#define MCASP_PDOUT_AXR4        0x00000010  
#define MCASP_PDOUT_AXR3        0x00000008  
#define MCASP_PDOUT_AXR2        0x00000004  
#define MCASP_PDOUT_AXR1        0x00000002  
#define MCASP_PDOUT_AXR0        0x00000001  





#define MCASP_PDSET_AFSR        0x80000000
#define MCASP_PDSET_AHCLKR      0x40000000
#define MCASP_PDSET_ACLKR       0x20000000
#define MCASP_PDSET_AFSX        0x10000000
#define MCASP_PDSET_AHCLKX      0x08000000
#define MCASP_PDSET_ACLKX       0x04000000
#define MCASP_PDSET_AMUTE       0x02000000
#define MCASP_PDSET_RESV_M      0x01FF0000  
#define MCASP_PDSET_RESV_S      16
#define MCASP_PDSET_AXR15       0x00008000
#define MCASP_PDSET_AXR14       0x00004000
#define MCASP_PDSET_AXR13       0x00002000
#define MCASP_PDSET_AXR12       0x00001000
#define MCASP_PDSET_AXR11       0x00000800
#define MCASP_PDSET_AXR10       0x00000400
#define MCASP_PDSET_AXR9        0x00000200
#define MCASP_PDSET_AXR8        0x00000100
#define MCASP_PDSET_AXR7        0x00000080
#define MCASP_PDSET_AXR6        0x00000040
#define MCASP_PDSET_AXR5        0x00000020
#define MCASP_PDSET_AXR4        0x00000010
#define MCASP_PDSET_AXR3        0x00000008
#define MCASP_PDSET_AXR2        0x00000004
#define MCASP_PDSET_AXR1        0x00000002
#define MCASP_PDSET_AXR0        0x00000001





#define MCASP_PDIN_AFSR         0x80000000
#define MCASP_PDIN_AHCLKR       0x40000000
#define MCASP_PDIN_ACLKR        0x20000000
#define MCASP_PDIN_AFSX         0x10000000
#define MCASP_PDIN_AHCLKX       0x08000000
#define MCASP_PDIN_ACLKX        0x04000000
#define MCASP_PDIN_AMUTE        0x02000000
#define MCASP_PDIN_RESV_M       0x01FF0000  
#define MCASP_PDIN_RESV_S       16
#define MCASP_PDIN_AXR15        0x00008000
#define MCASP_PDIN_AXR14        0x00004000
#define MCASP_PDIN_AXR13        0x00002000
#define MCASP_PDIN_AXR12        0x00001000
#define MCASP_PDIN_AXR11        0x00000800
#define MCASP_PDIN_AXR10        0x00000400
#define MCASP_PDIN_AXR9         0x00000200
#define MCASP_PDIN_AXR8         0x00000100
#define MCASP_PDIN_AXR7         0x00000080
#define MCASP_PDIN_AXR6         0x00000040
#define MCASP_PDIN_AXR5         0x00000020
#define MCASP_PDIN_AXR4         0x00000010
#define MCASP_PDIN_AXR3         0x00000008
#define MCASP_PDIN_AXR2         0x00000004
#define MCASP_PDIN_AXR1         0x00000002
#define MCASP_PDIN_AXR0         0x00000001





#define MCASP_PDCLR_AFSR        0x80000000  
#define MCASP_PDCLR_AHCLKR      0x40000000  
#define MCASP_PDCLR_ACLKR       0x20000000  
#define MCASP_PDCLR_AFSX        0x10000000  
#define MCASP_PDCLR_AHCLKX      0x08000000  
#define MCASP_PDCLR_ACLKX       0x04000000  
#define MCASP_PDCLR_AMUTE       0x02000000  
#define MCASP_PDCLR_RESV_M      0x01FF0000  
#define MCASP_PDCLR_RESV_S      16
#define MCASP_PDCLR_AXR15       0x00008000  
#define MCASP_PDCLR_AXR14       0x00004000  
#define MCASP_PDCLR_AXR13       0x00002000  
#define MCASP_PDCLR_AXR12       0x00001000  
#define MCASP_PDCLR_AXR11       0x00000800  
#define MCASP_PDCLR_AXR10       0x00000400  
#define MCASP_PDCLR_AXR9        0x00000200  
#define MCASP_PDCLR_AXR8        0x00000100  
#define MCASP_PDCLR_AXR7        0x00000080  
#define MCASP_PDCLR_AXR6        0x00000040  
#define MCASP_PDCLR_AXR5        0x00000020  
#define MCASP_PDCLR_AXR4        0x00000010  
#define MCASP_PDCLR_AXR3        0x00000008  
#define MCASP_PDCLR_AXR2        0x00000004  
#define MCASP_PDCLR_AXR1        0x00000002  
#define MCASP_PDCLR_AXR0        0x00000001  





#define MCASP_TLGC_RESV_M       0xFFFF0000  
#define MCASP_TLGC_RESV_S       16
#define MCASP_TLGC_MT_M         0x0000C000  
                                            
#define MCASP_TLGC_MT_S         14
#define MCASP_TLGC_RESV1_M      0x00003E00  
#define MCASP_TLGC_RESV1_S      9
#define MCASP_TLGC_MMS          0x00000100  
#define MCASP_TLGC_ESEL         0x00000080  
#define MCASP_TLGC_TOEN         0x00000040  
#define MCASP_TLGC_MC_M         0x00000030  
#define MCASP_TLGC_MC_S         4
#define MCASP_TLGC_PC_M         0x0000000E  
                                            
#define MCASP_TLGC_PC_S         1
#define MCASP_TLGC_TM           0x00000001  
                                            





#define MCASP_TLMR_TLMR_M       0xFFFFFFFF  
#define MCASP_TLMR_TLMR_S       0





#define MCASP_TLEC_TLEC_M       0xFFFFFFFF  
                                            
                                            
#define MCASP_TLEC_TLEC_S       0





#define MCASP_GBLCTL_XFRST      0x00001000  
#define MCASP_GBLCTL_XSMRST     0x00000800  
#define MCASP_GBLCTL_XSRCLR     0x00000400  
#define MCASP_GBLCTL_XHCLKRST   0x00000200  
#define MCASP_GBLCTL_XCLKRST    0x00000100  
#define MCASP_GBLCTL_RFRST      0x00000010  
#define MCASP_GBLCTL_RSMRST     0x00000008  
#define MCASP_GBLCTL_RSRCLR     0x00000004  
#define MCASP_GBLCTL_RHCLKRST   0x00000002  
#define MCASP_GBLCTL_RCLKRST    0x00000001  





#define MCASP_AMUTE_XDMAERR     0x00001000  
#define MCASP_AMUTE_RDMAERR     0x00000800  
#define MCASP_AMUTE_XCKFAIL     0x00000400  
#define MCASP_AMUTE_RCKFAIL     0x00000200  
#define MCASP_AMUTE_XSYNCERR    0x00000100  
#define MCASP_AMUTE_RSYNCERR    0x00000080  
#define MCASP_AMUTE_XUNDRN      0x00000040  
#define MCASP_AMUTE_ROVRN       0x00000020  
#define MCASP_AMUTE_INSTAT      0x00000010
#define MCASP_AMUTE_INEN        0x00000008  
                                            
#define MCASP_AMUTE_INPOL       0x00000004  
#define MCASP_AMUTE_MUTEN_M     0x00000003  
#define MCASP_AMUTE_MUTEN_S     0





#define MCASP_LBCTL_IOLBEN      0x00000010  
#define MCASP_LBCTL_MODE_M      0x0000000C  
                                            
#define MCASP_LBCTL_MODE_S      2
#define MCASP_LBCTL_ORD         0x00000002  
#define MCASP_LBCTL_DLBEN       0x00000001  





#define MCASP_TXDITCTL_VB       0x00000008  
#define MCASP_TXDITCTL_VA       0x00000004  
#define MCASP_TXDITCTL_DITEN    0x00000001  





#define MCASP_GBLCTLR_XFRST     0x00001000
#define MCASP_GBLCTLR_XSMRST    0x00000800
#define MCASP_GBLCTLR_XSRCLR    0x00000400
#define MCASP_GBLCTLR_XHCLKRST  0x00000200
#define MCASP_GBLCTLR_XCLKRST   0x00000100
#define MCASP_GBLCTLR_RFRST     0x00000010  
#define MCASP_GBLCTLR_RSMRST    0x00000008  
#define MCASP_GBLCTLR_RSRCLR    0x00000004  
#define MCASP_GBLCTLR_RHCLKRST  0x00000002  
#define MCASP_GBLCTLR_RCLKRST   0x00000001  





#define MCASP_RXMASK_RMASK31    0x80000000  
#define MCASP_RXMASK_RMASK30    0x40000000  
#define MCASP_RXMASK_RMASK29    0x20000000  
#define MCASP_RXMASK_RMASK28    0x10000000  
#define MCASP_RXMASK_RMASK27    0x08000000  
#define MCASP_RXMASK_RMASK26    0x04000000  
#define MCASP_RXMASK_RMASK25    0x02000000  
#define MCASP_RXMASK_RMASK24    0x01000000  
#define MCASP_RXMASK_RMASK23    0x00800000  
#define MCASP_RXMASK_RMASK22    0x00400000  
#define MCASP_RXMASK_RMASK21    0x00200000  
#define MCASP_RXMASK_RMASK20    0x00100000  
#define MCASP_RXMASK_RMASK19    0x00080000  
#define MCASP_RXMASK_RMASK18    0x00040000  
#define MCASP_RXMASK_RMASK17    0x00020000  
#define MCASP_RXMASK_RMASK16    0x00010000  
#define MCASP_RXMASK_RMASK15    0x00008000  
#define MCASP_RXMASK_RMASK14    0x00004000  
#define MCASP_RXMASK_RMASK13    0x00002000  
#define MCASP_RXMASK_RMASK12    0x00001000  
#define MCASP_RXMASK_RMASK11    0x00000800  
#define MCASP_RXMASK_RMASK10    0x00000400  
#define MCASP_RXMASK_RMASK9     0x00000200  
#define MCASP_RXMASK_RMASK8     0x00000100  
#define MCASP_RXMASK_RMASK7     0x00000080  
#define MCASP_RXMASK_RMASK6     0x00000040  
#define MCASP_RXMASK_RMASK5     0x00000020  
#define MCASP_RXMASK_RMASK4     0x00000010  
#define MCASP_RXMASK_RMASK3     0x00000008  
#define MCASP_RXMASK_RMASK2     0x00000004  
#define MCASP_RXMASK_RMASK1     0x00000002  
#define MCASP_RXMASK_RMASK0     0x00000001  





#define MCASP_RXFMT_RDATDLY_M   0x00030000  
                                            
                                            
#define MCASP_RXFMT_RDATDLY_S   16
#define MCASP_RXFMT_RRVRS       0x00008000  
#define MCASP_RXFMT_RPAD_M      0x00006000  
#define MCASP_RXFMT_RPAD_S      13
#define MCASP_RXFMT_RPBIT_M     0x00001F00  
#define MCASP_RXFMT_RPBIT_S     8
#define MCASP_RXFMT_RSSZ_M      0x000000F0  
                                            
                                            
#define MCASP_RXFMT_RSSZ_S      4
#define MCASP_RXFMT_RBUSEL      0x00000008  
                                            
#define MCASP_RXFMT_RROT_M      0x00000007  
                                            
#define MCASP_RXFMT_RROT_S      0





#define MCASP_RXFMCTL_RMOD_M    0x0000FF80  
#define MCASP_RXFMCTL_RMOD_S    7
#define MCASP_RXFMCTL_FRWID     0x00000010  
#define MCASP_RXFMCTL_FSRM      0x00000002  
#define MCASP_RXFMCTL_FSRP      0x00000001  





#define MCASP_ACLKRCTL_BUSY     0x00100000
#define MCASP_ACLKRCTL_DIVBUSY  0x00080000
#define MCASP_ACLKRCTL_ADJBUSY  0x00040000
#define MCASP_ACLKRCTL_CLKRADJ_M \
                                0x00030000

#define MCASP_ACLKRCTL_CLKRADJ_S 16
#define MCASP_ACLKRCTL_CLKRP    0x00000080  
#define MCASP_ACLKRCTL_CLKRM    0x00000020  
#define MCASP_ACLKRCTL_CLKRDIV_M \
                                0x0000001F  

#define MCASP_ACLKRCTL_CLKRDIV_S 0





#define MCASP_AHCLKRCTL_BUSY    0x00100000
#define MCASP_AHCLKRCTL_DIVBUSY 0x00080000
#define MCASP_AHCLKRCTL_ADJBUSY 0x00040000
#define MCASP_AHCLKRCTL_HCLKRADJ_M \
                                0x00030000

#define MCASP_AHCLKRCTL_HCLKRADJ_S 16
#define MCASP_AHCLKRCTL_HCLKRM  0x00008000  
#define MCASP_AHCLKRCTL_HCLKRP  0x00004000  
                                            
#define MCASP_AHCLKRCTL_HCLKRDIV_M \
                                0x00000FFF  

#define MCASP_AHCLKRCTL_HCLKRDIV_S 0





#define MCASP_RXTDM_RTDMS31     0x80000000  
                                            
#define MCASP_RXTDM_RTDMS30     0x40000000  
                                            
#define MCASP_RXTDM_RTDMS29     0x20000000  
                                            
#define MCASP_RXTDM_RTDMS28     0x10000000  
                                            
#define MCASP_RXTDM_RTDMS27     0x08000000  
                                            
#define MCASP_RXTDM_RTDMS26     0x04000000  
                                            
#define MCASP_RXTDM_RTDMS25     0x02000000  
                                            
#define MCASP_RXTDM_RTDMS24     0x01000000  
                                            
#define MCASP_RXTDM_RTDMS23     0x00800000  
                                            
#define MCASP_RXTDM_RTDMS22     0x00400000  
                                            
#define MCASP_RXTDM_RTDMS21     0x00200000  
                                            
#define MCASP_RXTDM_RTDMS20     0x00100000  
                                            
#define MCASP_RXTDM_RTDMS19     0x00080000  
                                            
#define MCASP_RXTDM_RTDMS18     0x00040000  
                                            
#define MCASP_RXTDM_RTDMS17     0x00020000  
                                            
#define MCASP_RXTDM_RTDMS16     0x00010000  
                                            
#define MCASP_RXTDM_RTDMS15     0x00008000  
                                            
#define MCASP_RXTDM_RTDMS14     0x00004000  
                                            
#define MCASP_RXTDM_RTDMS13     0x00002000  
                                            
#define MCASP_RXTDM_RTDMS12     0x00001000  
                                            
#define MCASP_RXTDM_RTDMS11     0x00000800  
                                            
#define MCASP_RXTDM_RTDMS10     0x00000400  
                                            
#define MCASP_RXTDM_RTDMS9      0x00000200  
                                            
#define MCASP_RXTDM_RTDMS8      0x00000100  
                                            
#define MCASP_RXTDM_RTDMS7      0x00000080  
                                            
#define MCASP_RXTDM_RTDMS6      0x00000040  
                                            
#define MCASP_RXTDM_RTDMS5      0x00000020  
                                            
#define MCASP_RXTDM_RTDMS4      0x00000010  
                                            
#define MCASP_RXTDM_RTDMS3      0x00000008  
                                            
#define MCASP_RXTDM_RTDMS2      0x00000004  
                                            
#define MCASP_RXTDM_RTDMS1      0x00000002  
                                            
#define MCASP_RXTDM_RTDMS0      0x00000001  
                                            





#define MCASP_EVTCTLR_RSTAFRM   0x00000080  
#define MCASP_EVTCTLR_RDATA     0x00000020  
#define MCASP_EVTCTLR_RLAST     0x00000010  
#define MCASP_EVTCTLR_RDMAERR   0x00000008  
#define MCASP_EVTCTLR_RCKFAIL   0x00000004  
#define MCASP_EVTCTLR_RSYNCERR  0x00000002  
#define MCASP_EVTCTLR_ROVRN     0x00000001  





#define MCASP_RXSTAT_RERR       0x00000100  
#define MCASP_RXSTAT_RDMAERR    0x00000080  
#define MCASP_RXSTAT_RSTAFRM    0x00000040  
#define MCASP_RXSTAT_RDATA      0x00000020  
#define MCASP_RXSTAT_RLAST      0x00000010  
#define MCASP_RXSTAT_RTDMSLOT   0x00000008  
#define MCASP_RXSTAT_RCKFAIL    0x00000004  
#define MCASP_RXSTAT_RSYNCERR   0x00000002  
                                            
#define MCASP_RXSTAT_ROVRN      0x00000001  





#define MCASP_RXTDMSLOT_RSLOTCNT_M \
                                0x000003FF  

#define MCASP_RXTDMSLOT_RSLOTCNT_S 0





#define MCASP_RXCLKCHK_RCNT_M   0xFF000000  
#define MCASP_RXCLKCHK_RCNT_S   24
#define MCASP_RXCLKCHK_RMAX_M   0x00FF0000  
#define MCASP_RXCLKCHK_RMAX_S   16
#define MCASP_RXCLKCHK_RMIN_M   0x0000FF00  
#define MCASP_RXCLKCHK_RMIN_S   8
#define MCASP_RXCLKCHK_RPS_M    0x0000000F  
                                            
#define MCASP_RXCLKCHK_RPS_S    0





#define MCASP_REVTCTL_RDATDMA   0x00000001  
                                            
                                            





#define MCASP_GBLCTLX_XFRST     0x00001000  
#define MCASP_GBLCTLX_XSMRST    0x00000800  
#define MCASP_GBLCTLX_XSRCLR    0x00000400  
#define MCASP_GBLCTLX_XHCLKRST  0x00000200  
#define MCASP_GBLCTLX_XCLKRST   0x00000100  
#define MCASP_GBLCTLX_RFRST     0x00000010
#define MCASP_GBLCTLX_RSMRST    0x00000008
#define MCASP_GBLCTLX_RSRCLKR   0x00000004
#define MCASP_GBLCTLX_RHCLKRST  0x00000002
#define MCASP_GBLCTLX_RCLKRST   0x00000001





#define MCASP_TXMASK_XMASK31    0x80000000  
#define MCASP_TXMASK_XMASK30    0x40000000  
#define MCASP_TXMASK_XMASK29    0x20000000  
#define MCASP_TXMASK_XMASK28    0x10000000  
#define MCASP_TXMASK_XMASK27    0x08000000  
#define MCASP_TXMASK_XMASK26    0x04000000  
#define MCASP_TXMASK_XMASK25    0x02000000  
#define MCASP_TXMASK_XMASK24    0x01000000  
#define MCASP_TXMASK_XMASK23    0x00800000  
#define MCASP_TXMASK_XMASK22    0x00400000  
#define MCASP_TXMASK_XMASK21    0x00200000  
#define MCASP_TXMASK_XMASK20    0x00100000  
#define MCASP_TXMASK_XMASK19    0x00080000  
#define MCASP_TXMASK_XMASK18    0x00040000  
#define MCASP_TXMASK_XMASK17    0x00020000  
#define MCASP_TXMASK_XMASK16    0x00010000  
#define MCASP_TXMASK_XMASK15    0x00008000  
#define MCASP_TXMASK_XMASK14    0x00004000  
#define MCASP_TXMASK_XMASK13    0x00002000  
#define MCASP_TXMASK_XMASK12    0x00001000  
#define MCASP_TXMASK_XMASK11    0x00000800  
#define MCASP_TXMASK_XMASK10    0x00000400  
#define MCASP_TXMASK_XMASK9     0x00000200  
#define MCASP_TXMASK_XMASK8     0x00000100  
#define MCASP_TXMASK_XMASK7     0x00000080  
#define MCASP_TXMASK_XMASK6     0x00000040  
#define MCASP_TXMASK_XMASK5     0x00000020  
#define MCASP_TXMASK_XMASK4     0x00000010  
#define MCASP_TXMASK_XMASK3     0x00000008  
#define MCASP_TXMASK_XMASK2     0x00000004  
#define MCASP_TXMASK_XMASK1     0x00000002  
#define MCASP_TXMASK_XMASK0     0x00000001  





#define MCASP_TXFMT_XDATDLY_M   0x00030000  
                                            
                                            
#define MCASP_TXFMT_XDATDLY_S   16
#define MCASP_TXFMT_XRVRS       0x00008000  
#define MCASP_TXFMT_XPAD_M      0x00006000  
#define MCASP_TXFMT_XPAD_S      13
#define MCASP_TXFMT_XPBIT_M     0x00001F00  
#define MCASP_TXFMT_XPBIT_S     8
#define MCASP_TXFMT_XSSZ_M      0x000000F0  
                                            
                                            
#define MCASP_TXFMT_XSSZ_S      4
#define MCASP_TXFMT_XBUSEL      0x00000008  
                                            
#define MCASP_TXFMT_XROT_M      0x00000007  
                                            
#define MCASP_TXFMT_XROT_S      0





#define MCASP_TXFMCTL_XMOD_M    0x0000FF80  
#define MCASP_TXFMCTL_XMOD_S    7
#define MCASP_TXFMCTL_FXWID     0x00000010  
#define MCASP_TXFMCTL_FSXM      0x00000002  
#define MCASP_TXFMCTL_FSXP      0x00000001  





#define MCASP_ACLKXCTL_BUSY     0x00100000
#define MCASP_ACLKXCTL_DIVBUSY  0x00080000
#define MCASP_ACLKXCTL_ADJBUSY  0x00040000
#define MCASP_ACLKXCTL_CLKXADJ_M \
                                0x00030000

#define MCASP_ACLKXCTL_CLKXADJ_S 16
#define MCASP_ACLKXCTL_CLKXP    0x00000080  
#define MCASP_ACLKXCTL_ASYNC    0x00000040  
                                            
#define MCASP_ACLKXCTL_CLKXM    0x00000020  
#define MCASP_ACLKXCTL_CLKXDIV_M \
                                0x0000001F  

#define MCASP_ACLKXCTL_CLKXDIV_S 0





#define MCASP_AHCLKXCTL_BUSY    0x00100000
#define MCASP_AHCLKXCTL_DIVBUSY 0x00080000
#define MCASP_AHCLKXCTL_ADJBUSY 0x00040000
#define MCASP_AHCLKXCTL_HCLKXADJ_M \
                                0x00030000

#define MCASP_AHCLKXCTL_HCLKXADJ_S 16
#define MCASP_AHCLKXCTL_HCLKXM  0x00008000  
#define MCASP_AHCLKXCTL_HCLKXP  0x00004000  
                                            
#define MCASP_AHCLKXCTL_HCLKXDIV_M \
                                0x00000FFF  

#define MCASP_AHCLKXCTL_HCLKXDIV_S 0





#define MCASP_TXTDM_XTDMS31     0x80000000  
                                            
#define MCASP_TXTDM_XTDMS30     0x40000000  
                                            
#define MCASP_TXTDM_XTDMS29     0x20000000  
                                            
#define MCASP_TXTDM_XTDMS28     0x10000000  
                                            
#define MCASP_TXTDM_XTDMS27     0x08000000  
                                            
#define MCASP_TXTDM_XTDMS26     0x04000000  
                                            
#define MCASP_TXTDM_XTDMS25     0x02000000  
                                            
#define MCASP_TXTDM_XTDMS24     0x01000000  
                                            
#define MCASP_TXTDM_XTDMS23     0x00800000  
                                            
#define MCASP_TXTDM_XTDMS22     0x00400000  
                                            
#define MCASP_TXTDM_XTDMS21     0x00200000  
                                            
#define MCASP_TXTDM_XTDMS20     0x00100000  
                                            
#define MCASP_TXTDM_XTDMS19     0x00080000  
                                            
#define MCASP_TXTDM_XTDMS18     0x00040000  
                                            
#define MCASP_TXTDM_XTDMS17     0x00020000  
                                            
#define MCASP_TXTDM_XTDMS16     0x00010000  
                                            
#define MCASP_TXTDM_XTDMS15     0x00008000  
                                            
#define MCASP_TXTDM_XTDMS14     0x00004000  
                                            
#define MCASP_TXTDM_XTDMS13     0x00002000  
                                            
#define MCASP_TXTDM_XTDMS12     0x00001000  
                                            
#define MCASP_TXTDM_XTDMS11     0x00000800  
                                            
#define MCASP_TXTDM_XTDMS10     0x00000400  
                                            
#define MCASP_TXTDM_XTDMS9      0x00000200  
                                            
#define MCASP_TXTDM_XTDMS8      0x00000100  
                                            
#define MCASP_TXTDM_XTDMS7      0x00000080  
                                            
#define MCASP_TXTDM_XTDMS6      0x00000040  
                                            
#define MCASP_TXTDM_XTDMS5      0x00000020  
                                            
#define MCASP_TXTDM_XTDMS4      0x00000010  
                                            
#define MCASP_TXTDM_XTDMS3      0x00000008  
                                            
#define MCASP_TXTDM_XTDMS2      0x00000004  
                                            
#define MCASP_TXTDM_XTDMS1      0x00000002  
                                            
#define MCASP_TXTDM_XTDMS0      0x00000001  
                                            





#define MCASP_EVTCTLX_XSTAFRM   0x00000080  
#define MCASP_EVTCTLX_XDATA     0x00000020  
#define MCASP_EVTCTLX_XLAST     0x00000010  
#define MCASP_EVTCTLX_XDMAERR   0x00000008  
#define MCASP_EVTCTLX_XCKFAIL   0x00000004  
#define MCASP_EVTCTLX_XSYNCERR  0x00000002  
#define MCASP_EVTCTLX_XUNDRN    0x00000001  





#define MCASP_TXSTAT_XERR       0x00000100  
#define MCASP_TXSTAT_XDMAERR    0x00000080  
#define MCASP_TXSTAT_XSTAFRM    0x00000040  
#define MCASP_TXSTAT_XDATA      0x00000020  
#define MCASP_TXSTAT_XLAST      0x00000010  
#define MCASP_TXSTAT_XTDMSLOT   0x00000008  
#define MCASP_TXSTAT_XCKFAIL    0x00000004  
#define MCASP_TXSTAT_XSYNCERR   0x00000002  
                                            
#define MCASP_TXSTAT_XUNDRN     0x00000001  





#define MCASP_TXTDMSLOT_XSLOTCNT_M \
                                0x000003FF  
                                            
                                            
                                            

#define MCASP_TXTDMSLOT_XSLOTCNT_S 0





#define MCASP_TXCLKCHK_XCNT_M   0xFF000000  
#define MCASP_TXCLKCHK_XCNT_S   24
#define MCASP_TXCLKCHK_XMAX_M   0x00FF0000  
#define MCASP_TXCLKCHK_XMAX_S   16
#define MCASP_TXCLKCHK_XMIN_M   0x0000FF00  
#define MCASP_TXCLKCHK_XMIN_S   8
#define MCASP_TXCLKCHK_RESV     0x00000080  
#define MCASP_TXCLKCHK_XPS_M    0x0000000F  
                                            
#define MCASP_TXCLKCHK_XPS_S    0





#define MCASP_XEVTCTL_XDATDMA   0x00000001  
                                            
                                            





#define MCASP_CLKADJEN_ENABLE   0x00000001  





#define MCASP_DITCSRA0_DITCSRA0_M \
                                0xFFFFFFFF  
                                            

#define MCASP_DITCSRA0_DITCSRA0_S 0





#define MCASP_DITCSRA1_DITCSRA1_M \
                                0xFFFFFFFF  
                                            

#define MCASP_DITCSRA1_DITCSRA1_S 0





#define MCASP_DITCSRA2_DITCSRA2_M \
                                0xFFFFFFFF  
                                            

#define MCASP_DITCSRA2_DITCSRA2_S 0





#define MCASP_DITCSRA3_DITCSRA3_M \
                                0xFFFFFFFF  
                                            

#define MCASP_DITCSRA3_DITCSRA3_S 0





#define MCASP_DITCSRA4_DITCSRA4_M \
                                0xFFFFFFFF  
                                            

#define MCASP_DITCSRA4_DITCSRA4_S 0





#define MCASP_DITCSRA5_DITCSRA5_M \
                                0xFFFFFFFF  
                                            

#define MCASP_DITCSRA5_DITCSRA5_S 0





#define MCASP_DITCSRB0_DITCSRB0_M \
                                0xFFFFFFFF  
                                            

#define MCASP_DITCSRB0_DITCSRB0_S 0





#define MCASP_DITCSRB1_DITCSRB1_M \
                                0xFFFFFFFF  
                                            

#define MCASP_DITCSRB1_DITCSRB1_S 0





#define MCASP_DITCSRB2_DITCSRB2_M \
                                0xFFFFFFFF  
                                            

#define MCASP_DITCSRB2_DITCSRB2_S 0





#define MCASP_DITCSRB3_DITCSRB3_M \
                                0xFFFFFFFF  
                                            

#define MCASP_DITCSRB3_DITCSRB3_S 0





#define MCASP_DITCSRB4_DITCSRB4_M \
                                0xFFFFFFFF  
                                            

#define MCASP_DITCSRB4_DITCSRB4_S 0





#define MCASP_DITCSRB5_DITCSRB5_M \
                                0xFFFFFFFF  
                                            

#define MCASP_DITCSRB5_DITCSRB5_S 0





#define MCASP_DITUDRA0_DITUDRA0_M \
                                0xFFFFFFFF  

#define MCASP_DITUDRA0_DITUDRA0_S 0





#define MCASP_DITUDRA1_DITUDRA1_M \
                                0xFFFFFFFF  

#define MCASP_DITUDRA1_DITUDRA1_S 0





#define MCASP_DITUDRA2_DITUDRA2_M \
                                0xFFFFFFFF  

#define MCASP_DITUDRA2_DITUDRA2_S 0





#define MCASP_DITUDRA3_DITUDRA3_M \
                                0xFFFFFFFF  

#define MCASP_DITUDRA3_DITUDRA3_S 0





#define MCASP_DITUDRA4_DITUDRA4_M \
                                0xFFFFFFFF  

#define MCASP_DITUDRA4_DITUDRA4_S 0





#define MCASP_DITUDRA5_DITUDRA5_M \
                                0xFFFFFFFF  

#define MCASP_DITUDRA5_DITUDRA5_S 0





#define MCASP_DITUDRB0_DITUDRB0_M \
                                0xFFFFFFFF  

#define MCASP_DITUDRB0_DITUDRB0_S 0





#define MCASP_DITUDRB1_DITUDRB1_M \
                                0xFFFFFFFF  

#define MCASP_DITUDRB1_DITUDRB1_S 0





#define MCASP_DITUDRB2_DITUDRB2_M \
                                0xFFFFFFFF  

#define MCASP_DITUDRB2_DITUDRB2_S 0





#define MCASP_DITUDRB3_DITUDRB3_M \
                                0xFFFFFFFF  

#define MCASP_DITUDRB3_DITUDRB3_S 0





#define MCASP_DITUDRB4_DITUDRB4_M \
                                0xFFFFFFFF  

#define MCASP_DITUDRB4_DITUDRB4_S 0





#define MCASP_DITUDRB5_DITUDRB5_M \
                                0xFFFFFFFF  

#define MCASP_DITUDRB5_DITUDRB5_S 0





#define MCASP_XRSRCTL0_RRDY     0x00000020
#define MCASP_XRSRCTL0_XRDY     0x00000010
#define MCASP_XRSRCTL0_DISMOD_M 0x0000000C  
                                            
                                            
#define MCASP_XRSRCTL0_DISMOD_S 2
#define MCASP_XRSRCTL0_SRMOD_M  0x00000003  
                                            
                                            
#define MCASP_XRSRCTL0_SRMOD_S  0





#define MCASP_XRSRCTL1_RRDY     0x00000020
#define MCASP_XRSRCTL1_XRDY     0x00000010
#define MCASP_XRSRCTL1_DISMOD_M 0x0000000C  
                                            
                                            
#define MCASP_XRSRCTL1_DISMOD_S 2
#define MCASP_XRSRCTL1_SRMOD_M  0x00000003  
                                            
                                            
#define MCASP_XRSRCTL1_SRMOD_S  0





#define MCASP_XRSRCTL2_RRDY     0x00000020
#define MCASP_XRSRCTL2_XRDY     0x00000010
#define MCASP_XRSRCTL2_DISMOD_M 0x0000000C  
                                            
                                            
#define MCASP_XRSRCTL2_DISMOD_S 2
#define MCASP_XRSRCTL2_SRMOD_M  0x00000003  
                                            
                                            
#define MCASP_XRSRCTL2_SRMOD_S  0





#define MCASP_XRSRCTL3_RRDY     0x00000020
#define MCASP_XRSRCTL3_XRDY     0x00000010
#define MCASP_XRSRCTL3_DISMOD_M 0x0000000C  
                                            
                                            
#define MCASP_XRSRCTL3_DISMOD_S 2
#define MCASP_XRSRCTL3_SRMOD_M  0x00000003  
                                            
                                            
#define MCASP_XRSRCTL3_SRMOD_S  0





#define MCASP_XRSRCTL4_RRDY     0x00000020
#define MCASP_XRSRCTL4_XRDY     0x00000010
#define MCASP_XRSRCTL4_DISMOD_M 0x0000000C  
                                            
                                            
#define MCASP_XRSRCTL4_DISMOD_S 2
#define MCASP_XRSRCTL4_SRMOD_M  0x00000003  
                                            
                                            
#define MCASP_XRSRCTL4_SRMOD_S  0





#define MCASP_XRSRCTL5_RRDY     0x00000020
#define MCASP_XRSRCTL5_XRDY     0x00000010
#define MCASP_XRSRCTL5_DISMOD_M 0x0000000C  
                                            
                                            
#define MCASP_XRSRCTL5_DISMOD_S 2
#define MCASP_XRSRCTL5_SRMOD_M  0x00000003  
                                            
                                            
#define MCASP_XRSRCTL5_SRMOD_S  0





#define MCASP_XRSRCTL6_RRDY     0x00000020
#define MCASP_XRSRCTL6_XRDY     0x00000010
#define MCASP_XRSRCTL6_DISMOD_M 0x0000000C  
                                            
                                            
#define MCASP_XRSRCTL6_DISMOD_S 2
#define MCASP_XRSRCTL6_SRMOD_M  0x00000003  
                                            
                                            
#define MCASP_XRSRCTL6_SRMOD_S  0





#define MCASP_XRSRCTL7_RRDY     0x00000020
#define MCASP_XRSRCTL7_XRDY     0x00000010
#define MCASP_XRSRCTL7_DISMOD_M 0x0000000C  
                                            
                                            
#define MCASP_XRSRCTL7_DISMOD_S 2
#define MCASP_XRSRCTL7_SRMOD_M  0x00000003  
                                            
                                            
#define MCASP_XRSRCTL7_SRMOD_S  0





#define MCASP_XRSRCTL8_RRDY     0x00000020
#define MCASP_XRSRCTL8_XRDY     0x00000010
#define MCASP_XRSRCTL8_DISMOD_M 0x0000000C  
                                            
                                            
#define MCASP_XRSRCTL8_DISMOD_S 2
#define MCASP_XRSRCTL8_SRMOD_M  0x00000003  
                                            
                                            
#define MCASP_XRSRCTL8_SRMOD_S  0





#define MCASP_XRSRCTL9_RRDY     0x00000020
#define MCASP_XRSRCTL9_XRDY     0x00000010
#define MCASP_XRSRCTL9_DISMOD_M 0x0000000C  
                                            
                                            
#define MCASP_XRSRCTL9_DISMOD_S 2
#define MCASP_XRSRCTL9_SRMOD_M  0x00000003  
                                            
                                            
#define MCASP_XRSRCTL9_SRMOD_S  0





#define MCASP_XRSRCTL10_RRDY    0x00000020
#define MCASP_XRSRCTL10_XRDY    0x00000010
#define MCASP_XRSRCTL10_DISMOD_M \
                                0x0000000C  
                                            
                                            

#define MCASP_XRSRCTL10_DISMOD_S 2
#define MCASP_XRSRCTL10_SRMOD_M 0x00000003  
                                            
                                            
#define MCASP_XRSRCTL10_SRMOD_S 0





#define MCASP_XRSRCTL11_RRDY    0x00000020
#define MCASP_XRSRCTL11_XRDY    0x00000010
#define MCASP_XRSRCTL11_DISMOD_M \
                                0x0000000C  
                                            
                                            

#define MCASP_XRSRCTL11_DISMOD_S 2
#define MCASP_XRSRCTL11_SRMOD_M 0x00000003  
                                            
                                            
#define MCASP_XRSRCTL11_SRMOD_S 0





#define MCASP_XRSRCTL12_RRDY    0x00000020
#define MCASP_XRSRCTL12_XRDY    0x00000010
#define MCASP_XRSRCTL12_DISMOD_M \
                                0x0000000C  
                                            
                                            

#define MCASP_XRSRCTL12_DISMOD_S 2
#define MCASP_XRSRCTL12_SRMOD_M 0x00000003  
                                            
                                            
#define MCASP_XRSRCTL12_SRMOD_S 0





#define MCASP_XRSRCTL13_RRDY    0x00000020
#define MCASP_XRSRCTL13_XRDY    0x00000010
#define MCASP_XRSRCTL13_DISMOD_M \
                                0x0000000C  
                                            
                                            

#define MCASP_XRSRCTL13_DISMOD_S 2
#define MCASP_XRSRCTL13_SRMOD_M 0x00000003  
                                            
                                            
#define MCASP_XRSRCTL13_SRMOD_S 0





#define MCASP_XRSRCTL14_RRDY    0x00000020
#define MCASP_XRSRCTL14_XRDY    0x00000010
#define MCASP_XRSRCTL14_DISMOD_M \
                                0x0000000C  
                                            
                                            

#define MCASP_XRSRCTL14_DISMOD_S 2
#define MCASP_XRSRCTL14_SRMOD_M 0x00000003  
                                            
                                            
#define MCASP_XRSRCTL14_SRMOD_S 0





#define MCASP_XRSRCTL15_RRDY    0x00000020
#define MCASP_XRSRCTL15_XRDY    0x00000010
#define MCASP_XRSRCTL15_DISMOD_M \
                                0x0000000C  
                                            
                                            

#define MCASP_XRSRCTL15_DISMOD_S 2
#define MCASP_XRSRCTL15_SRMOD_M 0x00000003  
                                            
                                            
#define MCASP_XRSRCTL15_SRMOD_S 0





#define MCASP_TXBUF0_XBUF0_M    0xFFFFFFFF  
#define MCASP_TXBUF0_XBUF0_S    0





#define MCASP_TXBUF1_XBUF1_M    0xFFFFFFFF  
#define MCASP_TXBUF1_XBUF1_S    0





#define MCASP_TXBUF2_XBUF2_M    0xFFFFFFFF  
#define MCASP_TXBUF2_XBUF2_S    0





#define MCASP_TXBUF3_XBUF3_M    0xFFFFFFFF  
#define MCASP_TXBUF3_XBUF3_S    0





#define MCASP_TXBUF4_XBUF4_M    0xFFFFFFFF  
#define MCASP_TXBUF4_XBUF4_S    0





#define MCASP_TXBUF5_XBUF5_M    0xFFFFFFFF  
#define MCASP_TXBUF5_XBUF5_S    0





#define MCASP_TXBUF6_XBUF6_M    0xFFFFFFFF  
#define MCASP_TXBUF6_XBUF6_S    0





#define MCASP_TXBUF7_XBUF7_M    0xFFFFFFFF  
#define MCASP_TXBUF7_XBUF7_S    0





#define MCASP_TXBUF8_XBUF8_M    0xFFFFFFFF  
#define MCASP_TXBUF8_XBUF8_S    0





#define MCASP_TXBUF9_XBUF9_M    0xFFFFFFFF  
#define MCASP_TXBUF9_XBUF9_S    0





#define MCASP_TXBUF10_XBUF10_M  0xFFFFFFFF  
#define MCASP_TXBUF10_XBUF10_S  0





#define MCASP_TXBUF11_XBUF11_M  0xFFFFFFFF  
#define MCASP_TXBUF11_XBUF11_S  0





#define MCASP_TXBUF12_XBUF12_M  0xFFFFFFFF  
#define MCASP_TXBUF12_XBUF12_S  0





#define MCASP_TXBUF13_XBUF13_M  0xFFFFFFFF  
#define MCASP_TXBUF13_XBUF13_S  0





#define MCASP_TXBUF14_XBUF14_M  0xFFFFFFFF  
#define MCASP_TXBUF14_XBUF14_S  0





#define MCASP_TXBUF15_XBUF15_M  0xFFFFFFFF  
#define MCASP_TXBUF15_XBUF15_S  0





#define MCASP_RXBUF0_RBUF0_M    0xFFFFFFFF  
#define MCASP_RXBUF0_RBUF0_S    0





#define MCASP_RXBUF1_RBUF1_M    0xFFFFFFFF  
#define MCASP_RXBUF1_RBUF1_S    0





#define MCASP_RXBUF2_RBUF2_M    0xFFFFFFFF  
#define MCASP_RXBUF2_RBUF2_S    0





#define MCASP_RXBUF3_RBUF3_M    0xFFFFFFFF  
#define MCASP_RXBUF3_RBUF3_S    0





#define MCASP_RXBUF4_RBUF4_M    0xFFFFFFFF  
#define MCASP_RXBUF4_RBUF4_S    0





#define MCASP_RXBUF5_RBUF5_M    0xFFFFFFFF  
#define MCASP_RXBUF5_RBUF5_S    0





#define MCASP_RXBUF6_RBUF6_M    0xFFFFFFFF  
#define MCASP_RXBUF6_RBUF6_S    0





#define MCASP_RXBUF7_RBUF7_M    0xFFFFFFFF  
#define MCASP_RXBUF7_RBUF7_S    0





#define MCASP_RXBUF8_RBUF8_M    0xFFFFFFFF  
#define MCASP_RXBUF8_RBUF8_S    0





#define MCASP_RXBUF9_RBUF9_M    0xFFFFFFFF  
#define MCASP_RXBUF9_RBUF9_S    0





#define MCASP_RXBUF10_RBUF10_M  0xFFFFFFFF  
#define MCASP_RXBUF10_RBUF10_S  0





#define MCASP_RXBUF11_RBUF11_M  0xFFFFFFFF  
#define MCASP_RXBUF11_RBUF11_S  0





#define MCASP_RXBUF12_RBUF12_M  0xFFFFFFFF  
#define MCASP_RXBUF12_RBUF12_S  0





#define MCASP_RXBUF13_RBUF13_M  0xFFFFFFFF  
#define MCASP_RXBUF13_RBUF13_S  0





#define MCASP_RXBUF14_RBUF14_M  0xFFFFFFFF  
#define MCASP_RXBUF14_RBUF14_S  0





#define MCASP_RXBUF15_RBUF15_M  0xFFFFFFFF  
#define MCASP_RXBUF15_RBUF15_S  0



#endif 
