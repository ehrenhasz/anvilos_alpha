


#ifndef U_MIDI2_H
#define U_MIDI2_H

#include <linux/usb/composite.h>
#include <sound/asound.h>

struct f_midi2_opts;
struct f_midi2_ep_opts;
struct f_midi2_block_opts;


struct f_midi2_block_info {
	unsigned int direction;		
	unsigned int first_group;	
	unsigned int num_groups;	
	unsigned int midi1_first_group;	
	unsigned int midi1_num_groups;	
	unsigned int ui_hint;		
	unsigned int midi_ci_version;	
	unsigned int sysex8_streams;	
	unsigned int is_midi1;		
	bool active;			
	const char *name;		
};


struct f_midi2_ep_info {
	unsigned int protocol_caps;	
	unsigned int protocol;		
	unsigned int manufacturer;	
	unsigned int family;		
	unsigned int model;		
	unsigned int sw_revision;	

	const char *ep_name;		
	const char *product_id;		
};

struct f_midi2_card_info {
	bool process_ump;		
	bool static_block;		
	unsigned int req_buf_size;	
	unsigned int num_reqs;		
	const char *iface_name;		
};

struct f_midi2_block_opts {
	struct config_group group;
	unsigned int id;
	struct f_midi2_block_info info;
	struct f_midi2_ep_opts *ep;
};

struct f_midi2_ep_opts {
	struct config_group group;
	unsigned int index;
	struct f_midi2_ep_info info;
	struct f_midi2_block_opts *blks[SNDRV_UMP_MAX_BLOCKS];
	struct f_midi2_opts *opts;
};

#define MAX_UMP_EPS		4
#define MAX_CABLES		16

struct f_midi2_opts {
	struct usb_function_instance func_inst;
	struct mutex lock;
	int refcnt;

	struct f_midi2_card_info info;

	unsigned int num_eps;
	struct f_midi2_ep_opts *eps[MAX_UMP_EPS];
};

#endif 
