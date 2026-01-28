#define V4L2_CTRL_COUNT 29  
enum {
	COMP_CAMERA = 0,
	COMP_PREVIEW,
	COMP_IMAGE_ENCODE,
	COMP_VIDEO_ENCODE,
	COMP_COUNT
};
enum {
	CAM_PORT_PREVIEW = 0,
	CAM_PORT_VIDEO,
	CAM_PORT_CAPTURE,
	CAM_PORT_COUNT
};
extern int bcm2835_v4l2_debug;
struct bcm2835_mmal_dev {
	struct v4l2_device v4l2_dev;
	struct video_device vdev;
	struct mutex mutex;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *ctrls[V4L2_CTRL_COUNT];
	enum v4l2_scene_mode scene_mode;
	struct mmal_colourfx colourfx;
	int hflip;
	int vflip;
	int red_gain;
	int blue_gain;
	enum mmal_parameter_exposuremode exposure_mode_user;
	enum v4l2_exposure_auto_type exposure_mode_v4l2_user;
	enum mmal_parameter_exposuremode exposure_mode_active;
	enum mmal_parameter_exposuremeteringmode metering_mode;
	unsigned int manual_shutter_speed;
	bool exp_auto_priority;
	bool manual_iso_enabled;
	u32 iso;
	struct vchiq_mmal_instance *instance;
	struct vchiq_mmal_component *component[COMP_COUNT];
	int camera_use_count;
	struct v4l2_window overlay;
	struct {
		unsigned int width;   
		unsigned int height;   
		unsigned int stride;   
		unsigned int buffersize;  
		struct mmal_fmt *fmt;
		struct v4l2_fract timeperframe;
		int encode_bitrate;
		int encode_bitrate_mode;
		enum v4l2_mpeg_video_h264_profile enc_profile;
		enum v4l2_mpeg_video_h264_level enc_level;
		int q_factor;
		struct vb2_queue vb_vidq;
		s64 vc_start_timestamp;
		ktime_t kernel_start_ts;
		u32 sequence;
		struct vchiq_mmal_port *port;  
		struct vchiq_mmal_port *camera_port;
		struct vchiq_mmal_component *encode_component;
		unsigned int frame_count;
		struct completion frame_cmplt;
	} capture;
	unsigned int camera_num;
	unsigned int max_width;
	unsigned int max_height;
	unsigned int rgb_bgr_swapped;
};
int bcm2835_mmal_init_controls(struct bcm2835_mmal_dev *dev, struct v4l2_ctrl_handler *hdl);
int bcm2835_mmal_set_all_camera_controls(struct bcm2835_mmal_dev *dev);
int set_framerate_params(struct bcm2835_mmal_dev *dev);
#define v4l2_dump_pix_format(level, debug, dev, pix_fmt, desc)	\
{	\
	v4l2_dbg(level, debug, dev,	\
"%s: w %u h %u field %u pfmt 0x%x bpl %u sz_img %u colorspace 0x%x priv %u\n", \
		desc,	\
		(pix_fmt)->width, (pix_fmt)->height, (pix_fmt)->field,	\
		(pix_fmt)->pixelformat, (pix_fmt)->bytesperline,	\
		(pix_fmt)->sizeimage, (pix_fmt)->colorspace, (pix_fmt)->priv); \
}
#define v4l2_dump_win_format(level, debug, dev, win_fmt, desc)	\
{	\
	v4l2_dbg(level, debug, dev,	\
"%s: w %u h %u l %u t %u  field %u chromakey %06X clip %p " \
"clipcount %u bitmap %p\n", \
		desc,	\
		(win_fmt)->w.width, (win_fmt)->w.height, \
		(win_fmt)->w.left, (win_fmt)->w.top, \
		(win_fmt)->field,	\
		(win_fmt)->chromakey,	\
		(win_fmt)->clips, (win_fmt)->clipcount,	\
		(win_fmt)->bitmap); \
}
