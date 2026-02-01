 

#ifndef _TA_SECUREDISPLAY_IF_H
#define _TA_SECUREDISPLAY_IF_H

 
 

 
enum ta_securedisplay_command {
	 
	TA_SECUREDISPLAY_COMMAND__QUERY_TA              = 1,
	 
	TA_SECUREDISPLAY_COMMAND__SEND_ROI_CRC          = 2,
	 
	TA_SECUREDISPLAY_COMMAND__MAX_ID                = 0x7FFFFFFF,
};

 
enum ta_securedisplay_status {
	TA_SECUREDISPLAY_STATUS__SUCCESS                 = 0x00,          
	TA_SECUREDISPLAY_STATUS__GENERIC_FAILURE         = 0x01,          
	TA_SECUREDISPLAY_STATUS__INVALID_PARAMETER       = 0x02,          
	TA_SECUREDISPLAY_STATUS__NULL_POINTER            = 0x03,          
	TA_SECUREDISPLAY_STATUS__I2C_WRITE_ERROR         = 0x04,          
	TA_SECUREDISPLAY_STATUS__READ_DIO_SCRATCH_ERROR  = 0x05,  
	TA_SECUREDISPLAY_STATUS__READ_CRC_ERROR          = 0x06,          
	TA_SECUREDISPLAY_STATUS__I2C_INIT_ERROR          = 0x07,      

	TA_SECUREDISPLAY_STATUS__MAX                     = 0x7FFFFFFF, 
};

 
enum  ta_securedisplay_phy_ID {
	TA_SECUREDISPLAY_PHY0                           = 0,
	TA_SECUREDISPLAY_PHY1                           = 1,
	TA_SECUREDISPLAY_PHY2                           = 2,
	TA_SECUREDISPLAY_PHY3                           = 3,
	TA_SECUREDISPLAY_MAX_PHY                        = 4,
};

 
enum ta_securedisplay_ta_query_cmd_ret {
	 
	TA_SECUREDISPLAY_QUERY_CMD_RET                 = 0xAB,
};

 
enum ta_securedisplay_buffer_size {
	 
	TA_SECUREDISPLAY_I2C_BUFFER_SIZE                = 15,
};

 
 
 

 
struct ta_securedisplay_send_roi_crc_input {
	uint32_t  phy_id;   
};

 
union ta_securedisplay_cmd_input {
	 
	struct ta_securedisplay_send_roi_crc_input        send_roi_crc;
	uint32_t                                          reserved[4];
};

 

 
struct ta_securedisplay_query_ta_output {
	 
	uint32_t  query_cmd_ret;
};

 
struct ta_securedisplay_send_roi_crc_output {
	uint8_t  i2c_buf[TA_SECUREDISPLAY_I2C_BUFFER_SIZE];   
	uint8_t  reserved;
};

 
union ta_securedisplay_cmd_output {
	 
	struct ta_securedisplay_query_ta_output            query_ta;
	 
	struct ta_securedisplay_send_roi_crc_output        send_roi_crc;
	uint32_t                                           reserved[4];
};

 
struct ta_securedisplay_cmd {
    uint32_t                                           cmd_id;                          
    enum ta_securedisplay_status                       status;                          
    uint32_t                                           reserved[2];                     
    union ta_securedisplay_cmd_input                   securedisplay_in_message;        
    union ta_securedisplay_cmd_output                  securedisplay_out_message;       
     
};

#endif   

