








































#ifndef __HW_INTS_H__
#define __HW_INTS_H__






#define FAULT_NMI               2           
#define FAULT_HARD              3           
#define FAULT_MPU               4           
#define FAULT_BUS               5           
#define FAULT_USAGE             6           
#define FAULT_SVCALL            11          
#define FAULT_DEBUG             12          
#define FAULT_PENDSV            14          
#define FAULT_SYSTICK           15          






#define INT_GPIOA0              16          
#define INT_GPIOA1              17          
#define INT_GPIOA2              18          
#define INT_GPIOA3              19          
#define INT_UARTA0              21          
#define INT_UARTA1              22          
#define INT_I2CA0               24          
#define INT_ADCCH0              30          
#define INT_ADCCH1              31          
#define INT_ADCCH2              32          
#define INT_ADCCH3              33          
#define INT_WDT                 34          
#define INT_TIMERA0A            35          
#define INT_TIMERA0B            36          
#define INT_TIMERA1A            37          
#define INT_TIMERA1B            38          
#define INT_TIMERA2A            39          
#define INT_TIMERA2B            40          
#define INT_FLASH               45          
#define INT_TIMERA3A            51          
#define INT_TIMERA3B            52          
#define INT_UDMA                62          
#define INT_UDMAERR             63          
#define INT_SHA                 164         
#define INT_AES                 167         
#define INT_DES                 169         
#define INT_MMCHS               175         
#define INT_I2S                 177         
#define INT_CAMERA              179         
#define INT_NWPIC               187         
#define INT_PRCM                188         
#define INT_SSPI                191         
#define INT_GSPI                192         
#define INT_LSPI                193         






#define NUM_INTERRUPTS          195 







#define NUM_PRIORITY            8
#define NUM_PRIORITY_BITS       3


#endif 
