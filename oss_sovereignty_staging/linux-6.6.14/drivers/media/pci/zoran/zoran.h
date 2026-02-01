 
 

#ifndef _BUZ_H_
#define _BUZ_H_

#include <linux/debugfs.h>
#include <linux/pci.h>
#include <linux/i2c-algo-bit.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>

#define ZR_NORM_PAL 0
#define ZR_NORM_NTSC 1
#define ZR_NORM_SECAM 2

struct zr_buffer {
	 
	struct vb2_v4l2_buffer          vbuf;
	struct list_head                queue;
};

static inline struct zr_buffer *vb2_to_zr_buffer(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);

	return container_of(vbuf, struct zr_buffer, vbuf);
}

#define ZORAN_NAME    "ZORAN"	 

#define ZR_DEVNAME(zr) ((zr)->name)

#define   BUZ_MAX_WIDTH   (zr->timing->wa)
#define   BUZ_MAX_HEIGHT  (zr->timing->ha)
#define   BUZ_MIN_WIDTH    32	 
#define   BUZ_MIN_HEIGHT   24	 

#define BUZ_NUM_STAT_COM    4
#define BUZ_MASK_STAT_COM   3

#define BUZ_MAX_INPUT       16

#include "zr36057.h"

enum card_type {
	UNKNOWN = -1,

	 
	DC10_OLD,		 
	DC10_NEW,		 
	DC10_PLUS,
	DC30,
	DC30_PLUS,

	 
	LML33,
	LML33R10,

	 
	BUZ,

	 
	AVS6EYES,

	 
	NUM_CARDS
};

enum zoran_codec_mode {
	BUZ_MODE_IDLE,		 
	BUZ_MODE_MOTION_COMPRESS,	 
	BUZ_MODE_MOTION_DECOMPRESS,	 
	BUZ_MODE_STILL_COMPRESS,	 
	BUZ_MODE_STILL_DECOMPRESS	 
};

enum zoran_map_mode {
	ZORAN_MAP_MODE_NONE,
	ZORAN_MAP_MODE_RAW,
	ZORAN_MAP_MODE_JPG_REC,
	ZORAN_MAP_MODE_JPG_PLAY,
};

enum gpio_type {
	ZR_GPIO_JPEG_SLEEP = 0,
	ZR_GPIO_JPEG_RESET,
	ZR_GPIO_JPEG_FRAME,
	ZR_GPIO_VID_DIR,
	ZR_GPIO_VID_EN,
	ZR_GPIO_VID_RESET,
	ZR_GPIO_CLK_SEL1,
	ZR_GPIO_CLK_SEL2,
	ZR_GPIO_MAX,
};

enum gpcs_type {
	GPCS_JPEG_RESET = 0,
	GPCS_JPEG_START,
	GPCS_MAX,
};

struct zoran_format {
	char *name;
	__u32 fourcc;
	int colorspace;
	int depth;
	__u32 flags;
	__u32 vfespfr;
};

 
#define ZORAN_FORMAT_COMPRESSED BIT(0)
#define ZORAN_FORMAT_OVERLAY BIT(1)
#define ZORAN_FORMAT_CAPTURE BIT(2)
#define ZORAN_FORMAT_PLAYBACK BIT(3)

 
struct zoran_v4l_settings {
	int width, height, bytesperline;	 
	const struct zoran_format *format;	 
};

 
struct zoran_jpg_settings {
	 
	int decimation;
	 
	int hor_dcm, ver_dcm, tmp_dcm;
	 
	int field_per_buff, odd_even;
	 
	int img_x, img_y, img_width, img_height;
	 
	struct v4l2_jpegcompression jpg_comp;
};

struct zoran;

 
struct zoran_fh {
	struct v4l2_fh fh;
	struct zoran *zr;
};

struct card_info {
	enum card_type type;
	char name[32];
	const char *i2c_decoder;	 
	const unsigned short *addrs_decoder;
	const char *i2c_encoder;	 
	const unsigned short *addrs_encoder;
	u16 video_vfe, video_codec;			 
	u16 audio_chip;					 

	int inputs;		 
	struct input {
		int muxsel;
		char name[32];
	} input[BUZ_MAX_INPUT];

	v4l2_std_id norms;
	const struct tvnorm *tvn[3];	 

	u32 jpeg_int;		 
	u32 vsync_int;		 
	s8 gpio[ZR_GPIO_MAX];
	u8 gpcs[GPCS_MAX];

	struct vfe_polarity vfe_pol;
	u8 gpio_pol[ZR_GPIO_MAX];

	 
	u8 gws_not_connected;

	 
	u8 input_mux;

	void (*init)(struct zoran *zr);
};

struct zoran {
	struct v4l2_device v4l2_dev;
	struct v4l2_ctrl_handler hdl;
	struct video_device *video_dev;
	struct vb2_queue vq;

	struct i2c_adapter i2c_adapter;	 
	struct i2c_algo_bit_data i2c_algo;	 
	u32 i2cbr;

	struct v4l2_subdev *decoder;	 
	struct v4l2_subdev *encoder;	 

	struct videocodec *codec;	 
	struct videocodec *vfe;	 

	struct mutex lock;	 

	u8 initialized;		 
	struct card_info card;
	const struct tvnorm *timing;

	unsigned short id;	 
	char name[32];		 
	struct pci_dev *pci_dev;	 
	unsigned char revision;	 
	unsigned char __iomem *zr36057_mem; 

	spinlock_t spinlock;	 

	 
	int input;	 
	v4l2_std_id norm;

	 
	unsigned int buffer_size;

	struct zoran_v4l_settings v4l_settings;	 

	 
	enum zoran_codec_mode codec_mode;	 
	struct zoran_jpg_settings jpg_settings;	 

	 
	 
	 
	unsigned long jpg_que_head;	 
	unsigned long jpg_dma_head;	 
	unsigned long jpg_dma_tail;	 
	unsigned long jpg_que_tail;	 
	unsigned long jpg_seq_num;	 
	unsigned long jpg_err_seq;	 
	unsigned long jpg_err_shift;
	unsigned long jpg_queued_num;	 
	unsigned long vbseq;

	 
	 
	__le32 *stat_com;

	 
	unsigned int ghost_int;
	int intr_counter_GIRQ1;
	int intr_counter_GIRQ0;
	int intr_counter_cod_rep_irq;
	int intr_counter_jpeg_rep_irq;
	int field_counter;
	int irq1_in;
	int irq1_out;
	int jpeg_in;
	int jpeg_out;
	int JPEG_0;
	int JPEG_1;
	int end_event_missed;
	int jpeg_missed;
	int jpeg_error;
	int num_errors;
	int jpeg_max_missed;
	int jpeg_min_missed;
	unsigned int prepared;
	unsigned int queued;

	u32 last_isr;
	unsigned long frame_num;
	int running;
	int buf_in_reserve;

	dma_addr_t p_sc;
	__le32 *stat_comb;
	dma_addr_t p_scb;
	enum zoran_map_mode map_mode;
	struct list_head queued_bufs;
	spinlock_t queued_bufs_lock;  
	struct zr_buffer *inuse[BUZ_NUM_STAT_COM * 2];
	struct dentry *dbgfs_dir;
};

static inline struct zoran *to_zoran(struct v4l2_device *v4l2_dev)
{
	return container_of(v4l2_dev, struct zoran, v4l2_dev);
}

 
#define btwrite(dat, adr)    writel((dat), zr->zr36057_mem + (adr))
#define btread(adr)         readl(zr->zr36057_mem + (adr))

#define btand(dat, adr)      btwrite((dat) & btread(adr), (adr))
#define btor(dat, adr)       btwrite((dat) | btread(adr), (adr))
#define btaor(dat, mask, adr) btwrite((dat) | ((mask) & btread(adr)), (adr))

#endif

 
#define zrdev_dbg(zr, format, args...) \
	pci_dbg((zr)->pci_dev, format, ##args) \

#define zrdev_err(zr, format, args...) \
	pci_err((zr)->pci_dev, format, ##args) \

#define zrdev_info(zr, format, args...) \
	pci_info((zr)->pci_dev, format, ##args) \

int zoran_queue_init(struct zoran *zr, struct vb2_queue *vq, int dir);
void zoran_queue_exit(struct zoran *zr);
int zr_set_buf(struct zoran *zr);
