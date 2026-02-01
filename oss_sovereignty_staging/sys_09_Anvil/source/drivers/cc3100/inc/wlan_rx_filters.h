 


 
 
 
#include "simplelink.h"

#ifndef RX_FILTERS_PREPROCESSOR_CLI_IF_H_
#define RX_FILTERS_PREPROCESSOR_CLI_IF_H_



#ifdef  __cplusplus
extern "C" {
#endif


 
 
 

 
#define SL_RX_FILTER_MAX_FILTERS 64

 
#define SL_RX_FILTER_MAX_PRE_PREPARED_FILTERS_SETS      (32)
 
#define SL_RX_FILTER_NUM_OF_FILTER_HEADER_ARGS          (2)
 
#define SL_RX_FILTER_NUM_OF_FILTER_PAYLOAD_ARGS         (2)
 
#define SL_RX_FILTER_NUM_OF_COMBINATION_TYPE_ARGS       (2)
 
#define SL_RX_FILTER_LENGTH_OF_REGX_PATTERN_LENGTH      (32)


 
#define RXFL_OK                                                (0)  
#define RXFL_OUTPUT_OR_INPUT_BUFFER_LENGTH_TOO_SMALL           (76)  
#define RXFL_DEPENDENT_FILTER_SOFTWARE_FILTER_NOT_FIT          (75)  
#define RXFL_DEPENDENCY_IS_NOT_PERSISTENT                      (74)   
#define RXFL_SYSTEM_STATE_NOT_SUPPORTED_FOR_THIS_FILTER        (72)  
#define RXFL_TRIGGER_USE_REG5_TO_REG8                          (71)  
#define RXFL_TRIGGER_USE_REG1_TO_REG4                          (70)  
#define RXFL_ACTION_USE_REG5_TO_REG8                           (69)  
#define RXFL_ACTION_USE_REG1_TO_REG4                           (68)  
#define RXFL_FIELD_SUPPORT_ONLY_EQUAL_AND_NOTEQUAL             (67)  
#define RXFL_WRONG_MULTICAST_BROADCAST_ADDRESS                 (66)  
#define RXFL_THE_FILTER_IS_NOT_OF_HEADER_TYPE                  (65)  
#define RXFL_WRONG_COMPARE_FUNC_FOR_BROADCAST_ADDRESS          (64)  
#define RXFL_WRONG_MULTICAST_ADDRESS                           (63)  
#define RXFL_DEPENDENT_FILTER_IS_NOT_PERSISTENT                (62)  
#define RXFL_DEPENDENT_FILTER_IS_NOT_ENABLED                   (61)  
#define RXFL_FILTER_HAS_CHILDS                                 (60)  
#define RXFL_CHILD_IS_ENABLED                                  (59)  
#define RXFL_DEPENDENCY_IS_DISABLED                            (58)  
#define RXFL_NUMBER_OF_CONNECTION_POINTS_EXCEEDED              (52)  
#define RXFL_DEPENDENT_FILTER_DEPENDENCY_ACTION_IS_DROP        (51)  
#define RXFL_FILTER_DO_NOT_EXISTS                              (50)  
#define RXFL_DEPEDENCY_NOT_ON_THE_SAME_LAYER                   (49)  
#define RXFL_NUMBER_OF_ARGS_EXCEEDED                           (48)  
#define RXFL_ACTION_NO_REG_NUMBER                              (47)  
#define RXFL_DEPENDENT_FILTER_LAYER_DO_NOT_FIT                 (46)  
#define RXFL_DEPENDENT_FILTER_SYSTEM_STATE_DO_NOT_FIT          (45)  
#define RXFL_DEPENDENT_FILTER_DO_NOT_EXIST_2                   (44)  
#define RXFL_DEPENDENT_FILTER_DO_NOT_EXIST_1                   (43)  
#define RXFL_RULE_HEADER_ACTION_TYPE_NOT_SUPPORTED             (42)  
#define RXFL_RULE_HEADER_TRIGGER_COMPARE_FUNC_OUT_OF_RANGE     (41)  
#define RXFL_RULE_HEADER_TRIGGER_OUT_OF_RANGE                  (40)  
#define RXFL_RULE_HEADER_COMPARE_FUNC_OUT_OF_RANGE             (39)  
#define RXFL_FRAME_TYPE_NOT_SUPPORTED                          (38)  
#define RXFL_RULE_FIELD_ID_NOT_SUPPORTED                       (37)  
#define RXFL_RULE_HEADER_FIELD_ID_ASCII_NOT_SUPPORTED          (36)  
#define RXFL_RULE_HEADER_NOT_SUPPORTED                         (35)  
#define RXFL_RULE_HEADER_OUT_OF_RANGE                          (34)  
#define RXFL_RULE_HEADER_COMBINATION_OPERATOR_OUT_OF_RANGE     (33)  
#define RXFL_RULE_HEADER_FIELD_ID_OUT_OF_RANGE                 (32)  
#define RXFL_UPDATE_NOT_SUPPORTED                              (31)  
#define RXFL_NO_FILTERS_ARE_DEFINED                            (24)  
#define RXFL_NUMBER_OF_FILTER_EXCEEDED                         (23)  


 
 
 

 
typedef  _i8    SlrxFilterID_t;


 
typedef _u8   SlrxFilterCompareMask_t;

 
typedef _u8   SlrxFilterIdMask_t[128/8];

 
typedef _u8  SlrxFilterPrePreparedFilters_t;
#define SL_ARP_AUTO_REPLY_PRE_PREPARED_FILTERS       (0)
#define SL_MULTICASTSIPV4_DROP_PREPREPARED_FILTERS   (1)
#define SL_MULTICASTSIPV6_DROP_PREPREPARED_FILTERS   (2)
#define SL_MULTICASTSWIFI_DROP_PREPREPARED_FILTERS   (3)



 
typedef _u8   SlrxFilterPrePreparedFiltersMask_t[SL_RX_FILTER_MAX_PRE_PREPARED_FILTERS_SETS/8];


 
typedef struct SlrxFilterRegxPattern_t
{
    _u8 x[SL_RX_FILTER_LENGTH_OF_REGX_PATTERN_LENGTH];
}SlrxFilterRegxPattern_t;


 
typedef _u8 SlrxFilterAsciiArg_t;


 
typedef _u8   SlrxFilterBinaryArg_t ;


 
typedef  _u8 SlrxFilterActionArg_t ;



 
typedef _u32   SlrxFilterOffset_t;



 
typedef _u8 SlrxFilterRuleType_t;
 
#define HEADER                    (0)
#define COMBINATION               (1)
#define EXACT_PATTERN             (2)
#define LIKELIHOOD_PATTERN        (3)
#define ALWAYS_TRUE               (4)
#define NUM_OF_FILTER_TYPES       (5)


 

#define RX_FILTER_BINARY          (0x1)
#define RX_FILTER_PERSISTENT      (0x8)
#define RX_FILTER_ENABLE          (0x10)

typedef union SlrxFilterFlags_t
{

     
         
         
         
         
         
         
         
         
         
     

    _u8 IntRepresentation;

}SlrxFilterFlags_t;

 
typedef _u8 SlrxFilterCompareFunction_t;
 
#define COMPARE_FUNC_IN_BETWEEN                 (0)
#define COMPARE_FUNC_EQUAL                      (1)
#define COMPARE_FUNC_NOT_EQUAL_TO               (2)
#define COMPARE_FUNC_NOT_IN_BETWEEN             (3)
#define COMPARE_FUNC_NUM_OF_FILTER_COMPARE_FUNC (4)

 
typedef _u8 SlrxTriggerCompareFunction_t;
 
#define TRIGGER_COMPARE_FUNC_EQUAL              (0)
 
#define TRIGGER_COMPARE_FUNC_NOT_EQUAL_TO       (1)
 
#define TRIGGER_COMPARE_FUNC_SMALLER_THAN       (2)
 
#define TRIGGER_COMPARE_FUNC_BIGGER_THAN        (3)
 
#define TRIGGER_COMPARE_FUNC_NUM_OF_FILTER_COMPARE_FUNC (4)


 
typedef _u8 SlrxFilterHdrField_t;
 
#define NULL_FIELD_ID_TYPE           (0)
 
#define FRAME_TYPE_FIELD             (1)
 
#define FRAME_SUBTYPE_FIELD          (2)
  
#define BSSID_FIELD                  (3)
  
#define MAC_SRC_ADDRESS_FIELD        (4)
  
#define MAC_DST_ADDRESS_FIELD        (5)
 
#define FRAME_LENGTH_FIELD           (6)
 
#define PROTOCOL_TYPE_FIELD          (7)
  
#define IP_VERSION_FIELD             (8)
  
#define IP_PROTOCOL_FIELD            (9)
  
#define IPV4_SRC_ADRRESS_FIELD       (10)
 
#define IPV4_DST_ADDRESS_FIELD       (11)
 
#define IPV6_SRC_ADRRESS_FIELD       (12)
 
#define IPV6_DST_ADDRESS_FIELD       (13)
  
#define SRC_PORT_FIELD               (14)
  
#define DST_PORT_FIELD               (15)
  
#define NUM_OF_FIELD_NAME_FIELD      (16)

 
 
typedef union SlrxFilterHeaderArg_t
{
     
     

    SlrxFilterBinaryArg_t RxFilterDB16BytesRuleArgs[SL_RX_FILTER_NUM_OF_FILTER_HEADER_ARGS][16 ];  
     
     
     
    SlrxFilterBinaryArg_t RxFilterDB6BytesRuleArgs[SL_RX_FILTER_NUM_OF_FILTER_HEADER_ARGS][6];  
     
    SlrxFilterAsciiArg_t RxFilterDB18BytesAsciiRuleArgs[SL_RX_FILTER_NUM_OF_FILTER_HEADER_ARGS][18];  
     
     
    SlrxFilterBinaryArg_t RxFilterDB4BytesRuleArgs[SL_RX_FILTER_NUM_OF_FILTER_HEADER_ARGS][4];  
     
    SlrxFilterAsciiArg_t RxFilterDB5BytesRuleAsciiArgs[SL_RX_FILTER_NUM_OF_FILTER_HEADER_ARGS][5];  
     
     
    SlrxFilterBinaryArg_t RxFilterDB1BytesRuleArgs[SL_RX_FILTER_NUM_OF_FILTER_HEADER_ARGS][1];  
}SlrxFilterHeaderArg_t;



 
 
typedef struct SlrxFilterRuleHeaderArgsAndMask_t
{
     
     
    SlrxFilterHeaderArg_t RuleHeaderArgs;

     
     
    SlrxFilterCompareMask_t RuleHeaderArgsMask[16];

}SlrxFilterRuleHeaderArgsAndMask_t;

 
 
typedef struct SlrxFilterHeaderType_t
{
     
      
    SlrxFilterRuleHeaderArgsAndMask_t RuleHeaderArgsAndMask;

     
     
    SlrxFilterHdrField_t RuleHeaderfield;

     
     
    SlrxFilterCompareFunction_t RuleCompareFunc;

     
     
    _u8 RulePadding[2];

}SlrxFilterHeaderType_t;

 
 
typedef struct SlrxFilterPayloadType_t
{
     
     
    SlrxFilterRegxPattern_t RegxPattern;
     
     
    SlrxFilterOffset_t LowerOffset;
     
     
    SlrxFilterOffset_t UpperOffset;
}SlrxFilterPayloadType_t;

 
typedef _u8 SlrxFilterCombinationTypeOperator_t;
 
 
#define COMBINED_FUNC_NOT     (0)
 
#define COMBINED_FUNC_AND     (1)
 
#define COMBINED_FUNC_OR      (2)

 
 
typedef struct SlrxFilterCombinationType_t
{
     
     
    SlrxFilterCombinationTypeOperator_t CombinationTypeOperator;
     
     
    SlrxFilterID_t CombinationFilterId[SL_RX_FILTER_NUM_OF_COMBINATION_TYPE_ARGS];
     
     
    _u8 Padding;
}SlrxFilterCombinationType_t;


 
 
typedef union SlrxFilterRule_t
{
     
     
    SlrxFilterHeaderType_t HeaderType;
     
     
    SlrxFilterPayloadType_t PayLoadHeaderType;  
     
     
    SlrxFilterCombinationType_t CombinationType;
}SlrxFilterRule_t;

 
#define RX_FILTER_ROLE_AP                            (1)
#define RX_FILTER_ROLE_STA                           (2)
#define RX_FILTER_ROLE_PROMISCUOUS                   (4)
#define RX_FILTER_ROLE_NULL                          (0)

typedef union SlrxFilterTriggerRoles_t
{
 
 
 
 
       
 
 
 
        
    _u8 IntRepresentation;

}SlrxFilterTriggerRoles_t;

 
#define RX_FILTER_CONNECTION_STATE_STA_CONNECTED     (1)
#define RX_FILTER_CONNECTION_STATE_STA_NOT_CONNECTED (2)
#define RX_FILTER_CONNECTION_STATE_STA_HAS_IP        (4)
#define RX_FILTER_CONNECTION_STATE_STA_HAS_NO_IP     (8)

typedef union SlrxFilterTriggerConnectionStates_t
{
 
 
 
 
 
 
 
 
 
 
     
    _u8 IntRepresentation;
        
}SlrxFilterTriggerConnectionStates_t;

 
typedef _u32  SlrxFilterDBTriggerArg_t;



 
typedef _u8 SlrxFilterCounterId_t;
 
#define NO_TRIGGER                                  (0)
#define RX_FILTER_COUNTER1                          (1)
#define RX_FILTER_COUNTER2                          (2)
#define RX_FILTER_COUNTER3                          (3)
#define RX_FILTER_COUNTER4                          (4)
#define RX_FILTER_COUNTER5                          (5)
#define RX_FILTER_COUNTER6                          (6)
#define RX_FILTER_COUNTER7                          (7)
#define RX_FILTER_COUNTER8                          (8)
#define MAX_RX_FILTER_COUNTER                       (9)



 

typedef _u8  SlrxFilterActionArgs_t;
 
#define ACTION_ARG_REG_1_4                          (0)
     
#define ACTION_ARG_TEMPLATE                         (1)
     
#define ACTION_ARG_EVENT                            (2)

 
#define ACTION_ARG_GPIO                             (4)
 
#define SL_RX_FILTER_NUM_OF_BYTES_FOR_ACTIONS_ARGS  (5)




 
 
typedef struct SlrxFilterTrigger_t
{
     
     
     
    SlrxFilterID_t ParentFilterID;
     
     
    SlrxFilterCounterId_t Trigger;
     
     
    SlrxFilterTriggerConnectionStates_t TriggerArgConnectionState;
     
     
    SlrxFilterTriggerRoles_t TriggerArgRoleStatus;
     
     
    SlrxFilterDBTriggerArg_t TriggerArg;
     
     
    SlrxTriggerCompareFunction_t TriggerCompareFunction;

     
     
    _u8 Padding[3];
} SlrxFilterTrigger_t;

 
#define RX_FILTER_ACTION_NULL               (0x0)
#define RX_FILTER_ACTION_DROP               (0x1)
#define RX_FILTER_ACTION_GPIO               (0x2)
#define RX_FILTER_ACTION_ON_REG_INCREASE    (0x4)
#define RX_FILTER_ACTION_ON_REG_DECREASE    (0x8)
#define RX_FILTER_ACTION_ON_REG_RESET       (0x10)
#define RX_FILTER_ACTION_SEND_TEMPLATE      (0x20)  
#define RX_FILTER_ACTION_EVENT_TO_HOST      (0x40)  

typedef union SlrxFilterActionType_t
{
 
 
          
          
 
          
 
          
 
 
 

          
 
          
 
 
 

    _u8 IntRepresentation;

}SlrxFilterActionType_t;

 
 
typedef struct SlrxFilterAction_t
{
     
     
    SlrxFilterActionType_t ActionType;
     
     
     
    SlrxFilterActionArg_t ActionArg[SL_RX_FILTER_NUM_OF_BYTES_FOR_ACTIONS_ARGS];

     
     
    _u8 Padding[2];

} SlrxFilterAction_t;


 
 
typedef struct _WlanRxFilterOperationCommandBuff_t
{
     
    SlrxFilterIdMask_t FilterIdMask;
     
    _u8 Padding[4];
}_WlanRxFilterOperationCommandBuff_t;



 
typedef struct _WlanRxFilterUpdateArgsCommandBuff_t
{
     
    _u8  FilterId;

     
     
    _u8 BinaryRepresentation;

     
    SlrxFilterRuleHeaderArgsAndMask_t FilterRuleHeaderArgsAndMask;

     
    _u8 Padding[2];
}_WlanRxFilterUpdateArgsCommandBuff_t;


 
 
typedef struct _WlanRxFilterRetrieveEnableStatusCommandResponseBuff_t
{

     
     
    SlrxFilterIdMask_t FilterIdMask;

}_WlanRxFilterRetrieveEnableStatusCommandResponseBuff_t;


 
typedef struct _WlanRxFilterPrePreparedFiltersCommandBuff_t
{
     
     
    SlrxFilterPrePreparedFiltersMask_t  FilterPrePreparedFiltersMask;

}_WlanRxFilterPrePreparedFiltersCommandBuff_t;


 
 
typedef struct _WlanRxFilterPrePreparedFiltersCommandResponseBuff_t
{
     
     
    SlrxFilterPrePreparedFiltersMask_t  FilterPrePreparedFiltersMask;

}_WlanRxFilterPrePreparedFiltersCommandResponseBuff_t;



typedef _u8 SLrxFilterOperation_t;
#define SL_ENABLE_DISABLE_RX_FILTER                         (0)
#define SL_REMOVE_RX_FILTER                                 (1)
#define SL_STORE_RX_FILTERS                                 (2)
#define SL_UPDATE_RX_FILTER_ARGS                            (3)
#define SL_FILTER_RETRIEVE_ENABLE_STATE                     (4)
#define SL_FILTER_PRE_PREPARED_RETRIEVE_CREATE_REMOVE_STATE (5)
#define SL_FILTER_PRE_PREPARED_SET_CREATE_REMOVE_STATE      (6)


 
#define ISBITSET8(x,i) ((x[i>>3] & (0x80>>(i&7)))!=0)  
#define SETBIT8(x,i) x[i>>3]|=(0x80>>(i&7));  
#define CLEARBIT8(x,i) x[i>>3]&=(0x80>>(i&7))^0xFF;  


 
 
 

 


 
#if _SL_INCLUDE_FUNC(sl_WlanRxFilterAdd)
SlrxFilterID_t sl_WlanRxFilterAdd(    SlrxFilterRuleType_t                 RuleType,
                                    SlrxFilterFlags_t                     FilterFlags,
                                    const SlrxFilterRule_t* const         Rule,
                                    const SlrxFilterTrigger_t* const     Trigger,
                                    const SlrxFilterAction_t* const     Action,
                                    SlrxFilterID_t*                     pFilterId);

#endif





 

#if _SL_INCLUDE_FUNC(sl_WlanRxFilterSet)
_i16 sl_WlanRxFilterSet(  const SLrxFilterOperation_t RxFilterOperation,
                          const _u8*  const pInputBuffer,
                         _u16 InputbufferLength);
#endif

 

#if _SL_INCLUDE_FUNC(sl_WlanRxFilterGet)
_i16 sl_WlanRxFilterGet(const SLrxFilterOperation_t RxFilterOperation,
                        _u8*  pOutputBuffer,
                        _u16  OutputbufferLength);
#endif


 

#ifdef  __cplusplus
}
#endif  

#endif  


