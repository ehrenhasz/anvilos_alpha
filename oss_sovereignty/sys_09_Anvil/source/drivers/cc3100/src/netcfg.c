 



 
 
 
#include "simplelink.h"
#include "protocol.h"
#include "driver.h"

 
 
 
typedef union
{
    _NetCfgSetGet_t    Cmd;
    _BasicResponse_t   Rsp;
}_SlNetCfgMsgSet_u;

#if _SL_INCLUDE_FUNC(sl_NetCfgSet)

const _SlCmdCtrl_t _SlNetCfgSetCmdCtrl =
{
    SL_OPCODE_DEVICE_NETCFG_SET_COMMAND,
    sizeof(_NetCfgSetGet_t),
    sizeof(_BasicResponse_t)
};

_i32 sl_NetCfgSet(const _u8 ConfigId ,const _u8 ConfigOpt,const _u8 ConfigLen,const _u8 *pValues)
{
    _SlNetCfgMsgSet_u         Msg;
    _SlCmdExt_t               CmdExt;


    _SlDrvResetCmdExt(&CmdExt);
    CmdExt.TxPayloadLen = (ConfigLen+3) & (~3);
    CmdExt.pTxPayload = (_u8 *)pValues;


    Msg.Cmd.ConfigId    = ConfigId;
    Msg.Cmd.ConfigLen   = ConfigLen;
    Msg.Cmd.ConfigOpt   = ConfigOpt;

    VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&_SlNetCfgSetCmdCtrl, &Msg, &CmdExt));

    return (_i16)Msg.Rsp.status;
}
#endif


 
 
 
typedef union
{
    _NetCfgSetGet_t	    Cmd;
    _NetCfgSetGet_t	    Rsp;
}_SlNetCfgMsgGet_u;

#if _SL_INCLUDE_FUNC(sl_NetCfgGet)

const _SlCmdCtrl_t _SlNetCfgGetCmdCtrl =
{
    SL_OPCODE_DEVICE_NETCFG_GET_COMMAND,
    sizeof(_NetCfgSetGet_t),
    sizeof(_NetCfgSetGet_t)
};

_i32 sl_NetCfgGet(const _u8 ConfigId, _u8 *pConfigOpt,_u8 *pConfigLen, _u8 *pValues)
{
    _SlNetCfgMsgGet_u         Msg;
    _SlCmdExt_t               CmdExt;

    if (*pConfigLen == 0)
    {
        return SL_EZEROLEN;
    }

    _SlDrvResetCmdExt(&CmdExt);
    CmdExt.RxPayloadLen = *pConfigLen;
    CmdExt.pRxPayload = (_u8 *)pValues;
    Msg.Cmd.ConfigLen    = *pConfigLen;
    Msg.Cmd.ConfigId     = ConfigId;

    if( pConfigOpt )
    {
        Msg.Cmd.ConfigOpt   = (_u16)*pConfigOpt;
    }
    VERIFY_RET_OK(_SlDrvCmdOp((_SlCmdCtrl_t *)&_SlNetCfgGetCmdCtrl, &Msg, &CmdExt));

    if( pConfigOpt )
    {
        *pConfigOpt = (_u8)Msg.Rsp.ConfigOpt;
    }
    if (CmdExt.RxPayloadLen < CmdExt.ActualRxPayloadLen) 
    {
         *pConfigLen = (_u8)CmdExt.RxPayloadLen;
         if( SL_MAC_ADDRESS_GET == ConfigId )
         {
           return SL_RET_CODE_OK;   
         }
         else
         {
           return SL_ESMALLBUF;
         }
    }
    else
    {
        *pConfigLen = (_u8)CmdExt.ActualRxPayloadLen;
    }

    return (_i16)Msg.Rsp.Status;
}
#endif

