 
 

#ifndef __FIRMWARE_H
#define __FIRMWARE_H

#include <linux/greybus.h>

#define FW_NAME_PREFIX	"gmp_"

 
#define FW_NAME_SIZE		56

 
int fw_mgmt_init(void);
void fw_mgmt_exit(void);
struct gb_connection *to_fw_mgmt_connection(struct device *dev);
int gb_fw_mgmt_request_handler(struct gb_operation *op);
int gb_fw_mgmt_connection_init(struct gb_connection *connection);
void gb_fw_mgmt_connection_exit(struct gb_connection *connection);

 
int gb_fw_download_request_handler(struct gb_operation *op);
int gb_fw_download_connection_init(struct gb_connection *connection);
void gb_fw_download_connection_exit(struct gb_connection *connection);

 
int cap_init(void);
void cap_exit(void);
int gb_cap_connection_init(struct gb_connection *connection);
void gb_cap_connection_exit(struct gb_connection *connection);

#endif  
