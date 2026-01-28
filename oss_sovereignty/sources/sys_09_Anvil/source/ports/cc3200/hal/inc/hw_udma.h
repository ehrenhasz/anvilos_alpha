


































#ifndef __HW_UDMA_H__
#define __HW_UDMA_H__






#define UDMA_O_STAT             0x00000000
#define UDMA_O_CFG              0x00000004
#define UDMA_O_CTLBASE          0x00000008
#define UDMA_O_ALTBASE          0x0000000C
#define UDMA_O_WAITSTAT         0x00000010
#define UDMA_O_SWREQ            0x00000014
#define UDMA_O_USEBURSTSET      0x00000018
#define UDMA_O_USEBURSTCLR      0x0000001C
#define UDMA_O_REQMASKSET       0x00000020
#define UDMA_O_REQMASKCLR       0x00000024
#define UDMA_O_ENASET           0x00000028
#define UDMA_O_ENACLR           0x0000002C
#define UDMA_O_ALTSET           0x00000030
#define UDMA_O_ALTCLR           0x00000034
#define UDMA_O_PRIOSET          0x00000038
#define UDMA_O_PRIOCLR          0x0000003C
#define UDMA_O_ERRCLR           0x0000004C
#define UDMA_O_CHASGN           0x00000500
#define UDMA_O_CHIS             0x00000504
#define UDMA_O_CHMAP0           0x00000510
#define UDMA_O_CHMAP1           0x00000514
#define UDMA_O_CHMAP2           0x00000518
#define UDMA_O_CHMAP3           0x0000051C
#define UDMA_O_PV               0x00000FB0








#define UDMA_STAT_DMACHANS_M  0x001F0000  
#define UDMA_STAT_DMACHANS_S  16
#define UDMA_STAT_STATE_M     0x000000F0  
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
                                            
#define UDMA_STAT_STATE_S     4
#define UDMA_STAT_MASTEN        0x00000001  





#define UDMA_CFG_MASTEN         0x00000001  





#define UDMA_CTLBASE_ADDR_M   0xFFFFFC00  
#define UDMA_CTLBASE_ADDR_S   10





#define UDMA_ALTBASE_ADDR_M   0xFFFFFFFF  
                                            
#define UDMA_ALTBASE_ADDR_S   0





#define UDMA_WAITSTAT_WAITREQ_M \
                                0xFFFFFFFF  

#define UDMA_WAITSTAT_WAITREQ_S 0





#define UDMA_SWREQ_M          0xFFFFFFFF  
#define UDMA_SWREQ_S          0






#define UDMA_USEBURSTSET_SET_M \
                                0xFFFFFFFF  

#define UDMA_USEBURSTSET_SET_S 0






#define UDMA_USEBURSTCLR_CLR_M \
                                0xFFFFFFFF  

#define UDMA_USEBURSTCLR_CLR_S 0





#define UDMA_REQMASKSET_SET_M 0xFFFFFFFF  
#define UDMA_REQMASKSET_SET_S 0





#define UDMA_REQMASKCLR_CLR_M 0xFFFFFFFF  
#define UDMA_REQMASKCLR_CLR_S 0





#define UDMA_ENASET_CHENSET_M 0xFFFFFFFF  
#define UDMA_ENASET_CHENSET_S 0





#define UDMA_ENACLR_CLR_M     0xFFFFFFFF  
#define UDMA_ENACLR_CLR_S     0





#define UDMA_ALTSET_SET_M     0xFFFFFFFF  
#define UDMA_ALTSET_SET_S     0





#define UDMA_ALTCLR_CLR_M     0xFFFFFFFF  
#define UDMA_ALTCLR_CLR_S     0





#define UDMA_PRIOSET_SET_M    0xFFFFFFFF  
#define UDMA_PRIOSET_SET_S    0





#define UDMA_PRIOCLR_CLR_M    0xFFFFFFFF  
#define UDMA_PRIOCLR_CLR_S    0





#define UDMA_ERRCLR_ERRCLR      0x00000001  





#define UDMA_CHASGN_M         0xFFFFFFFF  
#define UDMA_CHASGN_S         0





#define UDMA_CHIS_M           0xFFFFFFFF  
#define UDMA_CHIS_S           0





#define UDMA_CHMAP0_CH7SEL_M  0xF0000000  
#define UDMA_CHMAP0_CH7SEL_S  28
#define UDMA_CHMAP0_CH6SEL_M  0x0F000000  
#define UDMA_CHMAP0_CH6SEL_S  24
#define UDMA_CHMAP0_CH5SEL_M  0x00F00000  
#define UDMA_CHMAP0_CH5SEL_S  20
#define UDMA_CHMAP0_CH4SEL_M  0x000F0000  
#define UDMA_CHMAP0_CH4SEL_S  16
#define UDMA_CHMAP0_CH3SEL_M  0x0000F000  
#define UDMA_CHMAP0_CH3SEL_S  12
#define UDMA_CHMAP0_CH2SEL_M  0x00000F00  
#define UDMA_CHMAP0_CH2SEL_S  8
#define UDMA_CHMAP0_CH1SEL_M  0x000000F0  
#define UDMA_CHMAP0_CH1SEL_S  4
#define UDMA_CHMAP0_CH0SEL_M  0x0000000F  
#define UDMA_CHMAP0_CH0SEL_S  0





#define UDMA_CHMAP1_CH15SEL_M 0xF0000000  
#define UDMA_CHMAP1_CH15SEL_S 28
#define UDMA_CHMAP1_CH14SEL_M 0x0F000000  
#define UDMA_CHMAP1_CH14SEL_S 24
#define UDMA_CHMAP1_CH13SEL_M 0x00F00000  
#define UDMA_CHMAP1_CH13SEL_S 20
#define UDMA_CHMAP1_CH12SEL_M 0x000F0000  
#define UDMA_CHMAP1_CH12SEL_S 16
#define UDMA_CHMAP1_CH11SEL_M 0x0000F000  
#define UDMA_CHMAP1_CH11SEL_S 12
#define UDMA_CHMAP1_CH10SEL_M 0x00000F00  
#define UDMA_CHMAP1_CH10SEL_S 8
#define UDMA_CHMAP1_CH9SEL_M  0x000000F0  
#define UDMA_CHMAP1_CH9SEL_S  4
#define UDMA_CHMAP1_CH8SEL_M  0x0000000F  
#define UDMA_CHMAP1_CH8SEL_S  0





#define UDMA_CHMAP2_CH23SEL_M 0xF0000000  
#define UDMA_CHMAP2_CH23SEL_S 28
#define UDMA_CHMAP2_CH22SEL_M 0x0F000000  
#define UDMA_CHMAP2_CH22SEL_S 24
#define UDMA_CHMAP2_CH21SEL_M 0x00F00000  
#define UDMA_CHMAP2_CH21SEL_S 20
#define UDMA_CHMAP2_CH20SEL_M 0x000F0000  
#define UDMA_CHMAP2_CH20SEL_S 16
#define UDMA_CHMAP2_CH19SEL_M 0x0000F000  
#define UDMA_CHMAP2_CH19SEL_S 12
#define UDMA_CHMAP2_CH18SEL_M 0x00000F00  
#define UDMA_CHMAP2_CH18SEL_S 8
#define UDMA_CHMAP2_CH17SEL_M 0x000000F0  
#define UDMA_CHMAP2_CH17SEL_S 4
#define UDMA_CHMAP2_CH16SEL_M 0x0000000F  
#define UDMA_CHMAP2_CH16SEL_S 0





#define UDMA_CHMAP3_CH31SEL_M 0xF0000000  
#define UDMA_CHMAP3_CH31SEL_S 28
#define UDMA_CHMAP3_CH30SEL_M 0x0F000000  
#define UDMA_CHMAP3_CH30SEL_S 24
#define UDMA_CHMAP3_CH29SEL_M 0x00F00000  
#define UDMA_CHMAP3_CH29SEL_S 20
#define UDMA_CHMAP3_CH28SEL_M 0x000F0000  
#define UDMA_CHMAP3_CH28SEL_S 16
#define UDMA_CHMAP3_CH27SEL_M 0x0000F000  
#define UDMA_CHMAP3_CH27SEL_S 12
#define UDMA_CHMAP3_CH26SEL_M 0x00000F00  
#define UDMA_CHMAP3_CH26SEL_S 8
#define UDMA_CHMAP3_CH25SEL_M 0x000000F0  
#define UDMA_CHMAP3_CH25SEL_S 4
#define UDMA_CHMAP3_CH24SEL_M 0x0000000F  
#define UDMA_CHMAP3_CH24SEL_S 0





#define UDMA_PV_MAJOR_M       0x0000FF00  
#define UDMA_PV_MAJOR_S       8
#define UDMA_PV_MINOR_M       0x000000FF  
#define UDMA_PV_MINOR_S       0



#endif 
