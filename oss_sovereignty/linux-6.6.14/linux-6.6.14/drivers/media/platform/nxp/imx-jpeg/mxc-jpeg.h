#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>
#ifndef _MXC_JPEG_CORE_H
#define _MXC_JPEG_CORE_H
#define MXC_JPEG_NAME			"mxc-jpeg"
#define MXC_JPEG_FMT_TYPE_ENC		0
#define MXC_JPEG_FMT_TYPE_RAW		1
#define MXC_JPEG_DEFAULT_WIDTH		1280
#define MXC_JPEG_DEFAULT_HEIGHT		720
#define MXC_JPEG_DEFAULT_PFMT		V4L2_PIX_FMT_BGR24
#define MXC_JPEG_MIN_WIDTH		64
#define MXC_JPEG_MIN_HEIGHT		64
#define MXC_JPEG_MAX_WIDTH		0x2000
#define MXC_JPEG_MAX_HEIGHT		0x2000
#define MXC_JPEG_MAX_CFG_STREAM		0x1000
#define MXC_JPEG_H_ALIGN		3
#define MXC_JPEG_W_ALIGN		3
#define MXC_JPEG_MAX_SIZEIMAGE		0xFFFFFC00
#define MXC_JPEG_MAX_PLANES		2
enum mxc_jpeg_enc_state {
	MXC_JPEG_ENCODING	= 0,  
	MXC_JPEG_ENC_CONF	= 1,  
};
enum mxc_jpeg_mode {
	MXC_JPEG_DECODE	= 0,  
	MXC_JPEG_ENCODE	= 1,  
};
struct mxc_jpeg_fmt {
	const char				*name;
	u32					fourcc;
	enum v4l2_jpeg_chroma_subsampling	subsampling;
	int					nc;
	int					depth;
	int					mem_planes;
	int					comp_planes;
	int					h_align;
	int					v_align;
	u32					flags;
	u8					precision;
	u8					is_rgb;
};
struct mxc_jpeg_desc {
	u32 next_descpt_ptr;
	u32 buf_base0;
	u32 buf_base1;
	u32 line_pitch;
	u32 stm_bufbase;
	u32 stm_bufsize;
	u32 imgsize;
	u32 stm_ctrl;
} __packed;
struct mxc_jpeg_q_data {
	const struct mxc_jpeg_fmt	*fmt;
	u32				sizeimage[MXC_JPEG_MAX_PLANES];
	u32				bytesperline[MXC_JPEG_MAX_PLANES];
	int				w;
	int				w_adjusted;
	int				h;
	int				h_adjusted;
	unsigned int			sequence;
	struct v4l2_rect		crop;
};
struct mxc_jpeg_ctx {
	struct mxc_jpeg_dev		*mxc_jpeg;
	struct mxc_jpeg_q_data		out_q;
	struct mxc_jpeg_q_data		cap_q;
	struct v4l2_fh			fh;
	enum mxc_jpeg_enc_state		enc_state;
	int				slot;
	unsigned int			source_change;
	bool				need_initial_source_change_evt;
	bool				header_parsed;
	struct v4l2_ctrl_handler	ctrl_handler;
	u8				jpeg_quality;
	struct delayed_work		task_timer;
};
struct mxc_jpeg_slot_data {
	int slot;
	bool used;
	struct mxc_jpeg_desc *desc;  
	struct mxc_jpeg_desc *cfg_desc;  
	void *cfg_stream_vaddr;  
	unsigned int cfg_stream_size;
	dma_addr_t desc_handle;
	dma_addr_t cfg_desc_handle;  
	dma_addr_t cfg_stream_handle;  
};
struct mxc_jpeg_dev {
	spinlock_t			hw_lock;  
	unsigned int			mode;
	struct mutex			lock;  
	struct clk_bulk_data		*clks;
	int				num_clks;
	struct platform_device		*pdev;
	struct device			*dev;
	void __iomem			*base_reg;
	struct v4l2_device		v4l2_dev;
	struct v4l2_m2m_dev		*m2m_dev;
	struct video_device		*dec_vdev;
	struct mxc_jpeg_slot_data	slot_data;
	int				num_domains;
	struct device			**pd_dev;
	struct device_link		**pd_link;
};
struct mxc_jpeg_sof_comp {
	u8 id;
	u8 v :4;
	u8 h :4;
	u8 quantization_table_no;
} __packed;
#define MXC_JPEG_MAX_COMPONENTS 4
struct mxc_jpeg_sof {
	u16 length;
	u8 precision;
	u16 height, width;
	u8 components_no;
	struct mxc_jpeg_sof_comp comp[MXC_JPEG_MAX_COMPONENTS];
} __packed;
struct mxc_jpeg_sos_comp {
	u8 id;  
	u8 huffman_table_no;
} __packed;
struct mxc_jpeg_sos {
	u16 length;
	u8 components_no;
	struct mxc_jpeg_sos_comp comp[MXC_JPEG_MAX_COMPONENTS];
	u8 ignorable_bytes[3];
} __packed;
#endif
