#ifndef _USB_VIDEO_H_
#define _USB_VIDEO_H_
#ifndef __KERNEL__
#error "The uvcvideo.h header is deprecated, use linux/uvcvideo.h instead."
#endif  
#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/usb.h>
#include <linux/usb/video.h>
#include <linux/uvcvideo.h>
#include <linux/videodev2.h>
#include <linux/workqueue.h>
#include <media/media-device.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fh.h>
#include <media/videobuf2-v4l2.h>
#define UVC_TERM_INPUT			0x0000
#define UVC_TERM_OUTPUT			0x8000
#define UVC_TERM_DIRECTION(term)	((term)->type & 0x8000)
#define UVC_ENTITY_TYPE(entity)		((entity)->type & 0x7fff)
#define UVC_ENTITY_IS_UNIT(entity)	(((entity)->type & 0xff00) == 0)
#define UVC_ENTITY_IS_TERM(entity)	(((entity)->type & 0xff00) != 0)
#define UVC_ENTITY_IS_ITERM(entity) \
	(UVC_ENTITY_IS_TERM(entity) && \
	((entity)->type & 0x8000) == UVC_TERM_INPUT)
#define UVC_ENTITY_IS_OTERM(entity) \
	(UVC_ENTITY_IS_TERM(entity) && \
	((entity)->type & 0x8000) == UVC_TERM_OUTPUT)
#define UVC_EXT_GPIO_UNIT		0x7ffe
#define UVC_EXT_GPIO_UNIT_ID		0x100
#define DRIVER_VERSION		"1.1.1"
#define UVC_URBS		5
#define UVC_MAX_PACKETS		32
#define UVC_CTRL_CONTROL_TIMEOUT	5000
#define UVC_CTRL_STREAMING_TIMEOUT	5000
#define UVC_MAX_CONTROL_MAPPINGS	1024
#define UVC_MAX_CONTROL_MENU_ENTRIES	32
#define UVC_QUIRK_STATUS_INTERVAL	0x00000001
#define UVC_QUIRK_PROBE_MINMAX		0x00000002
#define UVC_QUIRK_PROBE_EXTRAFIELDS	0x00000004
#define UVC_QUIRK_BUILTIN_ISIGHT	0x00000008
#define UVC_QUIRK_STREAM_NO_FID		0x00000010
#define UVC_QUIRK_IGNORE_SELECTOR_UNIT	0x00000020
#define UVC_QUIRK_FIX_BANDWIDTH		0x00000080
#define UVC_QUIRK_PROBE_DEF		0x00000100
#define UVC_QUIRK_RESTRICT_FRAME_RATE	0x00000200
#define UVC_QUIRK_RESTORE_CTRLS_ON_INIT	0x00000400
#define UVC_QUIRK_FORCE_Y8		0x00000800
#define UVC_QUIRK_FORCE_BPP		0x00001000
#define UVC_QUIRK_WAKE_AUTOSUSPEND	0x00002000
#define UVC_FMT_FLAG_COMPRESSED		0x00000001
#define UVC_FMT_FLAG_STREAM		0x00000002
struct gpio_desc;
struct sg_table;
struct uvc_device;
struct uvc_control_info {
	struct list_head mappings;
	u8 entity[16];
	u8 index;	 
	u8 selector;
	u16 size;
	u32 flags;
};
struct uvc_control_mapping {
	struct list_head list;
	struct list_head ev_subs;
	u32 id;
	char *name;
	u8 entity[16];
	u8 selector;
	u8 size;
	u8 offset;
	enum v4l2_ctrl_type v4l2_type;
	u32 data_type;
	const u32 *menu_mapping;
	const char (*menu_names)[UVC_MENU_NAME_LEN];
	unsigned long menu_mask;
	u32 master_id;
	s32 master_manual;
	u32 slave_ids[2];
	s32 (*get)(struct uvc_control_mapping *mapping, u8 query,
		   const u8 *data);
	void (*set)(struct uvc_control_mapping *mapping, s32 value,
		    u8 *data);
};
struct uvc_control {
	struct uvc_entity *entity;
	struct uvc_control_info info;
	u8 index;	 
	u8 dirty:1,
	   loaded:1,
	   modified:1,
	   cached:1,
	   initialized:1;
	u8 *uvc_data;
	struct uvc_fh *handle;	 
};
#define UVC_ENTITY_FLAG_DEFAULT		(1 << 0)
struct uvc_entity {
	struct list_head list;		 
	struct list_head chain;		 
	unsigned int flags;
	u16 id;
	u16 type;
	char name[64];
	u8 guid[16];
	struct video_device *vdev;
	struct v4l2_subdev subdev;
	unsigned int num_pads;
	unsigned int num_links;
	struct media_pad *pads;
	union {
		struct {
			u16 wObjectiveFocalLengthMin;
			u16 wObjectiveFocalLengthMax;
			u16 wOcularFocalLength;
			u8  bControlSize;
			u8  *bmControls;
		} camera;
		struct {
			u8  bControlSize;
			u8  *bmControls;
			u8  bTransportModeSize;
			u8  *bmTransportModes;
		} media;
		struct {
		} output;
		struct {
			u16 wMaxMultiplier;
			u8  bControlSize;
			u8  *bmControls;
			u8  bmVideoStandards;
		} processing;
		struct {
		} selector;
		struct {
			u8  bNumControls;
			u8  bControlSize;
			u8  *bmControls;
			u8  *bmControlsType;
		} extension;
		struct {
			u8  bControlSize;
			u8  *bmControls;
			struct gpio_desc *gpio_privacy;
			int irq;
		} gpio;
	};
	u8 bNrInPins;
	u8 *baSourceID;
	int (*get_info)(struct uvc_device *dev, struct uvc_entity *entity,
			u8 cs, u8 *caps);
	int (*get_cur)(struct uvc_device *dev, struct uvc_entity *entity,
		       u8 cs, void *data, u16 size);
	unsigned int ncontrols;
	struct uvc_control *controls;
};
struct uvc_frame {
	u8  bFrameIndex;
	u8  bmCapabilities;
	u16 wWidth;
	u16 wHeight;
	u32 dwMinBitRate;
	u32 dwMaxBitRate;
	u32 dwMaxVideoFrameBufferSize;
	u8  bFrameIntervalType;
	u32 dwDefaultFrameInterval;
	const u32 *dwFrameInterval;
};
struct uvc_format {
	u8 type;
	u8 index;
	u8 bpp;
	enum v4l2_colorspace colorspace;
	enum v4l2_xfer_func xfer_func;
	enum v4l2_ycbcr_encoding ycbcr_enc;
	u32 fcc;
	u32 flags;
	unsigned int nframes;
	const struct uvc_frame *frames;
};
struct uvc_streaming_header {
	u8 bNumFormats;
	u8 bEndpointAddress;
	u8 bTerminalLink;
	u8 bControlSize;
	u8 *bmaControls;
	u8 bmInfo;
	u8 bStillCaptureMethod;
	u8 bTriggerSupport;
	u8 bTriggerUsage;
};
enum uvc_buffer_state {
	UVC_BUF_STATE_IDLE	= 0,
	UVC_BUF_STATE_QUEUED	= 1,
	UVC_BUF_STATE_ACTIVE	= 2,
	UVC_BUF_STATE_READY	= 3,
	UVC_BUF_STATE_DONE	= 4,
	UVC_BUF_STATE_ERROR	= 5,
};
struct uvc_buffer {
	struct vb2_v4l2_buffer buf;
	struct list_head queue;
	enum uvc_buffer_state state;
	unsigned int error;
	void *mem;
	unsigned int length;
	unsigned int bytesused;
	u32 pts;
	struct kref ref;
};
#define UVC_QUEUE_DISCONNECTED		(1 << 0)
#define UVC_QUEUE_DROP_CORRUPTED	(1 << 1)
struct uvc_video_queue {
	struct vb2_queue queue;
	struct mutex mutex;			 
	unsigned int flags;
	unsigned int buf_used;
	spinlock_t irqlock;			 
	struct list_head irqqueue;
};
struct uvc_video_chain {
	struct uvc_device *dev;
	struct list_head list;
	struct list_head entities;		 
	struct uvc_entity *processing;		 
	struct uvc_entity *selector;		 
	struct mutex ctrl_mutex;		 
	struct v4l2_prio_state prio;		 
	u32 caps;				 
	u8 ctrl_class_bitmap;			 
};
struct uvc_stats_frame {
	unsigned int size;		 
	unsigned int first_data;	 
	unsigned int nb_packets;	 
	unsigned int nb_empty;		 
	unsigned int nb_invalid;	 
	unsigned int nb_errors;		 
	unsigned int nb_pts;		 
	unsigned int nb_pts_diffs;	 
	unsigned int last_pts_diff;	 
	bool has_initial_pts;		 
	bool has_early_pts;		 
	u32 pts;			 
	unsigned int nb_scr;		 
	unsigned int nb_scr_diffs;	 
	u16 scr_sof;			 
	u32 scr_stc;			 
};
struct uvc_stats_stream {
	ktime_t start_ts;		 
	ktime_t stop_ts;		 
	unsigned int nb_frames;		 
	unsigned int nb_packets;	 
	unsigned int nb_empty;		 
	unsigned int nb_invalid;	 
	unsigned int nb_errors;		 
	unsigned int nb_pts_constant;	 
	unsigned int nb_pts_early;	 
	unsigned int nb_pts_initial;	 
	unsigned int nb_scr_count_ok;	 
	unsigned int nb_scr_diffs_ok;	 
	unsigned int scr_sof_count;	 
	unsigned int scr_sof;		 
	unsigned int min_sof;		 
	unsigned int max_sof;		 
};
#define UVC_METADATA_BUF_SIZE 10240
struct uvc_copy_op {
	struct uvc_buffer *buf;
	void *dst;
	const __u8 *src;
	size_t len;
};
struct uvc_urb {
	struct urb *urb;
	struct uvc_streaming *stream;
	char *buffer;
	dma_addr_t dma;
	struct sg_table *sgt;
	unsigned int async_operations;
	struct uvc_copy_op copy_operations[UVC_MAX_PACKETS];
	struct work_struct work;
};
struct uvc_streaming {
	struct list_head list;
	struct uvc_device *dev;
	struct video_device vdev;
	struct uvc_video_chain *chain;
	atomic_t active;
	struct usb_interface *intf;
	int intfnum;
	u16 maxpsize;
	struct uvc_streaming_header header;
	enum v4l2_buf_type type;
	unsigned int nformats;
	const struct uvc_format *formats;
	struct uvc_streaming_control ctrl;
	const struct uvc_format *def_format;
	const struct uvc_format *cur_format;
	const struct uvc_frame *cur_frame;
	struct mutex mutex;
	unsigned int frozen : 1;
	struct uvc_video_queue queue;
	struct workqueue_struct *async_wq;
	void (*decode)(struct uvc_urb *uvc_urb, struct uvc_buffer *buf,
		       struct uvc_buffer *meta_buf);
	struct {
		struct video_device vdev;
		struct uvc_video_queue queue;
		u32 format;
	} meta;
	struct {
		u8 header[256];
		unsigned int header_size;
		int skip_payload;
		u32 payload_size;
		u32 max_payload_size;
	} bulk;
	struct uvc_urb uvc_urb[UVC_URBS];
	unsigned int urb_size;
	u32 sequence;
	u8 last_fid;
	struct dentry *debugfs_dir;
	struct {
		struct uvc_stats_frame frame;
		struct uvc_stats_stream stream;
	} stats;
	struct uvc_clock {
		struct uvc_clock_sample {
			u32 dev_stc;
			u16 dev_sof;
			u16 host_sof;
			ktime_t host_time;
		} *samples;
		unsigned int head;
		unsigned int count;
		unsigned int size;
		u16 last_sof;
		u16 sof_offset;
		u8 last_scr[6];
		spinlock_t lock;
	} clock;
};
#define for_each_uvc_urb(uvc_urb, uvc_streaming) \
	for ((uvc_urb) = &(uvc_streaming)->uvc_urb[0]; \
	     (uvc_urb) < &(uvc_streaming)->uvc_urb[UVC_URBS]; \
	     ++(uvc_urb))
static inline u32 uvc_urb_index(const struct uvc_urb *uvc_urb)
{
	return uvc_urb - &uvc_urb->stream->uvc_urb[0];
}
struct uvc_device_info {
	u32	quirks;
	u32	meta_format;
	u16	uvc_version;
	const struct uvc_control_mapping **mappings;
};
struct uvc_status_streaming {
	u8	button;
} __packed;
struct uvc_status_control {
	u8	bSelector;
	u8	bAttribute;
	u8	bValue[11];
} __packed;
struct uvc_status {
	u8	bStatusType;
	u8	bOriginator;
	u8	bEvent;
	union {
		struct uvc_status_control control;
		struct uvc_status_streaming streaming;
	};
} __packed;
struct uvc_device {
	struct usb_device *udev;
	struct usb_interface *intf;
	unsigned long warnings;
	u32 quirks;
	int intfnum;
	char name[32];
	const struct uvc_device_info *info;
	struct mutex lock;		 
	unsigned int users;
	atomic_t nmappings;
#ifdef CONFIG_MEDIA_CONTROLLER
	struct media_device mdev;
#endif
	struct v4l2_device vdev;
	u16 uvc_version;
	u32 clock_frequency;
	struct list_head entities;
	struct list_head chains;
	struct list_head streams;
	struct kref ref;
	struct usb_host_endpoint *int_ep;
	struct urb *int_urb;
	struct uvc_status *status;
	bool flush_status;
	struct input_dev *input;
	char input_phys[64];
	struct uvc_ctrl_work {
		struct work_struct work;
		struct urb *urb;
		struct uvc_video_chain *chain;
		struct uvc_control *ctrl;
		const void *data;
	} async_ctrl;
	struct uvc_entity *gpio_unit;
};
enum uvc_handle_state {
	UVC_HANDLE_PASSIVE	= 0,
	UVC_HANDLE_ACTIVE	= 1,
};
struct uvc_fh {
	struct v4l2_fh vfh;
	struct uvc_video_chain *chain;
	struct uvc_streaming *stream;
	enum uvc_handle_state state;
};
struct uvc_driver {
	struct usb_driver driver;
};
#define UVC_DBG_PROBE		(1 << 0)
#define UVC_DBG_DESCR		(1 << 1)
#define UVC_DBG_CONTROL		(1 << 2)
#define UVC_DBG_FORMAT		(1 << 3)
#define UVC_DBG_CAPTURE		(1 << 4)
#define UVC_DBG_CALLS		(1 << 5)
#define UVC_DBG_FRAME		(1 << 7)
#define UVC_DBG_SUSPEND		(1 << 8)
#define UVC_DBG_STATUS		(1 << 9)
#define UVC_DBG_VIDEO		(1 << 10)
#define UVC_DBG_STATS		(1 << 11)
#define UVC_DBG_CLOCK		(1 << 12)
#define UVC_WARN_MINMAX		0
#define UVC_WARN_PROBE_DEF	1
#define UVC_WARN_XU_GET_RES	2
extern unsigned int uvc_clock_param;
extern unsigned int uvc_no_drop_param;
extern unsigned int uvc_dbg_param;
extern unsigned int uvc_timeout_param;
extern unsigned int uvc_hw_timestamps_param;
#define uvc_dbg(_dev, flag, fmt, ...)					\
do {									\
	if (uvc_dbg_param & UVC_DBG_##flag)				\
		dev_printk(KERN_DEBUG, &(_dev)->udev->dev, fmt,		\
			   ##__VA_ARGS__);				\
} while (0)
#define uvc_dbg_cont(flag, fmt, ...)					\
do {									\
	if (uvc_dbg_param & UVC_DBG_##flag)				\
		pr_cont(fmt, ##__VA_ARGS__);				\
} while (0)
#define uvc_warn_once(_dev, warn, fmt, ...)				\
do {									\
	if (!test_and_set_bit(warn, &(_dev)->warnings))			\
		dev_info(&(_dev)->udev->dev, fmt, ##__VA_ARGS__);	\
} while (0)
extern struct uvc_driver uvc_driver;
struct uvc_entity *uvc_entity_by_id(struct uvc_device *dev, int id);
int uvc_queue_init(struct uvc_video_queue *queue, enum v4l2_buf_type type,
		   int drop_corrupted);
void uvc_queue_release(struct uvc_video_queue *queue);
int uvc_request_buffers(struct uvc_video_queue *queue,
			struct v4l2_requestbuffers *rb);
int uvc_query_buffer(struct uvc_video_queue *queue,
		     struct v4l2_buffer *v4l2_buf);
int uvc_create_buffers(struct uvc_video_queue *queue,
		       struct v4l2_create_buffers *v4l2_cb);
int uvc_queue_buffer(struct uvc_video_queue *queue,
		     struct media_device *mdev,
		     struct v4l2_buffer *v4l2_buf);
int uvc_export_buffer(struct uvc_video_queue *queue,
		      struct v4l2_exportbuffer *exp);
int uvc_dequeue_buffer(struct uvc_video_queue *queue,
		       struct v4l2_buffer *v4l2_buf, int nonblocking);
int uvc_queue_streamon(struct uvc_video_queue *queue, enum v4l2_buf_type type);
int uvc_queue_streamoff(struct uvc_video_queue *queue, enum v4l2_buf_type type);
void uvc_queue_cancel(struct uvc_video_queue *queue, int disconnect);
struct uvc_buffer *uvc_queue_next_buffer(struct uvc_video_queue *queue,
					 struct uvc_buffer *buf);
struct uvc_buffer *uvc_queue_get_current_buffer(struct uvc_video_queue *queue);
void uvc_queue_buffer_release(struct uvc_buffer *buf);
int uvc_queue_mmap(struct uvc_video_queue *queue,
		   struct vm_area_struct *vma);
__poll_t uvc_queue_poll(struct uvc_video_queue *queue, struct file *file,
			poll_table *wait);
#ifndef CONFIG_MMU
unsigned long uvc_queue_get_unmapped_area(struct uvc_video_queue *queue,
					  unsigned long pgoff);
#endif
int uvc_queue_allocated(struct uvc_video_queue *queue);
static inline int uvc_queue_streaming(struct uvc_video_queue *queue)
{
	return vb2_is_streaming(&queue->queue);
}
static inline struct uvc_streaming *
uvc_queue_to_stream(struct uvc_video_queue *queue)
{
	return container_of(queue, struct uvc_streaming, queue);
}
extern const struct v4l2_ioctl_ops uvc_ioctl_ops;
extern const struct v4l2_file_operations uvc_fops;
int uvc_mc_register_entities(struct uvc_video_chain *chain);
void uvc_mc_cleanup_entity(struct uvc_entity *entity);
int uvc_video_init(struct uvc_streaming *stream);
int uvc_video_suspend(struct uvc_streaming *stream);
int uvc_video_resume(struct uvc_streaming *stream, int reset);
int uvc_video_start_streaming(struct uvc_streaming *stream);
void uvc_video_stop_streaming(struct uvc_streaming *stream);
int uvc_probe_video(struct uvc_streaming *stream,
		    struct uvc_streaming_control *probe);
int uvc_query_ctrl(struct uvc_device *dev, u8 query, u8 unit,
		   u8 intfnum, u8 cs, void *data, u16 size);
void uvc_video_clock_update(struct uvc_streaming *stream,
			    struct vb2_v4l2_buffer *vbuf,
			    struct uvc_buffer *buf);
int uvc_meta_register(struct uvc_streaming *stream);
int uvc_register_video_device(struct uvc_device *dev,
			      struct uvc_streaming *stream,
			      struct video_device *vdev,
			      struct uvc_video_queue *queue,
			      enum v4l2_buf_type type,
			      const struct v4l2_file_operations *fops,
			      const struct v4l2_ioctl_ops *ioctl_ops);
int uvc_status_init(struct uvc_device *dev);
void uvc_status_unregister(struct uvc_device *dev);
void uvc_status_cleanup(struct uvc_device *dev);
int uvc_status_start(struct uvc_device *dev, gfp_t flags);
void uvc_status_stop(struct uvc_device *dev);
extern const struct uvc_control_mapping uvc_ctrl_power_line_mapping_limited;
extern const struct uvc_control_mapping uvc_ctrl_power_line_mapping_uvc11;
extern const struct v4l2_subscribed_event_ops uvc_ctrl_sub_ev_ops;
int uvc_query_v4l2_ctrl(struct uvc_video_chain *chain,
			struct v4l2_queryctrl *v4l2_ctrl);
int uvc_query_v4l2_menu(struct uvc_video_chain *chain,
			struct v4l2_querymenu *query_menu);
int uvc_ctrl_add_mapping(struct uvc_video_chain *chain,
			 const struct uvc_control_mapping *mapping);
int uvc_ctrl_init_device(struct uvc_device *dev);
void uvc_ctrl_cleanup_device(struct uvc_device *dev);
int uvc_ctrl_restore_values(struct uvc_device *dev);
bool uvc_ctrl_status_event_async(struct urb *urb, struct uvc_video_chain *chain,
				 struct uvc_control *ctrl, const u8 *data);
void uvc_ctrl_status_event(struct uvc_video_chain *chain,
			   struct uvc_control *ctrl, const u8 *data);
int uvc_ctrl_begin(struct uvc_video_chain *chain);
int __uvc_ctrl_commit(struct uvc_fh *handle, int rollback,
		      struct v4l2_ext_controls *ctrls);
static inline int uvc_ctrl_commit(struct uvc_fh *handle,
				  struct v4l2_ext_controls *ctrls)
{
	return __uvc_ctrl_commit(handle, 0, ctrls);
}
static inline int uvc_ctrl_rollback(struct uvc_fh *handle)
{
	return __uvc_ctrl_commit(handle, 1, NULL);
}
int uvc_ctrl_get(struct uvc_video_chain *chain, struct v4l2_ext_control *xctrl);
int uvc_ctrl_set(struct uvc_fh *handle, struct v4l2_ext_control *xctrl);
int uvc_ctrl_is_accessible(struct uvc_video_chain *chain, u32 v4l2_id,
			   const struct v4l2_ext_controls *ctrls,
			   unsigned long ioctl);
int uvc_xu_ctrl_query(struct uvc_video_chain *chain,
		      struct uvc_xu_control_query *xqry);
struct usb_host_endpoint *uvc_find_endpoint(struct usb_host_interface *alts,
					    u8 epaddr);
u16 uvc_endpoint_max_bpi(struct usb_device *dev, struct usb_host_endpoint *ep);
void uvc_video_decode_isight(struct uvc_urb *uvc_urb,
			     struct uvc_buffer *buf,
			     struct uvc_buffer *meta_buf);
void uvc_debugfs_init(void);
void uvc_debugfs_cleanup(void);
void uvc_debugfs_init_stream(struct uvc_streaming *stream);
void uvc_debugfs_cleanup_stream(struct uvc_streaming *stream);
size_t uvc_video_stats_dump(struct uvc_streaming *stream, char *buf,
			    size_t size);
#endif
