#ifndef _VIMC_COMMON_H_
#define _VIMC_COMMON_H_
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <media/media-device.h>
#include <media/v4l2-device.h>
#define VIMC_PDEV_NAME "vimc"
#define VIMC_CID_VIMC_BASE		(0x00f00000 | 0xf000)
#define VIMC_CID_VIMC_CLASS		(0x00f00000 | 1)
#define VIMC_CID_TEST_PATTERN		(VIMC_CID_VIMC_BASE + 0)
#define VIMC_CID_MEAN_WIN_SIZE		(VIMC_CID_VIMC_BASE + 1)
#define VIMC_CID_OSD_TEXT_MODE		(VIMC_CID_VIMC_BASE + 2)
#define VIMC_FRAME_MAX_WIDTH 4096
#define VIMC_FRAME_MAX_HEIGHT 2160
#define VIMC_FRAME_MIN_WIDTH 16
#define VIMC_FRAME_MIN_HEIGHT 16
#define VIMC_FRAME_INDEX(lin, col, width, bpp) ((lin * width + col) * bpp)
#define VIMC_IS_SRC(pad)	(pad)
#define VIMC_IS_SINK(pad)	(!(pad))
#define VIMC_PIX_FMT_MAX_CODES 8
extern unsigned int vimc_allocator;
enum vimc_allocator_type {
	VIMC_ALLOCATOR_VMALLOC = 0,
	VIMC_ALLOCATOR_DMA_CONTIG = 1,
};
#define vimc_colorimetry_clamp(fmt)					\
do {									\
	if ((fmt)->colorspace == V4L2_COLORSPACE_DEFAULT		\
	    || (fmt)->colorspace > V4L2_COLORSPACE_DCI_P3) {		\
		(fmt)->colorspace = V4L2_COLORSPACE_DEFAULT;		\
		(fmt)->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;		\
		(fmt)->quantization = V4L2_QUANTIZATION_DEFAULT;	\
		(fmt)->xfer_func = V4L2_XFER_FUNC_DEFAULT;		\
	}								\
	if ((fmt)->ycbcr_enc > V4L2_YCBCR_ENC_SMPTE240M)		\
		(fmt)->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;		\
	if ((fmt)->quantization > V4L2_QUANTIZATION_LIM_RANGE)		\
		(fmt)->quantization = V4L2_QUANTIZATION_DEFAULT;	\
	if ((fmt)->xfer_func > V4L2_XFER_FUNC_SMPTE2084)		\
		(fmt)->xfer_func = V4L2_XFER_FUNC_DEFAULT;		\
} while (0)
struct vimc_pix_map {
	unsigned int code[VIMC_PIX_FMT_MAX_CODES];
	unsigned int bpp;
	u32 pixelformat;
	bool bayer;
};
struct vimc_ent_device {
	struct device *dev;
	struct media_entity *ent;
	void * (*process_frame)(struct vimc_ent_device *ved,
				const void *frame);
	void (*vdev_get_format)(struct vimc_ent_device *ved,
			      struct v4l2_pix_format *fmt);
};
struct vimc_device {
	const struct vimc_pipeline_config *pipe_cfg;
	struct vimc_ent_device **ent_devs;
	struct media_device mdev;
	struct v4l2_device v4l2_dev;
};
struct vimc_ent_type {
	struct vimc_ent_device *(*add)(struct vimc_device *vimc,
				       const char *vcfg_name);
	void (*unregister)(struct vimc_ent_device *ved);
	void (*release)(struct vimc_ent_device *ved);
};
struct vimc_ent_config {
	const char *name;
	struct vimc_ent_type *type;
};
bool vimc_is_source(struct media_entity *ent);
extern struct vimc_ent_type vimc_sensor_type;
extern struct vimc_ent_type vimc_debayer_type;
extern struct vimc_ent_type vimc_scaler_type;
extern struct vimc_ent_type vimc_capture_type;
extern struct vimc_ent_type vimc_lens_type;
const struct vimc_pix_map *vimc_pix_map_by_index(unsigned int i);
u32 vimc_mbus_code_by_index(unsigned int index);
const struct vimc_pix_map *vimc_pix_map_by_code(u32 code);
const struct vimc_pix_map *vimc_pix_map_by_pixelformat(u32 pixelformat);
int vimc_ent_sd_register(struct vimc_ent_device *ved,
			 struct v4l2_subdev *sd,
			 struct v4l2_device *v4l2_dev,
			 const char *const name,
			 u32 function,
			 u16 num_pads,
			 struct media_pad *pads,
			 const struct v4l2_subdev_ops *sd_ops);
int vimc_vdev_link_validate(struct media_link *link);
#endif
