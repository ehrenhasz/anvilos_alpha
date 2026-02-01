 
 

#include <linux/clk.h>
#include <linux/ktime.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>

#include <media/videobuf2-dma-contig.h>

#define BDISP_NAME              "bdisp"

 
#define MAX_OUTPUT_PLANES       2
#define MAX_VERTICAL_STRIDES    2
#define MAX_NB_NODE             (MAX_OUTPUT_PLANES * MAX_VERTICAL_STRIDES)

 
struct bdisp_ctrls {
	struct v4l2_ctrl        *hflip;
	struct v4l2_ctrl        *vflip;
};

 
struct bdisp_fmt {
	u32                     pixelformat;
	u8                      nb_planes;
	u8                      bpp;
	u8                      bpp_plane0;
	u8                      w_align;
	u8                      h_align;
};

 
struct bdisp_frame {
	u32                     width;
	u32                     height;
	const struct bdisp_fmt  *fmt;
	enum v4l2_field         field;
	u32                     bytesperline;
	u32                     sizeimage;
	enum v4l2_colorspace    colorspace;
	struct v4l2_rect        crop;
	dma_addr_t              paddr[4];
};

 
struct bdisp_request {
	struct bdisp_frame      src;
	struct bdisp_frame      dst;
	unsigned int            hflip:1;
	unsigned int            vflip:1;
	int                     nb_req;
};

 
struct bdisp_ctx {
	struct bdisp_frame      src;
	struct bdisp_frame      dst;
	u32                     state;
	unsigned int            hflip:1;
	unsigned int            vflip:1;
	struct bdisp_dev        *bdisp_dev;
	struct bdisp_node       *node[MAX_NB_NODE];
	dma_addr_t              node_paddr[MAX_NB_NODE];
	struct v4l2_fh          fh;
	struct v4l2_ctrl_handler ctrl_handler;
	struct bdisp_ctrls      bdisp_ctrls;
	bool                    ctrls_rdy;
};

 
struct bdisp_m2m_device {
	struct video_device     *vdev;
	struct v4l2_m2m_dev     *m2m_dev;
	struct bdisp_ctx        *ctx;
	int                     refcnt;
};

 
struct bdisp_dbg {
	struct dentry           *debugfs_entry;
	struct bdisp_node       *copy_node[MAX_NB_NODE];
	struct bdisp_request    copy_request;
	ktime_t                 hw_start;
	s64                     last_duration;
	s64                     min_duration;
	s64                     max_duration;
	s64                     tot_duration;
};

 
struct bdisp_dev {
	struct v4l2_device      v4l2_dev;
	struct video_device     vdev;
	struct platform_device  *pdev;
	struct device           *dev;
	spinlock_t              slock;
	struct mutex            lock;
	u16                     id;
	struct bdisp_m2m_device m2m;
	unsigned long           state;
	struct clk              *clock;
	void __iomem            *regs;
	wait_queue_head_t       irq_queue;
	struct workqueue_struct *work_queue;
	struct delayed_work     timeout_work;
	struct bdisp_dbg        dbg;
};

void bdisp_hw_free_nodes(struct bdisp_ctx *ctx);
int bdisp_hw_alloc_nodes(struct bdisp_ctx *ctx);
void bdisp_hw_free_filters(struct device *dev);
int bdisp_hw_alloc_filters(struct device *dev);
int bdisp_hw_reset(struct bdisp_dev *bdisp);
int bdisp_hw_get_and_clear_irq(struct bdisp_dev *bdisp);
int bdisp_hw_update(struct bdisp_ctx *ctx);

void bdisp_debugfs_remove(struct bdisp_dev *bdisp);
void bdisp_debugfs_create(struct bdisp_dev *bdisp);
void bdisp_dbg_perf_begin(struct bdisp_dev *bdisp);
void bdisp_dbg_perf_end(struct bdisp_dev *bdisp);
