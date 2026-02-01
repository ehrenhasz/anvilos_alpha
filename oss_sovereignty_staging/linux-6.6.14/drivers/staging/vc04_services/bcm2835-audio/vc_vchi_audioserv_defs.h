 
 

#ifndef _VC_AUDIO_DEFS_H_
#define _VC_AUDIO_DEFS_H_

#define VC_AUDIOSERV_MIN_VER 1
#define VC_AUDIOSERV_VER 2

 
#define VC_AUDIO_WRITE_COOKIE1 VCHIQ_MAKE_FOURCC('B', 'C', 'M', 'A')
#define VC_AUDIO_WRITE_COOKIE2 VCHIQ_MAKE_FOURCC('D', 'A', 'T', 'A')

 

enum vc_audio_msg_type {
	VC_AUDIO_MSG_TYPE_RESULT, 
	VC_AUDIO_MSG_TYPE_COMPLETE, 
	VC_AUDIO_MSG_TYPE_CONFIG, 
	VC_AUDIO_MSG_TYPE_CONTROL, 
	VC_AUDIO_MSG_TYPE_OPEN, 
	VC_AUDIO_MSG_TYPE_CLOSE, 
	VC_AUDIO_MSG_TYPE_START, 
	VC_AUDIO_MSG_TYPE_STOP, 
	VC_AUDIO_MSG_TYPE_WRITE, 
	VC_AUDIO_MSG_TYPE_MAX
};

 

struct vc_audio_config {
	u32 channels;
	u32 samplerate;
	u32 bps;
};

struct vc_audio_control {
	u32 volume;
	u32 dest;
};

struct vc_audio_open {
	u32 dummy;
};

struct vc_audio_close {
	u32 dummy;
};

struct vc_audio_start {
	u32 dummy;
};

struct vc_audio_stop {
	u32 draining;
};

 
struct vc_audio_write {
	u32 count; 
	u32 cookie1;
	u32 cookie2;
	s16 silence;
	s16 max_packet;
};

 
struct vc_audio_result {
	s32 success; 
};

 
struct vc_audio_complete {
	s32 count; 
	u32 cookie1;
	u32 cookie2;
};

 
struct vc_audio_msg {
	s32 type;  
	union {
		struct vc_audio_config config;
		struct vc_audio_control control;
		struct vc_audio_open open;
		struct vc_audio_close close;
		struct vc_audio_start start;
		struct vc_audio_stop stop;
		struct vc_audio_write write;
		struct vc_audio_result result;
		struct vc_audio_complete complete;
	};
};

#endif  
