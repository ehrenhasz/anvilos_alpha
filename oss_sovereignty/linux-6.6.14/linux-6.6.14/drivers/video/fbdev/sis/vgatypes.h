#ifndef _VGATYPES_H_
#define _VGATYPES_H_
#define SISIOMEMTYPE
typedef unsigned long SISIOADDRESS;
#include <linux/types.h>   
#undef SISIOMEMTYPE
#define SISIOMEMTYPE __iomem
typedef enum _SIS_CHIP_TYPE {
    SIS_VGALegacy = 0,
    SIS_530,
    SIS_OLD,
    SIS_300,
    SIS_630,
    SIS_730,
    SIS_540,
    SIS_315H,    
    SIS_315,
    SIS_315PRO,  
    SIS_550,
    SIS_650,
    SIS_740,
    SIS_330,
    SIS_661,
    SIS_741,
    SIS_670,
    SIS_660 = 35,
    SIS_760,
    SIS_761,
    SIS_762,
    SIS_770,
    SIS_340 = 55,
    SIS_341,
    SIS_342,
    XGI_20  = 75,
    XGI_21,
    XGI_40,
    MAX_SIS_CHIP
} SIS_CHIP_TYPE;
#endif
