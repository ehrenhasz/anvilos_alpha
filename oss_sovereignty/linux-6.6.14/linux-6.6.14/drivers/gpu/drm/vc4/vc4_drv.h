#ifndef _VC4_DRV_H_
#define _VC4_DRV_H_
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/refcount.h>
#include <linux/uaccess.h>
#include <drm/drm_atomic.h>
#include <drm/drm_debugfs.h>
#include <drm/drm_device.h>
#include <drm/drm_encoder.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_managed.h>
#include <drm/drm_mm.h>
#include <drm/drm_modeset_lock.h>
#include <kunit/test-bug.h>
#include "uapi/drm/vc4_drm.h"
struct drm_device;
struct drm_gem_object;
extern const struct drm_driver vc4_drm_driver;
extern const struct drm_driver vc5_drm_driver;
enum vc4_kernel_bo_type {
	VC4_BO_TYPE_KERNEL,
	VC4_BO_TYPE_V3D,
	VC4_BO_TYPE_V3D_SHADER,
	VC4_BO_TYPE_DUMB,
	VC4_BO_TYPE_BIN,
	VC4_BO_TYPE_RCL,
	VC4_BO_TYPE_BCL,
	VC4_BO_TYPE_KERNEL_CACHE,
	VC4_BO_TYPE_COUNT
};
struct vc4_perfmon {
	struct vc4_dev *dev;
	refcount_t refcnt;
	u8 ncounters;
	u8 events[DRM_VC4_MAX_PERF_COUNTERS];
	u64 counters[];
};
struct vc4_dev {
	struct drm_device base;
	struct device *dev;
	bool is_vc5;
	unsigned int irq;
	struct vc4_hvs *hvs;
	struct vc4_v3d *v3d;
	struct vc4_hang_state *hang_state;
	struct vc4_bo_cache {
		struct list_head *size_list;
		uint32_t size_list_size;
		struct list_head time_list;
		struct work_struct time_work;
		struct timer_list time_timer;
	} bo_cache;
	u32 num_labels;
	struct vc4_label {
		const char *name;
		u32 num_allocated;
		u32 size_allocated;
	} *bo_labels;
	struct mutex bo_lock;
	struct {
		struct list_head list;
		unsigned int num;
		size_t size;
		unsigned int purged_num;
		size_t purged_size;
		struct mutex lock;
	} purgeable;
	uint64_t dma_fence_context;
	uint64_t emit_seqno;
	uint64_t finished_seqno;
	struct list_head bin_job_list;
	struct list_head render_job_list;
	struct list_head job_done_list;
	spinlock_t job_lock;
	wait_queue_head_t job_wait_queue;
	struct work_struct job_done_work;
	struct vc4_perfmon *active_perfmon;
	struct list_head seqno_cb_list;
	struct vc4_bo *bin_bo;
	uint32_t bin_alloc_size;
	uint32_t bin_alloc_used;
	uint32_t bin_alloc_overflow;
	atomic_t underrun;
	struct work_struct overflow_mem_work;
	int power_refcount;
	bool load_tracker_enabled;
	struct mutex power_lock;
	struct {
		struct timer_list timer;
		struct work_struct reset_work;
	} hangcheck;
	struct drm_modeset_lock ctm_state_lock;
	struct drm_private_obj ctm_manager;
	struct drm_private_obj hvs_channels;
	struct drm_private_obj load_tracker;
	struct mutex bin_bo_lock;
	struct kref bin_bo_kref;
};
#define to_vc4_dev(_dev)			\
	container_of_const(_dev, struct vc4_dev, base)
struct vc4_bo {
	struct drm_gem_dma_object base;
	uint64_t seqno;
	uint64_t write_seqno;
	bool t_format;
	struct list_head unref_head;
	unsigned long free_time;
	struct list_head size_head;
	struct vc4_validated_shader_info *validated_shader;
	int label;
	refcount_t usecnt;
	u32 madv;
	struct mutex madv_lock;
};
#define to_vc4_bo(_bo)							\
	container_of_const(to_drm_gem_dma_obj(_bo), struct vc4_bo, base)
struct vc4_fence {
	struct dma_fence base;
	struct drm_device *dev;
	uint64_t seqno;
};
#define to_vc4_fence(_fence)					\
	container_of_const(_fence, struct vc4_fence, base)
struct vc4_seqno_cb {
	struct work_struct work;
	uint64_t seqno;
	void (*func)(struct vc4_seqno_cb *cb);
};
struct vc4_v3d {
	struct vc4_dev *vc4;
	struct platform_device *pdev;
	void __iomem *regs;
	struct clk *clk;
	struct debugfs_regset32 regset;
};
struct vc4_hvs {
	struct vc4_dev *vc4;
	struct platform_device *pdev;
	void __iomem *regs;
	u32 __iomem *dlist;
	struct clk *core_clk;
	unsigned long max_core_rate;
	struct drm_mm dlist_mm;
	struct drm_mm lbm_mm;
	spinlock_t mm_lock;
	struct drm_mm_node mitchell_netravali_filter;
	struct debugfs_regset32 regset;
	bool vc5_hdmi_enable_hdmi_20;
	bool vc5_hdmi_enable_4096by2160;
};
#define HVS_NUM_CHANNELS 3
struct vc4_hvs_state {
	struct drm_private_state base;
	unsigned long core_clock_rate;
	struct {
		unsigned in_use: 1;
		unsigned long fifo_load;
		struct drm_crtc_commit *pending_commit;
	} fifo_state[HVS_NUM_CHANNELS];
};
#define to_vc4_hvs_state(_state)				\
	container_of_const(_state, struct vc4_hvs_state, base)
struct vc4_hvs_state *vc4_hvs_get_global_state(struct drm_atomic_state *state);
struct vc4_hvs_state *vc4_hvs_get_old_global_state(const struct drm_atomic_state *state);
struct vc4_hvs_state *vc4_hvs_get_new_global_state(const struct drm_atomic_state *state);
struct vc4_plane {
	struct drm_plane base;
};
#define to_vc4_plane(_plane)					\
	container_of_const(_plane, struct vc4_plane, base)
enum vc4_scaling_mode {
	VC4_SCALING_NONE,
	VC4_SCALING_TPZ,
	VC4_SCALING_PPF,
};
struct vc4_plane_state {
	struct drm_plane_state base;
	u32 *dlist;
	u32 dlist_size;  
	u32 dlist_count;  
	u32 pos0_offset;
	u32 pos2_offset;
	u32 ptr0_offset;
	u32 lbm_offset;
	u32 __iomem *hw_dlist;
	int crtc_x, crtc_y, crtc_w, crtc_h;
	u32 src_x, src_y;
	u32 src_w[2], src_h[2];
	enum vc4_scaling_mode x_scaling[2], y_scaling[2];
	bool is_unity;
	bool is_yuv;
	u32 offsets[3];
	struct drm_mm_node lbm;
	bool needs_bg_fill;
	bool dlist_initialized;
	u64 hvs_load;
	u64 membus_load;
};
#define to_vc4_plane_state(_state)				\
	container_of_const(_state, struct vc4_plane_state, base)
enum vc4_encoder_type {
	VC4_ENCODER_TYPE_NONE,
	VC4_ENCODER_TYPE_HDMI0,
	VC4_ENCODER_TYPE_HDMI1,
	VC4_ENCODER_TYPE_VEC,
	VC4_ENCODER_TYPE_DSI0,
	VC4_ENCODER_TYPE_DSI1,
	VC4_ENCODER_TYPE_SMI,
	VC4_ENCODER_TYPE_DPI,
	VC4_ENCODER_TYPE_TXP,
};
struct vc4_encoder {
	struct drm_encoder base;
	enum vc4_encoder_type type;
	u32 clock_select;
	void (*pre_crtc_configure)(struct drm_encoder *encoder, struct drm_atomic_state *state);
	void (*pre_crtc_enable)(struct drm_encoder *encoder, struct drm_atomic_state *state);
	void (*post_crtc_enable)(struct drm_encoder *encoder, struct drm_atomic_state *state);
	void (*post_crtc_disable)(struct drm_encoder *encoder, struct drm_atomic_state *state);
	void (*post_crtc_powerdown)(struct drm_encoder *encoder, struct drm_atomic_state *state);
};
#define to_vc4_encoder(_encoder)				\
	container_of_const(_encoder, struct vc4_encoder, base)
static inline
struct drm_encoder *vc4_find_encoder_by_type(struct drm_device *drm,
					     enum vc4_encoder_type type)
{
	struct drm_encoder *encoder;
	drm_for_each_encoder(encoder, drm) {
		struct vc4_encoder *vc4_encoder = to_vc4_encoder(encoder);
		if (vc4_encoder->type == type)
			return encoder;
	}
	return NULL;
}
struct vc4_crtc_data {
	const char *name;
	const char *debugfs_name;
	unsigned int hvs_available_channels;
	int hvs_output;
};
extern const struct vc4_crtc_data vc4_txp_crtc_data;
struct vc4_pv_data {
	struct vc4_crtc_data	base;
	unsigned int fifo_depth;
	u8 pixels_per_clock;
	enum vc4_encoder_type encoder_types[4];
};
extern const struct vc4_pv_data bcm2835_pv0_data;
extern const struct vc4_pv_data bcm2835_pv1_data;
extern const struct vc4_pv_data bcm2835_pv2_data;
extern const struct vc4_pv_data bcm2711_pv0_data;
extern const struct vc4_pv_data bcm2711_pv1_data;
extern const struct vc4_pv_data bcm2711_pv2_data;
extern const struct vc4_pv_data bcm2711_pv3_data;
extern const struct vc4_pv_data bcm2711_pv4_data;
struct vc4_crtc {
	struct drm_crtc base;
	struct platform_device *pdev;
	const struct vc4_crtc_data *data;
	void __iomem *regs;
	ktime_t t_vblank;
	u8 lut_r[256];
	u8 lut_g[256];
	u8 lut_b[256];
	struct drm_pending_vblank_event *event;
	struct debugfs_regset32 regset;
	bool feeds_txp;
	spinlock_t irq_lock;
	unsigned int current_dlist;
	unsigned int current_hvs_channel;
};
#define to_vc4_crtc(_crtc)					\
	container_of_const(_crtc, struct vc4_crtc, base)
static inline const struct vc4_crtc_data *
vc4_crtc_to_vc4_crtc_data(const struct vc4_crtc *crtc)
{
	return crtc->data;
}
static inline const struct vc4_pv_data *
vc4_crtc_to_vc4_pv_data(const struct vc4_crtc *crtc)
{
	const struct vc4_crtc_data *data = vc4_crtc_to_vc4_crtc_data(crtc);
	return container_of_const(data, struct vc4_pv_data, base);
}
struct drm_encoder *vc4_get_crtc_encoder(struct drm_crtc *crtc,
					 struct drm_crtc_state *state);
struct vc4_crtc_state {
	struct drm_crtc_state base;
	struct drm_mm_node mm;
	bool txp_armed;
	unsigned int assigned_channel;
	struct {
		unsigned int left;
		unsigned int right;
		unsigned int top;
		unsigned int bottom;
	} margins;
	unsigned long hvs_load;
	bool update_muxing;
};
#define VC4_HVS_CHANNEL_DISABLED ((unsigned int)-1)
#define to_vc4_crtc_state(_state)				\
	container_of_const(_state, struct vc4_crtc_state, base)
#define V3D_READ(offset)								\
	({										\
		kunit_fail_current_test("Accessing a register in a unit test!\n");	\
		readl(vc4->v3d->regs + (offset));						\
	})
#define V3D_WRITE(offset, val)								\
	do {										\
		kunit_fail_current_test("Accessing a register in a unit test!\n");	\
		writel(val, vc4->v3d->regs + (offset));					\
	} while (0)
#define HVS_READ(offset)								\
	({										\
		kunit_fail_current_test("Accessing a register in a unit test!\n");	\
		readl(hvs->regs + (offset));						\
	})
#define HVS_WRITE(offset, val)								\
	do {										\
		kunit_fail_current_test("Accessing a register in a unit test!\n");	\
		writel(val, hvs->regs + (offset));					\
	} while (0)
#define VC4_REG32(reg) { .name = #reg, .offset = reg }
struct vc4_exec_info {
	struct vc4_dev *dev;
	uint64_t seqno;
	uint64_t bin_dep_seqno;
	struct dma_fence *fence;
	uint32_t last_ct0ca, last_ct1ca;
	struct drm_vc4_submit_cl *args;
	struct drm_gem_object **bo;
	uint32_t bo_count;
	struct drm_gem_dma_object *rcl_write_bo[4];
	uint32_t rcl_write_bo_count;
	struct list_head head;
	struct list_head unref_list;
	uint32_t bo_index[2];
	struct drm_gem_dma_object *exec_bo;
	struct vc4_shader_state {
		uint32_t addr;
		uint32_t max_index;
	} *shader_state;
	uint32_t shader_state_size;
	uint32_t shader_state_count;
	bool found_tile_binning_mode_config_packet;
	bool found_start_tile_binning_packet;
	bool found_increment_semaphore_packet;
	bool found_flush;
	uint8_t bin_tiles_x, bin_tiles_y;
	uint32_t tile_alloc_offset;
	uint32_t bin_slots;
	uint32_t ct0ca, ct0ea;
	uint32_t ct1ca, ct1ea;
	void *bin_u;
	void *shader_rec_u;
	void *shader_rec_v;
	uint32_t shader_rec_p;
	uint32_t shader_rec_size;
	void *uniforms_u;
	void *uniforms_v;
	uint32_t uniforms_p;
	uint32_t uniforms_size;
	struct vc4_perfmon *perfmon;
	bool bin_bo_used;
};
struct vc4_file {
	struct vc4_dev *dev;
	struct {
		struct idr idr;
		struct mutex lock;
	} perfmon;
	bool bin_bo_used;
};
static inline struct vc4_exec_info *
vc4_first_bin_job(struct vc4_dev *vc4)
{
	return list_first_entry_or_null(&vc4->bin_job_list,
					struct vc4_exec_info, head);
}
static inline struct vc4_exec_info *
vc4_first_render_job(struct vc4_dev *vc4)
{
	return list_first_entry_or_null(&vc4->render_job_list,
					struct vc4_exec_info, head);
}
static inline struct vc4_exec_info *
vc4_last_render_job(struct vc4_dev *vc4)
{
	if (list_empty(&vc4->render_job_list))
		return NULL;
	return list_last_entry(&vc4->render_job_list,
			       struct vc4_exec_info, head);
}
struct vc4_texture_sample_info {
	bool is_direct;
	uint32_t p_offset[4];
};
struct vc4_validated_shader_info {
	uint32_t uniforms_size;
	uint32_t uniforms_src_size;
	uint32_t num_texture_samples;
	struct vc4_texture_sample_info *texture_samples;
	uint32_t num_uniform_addr_offsets;
	uint32_t *uniform_addr_offsets;
	bool is_threaded;
};
#define __wait_for(OP, COND, US, Wmin, Wmax) ({ \
	const ktime_t end__ = ktime_add_ns(ktime_get_raw(), 1000ll * (US)); \
	long wait__ = (Wmin);  	\
	int ret__;							\
	might_sleep();							\
	for (;;) {							\
		const bool expired__ = ktime_after(ktime_get_raw(), end__); \
		OP;							\
		 		\
		barrier();						\
		if (COND) {						\
			ret__ = 0;					\
			break;						\
		}							\
		if (expired__) {					\
			ret__ = -ETIMEDOUT;				\
			break;						\
		}							\
		usleep_range(wait__, wait__ * 2);			\
		if (wait__ < (Wmax))					\
			wait__ <<= 1;					\
	}								\
	ret__;								\
})
#define _wait_for(COND, US, Wmin, Wmax)	__wait_for(, (COND), (US), (Wmin), \
						   (Wmax))
#define wait_for(COND, MS)		_wait_for((COND), (MS) * 1000, 10, 1000)
struct drm_gem_object *vc4_create_object(struct drm_device *dev, size_t size);
struct vc4_bo *vc4_bo_create(struct drm_device *dev, size_t size,
			     bool from_cache, enum vc4_kernel_bo_type type);
int vc4_bo_dumb_create(struct drm_file *file_priv,
		       struct drm_device *dev,
		       struct drm_mode_create_dumb *args);
int vc4_create_bo_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int vc4_create_shader_bo_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file_priv);
int vc4_mmap_bo_ioctl(struct drm_device *dev, void *data,
		      struct drm_file *file_priv);
int vc4_set_tiling_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
int vc4_get_tiling_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
int vc4_get_hang_state_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv);
int vc4_label_bo_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);
int vc4_bo_cache_init(struct drm_device *dev);
int vc4_bo_inc_usecnt(struct vc4_bo *bo);
void vc4_bo_dec_usecnt(struct vc4_bo *bo);
void vc4_bo_add_to_purgeable_pool(struct vc4_bo *bo);
void vc4_bo_remove_from_purgeable_pool(struct vc4_bo *bo);
int vc4_bo_debugfs_init(struct drm_minor *minor);
extern struct platform_driver vc4_crtc_driver;
int vc4_crtc_disable_at_boot(struct drm_crtc *crtc);
int __vc4_crtc_init(struct drm_device *drm, struct platform_device *pdev,
		    struct vc4_crtc *vc4_crtc, const struct vc4_crtc_data *data,
		    struct drm_plane *primary_plane,
		    const struct drm_crtc_funcs *crtc_funcs,
		    const struct drm_crtc_helper_funcs *crtc_helper_funcs,
		    bool feeds_txp);
int vc4_crtc_init(struct drm_device *drm, struct platform_device *pdev,
		  struct vc4_crtc *vc4_crtc, const struct vc4_crtc_data *data,
		  const struct drm_crtc_funcs *crtc_funcs,
		  const struct drm_crtc_helper_funcs *crtc_helper_funcs,
		  bool feeds_txp);
int vc4_page_flip(struct drm_crtc *crtc,
		  struct drm_framebuffer *fb,
		  struct drm_pending_vblank_event *event,
		  uint32_t flags,
		  struct drm_modeset_acquire_ctx *ctx);
int vc4_crtc_atomic_check(struct drm_crtc *crtc,
			  struct drm_atomic_state *state);
struct drm_crtc_state *vc4_crtc_duplicate_state(struct drm_crtc *crtc);
void vc4_crtc_destroy_state(struct drm_crtc *crtc,
			    struct drm_crtc_state *state);
void vc4_crtc_reset(struct drm_crtc *crtc);
void vc4_crtc_handle_vblank(struct vc4_crtc *crtc);
void vc4_crtc_send_vblank(struct drm_crtc *crtc);
int vc4_crtc_late_register(struct drm_crtc *crtc);
void vc4_crtc_get_margins(struct drm_crtc_state *state,
			  unsigned int *left, unsigned int *right,
			  unsigned int *top, unsigned int *bottom);
void vc4_debugfs_init(struct drm_minor *minor);
#ifdef CONFIG_DEBUG_FS
void vc4_debugfs_add_regset32(struct drm_device *drm,
			      const char *filename,
			      struct debugfs_regset32 *regset);
#else
static inline void vc4_debugfs_add_regset32(struct drm_device *drm,
					    const char *filename,
					    struct debugfs_regset32 *regset)
{}
#endif
void __iomem *vc4_ioremap_regs(struct platform_device *dev, int index);
int vc4_dumb_fixup_args(struct drm_mode_create_dumb *args);
extern struct platform_driver vc4_dpi_driver;
extern struct platform_driver vc4_dsi_driver;
extern const struct dma_fence_ops vc4_fence_ops;
int vc4_gem_init(struct drm_device *dev);
int vc4_submit_cl_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv);
int vc4_wait_seqno_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
int vc4_wait_bo_ioctl(struct drm_device *dev, void *data,
		      struct drm_file *file_priv);
void vc4_submit_next_bin_job(struct drm_device *dev);
void vc4_submit_next_render_job(struct drm_device *dev);
void vc4_move_job_to_render(struct drm_device *dev, struct vc4_exec_info *exec);
int vc4_wait_for_seqno(struct drm_device *dev, uint64_t seqno,
		       uint64_t timeout_ns, bool interruptible);
void vc4_job_handle_completed(struct vc4_dev *vc4);
int vc4_queue_seqno_cb(struct drm_device *dev,
		       struct vc4_seqno_cb *cb, uint64_t seqno,
		       void (*func)(struct vc4_seqno_cb *cb));
int vc4_gem_madvise_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv);
extern struct platform_driver vc4_hdmi_driver;
extern struct platform_driver vc4_vec_driver;
extern struct platform_driver vc4_txp_driver;
void vc4_irq_enable(struct drm_device *dev);
void vc4_irq_disable(struct drm_device *dev);
int vc4_irq_install(struct drm_device *dev, int irq);
void vc4_irq_uninstall(struct drm_device *dev);
void vc4_irq_reset(struct drm_device *dev);
extern struct platform_driver vc4_hvs_driver;
struct vc4_hvs *__vc4_hvs_alloc(struct vc4_dev *vc4, struct platform_device *pdev);
void vc4_hvs_stop_channel(struct vc4_hvs *hvs, unsigned int output);
int vc4_hvs_get_fifo_from_output(struct vc4_hvs *hvs, unsigned int output);
u8 vc4_hvs_get_fifo_frame_count(struct vc4_hvs *hvs, unsigned int fifo);
int vc4_hvs_atomic_check(struct drm_crtc *crtc, struct drm_atomic_state *state);
void vc4_hvs_atomic_begin(struct drm_crtc *crtc, struct drm_atomic_state *state);
void vc4_hvs_atomic_enable(struct drm_crtc *crtc, struct drm_atomic_state *state);
void vc4_hvs_atomic_disable(struct drm_crtc *crtc, struct drm_atomic_state *state);
void vc4_hvs_atomic_flush(struct drm_crtc *crtc, struct drm_atomic_state *state);
void vc4_hvs_dump_state(struct vc4_hvs *hvs);
void vc4_hvs_unmask_underrun(struct vc4_hvs *hvs, int channel);
void vc4_hvs_mask_underrun(struct vc4_hvs *hvs, int channel);
int vc4_hvs_debugfs_init(struct drm_minor *minor);
int vc4_kms_load(struct drm_device *dev);
struct drm_plane *vc4_plane_init(struct drm_device *dev,
				 enum drm_plane_type type,
				 uint32_t possible_crtcs);
int vc4_plane_create_additional_planes(struct drm_device *dev);
u32 vc4_plane_write_dlist(struct drm_plane *plane, u32 __iomem *dlist);
u32 vc4_plane_dlist_size(const struct drm_plane_state *state);
void vc4_plane_async_set_fb(struct drm_plane *plane,
			    struct drm_framebuffer *fb);
extern struct platform_driver vc4_v3d_driver;
extern const struct of_device_id vc4_v3d_dt_match[];
int vc4_v3d_get_bin_slot(struct vc4_dev *vc4);
int vc4_v3d_bin_bo_get(struct vc4_dev *vc4, bool *used);
void vc4_v3d_bin_bo_put(struct vc4_dev *vc4);
int vc4_v3d_pm_get(struct vc4_dev *vc4);
void vc4_v3d_pm_put(struct vc4_dev *vc4);
int vc4_v3d_debugfs_init(struct drm_minor *minor);
int
vc4_validate_bin_cl(struct drm_device *dev,
		    void *validated,
		    void *unvalidated,
		    struct vc4_exec_info *exec);
int
vc4_validate_shader_recs(struct drm_device *dev, struct vc4_exec_info *exec);
struct drm_gem_dma_object *vc4_use_bo(struct vc4_exec_info *exec,
				      uint32_t hindex);
int vc4_get_rcl(struct drm_device *dev, struct vc4_exec_info *exec);
bool vc4_check_tex_size(struct vc4_exec_info *exec,
			struct drm_gem_dma_object *fbo,
			uint32_t offset, uint8_t tiling_format,
			uint32_t width, uint32_t height, uint8_t cpp);
struct vc4_validated_shader_info *
vc4_validate_shader(struct drm_gem_dma_object *shader_obj);
void vc4_perfmon_get(struct vc4_perfmon *perfmon);
void vc4_perfmon_put(struct vc4_perfmon *perfmon);
void vc4_perfmon_start(struct vc4_dev *vc4, struct vc4_perfmon *perfmon);
void vc4_perfmon_stop(struct vc4_dev *vc4, struct vc4_perfmon *perfmon,
		      bool capture);
struct vc4_perfmon *vc4_perfmon_find(struct vc4_file *vc4file, int id);
void vc4_perfmon_open_file(struct vc4_file *vc4file);
void vc4_perfmon_close_file(struct vc4_file *vc4file);
int vc4_perfmon_create_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv);
int vc4_perfmon_destroy_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
int vc4_perfmon_get_values_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
#endif  
