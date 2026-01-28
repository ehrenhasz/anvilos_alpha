




#include "simplelink.h"

    
#ifndef __NETCFG_H__
#define __NETCFG_H__


#ifdef    __cplusplus
extern "C" {
#endif










#define SL_MAC_ADDR_LEN                          (6)
#define SL_IPV4_VAL(add_3,add_2,add_1,add_0)     ((((_u32)add_3 << 24) & 0xFF000000) | (((_u32)add_2 << 16) & 0xFF0000) | (((_u32)add_1 << 8) & 0xFF00) | ((_u32)add_0 & 0xFF) )
#define SL_IPV4_BYTE(val,index)                  ( (val >> (index*8)) & 0xFF )

#define IPCONFIG_MODE_DISABLE_IPV4               (0)
#define IPCONFIG_MODE_ENABLE_IPV4                (1)




typedef enum
{
    SL_MAC_ADDRESS_SET          = 1,
    SL_MAC_ADDRESS_GET          = 2,          
    SL_IPV4_STA_P2P_CL_GET_INFO           = 3,
    SL_IPV4_STA_P2P_CL_DHCP_ENABLE        = 4,
    SL_IPV4_STA_P2P_CL_STATIC_ENABLE      = 5,
    SL_IPV4_AP_P2P_GO_GET_INFO            = 6,
    SL_IPV4_AP_P2P_GO_STATIC_ENABLE       = 7,
    SL_SET_HOST_RX_AGGR                   = 8,
    MAX_SETTINGS = 0xFF
}Sl_NetCfg_e;


typedef struct
{
    _u32  ipV4;
    _u32  ipV4Mask;
    _u32  ipV4Gateway;
    _u32  ipV4DnsServer;
}SlNetCfgIpV4Args_t;







#if _SL_INCLUDE_FUNC(sl_NetCfgSet)
_i32 sl_NetCfgSet(const _u8 ConfigId,const _u8 ConfigOpt,const _u8 ConfigLen,const _u8 *pValues);
#endif



#if _SL_INCLUDE_FUNC(sl_NetCfgGet)
_i32 sl_NetCfgGet(const _u8 ConfigId ,_u8 *pConfigOpt, _u8 *pConfigLen, _u8 *pValues);
#endif




#ifdef  __cplusplus
}
#endif 

#endif    

