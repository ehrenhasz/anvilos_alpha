 
 

#ifndef _MTK_VCODEC_COM_DRV_H_
#define _MTK_VCODEC_COM_DRV_H_

#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-core.h>

#define MTK_VCODEC_MAX_PLANES	3

#define WAIT_INTR_TIMEOUT_MS	1000

 
enum mtk_q_type {
	MTK_Q_DATA_SRC = 0,
	MTK_Q_DATA_DST = 1,
};

 
enum mtk_hw_reg_idx {
	VDEC_SYS,
	VDEC_MISC,
	VDEC_LD,
	VDEC_TOP,
	VDEC_CM,
	VDEC_AD,
	VDEC_AV,
	VDEC_PP,
	VDEC_HWD,
	VDEC_HWQ,
	VDEC_HWB,
	VDEC_HWG,
	NUM_MAX_VDEC_REG_BASE,
	 
	VENC_SYS = NUM_MAX_VDEC_REG_BASE,
	 
	VENC_LT_SYS,
	NUM_MAX_VCODEC_REG_BASE
};

 
struct mtk_vcodec_clk_info {
	const char	*clk_name;
	struct clk	*vcodec_clk;
};

 
struct mtk_vcodec_clk {
	struct mtk_vcodec_clk_info	*clk_info;
	int	clk_num;
};

 
struct mtk_vcodec_pm {
	struct mtk_vcodec_clk	vdec_clk;
	struct mtk_vcodec_clk	venc_clk;
	struct device	*dev;
};

 
enum mtk_vdec_hw_id {
	MTK_VDEC_CORE,
	MTK_VDEC_LAT0,
	MTK_VDEC_LAT1,
	MTK_VDEC_LAT_SOC,
	MTK_VDEC_HW_MAX,
};

 
enum mtk_instance_state {
	MTK_STATE_FREE = 0,
	MTK_STATE_INIT = 1,
	MTK_STATE_HEADER = 2,
	MTK_STATE_FLUSH = 3,
	MTK_STATE_ABORT = 4,
};

enum mtk_fmt_type {
	MTK_FMT_DEC = 0,
	MTK_FMT_ENC = 1,
	MTK_FMT_FRAME = 2,
};

 
struct mtk_video_fmt {
	u32	fourcc;
	enum mtk_fmt_type	type;
	u32	num_planes;
	u32	flags;
	struct v4l2_frmsize_stepwise frmsize;
};

 
struct mtk_q_data {
	unsigned int	visible_width;
	unsigned int	visible_height;
	unsigned int	coded_width;
	unsigned int	coded_height;
	enum v4l2_field	field;
	unsigned int	bytesperline[MTK_VCODEC_MAX_PLANES];
	unsigned int	sizeimage[MTK_VCODEC_MAX_PLANES];
	const struct mtk_video_fmt	*fmt;
};

 
enum mtk_instance_type {
	MTK_INST_DECODER		= 0,
	MTK_INST_ENCODER		= 1,
};

#endif  
