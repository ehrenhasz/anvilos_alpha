 
 

#ifndef MMAL_VCHIQ_H
#define MMAL_VCHIQ_H

#include "mmal-common.h"
#include "mmal-msg-format.h"

#define MAX_PORT_COUNT 4

 
#define MMAL_FORMAT_EXTRADATA_MAX_SIZE 128

struct vchiq_mmal_instance;

enum vchiq_mmal_es_type {
	MMAL_ES_TYPE_UNKNOWN,      
	MMAL_ES_TYPE_CONTROL,      
	MMAL_ES_TYPE_AUDIO,        
	MMAL_ES_TYPE_VIDEO,        
	MMAL_ES_TYPE_SUBPICTURE    
};

struct vchiq_mmal_port_buffer {
	unsigned int num;  
	u32 size;  
	u32 alignment;  
};

struct vchiq_mmal_port;

typedef void (*vchiq_mmal_buffer_cb)(
		struct vchiq_mmal_instance  *instance,
		struct vchiq_mmal_port *port,
		int status, struct mmal_buffer *buffer);

struct vchiq_mmal_port {
	bool enabled;
	u32 handle;
	u32 type;  
	u32 index;  

	 
	struct vchiq_mmal_component *component;

	struct vchiq_mmal_port *connected;  

	 
	struct vchiq_mmal_port_buffer minimum_buffer;
	struct vchiq_mmal_port_buffer recommended_buffer;
	struct vchiq_mmal_port_buffer current_buffer;

	 
	struct mmal_es_format_local format;
	 
	union mmal_es_specific_format es;

	 
	struct list_head buffers;
	 
	spinlock_t slock;

	 
	atomic_t buffers_with_vpu;
	 
	vchiq_mmal_buffer_cb buffer_cb;
	 
	void *cb_ctx;
};

struct vchiq_mmal_component {
	bool in_use;
	bool enabled;
	u32 handle;   
	u32 inputs;   
	u32 outputs;  
	u32 clocks;   
	struct vchiq_mmal_port control;  
	struct vchiq_mmal_port input[MAX_PORT_COUNT];  
	struct vchiq_mmal_port output[MAX_PORT_COUNT];  
	struct vchiq_mmal_port clock[MAX_PORT_COUNT];  
	u32 client_component;	 
};

int vchiq_mmal_init(struct vchiq_mmal_instance **out_instance);
int vchiq_mmal_finalise(struct vchiq_mmal_instance *instance);

 
int vchiq_mmal_component_init(
		struct vchiq_mmal_instance *instance,
		const char *name,
		struct vchiq_mmal_component **component_out);

int vchiq_mmal_component_finalise(
		struct vchiq_mmal_instance *instance,
		struct vchiq_mmal_component *component);

int vchiq_mmal_component_enable(
		struct vchiq_mmal_instance *instance,
		struct vchiq_mmal_component *component);

int vchiq_mmal_component_disable(
		struct vchiq_mmal_instance *instance,
		struct vchiq_mmal_component *component);

 
int vchiq_mmal_port_enable(
		struct vchiq_mmal_instance *instance,
		struct vchiq_mmal_port *port,
		vchiq_mmal_buffer_cb buffer_cb);

 
int vchiq_mmal_port_disable(struct vchiq_mmal_instance *instance,
			    struct vchiq_mmal_port *port);

int vchiq_mmal_port_parameter_set(struct vchiq_mmal_instance *instance,
				  struct vchiq_mmal_port *port,
				  u32 parameter,
				  void *value,
				  u32 value_size);

int vchiq_mmal_port_parameter_get(struct vchiq_mmal_instance *instance,
				  struct vchiq_mmal_port *port,
				  u32 parameter,
				  void *value,
				  u32 *value_size);

int vchiq_mmal_port_set_format(struct vchiq_mmal_instance *instance,
			       struct vchiq_mmal_port *port);

int vchiq_mmal_port_connect_tunnel(struct vchiq_mmal_instance *instance,
				   struct vchiq_mmal_port *src,
				   struct vchiq_mmal_port *dst);

int vchiq_mmal_version(struct vchiq_mmal_instance *instance,
		       u32 *major_out,
		       u32 *minor_out);

int vchiq_mmal_submit_buffer(struct vchiq_mmal_instance *instance,
			     struct vchiq_mmal_port *port,
			     struct mmal_buffer *buf);

int mmal_vchi_buffer_init(struct vchiq_mmal_instance *instance,
			  struct mmal_buffer *buf);
int mmal_vchi_buffer_cleanup(struct mmal_buffer *buf);
#endif  
