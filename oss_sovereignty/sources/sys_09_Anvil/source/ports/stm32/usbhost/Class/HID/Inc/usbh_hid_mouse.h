 



#ifndef __USBH_HID_MOUSE_H
#define __USBH_HID_MOUSE_H


#include "usbh_hid.h"







 


 

typedef struct _HID_MOUSE_Info
{
  uint8_t              x; 
  uint8_t              y;  
  uint8_t              buttons[3];
}
HID_MOUSE_Info_TypeDef;

 

 
 

 
 

 
 

 
USBH_StatusTypeDef USBH_HID_MouseInit(USBH_HandleTypeDef *phost);
HID_MOUSE_Info_TypeDef *USBH_HID_GetMouseInfo(USBH_HandleTypeDef *phost);

 

#endif 

 

 

 

 

