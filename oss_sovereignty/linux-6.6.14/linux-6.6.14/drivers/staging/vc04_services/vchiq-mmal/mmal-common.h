#ifndef MMAL_COMMON_H
#define MMAL_COMMON_H
#define MMAL_FOURCC(a, b, c, d) ((a) | (b << 8) | (c << 16) | (d << 24))
#define MMAL_MAGIC MMAL_FOURCC('m', 'm', 'a', 'l')
#define MMAL_TIME_UNKNOWN BIT_ULL(63)
struct mmal_msg_context;
struct mmal_fmt {
	u32 fourcc;           
	int flags;            
	u32 mmal;
	int depth;
	u32 mmal_component;   
	u32 ybbp;             
	bool remove_padding;    
};
struct mmal_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head list;
	void *buffer;  
	unsigned long buffer_size;  
	struct mmal_msg_context *msg_context;
	unsigned long length;
	u32 mmal_flags;
	s64 dts;
	s64 pts;
};
struct mmal_colourfx {
	s32 enable;
	u32 u;
	u32 v;
};
#endif
