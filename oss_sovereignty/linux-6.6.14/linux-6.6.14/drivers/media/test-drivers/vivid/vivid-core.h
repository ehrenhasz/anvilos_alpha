#ifndef _VIVID_CORE_H_
#define _VIVID_CORE_H_
#include <linux/fb.h>
#include <linux/workqueue.h>
#include <media/cec.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ctrls.h>
#include <media/tpg/v4l2-tpg.h>
#include "vivid-rds-gen.h"
#include "vivid-vbi-gen.h"
#define dprintk(dev, level, fmt, arg...) \
	v4l2_dbg(level, vivid_debug, &dev->v4l2_dev, fmt, ## arg)
#define MAX_INPUTS 16
#define MAX_OUTPUTS 16
#define MAX_ZOOM  4
#define MAX_WIDTH  4096
#define MAX_HEIGHT 2160
#define MIN_WIDTH  16
#define MIN_HEIGHT MIN_WIDTH
#define PIXEL_ARRAY_DIV MIN_WIDTH
#define PLANE0_DATA_OFFSET 128
#define MIN_TV_FREQ (44U * 16U)
#define MAX_TV_FREQ (958U * 16U)
#define SDR_CAP_SAMPLES_PER_BUF 0x4000
#define JIFFIES_PER_DAY (3600U * 24U * HZ)
#define JIFFIES_RESYNC (JIFFIES_PER_DAY * (0xf0000000U / JIFFIES_PER_DAY))
extern const struct v4l2_rect vivid_min_rect;
extern const struct v4l2_rect vivid_max_rect;
extern unsigned vivid_debug;
struct vivid_fmt {
	u32	fourcc;           
	enum	tgp_color_enc color_enc;
	bool	can_do_overlay;
	u8	vdownsampling[TPG_MAX_PLANES];
	u32	alpha_mask;
	u8	planes;
	u8	buffers;
	u32	data_offset[TPG_MAX_PLANES];
	u32	bit_depth[TPG_MAX_PLANES];
};
extern struct vivid_fmt vivid_formats[];
struct vivid_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head	list;
};
enum vivid_input {
	WEBCAM,
	TV,
	SVID,
	HDMI,
};
enum vivid_signal_mode {
	CURRENT_DV_TIMINGS,
	CURRENT_STD = CURRENT_DV_TIMINGS,
	NO_SIGNAL,
	NO_LOCK,
	OUT_OF_RANGE,
	SELECTED_DV_TIMINGS,
	SELECTED_STD = SELECTED_DV_TIMINGS,
	CYCLE_DV_TIMINGS,
	CYCLE_STD = CYCLE_DV_TIMINGS,
	CUSTOM_DV_TIMINGS,
};
enum vivid_colorspace {
	VIVID_CS_170M,
	VIVID_CS_709,
	VIVID_CS_SRGB,
	VIVID_CS_OPRGB,
	VIVID_CS_2020,
	VIVID_CS_DCI_P3,
	VIVID_CS_240M,
	VIVID_CS_SYS_M,
	VIVID_CS_SYS_BG,
};
#define VIVID_INVALID_SIGNAL(mode) \
	((mode) == NO_SIGNAL || (mode) == NO_LOCK || (mode) == OUT_OF_RANGE)
struct vivid_cec_xfer {
	struct cec_adapter	*adap;
	u8			msg[CEC_MAX_MSG_SIZE];
	u32			len;
	u32			sft;
};
struct vivid_dev {
	unsigned			inst;
	struct v4l2_device		v4l2_dev;
#ifdef CONFIG_MEDIA_CONTROLLER
	struct media_device		mdev;
	struct media_pad		vid_cap_pad;
	struct media_pad		vid_out_pad;
	struct media_pad		vbi_cap_pad;
	struct media_pad		vbi_out_pad;
	struct media_pad		sdr_cap_pad;
	struct media_pad		meta_cap_pad;
	struct media_pad		meta_out_pad;
	struct media_pad		touch_cap_pad;
#endif
	struct v4l2_ctrl_handler	ctrl_hdl_user_gen;
	struct v4l2_ctrl_handler	ctrl_hdl_user_vid;
	struct v4l2_ctrl_handler	ctrl_hdl_user_aud;
	struct v4l2_ctrl_handler	ctrl_hdl_streaming;
	struct v4l2_ctrl_handler	ctrl_hdl_sdtv_cap;
	struct v4l2_ctrl_handler	ctrl_hdl_loop_cap;
	struct v4l2_ctrl_handler	ctrl_hdl_fb;
	struct video_device		vid_cap_dev;
	struct v4l2_ctrl_handler	ctrl_hdl_vid_cap;
	struct video_device		vid_out_dev;
	struct v4l2_ctrl_handler	ctrl_hdl_vid_out;
	struct video_device		vbi_cap_dev;
	struct v4l2_ctrl_handler	ctrl_hdl_vbi_cap;
	struct video_device		vbi_out_dev;
	struct v4l2_ctrl_handler	ctrl_hdl_vbi_out;
	struct video_device		radio_rx_dev;
	struct v4l2_ctrl_handler	ctrl_hdl_radio_rx;
	struct video_device		radio_tx_dev;
	struct v4l2_ctrl_handler	ctrl_hdl_radio_tx;
	struct video_device		sdr_cap_dev;
	struct v4l2_ctrl_handler	ctrl_hdl_sdr_cap;
	struct video_device		meta_cap_dev;
	struct v4l2_ctrl_handler	ctrl_hdl_meta_cap;
	struct video_device		meta_out_dev;
	struct v4l2_ctrl_handler	ctrl_hdl_meta_out;
	struct video_device		touch_cap_dev;
	struct v4l2_ctrl_handler	ctrl_hdl_touch_cap;
	spinlock_t			slock;
	struct mutex			mutex;
	u32				vid_cap_caps;
	u32				vid_out_caps;
	u32				vbi_cap_caps;
	u32				vbi_out_caps;
	u32				sdr_cap_caps;
	u32				radio_rx_caps;
	u32				radio_tx_caps;
	u32				meta_cap_caps;
	u32				meta_out_caps;
	u32				touch_cap_caps;
	bool				multiplanar;
	unsigned			num_inputs;
	unsigned int			num_hdmi_inputs;
	u8				input_type[MAX_INPUTS];
	u8				input_name_counter[MAX_INPUTS];
	unsigned			num_outputs;
	unsigned int			num_hdmi_outputs;
	u8				output_type[MAX_OUTPUTS];
	u8				output_name_counter[MAX_OUTPUTS];
	bool				has_audio_inputs;
	bool				has_audio_outputs;
	bool				has_vid_cap;
	bool				has_vid_out;
	bool				has_vbi_cap;
	bool				has_raw_vbi_cap;
	bool				has_sliced_vbi_cap;
	bool				has_vbi_out;
	bool				has_raw_vbi_out;
	bool				has_sliced_vbi_out;
	bool				has_radio_rx;
	bool				has_radio_tx;
	bool				has_sdr_cap;
	bool				has_fb;
	bool				has_meta_cap;
	bool				has_meta_out;
	bool				has_tv_tuner;
	bool				has_touch_cap;
	bool				can_loop_video;
	struct v4l2_ctrl		*brightness;
	struct v4l2_ctrl		*contrast;
	struct v4l2_ctrl		*saturation;
	struct v4l2_ctrl		*hue;
	struct {
		struct v4l2_ctrl	*autogain;
		struct v4l2_ctrl	*gain;
	};
	struct v4l2_ctrl		*volume;
	struct v4l2_ctrl		*mute;
	struct v4l2_ctrl		*alpha;
	struct v4l2_ctrl		*button;
	struct v4l2_ctrl		*boolean;
	struct v4l2_ctrl		*int32;
	struct v4l2_ctrl		*int64;
	struct v4l2_ctrl		*menu;
	struct v4l2_ctrl		*string;
	struct v4l2_ctrl		*bitmask;
	struct v4l2_ctrl		*int_menu;
	struct v4l2_ctrl		*ro_int32;
	struct v4l2_ctrl		*pixel_array;
	struct v4l2_ctrl		*test_pattern;
	struct v4l2_ctrl		*colorspace;
	struct v4l2_ctrl		*rgb_range_cap;
	struct v4l2_ctrl		*real_rgb_range_cap;
	struct {
		struct v4l2_ctrl	*ctrl_std_signal_mode;
		struct v4l2_ctrl	*ctrl_standard;
	};
	struct {
		struct v4l2_ctrl	*ctrl_dv_timings_signal_mode;
		struct v4l2_ctrl	*ctrl_dv_timings;
	};
	struct v4l2_ctrl		*ctrl_display_present;
	struct v4l2_ctrl		*ctrl_has_crop_cap;
	struct v4l2_ctrl		*ctrl_has_compose_cap;
	struct v4l2_ctrl		*ctrl_has_scaler_cap;
	struct v4l2_ctrl		*ctrl_has_crop_out;
	struct v4l2_ctrl		*ctrl_has_compose_out;
	struct v4l2_ctrl		*ctrl_has_scaler_out;
	struct v4l2_ctrl		*ctrl_tx_mode;
	struct v4l2_ctrl		*ctrl_tx_rgb_range;
	struct v4l2_ctrl		*ctrl_tx_edid_present;
	struct v4l2_ctrl		*ctrl_tx_hotplug;
	struct v4l2_ctrl		*ctrl_tx_rxsense;
	struct v4l2_ctrl		*ctrl_rx_power_present;
	struct v4l2_ctrl		*radio_tx_rds_pi;
	struct v4l2_ctrl		*radio_tx_rds_pty;
	struct v4l2_ctrl		*radio_tx_rds_mono_stereo;
	struct v4l2_ctrl		*radio_tx_rds_art_head;
	struct v4l2_ctrl		*radio_tx_rds_compressed;
	struct v4l2_ctrl		*radio_tx_rds_dyn_pty;
	struct v4l2_ctrl		*radio_tx_rds_ta;
	struct v4l2_ctrl		*radio_tx_rds_tp;
	struct v4l2_ctrl		*radio_tx_rds_ms;
	struct v4l2_ctrl		*radio_tx_rds_psname;
	struct v4l2_ctrl		*radio_tx_rds_radiotext;
	struct v4l2_ctrl		*radio_rx_rds_pty;
	struct v4l2_ctrl		*radio_rx_rds_ta;
	struct v4l2_ctrl		*radio_rx_rds_tp;
	struct v4l2_ctrl		*radio_rx_rds_ms;
	struct v4l2_ctrl		*radio_rx_rds_psname;
	struct v4l2_ctrl		*radio_rx_rds_radiotext;
	unsigned			input_brightness[MAX_INPUTS];
	unsigned			osd_mode;
	unsigned			button_pressed;
	bool				sensor_hflip;
	bool				sensor_vflip;
	bool				hflip;
	bool				vflip;
	bool				vbi_cap_interlaced;
	bool				loop_video;
	bool				reduced_fps;
	unsigned long			video_pbase;
	void				*video_vbase;
	u32				video_buffer_size;
	int				display_width;
	int				display_height;
	int				display_byte_stride;
	int				bits_per_pixel;
	int				bytes_per_pixel;
	struct fb_info			fb_info;
	struct fb_var_screeninfo	fb_defined;
	struct fb_fix_screeninfo	fb_fix;
	bool				disconnect_error;
	bool				queue_setup_error;
	bool				buf_prepare_error;
	bool				start_streaming_error;
	bool				dqbuf_error;
	bool				req_validate_error;
	bool				seq_wrap;
	u64				time_wrap;
	u64				time_wrap_offset;
	unsigned			perc_dropped_buffers;
	enum vivid_signal_mode		std_signal_mode[MAX_INPUTS];
	unsigned int			query_std_last[MAX_INPUTS];
	v4l2_std_id			query_std[MAX_INPUTS];
	enum tpg_video_aspect		std_aspect_ratio[MAX_INPUTS];
	enum vivid_signal_mode		dv_timings_signal_mode[MAX_INPUTS];
	char				**query_dv_timings_qmenu;
	char				*query_dv_timings_qmenu_strings;
	unsigned			query_dv_timings_size;
	unsigned int			query_dv_timings_last[MAX_INPUTS];
	unsigned int			query_dv_timings[MAX_INPUTS];
	enum tpg_video_aspect		dv_timings_aspect_ratio[MAX_INPUTS];
	unsigned			input;
	v4l2_std_id			std_cap[MAX_INPUTS];
	struct v4l2_dv_timings		dv_timings_cap[MAX_INPUTS];
	int				dv_timings_cap_sel[MAX_INPUTS];
	u32				service_set_cap;
	struct vivid_vbi_gen_data	vbi_gen;
	u8				*edid;
	unsigned			edid_blocks;
	unsigned			edid_max_blocks;
	unsigned			webcam_size_idx;
	unsigned			webcam_ival_idx;
	unsigned			tv_freq;
	unsigned			tv_audmode;
	unsigned			tv_field_cap;
	unsigned			tv_audio_input;
	u32				power_present;
	unsigned			output;
	v4l2_std_id			std_out;
	struct v4l2_dv_timings		dv_timings_out;
	u32				colorspace_out;
	u32				ycbcr_enc_out;
	u32				hsv_enc_out;
	u32				quantization_out;
	u32				xfer_func_out;
	u32				service_set_out;
	unsigned			bytesperline_out[TPG_MAX_PLANES];
	unsigned			tv_field_out;
	unsigned			tv_audio_output;
	bool				vbi_out_have_wss;
	u8				vbi_out_wss[2];
	bool				vbi_out_have_cc[2];
	u8				vbi_out_cc[2][2];
	bool				dvi_d_out;
	u8				*scaled_line;
	u8				*blended_line;
	unsigned			cur_scaled_line;
	bool				display_present[MAX_OUTPUTS];
	void				*fb_vbase_out;
	bool				overlay_out_enabled;
	int				overlay_out_top, overlay_out_left;
	unsigned			fbuf_out_flags;
	u32				chromakey_out;
	u8				global_alpha_out;
	struct tpg_data			tpg;
	unsigned			ms_vid_cap;
	bool				must_blank[VIDEO_MAX_FRAME];
	const struct vivid_fmt		*fmt_cap;
	struct v4l2_fract		timeperframe_vid_cap;
	enum v4l2_field			field_cap;
	struct v4l2_rect		src_rect;
	struct v4l2_rect		fmt_cap_rect;
	struct v4l2_rect		crop_cap;
	struct v4l2_rect		compose_cap;
	struct v4l2_rect		crop_bounds_cap;
	struct vb2_queue		vb_vid_cap_q;
	struct list_head		vid_cap_active;
	struct vb2_queue		vb_vbi_cap_q;
	struct list_head		vbi_cap_active;
	struct vb2_queue		vb_meta_cap_q;
	struct list_head		meta_cap_active;
	struct vb2_queue		vb_touch_cap_q;
	struct list_head		touch_cap_active;
	struct task_struct		*kthread_vid_cap;
	unsigned long			jiffies_vid_cap;
	u64				cap_stream_start;
	u64				cap_frame_period;
	u64				cap_frame_eof_offset;
	u32				cap_seq_offset;
	u32				cap_seq_count;
	bool				cap_seq_resync;
	u32				vid_cap_seq_start;
	u32				vid_cap_seq_count;
	bool				vid_cap_streaming;
	u32				vbi_cap_seq_start;
	u32				vbi_cap_seq_count;
	bool				vbi_cap_streaming;
	u32				meta_cap_seq_start;
	u32				meta_cap_seq_count;
	bool				meta_cap_streaming;
	struct task_struct		*kthread_touch_cap;
	unsigned long			jiffies_touch_cap;
	u64				touch_cap_stream_start;
	u32				touch_cap_seq_offset;
	bool				touch_cap_seq_resync;
	u32				touch_cap_seq_start;
	u32				touch_cap_seq_count;
	u32				touch_cap_with_seq_wrap_count;
	bool				touch_cap_streaming;
	struct v4l2_fract		timeperframe_tch_cap;
	struct v4l2_pix_format		tch_format;
	int				tch_pat_random;
	const struct vivid_fmt		*fmt_out;
	struct v4l2_fract		timeperframe_vid_out;
	enum v4l2_field			field_out;
	struct v4l2_rect		sink_rect;
	struct v4l2_rect		fmt_out_rect;
	struct v4l2_rect		crop_out;
	struct v4l2_rect		compose_out;
	struct v4l2_rect		compose_bounds_out;
	struct vb2_queue		vb_vid_out_q;
	struct list_head		vid_out_active;
	struct vb2_queue		vb_vbi_out_q;
	struct list_head		vbi_out_active;
	struct vb2_queue		vb_meta_out_q;
	struct list_head		meta_out_active;
	struct v4l2_rect		loop_vid_copy;
	struct v4l2_rect		loop_vid_out;
	struct v4l2_rect		loop_vid_cap;
	struct v4l2_rect		loop_fb_copy;
	struct v4l2_rect		loop_vid_overlay;
	struct v4l2_rect		loop_vid_overlay_cap;
	struct task_struct		*kthread_vid_out;
	unsigned long			jiffies_vid_out;
	u32				out_seq_offset;
	u32				out_seq_count;
	bool				out_seq_resync;
	u32				vid_out_seq_start;
	u32				vid_out_seq_count;
	bool				vid_out_streaming;
	u32				vbi_out_seq_start;
	u32				vbi_out_seq_count;
	bool				vbi_out_streaming;
	bool				stream_sliced_vbi_out;
	u32				meta_out_seq_start;
	u32				meta_out_seq_count;
	bool				meta_out_streaming;
	struct vb2_queue		vb_sdr_cap_q;
	struct list_head		sdr_cap_active;
	u32				sdr_pixelformat;  
	unsigned			sdr_buffersize;
	unsigned			sdr_adc_freq;
	unsigned			sdr_fm_freq;
	unsigned			sdr_fm_deviation;
	int				sdr_fixp_src_phase;
	int				sdr_fixp_mod_phase;
	bool				tstamp_src_is_soe;
	bool				has_crop_cap;
	bool				has_compose_cap;
	bool				has_scaler_cap;
	bool				has_crop_out;
	bool				has_compose_out;
	bool				has_scaler_out;
	struct task_struct		*kthread_sdr_cap;
	unsigned long			jiffies_sdr_cap;
	u32				sdr_cap_seq_offset;
	u32				sdr_cap_seq_start;
	u32				sdr_cap_seq_count;
	u32				sdr_cap_with_seq_wrap_count;
	bool				sdr_cap_seq_resync;
	struct vivid_rds_gen		rds_gen;
	unsigned			radio_rx_freq;
	unsigned			radio_rx_audmode;
	int				radio_rx_sig_qual;
	unsigned			radio_rx_hw_seek_mode;
	bool				radio_rx_hw_seek_prog_lim;
	bool				radio_rx_rds_controls;
	bool				radio_rx_rds_enabled;
	unsigned			radio_rx_rds_use_alternates;
	unsigned			radio_rx_rds_last_block;
	struct v4l2_fh			*radio_rx_rds_owner;
	unsigned			radio_tx_freq;
	unsigned			radio_tx_subchans;
	bool				radio_tx_rds_controls;
	unsigned			radio_tx_rds_last_block;
	struct v4l2_fh			*radio_tx_rds_owner;
	bool				radio_rds_loop;
	ktime_t				radio_rds_init_time;
	struct cec_adapter		*cec_rx_adap;
	struct cec_adapter		*cec_tx_adap[MAX_OUTPUTS];
	u8				cec_output2bus_map[MAX_OUTPUTS];
	struct task_struct		*kthread_cec;
	wait_queue_head_t		kthread_waitq_cec;
	struct vivid_cec_xfer	xfers[MAX_OUTPUTS];
	spinlock_t			cec_xfers_slock;  
	u32				cec_sft;  
	u8				last_initiator;
	char				osd[14];
	unsigned long			osd_jiffies;
	bool				meta_pts;
	bool				meta_scr;
};
static inline bool vivid_is_webcam(const struct vivid_dev *dev)
{
	return dev->input_type[dev->input] == WEBCAM;
}
static inline bool vivid_is_tv_cap(const struct vivid_dev *dev)
{
	return dev->input_type[dev->input] == TV;
}
static inline bool vivid_is_svid_cap(const struct vivid_dev *dev)
{
	return dev->input_type[dev->input] == SVID;
}
static inline bool vivid_is_hdmi_cap(const struct vivid_dev *dev)
{
	return dev->input_type[dev->input] == HDMI;
}
static inline bool vivid_is_sdtv_cap(const struct vivid_dev *dev)
{
	return vivid_is_tv_cap(dev) || vivid_is_svid_cap(dev);
}
static inline bool vivid_is_svid_out(const struct vivid_dev *dev)
{
	return dev->output_type[dev->output] == SVID;
}
static inline bool vivid_is_hdmi_out(const struct vivid_dev *dev)
{
	return dev->output_type[dev->output] == HDMI;
}
#endif
