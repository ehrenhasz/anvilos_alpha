 


#ifndef __USBH_TEMPLATE_CORE_H
#define __USBH_TEMPLATE_CORE_H


#include "usbh_core.h"








 


 

 




 

 

 

 
 

 
extern USBH_ClassTypeDef  TEMPLATE_Class;
#define USBH_TEMPLATE_CLASS    &TEMPLATE_Class

 

 
USBH_StatusTypeDef USBH_TEMPLATE_IOProcess (USBH_HandleTypeDef *phost);
USBH_StatusTypeDef USBH_TEMPLATE_Init (USBH_HandleTypeDef *phost);
 


#endif 

 

 

 

 


