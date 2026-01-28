#ifndef JPEG_CORE_H_
#define JPEG_CORE_H_
#include <linux/interrupt.h>
#include <media/jpeg.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ctrls.h>
#define S5P_JPEG_M2M_NAME		"s5p-jpeg"
#define JPEG_MAX_CLOCKS			4
#define S5P_JPEG_COMPR_QUAL_BEST	0
#define S5P_JPEG_COMPR_QUAL_WORST	3
#define S5P_JPEG_COEF11			0x4d
#define S5P_JPEG_COEF12			0x97
#define S5P_JPEG_COEF13			0x1e
#define S5P_JPEG_COEF21			0x2c
#define S5P_JPEG_COEF22			0x57
#define S5P_JPEG_COEF23			0x83
#define S5P_JPEG_COEF31			0x83
#define S5P_JPEG_COEF32			0x6e
#define S5P_JPEG_COEF33			0x13
#define EXYNOS3250_IRQ_TIMEOUT		0x10000000
#define SJPEG_FMT_FLAG_ENC_CAPTURE	(1 << 0)
#define SJPEG_FMT_FLAG_ENC_OUTPUT	(1 << 1)
#define SJPEG_FMT_FLAG_DEC_CAPTURE	(1 << 2)
#define SJPEG_FMT_FLAG_DEC_OUTPUT	(1 << 3)
#define SJPEG_FMT_FLAG_S5P		(1 << 4)
#define SJPEG_FMT_FLAG_EXYNOS3250	(1 << 5)
#define SJPEG_FMT_FLAG_EXYNOS4		(1 << 6)
#define SJPEG_FMT_RGB			(1 << 7)
#define SJPEG_FMT_NON_RGB		(1 << 8)
#define S5P_JPEG_ENCODE		0
#define S5P_JPEG_DECODE		1
#define S5P_JPEG_DISABLE	-1
#define FMT_TYPE_OUTPUT		0
#define FMT_TYPE_CAPTURE	1
#define SJPEG_SUBSAMPLING_444	0x11
#define SJPEG_SUBSAMPLING_422	0x21
#define SJPEG_SUBSAMPLING_420	0x22
#define S5P_JPEG_MAX_MARKER	4
enum sjpeg_version {
	SJPEG_S5P,
	SJPEG_EXYNOS3250,
	SJPEG_EXYNOS4,
	SJPEG_EXYNOS5420,
	SJPEG_EXYNOS5433,
};
enum exynos4_jpeg_result {
	OK_ENC_OR_DEC,
	ERR_PROT,
	ERR_DEC_INVALID_FORMAT,
	ERR_MULTI_SCAN,
	ERR_FRAME,
	ERR_UNKNOWN,
};
enum  exynos4_jpeg_img_quality_level {
	QUALITY_LEVEL_1 = 0,	 
	QUALITY_LEVEL_2,
	QUALITY_LEVEL_3,
	QUALITY_LEVEL_4,	 
};
enum s5p_jpeg_ctx_state {
	JPEGCTX_RUNNING = 0,
	JPEGCTX_RESOLUTION_CHANGE,
};
struct s5p_jpeg {
	struct mutex		lock;
	spinlock_t		slock;
	struct v4l2_device	v4l2_dev;
	struct video_device	*vfd_encoder;
	struct video_device	*vfd_decoder;
	struct v4l2_m2m_dev	*m2m_dev;
	void __iomem		*regs;
	unsigned int		irq;
	enum exynos4_jpeg_result irq_ret;
	struct clk		*clocks[JPEG_MAX_CLOCKS];
	struct device		*dev;
	struct s5p_jpeg_variant *variant;
	u32			irq_status;
};
struct s5p_jpeg_variant {
	unsigned int		version;
	unsigned int		fmt_ver_flag;
	unsigned int		hw3250_compat:1;
	unsigned int		htbl_reinit:1;
	unsigned int		hw_ex4_compat:1;
	const struct v4l2_m2m_ops *m2m_ops;
	irqreturn_t		(*jpeg_irq)(int irq, void *priv);
	const char		*clk_names[JPEG_MAX_CLOCKS];
	int			num_clocks;
};
struct s5p_jpeg_fmt {
	u32	fourcc;
	int	depth;
	int	colplanes;
	int	memplanes;
	int	h_align;
	int	v_align;
	int	subsampling;
	u32	flags;
};
struct s5p_jpeg_marker {
	u32	marker[S5P_JPEG_MAX_MARKER];
	u32	len[S5P_JPEG_MAX_MARKER];
	u32	n;
};
struct s5p_jpeg_q_data {
	struct s5p_jpeg_fmt	*fmt;
	u32			w;
	u32			h;
	u32			sos;
	struct s5p_jpeg_marker	dht;
	struct s5p_jpeg_marker	dqt;
	u32			sof;
	u32			sof_len;
	u32			size;
};
struct s5p_jpeg_ctx {
	struct s5p_jpeg		*jpeg;
	unsigned int		mode;
	unsigned short		compr_quality;
	unsigned short		restart_interval;
	unsigned short		subsampling;
	struct s5p_jpeg_q_data	out_q;
	struct s5p_jpeg_q_data	cap_q;
	unsigned int		scale_factor;
	struct v4l2_rect	crop_rect;
	struct v4l2_fh		fh;
	bool			hdr_parsed;
	bool			crop_altered;
	struct v4l2_ctrl_handler ctrl_handler;
	enum s5p_jpeg_ctx_state	state;
};
struct s5p_jpeg_buffer {
	unsigned long size;
	unsigned long curr;
	unsigned long data;
};
struct s5p_jpeg_addr {
	u32     y;
	u32     cb;
	u32     cr;
};
#endif  
