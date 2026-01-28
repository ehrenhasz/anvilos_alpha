 


#ifndef __USBH_CDC_CORE_H
#define __USBH_CDC_CORE_H


#include "usbh_core.h"








 





#define USB_CDC_CLASS                                           0x02
#define COMMUNICATION_INTERFACE_CLASS_CODE                      0x02


#define DATA_INTERFACE_CLASS_CODE                               0x0A


#define RESERVED                                                0x00
#define DIRECT_LINE_CONTROL_MODEL                               0x01
#define ABSTRACT_CONTROL_MODEL                                  0x02
#define TELEPHONE_CONTROL_MODEL                                 0x03
#define MULTICHANNEL_CONTROL_MODEL                              0x04   
#define CAPI_CONTROL_MODEL                                      0x05
#define ETHERNET_NETWORKING_CONTROL_MODEL                       0x06
#define ATM_NETWORKING_CONTROL_MODEL                            0x07



#define NO_CLASS_SPECIFIC_PROTOCOL_CODE                         0x00
#define COMMON_AT_COMMAND                                       0x01
#define VENDOR_SPECIFIC                                         0xFF


#define CS_INTERFACE                                            0x24
#define CDC_PAGE_SIZE_64                                        0x40


#define CDC_SEND_ENCAPSULATED_COMMAND                           0x00
#define CDC_GET_ENCAPSULATED_RESPONSE                           0x01
#define CDC_SET_COMM_FEATURE                                    0x02
#define CDC_GET_COMM_FEATURE                                    0x03
#define CDC_CLEAR_COMM_FEATURE                                  0x04

#define CDC_SET_AUX_LINE_STATE                                  0x10
#define CDC_SET_HOOK_STATE                                      0x11
#define CDC_PULSE_SETUP                                         0x12
#define CDC_SEND_PULSE                                          0x13
#define CDC_SET_PULSE_TIME                                      0x14
#define CDC_RING_AUX_JACK                                       0x15

#define CDC_SET_LINE_CODING                                     0x20
#define CDC_GET_LINE_CODING                                     0x21
#define CDC_SET_CONTROL_LINE_STATE                              0x22
#define CDC_SEND_BREAK                                          0x23

#define CDC_SET_RINGER_PARMS                                    0x30
#define CDC_GET_RINGER_PARMS                                    0x31
#define CDC_SET_OPERATION_PARMS                                 0x32
#define CDC_GET_OPERATION_PARMS                                 0x33  
#define CDC_SET_LINE_PARMS                                      0x34
#define CDC_GET_LINE_PARMS                                      0x35
#define CDC_DIAL_DIGITS                                         0x36
#define CDC_SET_UNIT_PARAMETER                                  0x37  
#define CDC_GET_UNIT_PARAMETER                                  0x38
#define CDC_CLEAR_UNIT_PARAMETER                                0x39
#define CDC_GET_PROFILE                                         0x3A

#define CDC_SET_ETHERNET_MULTICAST_FILTERS                      0x40
#define CDC_SET_ETHERNET_POWER_MANAGEMENT_PATTERN FILTER        0x41
#define CDC_GET_ETHERNET_POWER_MANAGEMENT_PATTERN FILTER        0x42
#define CDC_SET_ETHERNET_PACKET_FILTER                          0x43
#define CDC_GET_ETHERNET_STATISTIC                              0x44

#define CDC_SET_ATM_DATA_FORMAT                                 0x50  
#define CDC_GET_ATM_DEVICE_STATISTICS                           0x51
#define CDC_SET_ATM_DEFAULT_VC                                  0x52
#define CDC_GET_ATM_VC_STATISTICS                               0x53



#define CDC_ACTIVATE_CARRIER_SIGNAL_RTS                         0x0002
#define CDC_DEACTIVATE_CARRIER_SIGNAL_RTS                       0x0000
#define CDC_ACTIVATE_SIGNAL_DTR                                 0x0001
#define CDC_DEACTIVATE_SIGNAL_DTR                               0x0000

#define LINE_CODING_STRUCTURE_SIZE                              0x07
 

 


typedef enum
{
  CDC_IDLE= 0,
  CDC_SEND_DATA,
  CDC_SEND_DATA_WAIT,
  CDC_RECEIVE_DATA,
  CDC_RECEIVE_DATA_WAIT,  
}
CDC_DataStateTypeDef;

typedef enum
{
  CDC_IDLE_STATE= 0,
  CDC_SET_LINE_CODING_STATE,  
  CDC_GET_LAST_LINE_CODING_STATE,    
  CDC_TRANSFER_DATA, 
  CDC_ERROR_STATE,  
}
CDC_StateTypeDef;



typedef union _CDC_LineCodingStructure
{
  uint8_t Array[LINE_CODING_STRUCTURE_SIZE];
  
  struct
  {
    
    uint32_t             dwDTERate;     
    uint8_t              bCharFormat;   
    uint8_t              bParityType;   
    uint8_t                bDataBits;     
  }b;
}
CDC_LineCodingTypeDef;




typedef struct _FunctionalDescriptorHeader
{
  uint8_t     bLength;            
  uint8_t     bDescriptorType;    
  uint8_t     bDescriptorSubType; 
  uint16_t    bcdCDC;             
}
CDC_HeaderFuncDesc_TypeDef;

typedef struct _CallMgmtFunctionalDescriptor
{
  uint8_t    bLength;            
  uint8_t    bDescriptorType;    
  uint8_t    bDescriptorSubType; 
  uint8_t    bmCapabilities;      
  uint8_t    bDataInterface;      
}
CDC_CallMgmtFuncDesc_TypeDef;

typedef struct _AbstractCntrlMgmtFunctionalDescriptor
{
  uint8_t    bLength;            
  uint8_t    bDescriptorType;    
  uint8_t    bDescriptorSubType; 
  uint8_t    bmCapabilities;      
}
CDC_AbstCntrlMgmtFuncDesc_TypeDef;

typedef struct _UnionFunctionalDescriptor
{
  uint8_t    bLength;            
  uint8_t    bDescriptorType;    
  uint8_t    bDescriptorSubType; 
  uint8_t    bMasterInterface;   
  uint8_t    bSlaveInterface0;   
}
CDC_UnionFuncDesc_TypeDef;

typedef struct _USBH_CDCInterfaceDesc
{
  CDC_HeaderFuncDesc_TypeDef           CDC_HeaderFuncDesc;
  CDC_CallMgmtFuncDesc_TypeDef         CDC_CallMgmtFuncDesc;
  CDC_AbstCntrlMgmtFuncDesc_TypeDef    CDC_AbstCntrlMgmtFuncDesc;
  CDC_UnionFuncDesc_TypeDef            CDC_UnionFuncDesc;  
}
CDC_InterfaceDesc_Typedef;



typedef struct
{
  uint8_t              NotifPipe; 
  uint8_t              NotifEp;
  uint8_t              buff[8];
  uint16_t             NotifEpSize;
}
CDC_CommItfTypedef ;

typedef struct
{
  uint8_t              InPipe; 
  uint8_t              OutPipe;
  uint8_t              OutEp;
  uint8_t              InEp;
  uint8_t              buff[8];
  uint16_t             OutEpSize;
  uint16_t             InEpSize;  
}
CDC_DataItfTypedef ;


typedef struct _CDC_Process
{
  CDC_CommItfTypedef                CommItf;
  CDC_DataItfTypedef                DataItf;
  uint8_t                           *pTxData;
  uint8_t                           *pRxData; 
  uint32_t                           TxDataLength;
  uint32_t                           RxDataLength;  
  CDC_InterfaceDesc_Typedef         CDC_Desc;
  CDC_LineCodingTypeDef             LineCoding;
  CDC_LineCodingTypeDef             *pUserLineCoding;  
  CDC_StateTypeDef                  state;
  CDC_DataStateTypeDef              data_tx_state;
  CDC_DataStateTypeDef              data_rx_state; 
  uint8_t                           Rx_Poll;
}
CDC_HandleTypeDef;

 

 

 

 
 

 
extern USBH_ClassTypeDef  CDC_Class;
#define USBH_CDC_CLASS    &CDC_Class

 

 

USBH_StatusTypeDef  USBH_CDC_SetLineCoding(USBH_HandleTypeDef *phost, 
                                           CDC_LineCodingTypeDef *linecoding);

USBH_StatusTypeDef  USBH_CDC_GetLineCoding(USBH_HandleTypeDef *phost, 
                                           CDC_LineCodingTypeDef *linecoding);

USBH_StatusTypeDef  USBH_CDC_Transmit(USBH_HandleTypeDef *phost, 
                                      uint8_t *pbuff, 
                                      uint32_t length);

USBH_StatusTypeDef  USBH_CDC_Receive(USBH_HandleTypeDef *phost, 
                                     uint8_t *pbuff, 
                                     uint32_t length);


uint16_t            USBH_CDC_GetLastReceivedDataSize(USBH_HandleTypeDef *phost);

USBH_StatusTypeDef  USBH_CDC_Stop(USBH_HandleTypeDef *phost);

void USBH_CDC_LineCodingChanged(USBH_HandleTypeDef *phost);

void USBH_CDC_TransmitCallback(USBH_HandleTypeDef *phost);

void USBH_CDC_ReceiveCallback(USBH_HandleTypeDef *phost);

 


#endif 

 

 

 

 


