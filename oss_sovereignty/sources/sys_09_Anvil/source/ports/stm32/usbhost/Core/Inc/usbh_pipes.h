 


#ifndef __USBH_PIPES_H
#define __USBH_PIPES_H


#include "usbh_core.h"




  
 


 

 
 


 
 

 
 



USBH_StatusTypeDef USBH_OpenPipe  (USBH_HandleTypeDef *phost,
                            uint8_t ch_num,
                            uint8_t epnum,                            
                            uint8_t dev_address,
                            uint8_t speed,
                            uint8_t ep_type,
                            uint16_t mps);

USBH_StatusTypeDef USBH_ClosePipe  (USBH_HandleTypeDef *phost,
                            uint8_t pipe_num);

uint8_t USBH_AllocPipe  (USBH_HandleTypeDef *phost, 
                         uint8_t ep_addr);

USBH_StatusTypeDef USBH_FreePipe  (USBH_HandleTypeDef *phost, 
                                   uint8_t idx);




 



#endif 


 

 

 




