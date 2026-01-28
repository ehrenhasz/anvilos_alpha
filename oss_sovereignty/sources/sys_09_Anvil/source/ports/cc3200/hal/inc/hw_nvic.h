








































#ifndef __HW_NVIC_H__
#define __HW_NVIC_H__






#define NVIC_INT_TYPE           0xE000E004  
#define NVIC_ACTLR              0xE000E008  
#define NVIC_ST_CTRL            0xE000E010  
                                            
#define NVIC_ST_RELOAD          0xE000E014  
#define NVIC_ST_CURRENT         0xE000E018  
#define NVIC_ST_CAL             0xE000E01C  

#define NVIC_EN0                0xE000E100  
#define NVIC_EN1                0xE000E104  
#define NVIC_EN2                0xE000E108  
#define NVIC_EN3                0xE000E10C  
#define NVIC_EN4                0xE000E110  
#define NVIC_EN5                0xE000E114  

#define NVIC_DIS0               0xE000E180  
#define NVIC_DIS1               0xE000E184  

#define NVIC_DIS2               0xE000E188  
#define NVIC_DIS3               0xE000E18C  
#define NVIC_DIS4               0xE000E190  
#define NVIC_DIS5               0xE000E194  

#define NVIC_PEND0              0xE000E200  
#define NVIC_PEND1              0xE000E204  

#define NVIC_PEND2              0xE000E208  
#define NVIC_PEND3              0xE000E20C  
#define NVIC_PEND4              0xE000E210  
#define NVIC_PEND5              0xE000E214  

#define NVIC_UNPEND0            0xE000E280  
#define NVIC_UNPEND1            0xE000E284  

#define NVIC_UNPEND2            0xE000E288  
#define NVIC_UNPEND3            0xE000E28C  
#define NVIC_UNPEND4            0xE000E290  
#define NVIC_UNPEND5            0xE000E294  

#define NVIC_ACTIVE0            0xE000E300  
#define NVIC_ACTIVE1            0xE000E304  

#define NVIC_ACTIVE2            0xE000E308  
#define NVIC_ACTIVE3            0xE000E30C  
#define NVIC_ACTIVE4            0xE000E310  
#define NVIC_ACTIVE5            0xE000E314  

#define NVIC_PRI0               0xE000E400  
#define NVIC_PRI1               0xE000E404  
#define NVIC_PRI2               0xE000E408  
#define NVIC_PRI3               0xE000E40C  
#define NVIC_PRI4               0xE000E410  
#define NVIC_PRI5               0xE000E414  
#define NVIC_PRI6               0xE000E418  
#define NVIC_PRI7               0xE000E41C  
#define NVIC_PRI8               0xE000E420  
#define NVIC_PRI9               0xE000E424  
#define NVIC_PRI10              0xE000E428  
#define NVIC_PRI11              0xE000E42C  
#define NVIC_PRI12              0xE000E430  
#define NVIC_PRI13              0xE000E434  

#define NVIC_PRI14              0xE000E438  
#define NVIC_PRI15              0xE000E43C  
#define NVIC_PRI16              0xE000E440  
#define NVIC_PRI17              0xE000E444  
#define NVIC_PRI18              0xE000E448  
#define NVIC_PRI19              0xE000E44C  
#define NVIC_PRI20              0xE000E450  
#define NVIC_PRI21              0xE000E454  
#define NVIC_PRI22              0xE000E458  
#define NVIC_PRI23              0xE000E45C  
#define NVIC_PRI24              0xE000E460  
#define NVIC_PRI25              0xE000E464  
#define NVIC_PRI26              0xE000E468  
#define NVIC_PRI27              0xE000E46C  
#define NVIC_PRI28              0xE000E470  
#define NVIC_PRI29              0xE000E474  
#define NVIC_PRI30              0xE000E478  
#define NVIC_PRI31              0xE000E47C  
#define NVIC_PRI32              0xE000E480  
#define NVIC_PRI33              0xE000E484  
#define NVIC_PRI34              0xE000E488  
#define NVIC_PRI35              0xE000E48C  
#define NVIC_PRI36              0xE000E490  
#define NVIC_PRI37              0xE000E494  
#define NVIC_PRI38              0xE000E498  
#define NVIC_PRI39              0xE000E49C  
#define NVIC_PRI40              0xE000E4A0  
#define NVIC_PRI41              0xE000E4A4  
#define NVIC_PRI42              0xE000E4A8  
#define NVIC_PRI43              0xE000E4AC  
#define NVIC_PRI44              0xE000E4B0  
#define NVIC_PRI45              0xE000E4B4  
#define NVIC_PRI46              0xE000E4B8  
#define NVIC_PRI47              0xE000E4BC  
#define NVIC_PRI48              0xE000E4C0  



#define NVIC_CPUID              0xE000ED00  
#define NVIC_INT_CTRL           0xE000ED04  
#define NVIC_VTABLE             0xE000ED08  
#define NVIC_APINT              0xE000ED0C  
                                            
#define NVIC_SYS_CTRL           0xE000ED10  
#define NVIC_CFG_CTRL           0xE000ED14  
#define NVIC_SYS_PRI1           0xE000ED18  
#define NVIC_SYS_PRI2           0xE000ED1C  
#define NVIC_SYS_PRI3           0xE000ED20  
#define NVIC_SYS_HND_CTRL       0xE000ED24  
#define NVIC_FAULT_STAT         0xE000ED28  
#define NVIC_HFAULT_STAT        0xE000ED2C  
#define NVIC_DEBUG_STAT         0xE000ED30  
#define NVIC_MM_ADDR            0xE000ED34  
#define NVIC_FAULT_ADDR         0xE000ED38  
#define NVIC_MPU_TYPE           0xE000ED90  
#define NVIC_MPU_CTRL           0xE000ED94  
#define NVIC_MPU_NUMBER         0xE000ED98  
#define NVIC_MPU_BASE           0xE000ED9C  
#define NVIC_MPU_ATTR           0xE000EDA0  
#define NVIC_MPU_BASE1          0xE000EDA4  
#define NVIC_MPU_ATTR1          0xE000EDA8  
                                            
#define NVIC_MPU_BASE2          0xE000EDAC  
#define NVIC_MPU_ATTR2          0xE000EDB0  
                                            
#define NVIC_MPU_BASE3          0xE000EDB4  
#define NVIC_MPU_ATTR3          0xE000EDB8  
                                            
#define NVIC_DBG_CTRL           0xE000EDF0  
#define NVIC_DBG_XFER           0xE000EDF4  
#define NVIC_DBG_DATA           0xE000EDF8  
#define NVIC_DBG_INT            0xE000EDFC  
#define NVIC_SW_TRIG            0xE000EF00  






#define NVIC_INT_TYPE_LINES_M   0x0000001F  
#define NVIC_INT_TYPE_LINES_S   0






#define NVIC_ACTLR_DISFOLD      0x00000004  
#define NVIC_ACTLR_DISWBUF      0x00000002  
#define NVIC_ACTLR_DISMCYC      0x00000001  
                                            






#define NVIC_ST_CTRL_COUNT      0x00010000  
#define NVIC_ST_CTRL_CLK_SRC    0x00000004  
#define NVIC_ST_CTRL_INTEN      0x00000002  
#define NVIC_ST_CTRL_ENABLE     0x00000001  






#define NVIC_ST_RELOAD_M        0x00FFFFFF  
#define NVIC_ST_RELOAD_S        0







#define NVIC_ST_CURRENT_M       0x00FFFFFF  
#define NVIC_ST_CURRENT_S       0






#define NVIC_ST_CAL_NOREF       0x80000000  
#define NVIC_ST_CAL_SKEW        0x40000000  
#define NVIC_ST_CAL_ONEMS_M     0x00FFFFFF  
#define NVIC_ST_CAL_ONEMS_S     0






#define NVIC_EN0_INT_M          0xFFFFFFFF  
#define NVIC_EN0_INT0           0x00000001  
#define NVIC_EN0_INT1           0x00000002  
#define NVIC_EN0_INT2           0x00000004  
#define NVIC_EN0_INT3           0x00000008  
#define NVIC_EN0_INT4           0x00000010  
#define NVIC_EN0_INT5           0x00000020  
#define NVIC_EN0_INT6           0x00000040  
#define NVIC_EN0_INT7           0x00000080  
#define NVIC_EN0_INT8           0x00000100  
#define NVIC_EN0_INT9           0x00000200  
#define NVIC_EN0_INT10          0x00000400  
#define NVIC_EN0_INT11          0x00000800  
#define NVIC_EN0_INT12          0x00001000  
#define NVIC_EN0_INT13          0x00002000  
#define NVIC_EN0_INT14          0x00004000  
#define NVIC_EN0_INT15          0x00008000  
#define NVIC_EN0_INT16          0x00010000  
#define NVIC_EN0_INT17          0x00020000  
#define NVIC_EN0_INT18          0x00040000  
#define NVIC_EN0_INT19          0x00080000  
#define NVIC_EN0_INT20          0x00100000  
#define NVIC_EN0_INT21          0x00200000  
#define NVIC_EN0_INT22          0x00400000  
#define NVIC_EN0_INT23          0x00800000  
#define NVIC_EN0_INT24          0x01000000  
#define NVIC_EN0_INT25          0x02000000  
#define NVIC_EN0_INT26          0x04000000  
#define NVIC_EN0_INT27          0x08000000  
#define NVIC_EN0_INT28          0x10000000  
#define NVIC_EN0_INT29          0x20000000  
#define NVIC_EN0_INT30          0x40000000  
#define NVIC_EN0_INT31          0x80000000  






#define NVIC_EN1_INT_M          0x007FFFFF  

#undef NVIC_EN1_INT_M
#define NVIC_EN1_INT_M          0xFFFFFFFF  

#define NVIC_EN1_INT32          0x00000001  
#define NVIC_EN1_INT33          0x00000002  
#define NVIC_EN1_INT34          0x00000004  
#define NVIC_EN1_INT35          0x00000008  
#define NVIC_EN1_INT36          0x00000010  
#define NVIC_EN1_INT37          0x00000020  
#define NVIC_EN1_INT38          0x00000040  
#define NVIC_EN1_INT39          0x00000080  
#define NVIC_EN1_INT40          0x00000100  
#define NVIC_EN1_INT41          0x00000200  
#define NVIC_EN1_INT42          0x00000400  
#define NVIC_EN1_INT43          0x00000800  
#define NVIC_EN1_INT44          0x00001000  
#define NVIC_EN1_INT45          0x00002000  
#define NVIC_EN1_INT46          0x00004000  
#define NVIC_EN1_INT47          0x00008000  
#define NVIC_EN1_INT48          0x00010000  
#define NVIC_EN1_INT49          0x00020000  
#define NVIC_EN1_INT50          0x00040000  
#define NVIC_EN1_INT51          0x00080000  
#define NVIC_EN1_INT52          0x00100000  
#define NVIC_EN1_INT53          0x00200000  
#define NVIC_EN1_INT54          0x00400000  







#define NVIC_EN2_INT_M          0xFFFFFFFF  






#define NVIC_EN3_INT_M          0xFFFFFFFF  






#define NVIC_EN4_INT_M          0x0000000F  







#define NVIC_DIS0_INT_M         0xFFFFFFFF  
#define NVIC_DIS0_INT0          0x00000001  
#define NVIC_DIS0_INT1          0x00000002  
#define NVIC_DIS0_INT2          0x00000004  
#define NVIC_DIS0_INT3          0x00000008  
#define NVIC_DIS0_INT4          0x00000010  
#define NVIC_DIS0_INT5          0x00000020  
#define NVIC_DIS0_INT6          0x00000040  
#define NVIC_DIS0_INT7          0x00000080  
#define NVIC_DIS0_INT8          0x00000100  
#define NVIC_DIS0_INT9          0x00000200  
#define NVIC_DIS0_INT10         0x00000400  
#define NVIC_DIS0_INT11         0x00000800  
#define NVIC_DIS0_INT12         0x00001000  
#define NVIC_DIS0_INT13         0x00002000  
#define NVIC_DIS0_INT14         0x00004000  
#define NVIC_DIS0_INT15         0x00008000  
#define NVIC_DIS0_INT16         0x00010000  
#define NVIC_DIS0_INT17         0x00020000  
#define NVIC_DIS0_INT18         0x00040000  
#define NVIC_DIS0_INT19         0x00080000  
#define NVIC_DIS0_INT20         0x00100000  
#define NVIC_DIS0_INT21         0x00200000  
#define NVIC_DIS0_INT22         0x00400000  
#define NVIC_DIS0_INT23         0x00800000  
#define NVIC_DIS0_INT24         0x01000000  
#define NVIC_DIS0_INT25         0x02000000  
#define NVIC_DIS0_INT26         0x04000000  
#define NVIC_DIS0_INT27         0x08000000  
#define NVIC_DIS0_INT28         0x10000000  
#define NVIC_DIS0_INT29         0x20000000  
#define NVIC_DIS0_INT30         0x40000000  
#define NVIC_DIS0_INT31         0x80000000  






#define NVIC_DIS1_INT_M         0x00FFFFFF  

#undef NVIC_DIS1_INT_M
#define NVIC_DIS1_INT_M         0xFFFFFFFF  

#define NVIC_DIS1_INT32         0x00000001  
#define NVIC_DIS1_INT33         0x00000002  
#define NVIC_DIS1_INT34         0x00000004  
#define NVIC_DIS1_INT35         0x00000008  
#define NVIC_DIS1_INT36         0x00000010  
#define NVIC_DIS1_INT37         0x00000020  
#define NVIC_DIS1_INT38         0x00000040  
#define NVIC_DIS1_INT39         0x00000080  
#define NVIC_DIS1_INT40         0x00000100  
#define NVIC_DIS1_INT41         0x00000200  
#define NVIC_DIS1_INT42         0x00000400  
#define NVIC_DIS1_INT43         0x00000800  
#define NVIC_DIS1_INT44         0x00001000  
#define NVIC_DIS1_INT45         0x00002000  
#define NVIC_DIS1_INT46         0x00004000  
#define NVIC_DIS1_INT47         0x00008000  
#define NVIC_DIS1_INT48         0x00010000  
#define NVIC_DIS1_INT49         0x00020000  
#define NVIC_DIS1_INT50         0x00040000  
#define NVIC_DIS1_INT51         0x00080000  
#define NVIC_DIS1_INT52         0x00100000  
#define NVIC_DIS1_INT53         0x00200000  
#define NVIC_DIS1_INT54         0x00400000  
#define NVIC_DIS1_INT55         0x00800000  







#define NVIC_DIS2_INT_M         0xFFFFFFFF  






#define NVIC_DIS3_INT_M         0xFFFFFFFF  






#define NVIC_DIS4_INT_M         0x0000000F  







#define NVIC_PEND0_INT_M        0xFFFFFFFF  
#define NVIC_PEND0_INT0         0x00000001  
#define NVIC_PEND0_INT1         0x00000002  
#define NVIC_PEND0_INT2         0x00000004  
#define NVIC_PEND0_INT3         0x00000008  
#define NVIC_PEND0_INT4         0x00000010  
#define NVIC_PEND0_INT5         0x00000020  
#define NVIC_PEND0_INT6         0x00000040  
#define NVIC_PEND0_INT7         0x00000080  
#define NVIC_PEND0_INT8         0x00000100  
#define NVIC_PEND0_INT9         0x00000200  
#define NVIC_PEND0_INT10        0x00000400  
#define NVIC_PEND0_INT11        0x00000800  
#define NVIC_PEND0_INT12        0x00001000  
#define NVIC_PEND0_INT13        0x00002000  
#define NVIC_PEND0_INT14        0x00004000  
#define NVIC_PEND0_INT15        0x00008000  
#define NVIC_PEND0_INT16        0x00010000  
#define NVIC_PEND0_INT17        0x00020000  
#define NVIC_PEND0_INT18        0x00040000  
#define NVIC_PEND0_INT19        0x00080000  
#define NVIC_PEND0_INT20        0x00100000  
#define NVIC_PEND0_INT21        0x00200000  
#define NVIC_PEND0_INT22        0x00400000  
#define NVIC_PEND0_INT23        0x00800000  
#define NVIC_PEND0_INT24        0x01000000  
#define NVIC_PEND0_INT25        0x02000000  
#define NVIC_PEND0_INT26        0x04000000  
#define NVIC_PEND0_INT27        0x08000000  
#define NVIC_PEND0_INT28        0x10000000  
#define NVIC_PEND0_INT29        0x20000000  
#define NVIC_PEND0_INT30        0x40000000  
#define NVIC_PEND0_INT31        0x80000000  






#define NVIC_PEND1_INT_M        0x00FFFFFF  

#undef NVIC_PEND1_INT_M
#define NVIC_PEND1_INT_M        0xFFFFFFFF  

#define NVIC_PEND1_INT32        0x00000001  
#define NVIC_PEND1_INT33        0x00000002  
#define NVIC_PEND1_INT34        0x00000004  
#define NVIC_PEND1_INT35        0x00000008  
#define NVIC_PEND1_INT36        0x00000010  
#define NVIC_PEND1_INT37        0x00000020  
#define NVIC_PEND1_INT38        0x00000040  
#define NVIC_PEND1_INT39        0x00000080  
#define NVIC_PEND1_INT40        0x00000100  
#define NVIC_PEND1_INT41        0x00000200  
#define NVIC_PEND1_INT42        0x00000400  
#define NVIC_PEND1_INT43        0x00000800  
#define NVIC_PEND1_INT44        0x00001000  
#define NVIC_PEND1_INT45        0x00002000  
#define NVIC_PEND1_INT46        0x00004000  
#define NVIC_PEND1_INT47        0x00008000  
#define NVIC_PEND1_INT48        0x00010000  
#define NVIC_PEND1_INT49        0x00020000  
#define NVIC_PEND1_INT50        0x00040000  
#define NVIC_PEND1_INT51        0x00080000  
#define NVIC_PEND1_INT52        0x00100000  
#define NVIC_PEND1_INT53        0x00200000  
#define NVIC_PEND1_INT54        0x00400000  
#define NVIC_PEND1_INT55        0x00800000  







#define NVIC_PEND2_INT_M        0xFFFFFFFF  






#define NVIC_PEND3_INT_M        0xFFFFFFFF  






#define NVIC_PEND4_INT_M        0x0000000F  







#define NVIC_UNPEND0_INT_M      0xFFFFFFFF  
#define NVIC_UNPEND0_INT0       0x00000001  
#define NVIC_UNPEND0_INT1       0x00000002  
#define NVIC_UNPEND0_INT2       0x00000004  
#define NVIC_UNPEND0_INT3       0x00000008  
#define NVIC_UNPEND0_INT4       0x00000010  
#define NVIC_UNPEND0_INT5       0x00000020  
#define NVIC_UNPEND0_INT6       0x00000040  
#define NVIC_UNPEND0_INT7       0x00000080  
#define NVIC_UNPEND0_INT8       0x00000100  
#define NVIC_UNPEND0_INT9       0x00000200  
#define NVIC_UNPEND0_INT10      0x00000400  
#define NVIC_UNPEND0_INT11      0x00000800  
#define NVIC_UNPEND0_INT12      0x00001000  
#define NVIC_UNPEND0_INT13      0x00002000  
#define NVIC_UNPEND0_INT14      0x00004000  
#define NVIC_UNPEND0_INT15      0x00008000  
#define NVIC_UNPEND0_INT16      0x00010000  
#define NVIC_UNPEND0_INT17      0x00020000  
#define NVIC_UNPEND0_INT18      0x00040000  
#define NVIC_UNPEND0_INT19      0x00080000  
#define NVIC_UNPEND0_INT20      0x00100000  
#define NVIC_UNPEND0_INT21      0x00200000  
#define NVIC_UNPEND0_INT22      0x00400000  
#define NVIC_UNPEND0_INT23      0x00800000  
#define NVIC_UNPEND0_INT24      0x01000000  
#define NVIC_UNPEND0_INT25      0x02000000  
#define NVIC_UNPEND0_INT26      0x04000000  
#define NVIC_UNPEND0_INT27      0x08000000  
#define NVIC_UNPEND0_INT28      0x10000000  
#define NVIC_UNPEND0_INT29      0x20000000  
#define NVIC_UNPEND0_INT30      0x40000000  
#define NVIC_UNPEND0_INT31      0x80000000  






#define NVIC_UNPEND1_INT_M      0x00FFFFFF  

#undef NVIC_UNPEND1_INT_M
#define NVIC_UNPEND1_INT_M      0xFFFFFFFF  

#define NVIC_UNPEND1_INT32      0x00000001  
#define NVIC_UNPEND1_INT33      0x00000002  
#define NVIC_UNPEND1_INT34      0x00000004  
#define NVIC_UNPEND1_INT35      0x00000008  
#define NVIC_UNPEND1_INT36      0x00000010  
#define NVIC_UNPEND1_INT37      0x00000020  
#define NVIC_UNPEND1_INT38      0x00000040  
#define NVIC_UNPEND1_INT39      0x00000080  
#define NVIC_UNPEND1_INT40      0x00000100  
#define NVIC_UNPEND1_INT41      0x00000200  
#define NVIC_UNPEND1_INT42      0x00000400  
#define NVIC_UNPEND1_INT43      0x00000800  
#define NVIC_UNPEND1_INT44      0x00001000  
#define NVIC_UNPEND1_INT45      0x00002000  
#define NVIC_UNPEND1_INT46      0x00004000  
#define NVIC_UNPEND1_INT47      0x00008000  
#define NVIC_UNPEND1_INT48      0x00010000  
#define NVIC_UNPEND1_INT49      0x00020000  
#define NVIC_UNPEND1_INT50      0x00040000  
#define NVIC_UNPEND1_INT51      0x00080000  
#define NVIC_UNPEND1_INT52      0x00100000  
#define NVIC_UNPEND1_INT53      0x00200000  
#define NVIC_UNPEND1_INT54      0x00400000  
#define NVIC_UNPEND1_INT55      0x00800000  







#define NVIC_UNPEND2_INT_M      0xFFFFFFFF  






#define NVIC_UNPEND3_INT_M      0xFFFFFFFF  






#define NVIC_UNPEND4_INT_M      0x0000000F  







#define NVIC_ACTIVE0_INT_M      0xFFFFFFFF  
#define NVIC_ACTIVE0_INT0       0x00000001  
#define NVIC_ACTIVE0_INT1       0x00000002  
#define NVIC_ACTIVE0_INT2       0x00000004  
#define NVIC_ACTIVE0_INT3       0x00000008  
#define NVIC_ACTIVE0_INT4       0x00000010  
#define NVIC_ACTIVE0_INT5       0x00000020  
#define NVIC_ACTIVE0_INT6       0x00000040  
#define NVIC_ACTIVE0_INT7       0x00000080  
#define NVIC_ACTIVE0_INT8       0x00000100  
#define NVIC_ACTIVE0_INT9       0x00000200  
#define NVIC_ACTIVE0_INT10      0x00000400  
#define NVIC_ACTIVE0_INT11      0x00000800  
#define NVIC_ACTIVE0_INT12      0x00001000  
#define NVIC_ACTIVE0_INT13      0x00002000  
#define NVIC_ACTIVE0_INT14      0x00004000  
#define NVIC_ACTIVE0_INT15      0x00008000  
#define NVIC_ACTIVE0_INT16      0x00010000  
#define NVIC_ACTIVE0_INT17      0x00020000  
#define NVIC_ACTIVE0_INT18      0x00040000  
#define NVIC_ACTIVE0_INT19      0x00080000  
#define NVIC_ACTIVE0_INT20      0x00100000  
#define NVIC_ACTIVE0_INT21      0x00200000  
#define NVIC_ACTIVE0_INT22      0x00400000  
#define NVIC_ACTIVE0_INT23      0x00800000  
#define NVIC_ACTIVE0_INT24      0x01000000  
#define NVIC_ACTIVE0_INT25      0x02000000  
#define NVIC_ACTIVE0_INT26      0x04000000  
#define NVIC_ACTIVE0_INT27      0x08000000  
#define NVIC_ACTIVE0_INT28      0x10000000  
#define NVIC_ACTIVE0_INT29      0x20000000  
#define NVIC_ACTIVE0_INT30      0x40000000  
#define NVIC_ACTIVE0_INT31      0x80000000  






#define NVIC_ACTIVE1_INT_M      0x00FFFFFF  

#undef NVIC_ACTIVE1_INT_M
#define NVIC_ACTIVE1_INT_M      0xFFFFFFFF  

#define NVIC_ACTIVE1_INT32      0x00000001  
#define NVIC_ACTIVE1_INT33      0x00000002  
#define NVIC_ACTIVE1_INT34      0x00000004  
#define NVIC_ACTIVE1_INT35      0x00000008  
#define NVIC_ACTIVE1_INT36      0x00000010  
#define NVIC_ACTIVE1_INT37      0x00000020  
#define NVIC_ACTIVE1_INT38      0x00000040  
#define NVIC_ACTIVE1_INT39      0x00000080  
#define NVIC_ACTIVE1_INT40      0x00000100  
#define NVIC_ACTIVE1_INT41      0x00000200  
#define NVIC_ACTIVE1_INT42      0x00000400  
#define NVIC_ACTIVE1_INT43      0x00000800  
#define NVIC_ACTIVE1_INT44      0x00001000  
#define NVIC_ACTIVE1_INT45      0x00002000  
#define NVIC_ACTIVE1_INT46      0x00004000  
#define NVIC_ACTIVE1_INT47      0x00008000  
#define NVIC_ACTIVE1_INT48      0x00010000  
#define NVIC_ACTIVE1_INT49      0x00020000  
#define NVIC_ACTIVE1_INT50      0x00040000  
#define NVIC_ACTIVE1_INT51      0x00080000  
#define NVIC_ACTIVE1_INT52      0x00100000  
#define NVIC_ACTIVE1_INT53      0x00200000  
#define NVIC_ACTIVE1_INT54      0x00400000  
#define NVIC_ACTIVE1_INT55      0x00800000  







#define NVIC_ACTIVE2_INT_M      0xFFFFFFFF  






#define NVIC_ACTIVE3_INT_M      0xFFFFFFFF  






#define NVIC_ACTIVE4_INT_M      0x0000000F  







#define NVIC_PRI0_INT3_M        0xE0000000  
#define NVIC_PRI0_INT2_M        0x00E00000  
#define NVIC_PRI0_INT1_M        0x0000E000  
#define NVIC_PRI0_INT0_M        0x000000E0  
#define NVIC_PRI0_INT3_S        29
#define NVIC_PRI0_INT2_S        21
#define NVIC_PRI0_INT1_S        13
#define NVIC_PRI0_INT0_S        5






#define NVIC_PRI1_INT7_M        0xE0000000  
#define NVIC_PRI1_INT6_M        0x00E00000  
#define NVIC_PRI1_INT5_M        0x0000E000  
#define NVIC_PRI1_INT4_M        0x000000E0  
#define NVIC_PRI1_INT7_S        29
#define NVIC_PRI1_INT6_S        21
#define NVIC_PRI1_INT5_S        13
#define NVIC_PRI1_INT4_S        5






#define NVIC_PRI2_INT11_M       0xE0000000  
#define NVIC_PRI2_INT10_M       0x00E00000  
#define NVIC_PRI2_INT9_M        0x0000E000  
#define NVIC_PRI2_INT8_M        0x000000E0  
#define NVIC_PRI2_INT11_S       29
#define NVIC_PRI2_INT10_S       21
#define NVIC_PRI2_INT9_S        13
#define NVIC_PRI2_INT8_S        5






#define NVIC_PRI3_INT15_M       0xE0000000  
#define NVIC_PRI3_INT14_M       0x00E00000  
#define NVIC_PRI3_INT13_M       0x0000E000  
#define NVIC_PRI3_INT12_M       0x000000E0  
#define NVIC_PRI3_INT15_S       29
#define NVIC_PRI3_INT14_S       21
#define NVIC_PRI3_INT13_S       13
#define NVIC_PRI3_INT12_S       5






#define NVIC_PRI4_INT19_M       0xE0000000  
#define NVIC_PRI4_INT18_M       0x00E00000  
#define NVIC_PRI4_INT17_M       0x0000E000  
#define NVIC_PRI4_INT16_M       0x000000E0  
#define NVIC_PRI4_INT19_S       29
#define NVIC_PRI4_INT18_S       21
#define NVIC_PRI4_INT17_S       13
#define NVIC_PRI4_INT16_S       5






#define NVIC_PRI5_INT23_M       0xE0000000  
#define NVIC_PRI5_INT22_M       0x00E00000  
#define NVIC_PRI5_INT21_M       0x0000E000  
#define NVIC_PRI5_INT20_M       0x000000E0  
#define NVIC_PRI5_INT23_S       29
#define NVIC_PRI5_INT22_S       21
#define NVIC_PRI5_INT21_S       13
#define NVIC_PRI5_INT20_S       5






#define NVIC_PRI6_INT27_M       0xE0000000  
#define NVIC_PRI6_INT26_M       0x00E00000  
#define NVIC_PRI6_INT25_M       0x0000E000  
#define NVIC_PRI6_INT24_M       0x000000E0  
#define NVIC_PRI6_INT27_S       29
#define NVIC_PRI6_INT26_S       21
#define NVIC_PRI6_INT25_S       13
#define NVIC_PRI6_INT24_S       5






#define NVIC_PRI7_INT31_M       0xE0000000  
#define NVIC_PRI7_INT30_M       0x00E00000  
#define NVIC_PRI7_INT29_M       0x0000E000  
#define NVIC_PRI7_INT28_M       0x000000E0  
#define NVIC_PRI7_INT31_S       29
#define NVIC_PRI7_INT30_S       21
#define NVIC_PRI7_INT29_S       13
#define NVIC_PRI7_INT28_S       5






#define NVIC_PRI8_INT35_M       0xE0000000  
#define NVIC_PRI8_INT34_M       0x00E00000  
#define NVIC_PRI8_INT33_M       0x0000E000  
#define NVIC_PRI8_INT32_M       0x000000E0  
#define NVIC_PRI8_INT35_S       29
#define NVIC_PRI8_INT34_S       21
#define NVIC_PRI8_INT33_S       13
#define NVIC_PRI8_INT32_S       5






#define NVIC_PRI9_INT39_M       0xE0000000  
#define NVIC_PRI9_INT38_M       0x00E00000  
#define NVIC_PRI9_INT37_M       0x0000E000  
#define NVIC_PRI9_INT36_M       0x000000E0  
#define NVIC_PRI9_INT39_S       29
#define NVIC_PRI9_INT38_S       21
#define NVIC_PRI9_INT37_S       13
#define NVIC_PRI9_INT36_S       5






#define NVIC_PRI10_INT43_M      0xE0000000  
#define NVIC_PRI10_INT42_M      0x00E00000  
#define NVIC_PRI10_INT41_M      0x0000E000  
#define NVIC_PRI10_INT40_M      0x000000E0  
#define NVIC_PRI10_INT43_S      29
#define NVIC_PRI10_INT42_S      21
#define NVIC_PRI10_INT41_S      13
#define NVIC_PRI10_INT40_S      5






#define NVIC_PRI11_INT47_M      0xE0000000  
#define NVIC_PRI11_INT46_M      0x00E00000  
#define NVIC_PRI11_INT45_M      0x0000E000  
#define NVIC_PRI11_INT44_M      0x000000E0  
#define NVIC_PRI11_INT47_S      29
#define NVIC_PRI11_INT46_S      21
#define NVIC_PRI11_INT45_S      13
#define NVIC_PRI11_INT44_S      5






#define NVIC_PRI12_INT51_M      0xE0000000  
#define NVIC_PRI12_INT50_M      0x00E00000  
#define NVIC_PRI12_INT49_M      0x0000E000  
#define NVIC_PRI12_INT48_M      0x000000E0  
#define NVIC_PRI12_INT51_S      29
#define NVIC_PRI12_INT50_S      21
#define NVIC_PRI12_INT49_S      13
#define NVIC_PRI12_INT48_S      5






#define NVIC_PRI13_INT55_M      0xE0000000  
#define NVIC_PRI13_INT54_M      0x00E00000  
#define NVIC_PRI13_INT53_M      0x0000E000  
#define NVIC_PRI13_INT52_M      0x000000E0  
#define NVIC_PRI13_INT55_S      29
#define NVIC_PRI13_INT54_S      21
#define NVIC_PRI13_INT53_S      13
#define NVIC_PRI13_INT52_S      5







#define NVIC_PRI14_INTD_M       0xE0000000  
#define NVIC_PRI14_INTC_M       0x00E00000  
#define NVIC_PRI14_INTB_M       0x0000E000  
#define NVIC_PRI14_INTA_M       0x000000E0  
#define NVIC_PRI14_INTD_S       29
#define NVIC_PRI14_INTC_S       21
#define NVIC_PRI14_INTB_S       13
#define NVIC_PRI14_INTA_S       5






#define NVIC_PRI15_INTD_M       0xE0000000  
#define NVIC_PRI15_INTC_M       0x00E00000  
#define NVIC_PRI15_INTB_M       0x0000E000  
#define NVIC_PRI15_INTA_M       0x000000E0  
#define NVIC_PRI15_INTD_S       29
#define NVIC_PRI15_INTC_S       21
#define NVIC_PRI15_INTB_S       13
#define NVIC_PRI15_INTA_S       5






#define NVIC_PRI16_INTD_M       0xE0000000  
#define NVIC_PRI16_INTC_M       0x00E00000  
#define NVIC_PRI16_INTB_M       0x0000E000  
#define NVIC_PRI16_INTA_M       0x000000E0  
#define NVIC_PRI16_INTD_S       29
#define NVIC_PRI16_INTC_S       21
#define NVIC_PRI16_INTB_S       13
#define NVIC_PRI16_INTA_S       5






#define NVIC_PRI17_INTD_M       0xE0000000  
#define NVIC_PRI17_INTC_M       0x00E00000  
#define NVIC_PRI17_INTB_M       0x0000E000  
#define NVIC_PRI17_INTA_M       0x000000E0  
#define NVIC_PRI17_INTD_S       29
#define NVIC_PRI17_INTC_S       21
#define NVIC_PRI17_INTB_S       13
#define NVIC_PRI17_INTA_S       5






#define NVIC_PRI18_INTD_M       0xE0000000  
#define NVIC_PRI18_INTC_M       0x00E00000  
#define NVIC_PRI18_INTB_M       0x0000E000  
#define NVIC_PRI18_INTA_M       0x000000E0  
#define NVIC_PRI18_INTD_S       29
#define NVIC_PRI18_INTC_S       21
#define NVIC_PRI18_INTB_S       13
#define NVIC_PRI18_INTA_S       5






#define NVIC_PRI19_INTD_M       0xE0000000  
#define NVIC_PRI19_INTC_M       0x00E00000  
#define NVIC_PRI19_INTB_M       0x0000E000  
#define NVIC_PRI19_INTA_M       0x000000E0  
#define NVIC_PRI19_INTD_S       29
#define NVIC_PRI19_INTC_S       21
#define NVIC_PRI19_INTB_S       13
#define NVIC_PRI19_INTA_S       5






#define NVIC_PRI20_INTD_M       0xE0000000  
#define NVIC_PRI20_INTC_M       0x00E00000  
#define NVIC_PRI20_INTB_M       0x0000E000  
#define NVIC_PRI20_INTA_M       0x000000E0  
#define NVIC_PRI20_INTD_S       29
#define NVIC_PRI20_INTC_S       21
#define NVIC_PRI20_INTB_S       13
#define NVIC_PRI20_INTA_S       5






#define NVIC_PRI21_INTD_M       0xE0000000  
#define NVIC_PRI21_INTC_M       0x00E00000  
#define NVIC_PRI21_INTB_M       0x0000E000  
#define NVIC_PRI21_INTA_M       0x000000E0  
#define NVIC_PRI21_INTD_S       29
#define NVIC_PRI21_INTC_S       21
#define NVIC_PRI21_INTB_S       13
#define NVIC_PRI21_INTA_S       5






#define NVIC_PRI22_INTD_M       0xE0000000  
#define NVIC_PRI22_INTC_M       0x00E00000  
#define NVIC_PRI22_INTB_M       0x0000E000  
#define NVIC_PRI22_INTA_M       0x000000E0  
#define NVIC_PRI22_INTD_S       29
#define NVIC_PRI22_INTC_S       21
#define NVIC_PRI22_INTB_S       13
#define NVIC_PRI22_INTA_S       5






#define NVIC_PRI23_INTD_M       0xE0000000  
#define NVIC_PRI23_INTC_M       0x00E00000  
#define NVIC_PRI23_INTB_M       0x0000E000  
#define NVIC_PRI23_INTA_M       0x000000E0  
#define NVIC_PRI23_INTD_S       29
#define NVIC_PRI23_INTC_S       21
#define NVIC_PRI23_INTB_S       13
#define NVIC_PRI23_INTA_S       5






#define NVIC_PRI24_INTD_M       0xE0000000  
#define NVIC_PRI24_INTC_M       0x00E00000  
#define NVIC_PRI24_INTB_M       0x0000E000  
#define NVIC_PRI24_INTA_M       0x000000E0  
#define NVIC_PRI24_INTD_S       29
#define NVIC_PRI24_INTC_S       21
#define NVIC_PRI24_INTB_S       13
#define NVIC_PRI24_INTA_S       5






#define NVIC_PRI25_INTD_M       0xE0000000  
#define NVIC_PRI25_INTC_M       0x00E00000  
#define NVIC_PRI25_INTB_M       0x0000E000  
#define NVIC_PRI25_INTA_M       0x000000E0  
#define NVIC_PRI25_INTD_S       29
#define NVIC_PRI25_INTC_S       21
#define NVIC_PRI25_INTB_S       13
#define NVIC_PRI25_INTA_S       5






#define NVIC_PRI26_INTD_M       0xE0000000  
#define NVIC_PRI26_INTC_M       0x00E00000  
#define NVIC_PRI26_INTB_M       0x0000E000  
#define NVIC_PRI26_INTA_M       0x000000E0  
#define NVIC_PRI26_INTD_S       29
#define NVIC_PRI26_INTC_S       21
#define NVIC_PRI26_INTB_S       13
#define NVIC_PRI26_INTA_S       5






#define NVIC_PRI27_INTD_M       0xE0000000  
#define NVIC_PRI27_INTC_M       0x00E00000  
#define NVIC_PRI27_INTB_M       0x0000E000  
#define NVIC_PRI27_INTA_M       0x000000E0  
#define NVIC_PRI27_INTD_S       29
#define NVIC_PRI27_INTC_S       21
#define NVIC_PRI27_INTB_S       13
#define NVIC_PRI27_INTA_S       5






#define NVIC_PRI28_INTD_M       0xE0000000  
#define NVIC_PRI28_INTC_M       0x00E00000  
#define NVIC_PRI28_INTB_M       0x0000E000  
#define NVIC_PRI28_INTA_M       0x000000E0  
#define NVIC_PRI28_INTD_S       29
#define NVIC_PRI28_INTC_S       21
#define NVIC_PRI28_INTB_S       13
#define NVIC_PRI28_INTA_S       5






#define NVIC_PRI29_INTD_M       0xE0000000  
#define NVIC_PRI29_INTC_M       0x00E00000  
#define NVIC_PRI29_INTB_M       0x0000E000  
#define NVIC_PRI29_INTA_M       0x000000E0  
#define NVIC_PRI29_INTD_S       29
#define NVIC_PRI29_INTC_S       21
#define NVIC_PRI29_INTB_S       13
#define NVIC_PRI29_INTA_S       5






#define NVIC_PRI30_INTD_M       0xE0000000  
#define NVIC_PRI30_INTC_M       0x00E00000  
#define NVIC_PRI30_INTB_M       0x0000E000  
#define NVIC_PRI30_INTA_M       0x000000E0  
#define NVIC_PRI30_INTD_S       29
#define NVIC_PRI30_INTC_S       21
#define NVIC_PRI30_INTB_S       13
#define NVIC_PRI30_INTA_S       5






#define NVIC_PRI31_INTD_M       0xE0000000  
#define NVIC_PRI31_INTC_M       0x00E00000  
#define NVIC_PRI31_INTB_M       0x0000E000  
#define NVIC_PRI31_INTA_M       0x000000E0  
#define NVIC_PRI31_INTD_S       29
#define NVIC_PRI31_INTC_S       21
#define NVIC_PRI31_INTB_S       13
#define NVIC_PRI31_INTA_S       5






#define NVIC_PRI32_INTD_M       0xE0000000  
#define NVIC_PRI32_INTC_M       0x00E00000  
#define NVIC_PRI32_INTB_M       0x0000E000  
#define NVIC_PRI32_INTA_M       0x000000E0  
#define NVIC_PRI32_INTD_S       29
#define NVIC_PRI32_INTC_S       21
#define NVIC_PRI32_INTB_S       13
#define NVIC_PRI32_INTA_S       5







#define NVIC_CPUID_IMP_M        0xFF000000  
#define NVIC_CPUID_IMP_ARM      0x41000000  
#define NVIC_CPUID_VAR_M        0x00F00000  
#define NVIC_CPUID_CON_M        0x000F0000  
#define NVIC_CPUID_PARTNO_M     0x0000FFF0  
#define NVIC_CPUID_PARTNO_CM3   0x0000C230  

#define NVIC_CPUID_PARTNO_CM4   0x0000C240  

#define NVIC_CPUID_REV_M        0x0000000F  






#define NVIC_INT_CTRL_NMI_SET   0x80000000  
#define NVIC_INT_CTRL_PEND_SV   0x10000000  
#define NVIC_INT_CTRL_UNPEND_SV 0x08000000  
#define NVIC_INT_CTRL_PENDSTSET 0x04000000  
#define NVIC_INT_CTRL_PENDSTCLR 0x02000000  
#define NVIC_INT_CTRL_ISR_PRE   0x00800000  
#define NVIC_INT_CTRL_ISR_PEND  0x00400000  
#define NVIC_INT_CTRL_VEC_PEN_M 0x0007F000  

#undef NVIC_INT_CTRL_VEC_PEN_M
#define NVIC_INT_CTRL_VEC_PEN_M 0x000FF000  

#define NVIC_INT_CTRL_VEC_PEN_NMI \
                                0x00002000  
#define NVIC_INT_CTRL_VEC_PEN_HARD \
                                0x00003000  
#define NVIC_INT_CTRL_VEC_PEN_MEM \
                                0x00004000  
#define NVIC_INT_CTRL_VEC_PEN_BUS \
                                0x00005000  
#define NVIC_INT_CTRL_VEC_PEN_USG \
                                0x00006000  
#define NVIC_INT_CTRL_VEC_PEN_SVC \
                                0x0000B000  
#define NVIC_INT_CTRL_VEC_PEN_PNDSV \
                                0x0000E000  
#define NVIC_INT_CTRL_VEC_PEN_TICK \
                                0x0000F000  
#define NVIC_INT_CTRL_RET_BASE  0x00000800  
#define NVIC_INT_CTRL_VEC_ACT_M 0x0000007F  

#undef NVIC_INT_CTRL_VEC_ACT_M
#define NVIC_INT_CTRL_VEC_ACT_M 0x000000FF  

#define NVIC_INT_CTRL_VEC_PEN_S 12
#define NVIC_INT_CTRL_VEC_ACT_S 0






#define NVIC_VTABLE_BASE        0x20000000  
#define NVIC_VTABLE_OFFSET_M    0x1FFFFE00  

#undef NVIC_VTABLE_OFFSET_M
#define NVIC_VTABLE_OFFSET_M    0x1FFFFC00  

#define NVIC_VTABLE_OFFSET_S    9

#undef NVIC_VTABLE_OFFSET_S
#define NVIC_VTABLE_OFFSET_S    10







#define NVIC_APINT_VECTKEY_M    0xFFFF0000  
#define NVIC_APINT_VECTKEY      0x05FA0000  
#define NVIC_APINT_ENDIANESS    0x00008000  
#define NVIC_APINT_PRIGROUP_M   0x00000700  
#define NVIC_APINT_PRIGROUP_7_1 0x00000000  
#define NVIC_APINT_PRIGROUP_6_2 0x00000100  
#define NVIC_APINT_PRIGROUP_5_3 0x00000200  
#define NVIC_APINT_PRIGROUP_4_4 0x00000300  
#define NVIC_APINT_PRIGROUP_3_5 0x00000400  
#define NVIC_APINT_PRIGROUP_2_6 0x00000500  
#define NVIC_APINT_PRIGROUP_1_7 0x00000600  
#define NVIC_APINT_PRIGROUP_0_8 0x00000700  
#define NVIC_APINT_SYSRESETREQ  0x00000004  
#define NVIC_APINT_VECT_CLR_ACT 0x00000002  
#define NVIC_APINT_VECT_RESET   0x00000001  






#define NVIC_SYS_CTRL_SEVONPEND 0x00000010  
#define NVIC_SYS_CTRL_SLEEPDEEP 0x00000004  
#define NVIC_SYS_CTRL_SLEEPEXIT 0x00000002  






#define NVIC_CFG_CTRL_STKALIGN  0x00000200  
                                            
#define NVIC_CFG_CTRL_BFHFNMIGN 0x00000100  
                                            
#define NVIC_CFG_CTRL_DIV0      0x00000010  
#define NVIC_CFG_CTRL_UNALIGNED 0x00000008  
#define NVIC_CFG_CTRL_MAIN_PEND 0x00000002  
#define NVIC_CFG_CTRL_BASE_THR  0x00000001  






#define NVIC_SYS_PRI1_USAGE_M   0x00E00000  
#define NVIC_SYS_PRI1_BUS_M     0x0000E000  
#define NVIC_SYS_PRI1_MEM_M     0x000000E0  
#define NVIC_SYS_PRI1_USAGE_S   21
#define NVIC_SYS_PRI1_BUS_S     13
#define NVIC_SYS_PRI1_MEM_S     5






#define NVIC_SYS_PRI2_SVC_M     0xE0000000  
#define NVIC_SYS_PRI2_SVC_S     29






#define NVIC_SYS_PRI3_TICK_M    0xE0000000  
#define NVIC_SYS_PRI3_PENDSV_M  0x00E00000  
#define NVIC_SYS_PRI3_DEBUG_M   0x000000E0  
#define NVIC_SYS_PRI3_TICK_S    29
#define NVIC_SYS_PRI3_PENDSV_S  21
#define NVIC_SYS_PRI3_DEBUG_S   5







#define NVIC_SYS_HND_CTRL_USAGE 0x00040000  
#define NVIC_SYS_HND_CTRL_BUS   0x00020000  
#define NVIC_SYS_HND_CTRL_MEM   0x00010000  
#define NVIC_SYS_HND_CTRL_SVC   0x00008000  
#define NVIC_SYS_HND_CTRL_BUSP  0x00004000  
#define NVIC_SYS_HND_CTRL_MEMP  0x00002000  
#define NVIC_SYS_HND_CTRL_USAGEP \
                                0x00001000  
#define NVIC_SYS_HND_CTRL_TICK  0x00000800  
#define NVIC_SYS_HND_CTRL_PNDSV 0x00000400  
#define NVIC_SYS_HND_CTRL_MON   0x00000100  
#define NVIC_SYS_HND_CTRL_SVCA  0x00000080  
#define NVIC_SYS_HND_CTRL_USGA  0x00000008  
#define NVIC_SYS_HND_CTRL_BUSA  0x00000002  
#define NVIC_SYS_HND_CTRL_MEMA  0x00000001  







#define NVIC_FAULT_STAT_DIV0    0x02000000  
#define NVIC_FAULT_STAT_UNALIGN 0x01000000  
#define NVIC_FAULT_STAT_NOCP    0x00080000  
#define NVIC_FAULT_STAT_INVPC   0x00040000  
#define NVIC_FAULT_STAT_INVSTAT 0x00020000  
#define NVIC_FAULT_STAT_UNDEF   0x00010000  
                                            
#define NVIC_FAULT_STAT_BFARV   0x00008000  

#define NVIC_FAULT_STAT_BLSPERR 0x00002000  
                                            

#define NVIC_FAULT_STAT_BSTKE   0x00001000  
#define NVIC_FAULT_STAT_BUSTKE  0x00000800  
#define NVIC_FAULT_STAT_IMPRE   0x00000400  
#define NVIC_FAULT_STAT_PRECISE 0x00000200  
#define NVIC_FAULT_STAT_IBUS    0x00000100  
#define NVIC_FAULT_STAT_MMARV   0x00000080  
                                            

#define NVIC_FAULT_STAT_MLSPERR 0x00000020  
                                            
                                            

#define NVIC_FAULT_STAT_MSTKE   0x00000010  
#define NVIC_FAULT_STAT_MUSTKE  0x00000008  
#define NVIC_FAULT_STAT_DERR    0x00000002  
#define NVIC_FAULT_STAT_IERR    0x00000001  







#define NVIC_HFAULT_STAT_DBG    0x80000000  
#define NVIC_HFAULT_STAT_FORCED 0x40000000  
#define NVIC_HFAULT_STAT_VECT   0x00000002  







#define NVIC_DEBUG_STAT_EXTRNL  0x00000010  
#define NVIC_DEBUG_STAT_VCATCH  0x00000008  
#define NVIC_DEBUG_STAT_DWTTRAP 0x00000004  
#define NVIC_DEBUG_STAT_BKPT    0x00000002  
#define NVIC_DEBUG_STAT_HALTED  0x00000001  






#define NVIC_MM_ADDR_M          0xFFFFFFFF  
#define NVIC_MM_ADDR_S          0







#define NVIC_FAULT_ADDR_M       0xFFFFFFFF  
#define NVIC_FAULT_ADDR_S       0






#define NVIC_MPU_TYPE_IREGION_M 0x00FF0000  
#define NVIC_MPU_TYPE_DREGION_M 0x0000FF00  
#define NVIC_MPU_TYPE_SEPARATE  0x00000001  
#define NVIC_MPU_TYPE_IREGION_S 16
#define NVIC_MPU_TYPE_DREGION_S 8






#define NVIC_MPU_CTRL_PRIVDEFEN 0x00000004  
#define NVIC_MPU_CTRL_HFNMIENA  0x00000002  
#define NVIC_MPU_CTRL_ENABLE    0x00000001  







#define NVIC_MPU_NUMBER_M       0x00000007  
#define NVIC_MPU_NUMBER_S       0






#define NVIC_MPU_BASE_ADDR_M    0xFFFFFFE0  
#define NVIC_MPU_BASE_VALID     0x00000010  
#define NVIC_MPU_BASE_REGION_M  0x00000007  
#define NVIC_MPU_BASE_ADDR_S    5
#define NVIC_MPU_BASE_REGION_S  0






#define NVIC_MPU_ATTR_M         0xFFFF0000  
#define NVIC_MPU_ATTR_XN        0x10000000  
#define NVIC_MPU_ATTR_AP_M      0x07000000  
#define NVIC_MPU_ATTR_AP_NO_NO  0x00000000  
#define NVIC_MPU_ATTR_AP_RW_NO  0x01000000  
#define NVIC_MPU_ATTR_AP_RW_RO  0x02000000  
#define NVIC_MPU_ATTR_AP_RW_RW  0x03000000  
#define NVIC_MPU_ATTR_AP_RO_NO  0x05000000  
#define NVIC_MPU_ATTR_AP_RO_RO  0x06000000  
#define NVIC_MPU_ATTR_TEX_M     0x00380000  
#define NVIC_MPU_ATTR_SHAREABLE 0x00040000  
#define NVIC_MPU_ATTR_CACHEABLE 0x00020000  
#define NVIC_MPU_ATTR_BUFFRABLE 0x00010000  
#define NVIC_MPU_ATTR_SRD_M     0x0000FF00  
#define NVIC_MPU_ATTR_SRD_0     0x00000100  
#define NVIC_MPU_ATTR_SRD_1     0x00000200  
#define NVIC_MPU_ATTR_SRD_2     0x00000400  
#define NVIC_MPU_ATTR_SRD_3     0x00000800  
#define NVIC_MPU_ATTR_SRD_4     0x00001000  
#define NVIC_MPU_ATTR_SRD_5     0x00002000  
#define NVIC_MPU_ATTR_SRD_6     0x00004000  
#define NVIC_MPU_ATTR_SRD_7     0x00008000  
#define NVIC_MPU_ATTR_SIZE_M    0x0000003E  
#define NVIC_MPU_ATTR_SIZE_32B  0x00000008  
#define NVIC_MPU_ATTR_SIZE_64B  0x0000000A  
#define NVIC_MPU_ATTR_SIZE_128B 0x0000000C  
#define NVIC_MPU_ATTR_SIZE_256B 0x0000000E  
#define NVIC_MPU_ATTR_SIZE_512B 0x00000010  
#define NVIC_MPU_ATTR_SIZE_1K   0x00000012  
#define NVIC_MPU_ATTR_SIZE_2K   0x00000014  
#define NVIC_MPU_ATTR_SIZE_4K   0x00000016  
#define NVIC_MPU_ATTR_SIZE_8K   0x00000018  
#define NVIC_MPU_ATTR_SIZE_16K  0x0000001A  
#define NVIC_MPU_ATTR_SIZE_32K  0x0000001C  
#define NVIC_MPU_ATTR_SIZE_64K  0x0000001E  
#define NVIC_MPU_ATTR_SIZE_128K 0x00000020  
#define NVIC_MPU_ATTR_SIZE_256K 0x00000022  
#define NVIC_MPU_ATTR_SIZE_512K 0x00000024  
#define NVIC_MPU_ATTR_SIZE_1M   0x00000026  
#define NVIC_MPU_ATTR_SIZE_2M   0x00000028  
#define NVIC_MPU_ATTR_SIZE_4M   0x0000002A  
#define NVIC_MPU_ATTR_SIZE_8M   0x0000002C  
#define NVIC_MPU_ATTR_SIZE_16M  0x0000002E  
#define NVIC_MPU_ATTR_SIZE_32M  0x00000030  
#define NVIC_MPU_ATTR_SIZE_64M  0x00000032  
#define NVIC_MPU_ATTR_SIZE_128M 0x00000034  
#define NVIC_MPU_ATTR_SIZE_256M 0x00000036  
#define NVIC_MPU_ATTR_SIZE_512M 0x00000038  
#define NVIC_MPU_ATTR_SIZE_1G   0x0000003A  
#define NVIC_MPU_ATTR_SIZE_2G   0x0000003C  
#define NVIC_MPU_ATTR_SIZE_4G   0x0000003E  
#define NVIC_MPU_ATTR_ENABLE    0x00000001  






#define NVIC_MPU_BASE1_ADDR_M   0xFFFFFFE0  
#define NVIC_MPU_BASE1_VALID    0x00000010  
#define NVIC_MPU_BASE1_REGION_M 0x00000007  
#define NVIC_MPU_BASE1_ADDR_S   5
#define NVIC_MPU_BASE1_REGION_S 0






#define NVIC_MPU_ATTR1_XN       0x10000000  
#define NVIC_MPU_ATTR1_AP_M     0x07000000  
#define NVIC_MPU_ATTR1_TEX_M    0x00380000  
#define NVIC_MPU_ATTR1_SHAREABLE \
                                0x00040000  
#define NVIC_MPU_ATTR1_CACHEABLE \
                                0x00020000  
#define NVIC_MPU_ATTR1_BUFFRABLE \
                                0x00010000  
#define NVIC_MPU_ATTR1_SRD_M    0x0000FF00  
#define NVIC_MPU_ATTR1_SIZE_M   0x0000003E  
#define NVIC_MPU_ATTR1_ENABLE   0x00000001  






#define NVIC_MPU_BASE2_ADDR_M   0xFFFFFFE0  
#define NVIC_MPU_BASE2_VALID    0x00000010  
#define NVIC_MPU_BASE2_REGION_M 0x00000007  
#define NVIC_MPU_BASE2_ADDR_S   5
#define NVIC_MPU_BASE2_REGION_S 0






#define NVIC_MPU_ATTR2_XN       0x10000000  
#define NVIC_MPU_ATTR2_AP_M     0x07000000  
#define NVIC_MPU_ATTR2_TEX_M    0x00380000  
#define NVIC_MPU_ATTR2_SHAREABLE \
                                0x00040000  
#define NVIC_MPU_ATTR2_CACHEABLE \
                                0x00020000  
#define NVIC_MPU_ATTR2_BUFFRABLE \
                                0x00010000  
#define NVIC_MPU_ATTR2_SRD_M    0x0000FF00  
#define NVIC_MPU_ATTR2_SIZE_M   0x0000003E  
#define NVIC_MPU_ATTR2_ENABLE   0x00000001  






#define NVIC_MPU_BASE3_ADDR_M   0xFFFFFFE0  
#define NVIC_MPU_BASE3_VALID    0x00000010  
#define NVIC_MPU_BASE3_REGION_M 0x00000007  
#define NVIC_MPU_BASE3_ADDR_S   5
#define NVIC_MPU_BASE3_REGION_S 0






#define NVIC_MPU_ATTR3_XN       0x10000000  
#define NVIC_MPU_ATTR3_AP_M     0x07000000  
#define NVIC_MPU_ATTR3_TEX_M    0x00380000  
#define NVIC_MPU_ATTR3_SHAREABLE \
                                0x00040000  
#define NVIC_MPU_ATTR3_CACHEABLE \
                                0x00020000  
#define NVIC_MPU_ATTR3_BUFFRABLE \
                                0x00010000  
#define NVIC_MPU_ATTR3_SRD_M    0x0000FF00  
#define NVIC_MPU_ATTR3_SIZE_M   0x0000003E  
#define NVIC_MPU_ATTR3_ENABLE   0x00000001  






#define NVIC_DBG_CTRL_DBGKEY_M  0xFFFF0000  
#define NVIC_DBG_CTRL_DBGKEY    0xA05F0000  
#define NVIC_DBG_CTRL_S_RESET_ST \
                                0x02000000  
#define NVIC_DBG_CTRL_S_RETIRE_ST \
                                0x01000000  
                                            
#define NVIC_DBG_CTRL_S_LOCKUP  0x00080000  
#define NVIC_DBG_CTRL_S_SLEEP   0x00040000  
#define NVIC_DBG_CTRL_S_HALT    0x00020000  
#define NVIC_DBG_CTRL_S_REGRDY  0x00010000  
#define NVIC_DBG_CTRL_C_SNAPSTALL \
                                0x00000020  
#define NVIC_DBG_CTRL_C_MASKINT 0x00000008  
#define NVIC_DBG_CTRL_C_STEP    0x00000004  
#define NVIC_DBG_CTRL_C_HALT    0x00000002  
#define NVIC_DBG_CTRL_C_DEBUGEN 0x00000001  






#define NVIC_DBG_XFER_REG_WNR   0x00010000  
#define NVIC_DBG_XFER_REG_SEL_M 0x0000001F  
#define NVIC_DBG_XFER_REG_R0    0x00000000  
#define NVIC_DBG_XFER_REG_R1    0x00000001  
#define NVIC_DBG_XFER_REG_R2    0x00000002  
#define NVIC_DBG_XFER_REG_R3    0x00000003  
#define NVIC_DBG_XFER_REG_R4    0x00000004  
#define NVIC_DBG_XFER_REG_R5    0x00000005  
#define NVIC_DBG_XFER_REG_R6    0x00000006  
#define NVIC_DBG_XFER_REG_R7    0x00000007  
#define NVIC_DBG_XFER_REG_R8    0x00000008  
#define NVIC_DBG_XFER_REG_R9    0x00000009  
#define NVIC_DBG_XFER_REG_R10   0x0000000A  
#define NVIC_DBG_XFER_REG_R11   0x0000000B  
#define NVIC_DBG_XFER_REG_R12   0x0000000C  
#define NVIC_DBG_XFER_REG_R13   0x0000000D  
#define NVIC_DBG_XFER_REG_R14   0x0000000E  
#define NVIC_DBG_XFER_REG_R15   0x0000000F  
#define NVIC_DBG_XFER_REG_FLAGS 0x00000010  
#define NVIC_DBG_XFER_REG_MSP   0x00000011  
#define NVIC_DBG_XFER_REG_PSP   0x00000012  
#define NVIC_DBG_XFER_REG_DSP   0x00000013  
#define NVIC_DBG_XFER_REG_CFBP  0x00000014  






#define NVIC_DBG_DATA_M         0xFFFFFFFF  
#define NVIC_DBG_DATA_S         0






#define NVIC_DBG_INT_HARDERR    0x00000400  
#define NVIC_DBG_INT_INTERR     0x00000200  
#define NVIC_DBG_INT_BUSERR     0x00000100  
#define NVIC_DBG_INT_STATERR    0x00000080  
#define NVIC_DBG_INT_CHKERR     0x00000040  
#define NVIC_DBG_INT_NOCPERR    0x00000020  
#define NVIC_DBG_INT_MMERR      0x00000010  
#define NVIC_DBG_INT_RESET      0x00000008  
#define NVIC_DBG_INT_RSTPENDCLR 0x00000004  
#define NVIC_DBG_INT_RSTPENDING 0x00000002  
#define NVIC_DBG_INT_RSTVCATCH  0x00000001  






#define NVIC_SW_TRIG_INTID_M    0x0000003F  

#undef NVIC_SW_TRIG_INTID_M
#define NVIC_SW_TRIG_INTID_M    0x000000FF  

#define NVIC_SW_TRIG_INTID_S    0

#endif 
