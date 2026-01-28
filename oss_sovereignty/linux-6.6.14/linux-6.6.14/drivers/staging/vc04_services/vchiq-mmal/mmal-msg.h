#ifndef MMAL_MSG_H
#define MMAL_MSG_H
#define VC_MMAL_VER 15
#define VC_MMAL_MIN_VER 10
#define MMAL_MSG_MAX_SIZE 512
#define MMAL_MSG_MAX_PAYLOAD 488
#include "mmal-msg-common.h"
#include "mmal-msg-format.h"
#include "mmal-msg-port.h"
#include "mmal-vchiq.h"
enum mmal_msg_type {
	MMAL_MSG_TYPE_QUIT = 1,
	MMAL_MSG_TYPE_SERVICE_CLOSED,
	MMAL_MSG_TYPE_GET_VERSION,
	MMAL_MSG_TYPE_COMPONENT_CREATE,
	MMAL_MSG_TYPE_COMPONENT_DESTROY,	 
	MMAL_MSG_TYPE_COMPONENT_ENABLE,
	MMAL_MSG_TYPE_COMPONENT_DISABLE,
	MMAL_MSG_TYPE_PORT_INFO_GET,
	MMAL_MSG_TYPE_PORT_INFO_SET,
	MMAL_MSG_TYPE_PORT_ACTION,		 
	MMAL_MSG_TYPE_BUFFER_FROM_HOST,
	MMAL_MSG_TYPE_BUFFER_TO_HOST,
	MMAL_MSG_TYPE_GET_STATS,
	MMAL_MSG_TYPE_PORT_PARAMETER_SET,
	MMAL_MSG_TYPE_PORT_PARAMETER_GET,	 
	MMAL_MSG_TYPE_EVENT_TO_HOST,
	MMAL_MSG_TYPE_GET_CORE_STATS_FOR_PORT,
	MMAL_MSG_TYPE_OPAQUE_ALLOCATOR,
	MMAL_MSG_TYPE_CONSUME_MEM,
	MMAL_MSG_TYPE_LMK,			 
	MMAL_MSG_TYPE_OPAQUE_ALLOCATOR_DESC,
	MMAL_MSG_TYPE_DRM_GET_LHS32,
	MMAL_MSG_TYPE_DRM_GET_TIME,
	MMAL_MSG_TYPE_BUFFER_FROM_HOST_ZEROLEN,
	MMAL_MSG_TYPE_PORT_FLUSH,		 
	MMAL_MSG_TYPE_HOST_LOG,
	MMAL_MSG_TYPE_MSG_LAST
};
enum mmal_msg_port_action_type {
	MMAL_MSG_PORT_ACTION_TYPE_UNKNOWN = 0,	 
	MMAL_MSG_PORT_ACTION_TYPE_ENABLE,	 
	MMAL_MSG_PORT_ACTION_TYPE_DISABLE,	 
	MMAL_MSG_PORT_ACTION_TYPE_FLUSH,	 
	MMAL_MSG_PORT_ACTION_TYPE_CONNECT,	 
	MMAL_MSG_PORT_ACTION_TYPE_DISCONNECT,	 
	MMAL_MSG_PORT_ACTION_TYPE_SET_REQUIREMENTS,  
};
struct mmal_msg_header {
	u32 magic;
	u32 type;	 
	u32 control_service;
	u32 context;	 
	u32 status;	 
	u32 padding;
};
struct mmal_msg_version {
	u32 flags;
	u32 major;
	u32 minor;
	u32 minimum;
};
struct mmal_msg_component_create {
	u32 client_component;	 
	char name[128];
	u32 pid;		 
};
struct mmal_msg_component_create_reply {
	u32 status;	 
	u32 component_handle;  
	u32 input_num;         
	u32 output_num;        
	u32 clock_num;         
};
struct mmal_msg_component_destroy {
	u32 component_handle;
};
struct mmal_msg_component_destroy_reply {
	u32 status;  
};
struct mmal_msg_component_enable {
	u32 component_handle;
};
struct mmal_msg_component_enable_reply {
	u32 status;  
};
struct mmal_msg_component_disable {
	u32 component_handle;
};
struct mmal_msg_component_disable_reply {
	u32 status;  
};
struct mmal_msg_port_info_get {
	u32 component_handle;   
	u32 port_type;          
	u32 index;              
};
struct mmal_msg_port_info_get_reply {
	u32 status;		 
	u32 component_handle;	 
	u32 port_type;		 
	u32 port_index;		 
	s32 found;		 
	u32 port_handle;	 
	struct mmal_port port;
	struct mmal_es_format format;  
	union mmal_es_specific_format es;  
	u8 extradata[MMAL_FORMAT_EXTRADATA_MAX_SIZE];  
};
struct mmal_msg_port_info_set {
	u32 component_handle;
	u32 port_type;		 
	u32 port_index;		 
	struct mmal_port port;
	struct mmal_es_format format;
	union mmal_es_specific_format es;
	u8 extradata[MMAL_FORMAT_EXTRADATA_MAX_SIZE];
};
struct mmal_msg_port_info_set_reply {
	u32 status;
	u32 component_handle;	 
	u32 port_type;		 
	u32 index;		 
	s32 found;		 
	u32 port_handle;	 
	struct mmal_port port;
	struct mmal_es_format format;
	union mmal_es_specific_format es;
	u8 extradata[MMAL_FORMAT_EXTRADATA_MAX_SIZE];
};
struct mmal_msg_port_action_port {
	u32 component_handle;
	u32 port_handle;
	u32 action;		 
	struct mmal_port port;
};
struct mmal_msg_port_action_handle {
	u32 component_handle;
	u32 port_handle;
	u32 action;		 
	u32 connect_component_handle;
	u32 connect_port_handle;
};
struct mmal_msg_port_action_reply {
	u32 status;	 
};
#define MMAL_VC_SHORT_DATA 128
#define MMAL_BUFFER_HEADER_FLAG_EOS                    BIT(0)
#define MMAL_BUFFER_HEADER_FLAG_FRAME_START            BIT(1)
#define MMAL_BUFFER_HEADER_FLAG_FRAME_END              BIT(2)
#define MMAL_BUFFER_HEADER_FLAG_FRAME                  \
	(MMAL_BUFFER_HEADER_FLAG_FRAME_START | \
	 MMAL_BUFFER_HEADER_FLAG_FRAME_END)
#define MMAL_BUFFER_HEADER_FLAG_KEYFRAME               BIT(3)
#define MMAL_BUFFER_HEADER_FLAG_DISCONTINUITY          BIT(4)
#define MMAL_BUFFER_HEADER_FLAG_CONFIG                 BIT(5)
#define MMAL_BUFFER_HEADER_FLAG_ENCRYPTED              BIT(6)
#define MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO          BIT(7)
#define MMAL_BUFFER_HEADER_FLAGS_SNAPSHOT              BIT(8)
#define MMAL_BUFFER_HEADER_FLAG_CORRUPTED              BIT(9)
#define MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED    BIT(10)
struct mmal_driver_buffer {
	u32 magic;
	u32 component_handle;
	u32 port_handle;
	u32 client_context;
};
struct mmal_buffer_header {
	u32 next;	 
	u32 priv;	 
	u32 cmd;
	u32 data;
	u32 alloc_size;
	u32 length;
	u32 offset;
	u32 flags;
	s64 pts;
	s64 dts;
	u32 type;
	u32 user_data;
};
struct mmal_buffer_header_type_specific {
	union {
		struct {
		u32 planes;
		u32 offset[4];
		u32 pitch[4];
		u32 flags;
		} video;
	} u;
};
struct mmal_msg_buffer_from_host {
	struct mmal_driver_buffer drvbuf;
	struct mmal_driver_buffer drvbuf_ref;
	struct mmal_buffer_header buffer_header;  
	struct mmal_buffer_header_type_specific buffer_header_type_specific;
	s32 is_zero_copy;
	s32 has_reference;
	u32 payload_in_message;
	u8 short_data[MMAL_VC_SHORT_DATA];
};
#define MMAL_WORKER_PORT_PARAMETER_SPACE      96
struct mmal_msg_port_parameter_set {
	u32 component_handle;	 
	u32 port_handle;	 
	u32 id;			 
	u32 size;		 
	u32 value[MMAL_WORKER_PORT_PARAMETER_SPACE];
};
struct mmal_msg_port_parameter_set_reply {
	u32 status;	 
};
struct mmal_msg_port_parameter_get {
	u32 component_handle;	 
	u32 port_handle;	 
	u32 id;			 
	u32 size;		 
};
struct mmal_msg_port_parameter_get_reply {
	u32 status;		 
	u32 id;			 
	u32 size;		 
	u32 value[MMAL_WORKER_PORT_PARAMETER_SPACE];
};
#define MMAL_WORKER_EVENT_SPACE 256
struct mmal_msg_event_to_host {
	u32 client_component;	 
	u32 port_type;
	u32 port_num;
	u32 cmd;
	u32 length;
	u8 data[MMAL_WORKER_EVENT_SPACE];
	u32 delayed_buffer;
};
struct mmal_msg {
	struct mmal_msg_header h;
	union {
		struct mmal_msg_version version;
		struct mmal_msg_component_create component_create;
		struct mmal_msg_component_create_reply component_create_reply;
		struct mmal_msg_component_destroy component_destroy;
		struct mmal_msg_component_destroy_reply component_destroy_reply;
		struct mmal_msg_component_enable component_enable;
		struct mmal_msg_component_enable_reply component_enable_reply;
		struct mmal_msg_component_disable component_disable;
		struct mmal_msg_component_disable_reply component_disable_reply;
		struct mmal_msg_port_info_get port_info_get;
		struct mmal_msg_port_info_get_reply port_info_get_reply;
		struct mmal_msg_port_info_set port_info_set;
		struct mmal_msg_port_info_set_reply port_info_set_reply;
		struct mmal_msg_port_action_port port_action_port;
		struct mmal_msg_port_action_handle port_action_handle;
		struct mmal_msg_port_action_reply port_action_reply;
		struct mmal_msg_buffer_from_host buffer_from_host;
		struct mmal_msg_port_parameter_set port_parameter_set;
		struct mmal_msg_port_parameter_set_reply
			port_parameter_set_reply;
		struct mmal_msg_port_parameter_get
			port_parameter_get;
		struct mmal_msg_port_parameter_get_reply
			port_parameter_get_reply;
		struct mmal_msg_event_to_host event_to_host;
		u8 payload[MMAL_MSG_MAX_PAYLOAD];
	} u;
};
#endif
