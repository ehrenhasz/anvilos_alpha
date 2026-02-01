 
 

 
enum mmal_port_type {
	MMAL_PORT_TYPE_UNKNOWN = 0,	 
	MMAL_PORT_TYPE_CONTROL,		 
	MMAL_PORT_TYPE_INPUT,		 
	MMAL_PORT_TYPE_OUTPUT,		 
	MMAL_PORT_TYPE_CLOCK,		 
};

 
#define MMAL_PORT_CAPABILITY_PASSTHROUGH                       0x01
 
#define MMAL_PORT_CAPABILITY_ALLOCATION                        0x02
 
#define MMAL_PORT_CAPABILITY_SUPPORTS_EVENT_FORMAT_CHANGE      0x04

 
struct mmal_port {
	u32 priv;	 
	u32 name;	 

	u32 type;	 
	u16 index;	 
	u16 index_all;	 

	u32 is_enabled;	 
	u32 format;	 

	u32 buffer_num_min;	 

	u32 buffer_size_min;	 

	u32 buffer_alignment_min; 

	u32 buffer_num_recommended;	 

	u32 buffer_size_recommended;	 

	u32 buffer_num;	 

	u32 buffer_size;  

	u32 component;	 

	u32 userdata;	 

	u32 capabilities;	 
};
