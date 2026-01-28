#ifndef S5P_MFC_COMMON_H_
#define S5P_MFC_COMMON_H_
#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-v4l2.h>
#include "regs-mfc.h"
#include "regs-mfc-v10.h"
#define S5P_MFC_NAME		"s5p-mfc"
#define DST_QUEUE_OFF_BASE	(1 << 30)
#define BANK_L_CTX	0
#define BANK_R_CTX	1
#define BANK_CTX_NUM	2
#define MFC_BANK1_ALIGN_ORDER	13
#define MFC_BANK2_ALIGN_ORDER	13
#define MFC_BASE_ALIGN_ORDER	17
#define MFC_FW_MAX_VERSIONS	2
#include <media/videobuf2-dma-contig.h>
#define MFC_MAX_EXTRA_DPB       5
#define MFC_MAX_BUFFERS		32
#define MFC_NUM_CONTEXTS	4
#define MFC_INT_TIMEOUT		2000
#define MFC_BW_TIMEOUT		500
#define MFC_WATCHDOG_INTERVAL   1000
#define MFC_WATCHDOG_CNT        10
#define MFC_NO_INSTANCE_SET	-1
#define MFC_ENC_CAP_PLANE_COUNT	1
#define MFC_ENC_OUT_PLANE_COUNT	2
#define STUFF_BYTE		4
#define MFC_MAX_CTRLS		128
#define S5P_MFC_CODEC_NONE		-1
#define S5P_MFC_CODEC_H264_DEC		0
#define S5P_MFC_CODEC_H264_MVC_DEC	1
#define S5P_MFC_CODEC_VC1_DEC		2
#define S5P_MFC_CODEC_MPEG4_DEC		3
#define S5P_MFC_CODEC_MPEG2_DEC		4
#define S5P_MFC_CODEC_H263_DEC		5
#define S5P_MFC_CODEC_VC1RCV_DEC	6
#define S5P_MFC_CODEC_VP8_DEC		7
#define S5P_MFC_CODEC_HEVC_DEC		17
#define S5P_MFC_CODEC_VP9_DEC		18
#define S5P_MFC_CODEC_H264_ENC		20
#define S5P_MFC_CODEC_H264_MVC_ENC	21
#define S5P_MFC_CODEC_MPEG4_ENC		22
#define S5P_MFC_CODEC_H263_ENC		23
#define S5P_MFC_CODEC_VP8_ENC		24
#define S5P_MFC_CODEC_HEVC_ENC		26
#define S5P_MFC_R2H_CMD_EMPTY			0
#define S5P_MFC_R2H_CMD_SYS_INIT_RET		1
#define S5P_MFC_R2H_CMD_OPEN_INSTANCE_RET	2
#define S5P_MFC_R2H_CMD_SEQ_DONE_RET		3
#define S5P_MFC_R2H_CMD_INIT_BUFFERS_RET	4
#define S5P_MFC_R2H_CMD_CLOSE_INSTANCE_RET	6
#define S5P_MFC_R2H_CMD_SLEEP_RET		7
#define S5P_MFC_R2H_CMD_WAKEUP_RET		8
#define S5P_MFC_R2H_CMD_COMPLETE_SEQ_RET	9
#define S5P_MFC_R2H_CMD_DPB_FLUSH_RET		10
#define S5P_MFC_R2H_CMD_NAL_ABORT_RET		11
#define S5P_MFC_R2H_CMD_FW_STATUS_RET		12
#define S5P_MFC_R2H_CMD_FRAME_DONE_RET		13
#define S5P_MFC_R2H_CMD_FIELD_DONE_RET		14
#define S5P_MFC_R2H_CMD_SLICE_DONE_RET		15
#define S5P_MFC_R2H_CMD_ENC_BUFFER_FUL_RET	16
#define S5P_MFC_R2H_CMD_ERR_RET			32
#define MFC_MAX_CLOCKS		4
#define mfc_read(dev, offset)		readl(dev->regs_base + (offset))
#define mfc_write(dev, data, offset)	writel((data), dev->regs_base + \
								(offset))
enum s5p_mfc_fmt_type {
	MFC_FMT_DEC,
	MFC_FMT_ENC,
	MFC_FMT_RAW,
};
enum s5p_mfc_inst_type {
	MFCINST_INVALID,
	MFCINST_DECODER,
	MFCINST_ENCODER,
};
enum s5p_mfc_inst_state {
	MFCINST_FREE = 0,
	MFCINST_INIT = 100,
	MFCINST_GOT_INST,
	MFCINST_HEAD_PARSED,
	MFCINST_HEAD_PRODUCED,
	MFCINST_BUFS_SET,
	MFCINST_RUNNING,
	MFCINST_FINISHING,
	MFCINST_FINISHED,
	MFCINST_RETURN_INST,
	MFCINST_ERROR,
	MFCINST_ABORT,
	MFCINST_FLUSH,
	MFCINST_RES_CHANGE_INIT,
	MFCINST_RES_CHANGE_FLUSH,
	MFCINST_RES_CHANGE_END,
};
enum s5p_mfc_queue_state {
	QUEUE_FREE,
	QUEUE_BUFS_REQUESTED,
	QUEUE_BUFS_QUERIED,
	QUEUE_BUFS_MMAPED,
};
enum s5p_mfc_decode_arg {
	MFC_DEC_FRAME,
	MFC_DEC_LAST_FRAME,
	MFC_DEC_RES_CHANGE,
};
enum s5p_mfc_fw_ver {
	MFC_FW_V1,
	MFC_FW_V2,
};
#define MFC_BUF_FLAG_USED	(1 << 0)
#define MFC_BUF_FLAG_EOS	(1 << 1)
struct s5p_mfc_ctx;
struct s5p_mfc_buf {
	struct vb2_v4l2_buffer *b;
	struct list_head list;
	union {
		struct {
			size_t luma;
			size_t chroma;
		} raw;
		size_t stream;
	} cookie;
	int flags;
};
struct s5p_mfc_pm {
	struct clk	*clock_gate;
	const char * const *clk_names;
	struct clk	*clocks[MFC_MAX_CLOCKS];
	int		num_clocks;
	bool		use_clock_gating;
	struct device	*device;
};
struct s5p_mfc_buf_size_v5 {
	unsigned int h264_ctx;
	unsigned int non_h264_ctx;
	unsigned int dsc;
	unsigned int shm;
};
struct s5p_mfc_buf_size_v6 {
	unsigned int dev_ctx;
	unsigned int h264_dec_ctx;
	unsigned int other_dec_ctx;
	unsigned int h264_enc_ctx;
	unsigned int hevc_enc_ctx;
	unsigned int other_enc_ctx;
};
struct s5p_mfc_buf_size {
	unsigned int fw;
	unsigned int cpb;
	void *priv;
};
struct s5p_mfc_variant {
	unsigned int version;
	unsigned int port_num;
	u32 version_bit;
	struct s5p_mfc_buf_size *buf_size;
	char	*fw_name[MFC_FW_MAX_VERSIONS];
	const char	*clk_names[MFC_MAX_CLOCKS];
	int		num_clocks;
	bool		use_clock_gating;
};
struct s5p_mfc_priv_buf {
	unsigned long	ofs;
	void		*virt;
	dma_addr_t	dma;
	size_t		size;
	unsigned int	ctx;
};
struct s5p_mfc_dev {
	struct v4l2_device	v4l2_dev;
	struct video_device	*vfd_dec;
	struct video_device	*vfd_enc;
	struct platform_device	*plat_dev;
	struct device		*mem_dev[BANK_CTX_NUM];
	void __iomem		*regs_base;
	int			irq;
	struct v4l2_ctrl_handler dec_ctrl_handler;
	struct v4l2_ctrl_handler enc_ctrl_handler;
	struct s5p_mfc_pm	pm;
	const struct s5p_mfc_variant	*variant;
	int num_inst;
	spinlock_t irqlock;	 
	spinlock_t condlock;	 
	struct mutex mfc_mutex;  
	int int_cond;
	int int_type;
	unsigned int int_err;
	wait_queue_head_t queue;
	struct s5p_mfc_priv_buf fw_buf;
	size_t mem_size;
	dma_addr_t mem_base;
	unsigned long *mem_bitmap;
	void *mem_virt;
	dma_addr_t dma_base[BANK_CTX_NUM];
	unsigned long hw_lock;
	struct s5p_mfc_ctx *ctx[MFC_NUM_CONTEXTS];
	int curr_ctx;
	unsigned long ctx_work_bits;
	atomic_t watchdog_cnt;
	struct timer_list watchdog_timer;
	struct workqueue_struct *watchdog_workqueue;
	struct work_struct watchdog_work;
	unsigned long enter_suspend;
	struct s5p_mfc_priv_buf ctx_buf;
	int warn_start;
	struct s5p_mfc_hw_ops *mfc_ops;
	struct s5p_mfc_hw_cmds *mfc_cmds;
	const struct s5p_mfc_regs *mfc_regs;
	enum s5p_mfc_fw_ver fw_ver;
	bool fw_get_done;
	bool risc_on;  
};
struct s5p_mfc_h264_enc_params {
	enum v4l2_mpeg_video_h264_profile profile;
	enum v4l2_mpeg_video_h264_loop_filter_mode loop_filter_mode;
	s8 loop_filter_alpha;
	s8 loop_filter_beta;
	enum v4l2_mpeg_video_h264_entropy_mode entropy_mode;
	u8 max_ref_pic;
	u8 num_ref_pic_4p;
	int _8x8_transform;
	int rc_mb_dark;
	int rc_mb_smooth;
	int rc_mb_static;
	int rc_mb_activity;
	int vui_sar;
	u8 vui_sar_idc;
	u16 vui_ext_sar_width;
	u16 vui_ext_sar_height;
	int open_gop;
	u16 open_gop_size;
	u8 rc_frame_qp;
	u8 rc_min_qp;
	u8 rc_max_qp;
	u8 rc_p_frame_qp;
	u8 rc_b_frame_qp;
	enum v4l2_mpeg_video_h264_level level_v4l2;
	int level;
	u16 cpb_size;
	int interlace;
	u8 hier_qp;
	u8 hier_qp_type;
	u8 hier_qp_layer;
	u8 hier_qp_layer_qp[7];
	u8 sei_frame_packing;
	u8 sei_fp_curr_frame_0;
	u8 sei_fp_arrangement_type;
	u8 fmo;
	u8 fmo_map_type;
	u8 fmo_slice_grp;
	u8 fmo_chg_dir;
	u32 fmo_chg_rate;
	u32 fmo_run_len[4];
	u8 aso;
	u32 aso_slice_order[8];
};
struct s5p_mfc_mpeg4_enc_params {
	enum v4l2_mpeg_video_mpeg4_profile profile;
	int quarter_pixel;
	u16 vop_time_res;
	u16 vop_frm_delta;
	u8 rc_frame_qp;
	u8 rc_min_qp;
	u8 rc_max_qp;
	u8 rc_p_frame_qp;
	u8 rc_b_frame_qp;
	enum v4l2_mpeg_video_mpeg4_level level_v4l2;
	int level;
};
struct s5p_mfc_vp8_enc_params {
	u8 imd_4x4;
	enum v4l2_vp8_num_partitions num_partitions;
	enum v4l2_vp8_num_ref_frames num_ref;
	u8 filter_level;
	u8 filter_sharpness;
	u32 golden_frame_ref_period;
	enum v4l2_vp8_golden_frame_sel golden_frame_sel;
	u8 hier_layer;
	u8 hier_layer_qp[3];
	u8 rc_min_qp;
	u8 rc_max_qp;
	u8 rc_frame_qp;
	u8 rc_p_frame_qp;
	u8 profile;
};
struct s5p_mfc_hevc_enc_params {
	enum v4l2_mpeg_video_hevc_profile profile;
	int level;
	enum v4l2_mpeg_video_h264_level level_v4l2;
	u8 tier;
	u32 rc_framerate;
	u8 rc_min_qp;
	u8 rc_max_qp;
	u8 rc_lcu_dark;
	u8 rc_lcu_smooth;
	u8 rc_lcu_static;
	u8 rc_lcu_activity;
	u8 rc_frame_qp;
	u8 rc_p_frame_qp;
	u8 rc_b_frame_qp;
	u8 max_partition_depth;
	u8 num_refs_for_p;
	u8 refreshtype;
	u16 refreshperiod;
	s32 lf_beta_offset_div2;
	s32 lf_tc_offset_div2;
	u8 loopfilter;
	u8 loopfilter_disable;
	u8 loopfilter_across;
	u8 nal_control_length_filed;
	u8 nal_control_user_ref;
	u8 nal_control_store_ref;
	u8 const_intra_period_enable;
	u8 lossless_cu_enable;
	u8 wavefront_enable;
	u8 enable_ltr;
	u8 hier_qp_enable;
	enum v4l2_mpeg_video_hevc_hier_coding_type hier_qp_type;
	u8 num_hier_layer;
	u8 hier_qp_layer[7];
	u32 hier_bit_layer[7];
	u8 sign_data_hiding;
	u8 general_pb_enable;
	u8 temporal_id_enable;
	u8 strong_intra_smooth;
	u8 intra_pu_split_disable;
	u8 tmv_prediction_disable;
	u8 max_num_merge_mv;
	u8 eco_mode_enable;
	u8 encoding_nostartcode_enable;
	u8 size_of_length_field;
	u8 prepend_sps_pps_to_idr;
};
struct s5p_mfc_enc_params {
	u16 width;
	u16 height;
	u32 mv_h_range;
	u32 mv_v_range;
	u16 gop_size;
	enum v4l2_mpeg_video_multi_slice_mode slice_mode;
	u16 slice_mb;
	u32 slice_bit;
	u16 intra_refresh_mb;
	int pad;
	u8 pad_luma;
	u8 pad_cb;
	u8 pad_cr;
	int rc_frame;
	int rc_mb;
	u32 rc_bitrate;
	u16 rc_reaction_coeff;
	u16 vbv_size;
	u32 vbv_delay;
	enum v4l2_mpeg_video_header_mode seq_hdr_mode;
	enum v4l2_mpeg_mfc51_video_frame_skip_mode frame_skip_mode;
	int fixed_target_bit;
	u8 num_b_frame;
	u32 rc_framerate_num;
	u32 rc_framerate_denom;
	struct {
		struct s5p_mfc_h264_enc_params h264;
		struct s5p_mfc_mpeg4_enc_params mpeg4;
		struct s5p_mfc_vp8_enc_params vp8;
		struct s5p_mfc_hevc_enc_params hevc;
	} codec;
};
struct s5p_mfc_codec_ops {
	int (*pre_seq_start) (struct s5p_mfc_ctx *ctx);
	int (*post_seq_start) (struct s5p_mfc_ctx *ctx);
	int (*pre_frame_start) (struct s5p_mfc_ctx *ctx);
	int (*post_frame_start) (struct s5p_mfc_ctx *ctx);
};
#define call_cop(c, op, args...)				\
	(((c)->c_ops->op) ?					\
		((c)->c_ops->op(args)) : 0)
struct s5p_mfc_ctx {
	struct s5p_mfc_dev *dev;
	struct v4l2_fh fh;
	int num;
	int int_cond;
	int int_type;
	unsigned int int_err;
	wait_queue_head_t queue;
	struct s5p_mfc_fmt *src_fmt;
	struct s5p_mfc_fmt *dst_fmt;
	struct vb2_queue vq_src;
	struct vb2_queue vq_dst;
	struct list_head src_queue;
	struct list_head dst_queue;
	unsigned int src_queue_cnt;
	unsigned int dst_queue_cnt;
	enum s5p_mfc_inst_type type;
	enum s5p_mfc_inst_state state;
	int inst_no;
	int img_width;
	int img_height;
	int buf_width;
	int buf_height;
	int luma_size;
	int chroma_size;
	int mv_size;
	unsigned long consumed_stream;
	unsigned int dpb_flush_flag;
	unsigned int head_processed;
	struct s5p_mfc_priv_buf bank1;
	struct s5p_mfc_priv_buf bank2;
	enum s5p_mfc_queue_state capture_state;
	enum s5p_mfc_queue_state output_state;
	struct s5p_mfc_buf src_bufs[MFC_MAX_BUFFERS];
	int src_bufs_cnt;
	struct s5p_mfc_buf dst_bufs[MFC_MAX_BUFFERS];
	int dst_bufs_cnt;
	unsigned int sequence;
	unsigned long dec_dst_flag;
	size_t dec_src_buf_size;
	int codec_mode;
	int slice_interface;
	int loop_filter_mpeg4;
	int display_delay;
	int display_delay_enable;
	int after_packed_pb;
	int sei_fp_parse;
	int pb_count;
	int total_dpb_count;
	int mv_count;
	struct s5p_mfc_priv_buf ctx;
	struct s5p_mfc_priv_buf dsc;
	struct s5p_mfc_priv_buf shm;
	struct s5p_mfc_enc_params enc_params;
	size_t enc_dst_buf_size;
	size_t luma_dpb_size;
	size_t chroma_dpb_size;
	size_t me_buffer_size;
	size_t tmv_buffer_size;
	enum v4l2_mpeg_mfc51_video_force_frame_type force_frame_type;
	struct list_head ref_queue;
	unsigned int ref_queue_cnt;
	enum v4l2_mpeg_video_multi_slice_mode slice_mode;
	union {
		unsigned int mb;
		unsigned int bits;
	} slice_size;
	const struct s5p_mfc_codec_ops *c_ops;
	struct v4l2_ctrl *ctrls[MFC_MAX_CTRLS];
	struct v4l2_ctrl_handler ctrl_handler;
	size_t scratch_buf_size;
};
struct s5p_mfc_fmt {
	u32 fourcc;
	u32 codec_mode;
	enum s5p_mfc_fmt_type type;
	u32 num_planes;
	u32 versions;
	u32 flags;
};
struct mfc_control {
	__u32			id;
	enum v4l2_ctrl_type	type;
	__u8			name[32];   
	__s32			minimum;    
	__s32			maximum;
	__s32			step;
	__u32			menu_skip_mask;
	__s32			default_value;
	__u32			flags;
	__u32			reserved[2];
	__u8			is_volatile;
};
#define s5p_mfc_hw_call(f, op, args...) \
	((f && f->op) ? f->op(args) : (typeof(f->op(args)))(-ENODEV))
#define fh_to_ctx(__fh) container_of(__fh, struct s5p_mfc_ctx, fh)
#define ctrl_to_ctx(__ctrl) \
	container_of((__ctrl)->handler, struct s5p_mfc_ctx, ctrl_handler)
void clear_work_bit(struct s5p_mfc_ctx *ctx);
void set_work_bit(struct s5p_mfc_ctx *ctx);
void clear_work_bit_irqsave(struct s5p_mfc_ctx *ctx);
void set_work_bit_irqsave(struct s5p_mfc_ctx *ctx);
int s5p_mfc_get_new_ctx(struct s5p_mfc_dev *dev);
void s5p_mfc_cleanup_queue(struct list_head *lh, struct vb2_queue *vq);
#define HAS_PORTNUM(dev)	(dev ? (dev->variant ? \
				(dev->variant->port_num ? 1 : 0) : 0) : 0)
#define IS_TWOPORT(dev)		(dev->variant->port_num == 2 ? 1 : 0)
#define IS_MFCV6_PLUS(dev)	(dev->variant->version >= 0x60 ? 1 : 0)
#define IS_MFCV7_PLUS(dev)	(dev->variant->version >= 0x70 ? 1 : 0)
#define IS_MFCV8_PLUS(dev)	(dev->variant->version >= 0x80 ? 1 : 0)
#define IS_MFCV10(dev)		(dev->variant->version >= 0xA0 ? 1 : 0)
#define FW_HAS_E_MIN_SCRATCH_BUF(dev) (IS_MFCV10(dev))
#define MFC_V5_BIT	BIT(0)
#define MFC_V6_BIT	BIT(1)
#define MFC_V7_BIT	BIT(2)
#define MFC_V8_BIT	BIT(3)
#define MFC_V10_BIT	BIT(5)
#define MFC_V5PLUS_BITS		(MFC_V5_BIT | MFC_V6_BIT | MFC_V7_BIT | \
					MFC_V8_BIT | MFC_V10_BIT)
#define MFC_V6PLUS_BITS		(MFC_V6_BIT | MFC_V7_BIT | MFC_V8_BIT | \
					MFC_V10_BIT)
#define MFC_V7PLUS_BITS		(MFC_V7_BIT | MFC_V8_BIT | MFC_V10_BIT)
#endif  
