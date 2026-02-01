 
 

#ifndef _DELL_WMI_DESCRIPTOR_H_
#define _DELL_WMI_DESCRIPTOR_H_

#include <linux/wmi.h>

 
int dell_wmi_get_descriptor_valid(void);

bool dell_wmi_get_interface_version(u32 *version);
bool dell_wmi_get_size(u32 *size);
bool dell_wmi_get_hotfix(u32 *hotfix);

#endif
