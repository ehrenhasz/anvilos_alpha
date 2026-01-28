 
#ifndef _HID_PARSER_H_
#define _HID_PARSER_H_


#include "usbh_hid.h"
#include "usbh_hid_usage.h"






  
 


 
typedef struct 
{
  uint8_t  *data;
  uint32_t size;
  uint8_t  shift;
  uint8_t  count;
  uint8_t  sign;
  uint32_t logical_min;  
  uint32_t logical_max;  
  uint32_t physical_min; 
  uint32_t physical_max; 
  uint32_t resolution;
} 
HID_Report_ItemTypedef;


uint32_t HID_ReadItem (HID_Report_ItemTypedef *ri, uint8_t ndx);
uint32_t HID_WriteItem(HID_Report_ItemTypedef *ri, uint32_t value, uint8_t ndx);


 

#endif 

 

 

 

 

