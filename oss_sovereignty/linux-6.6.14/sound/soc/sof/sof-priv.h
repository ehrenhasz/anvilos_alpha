 
 

#ifndef __SOUND_SOC_SOF_PRIV_H
#define __SOUND_SOC_SOF_PRIV_H

#include <linux/device.h>
#include <sound/hdaudio.h>
#include <sound/sof.h>
#include <sound/sof/info.h>
#include <sound/sof/pm.h>
#include <sound/sof/trace.h>
#include <uapi/sound/sof/fw.h>
#include <sound/sof/ext_manifest.h>

struct snd_sof_pcm_stream;

 
#define SOF_DBG_ENABLE_TRACE	BIT(0)
#define SOF_DBG_RETAIN_CTX	BIT(1)	 
#define SOF_DBG_VERIFY_TPLG	BIT(2)  
#define SOF_DBG_DYNAMIC_PIPELINES_OVERRIDE	BIT(3)  
#define SOF_DBG_DYNAMIC_PIPELINES_ENABLE	BIT(4)  
#define SOF_DBG_DISABLE_MULTICORE		BIT(5)  
#define SOF_DBG_PRINT_ALL_DUMPS		BIT(6)  
#define SOF_DBG_IGNORE_D3_PERSISTENT		BIT(7)  
#define SOF_DBG_PRINT_DMA_POSITION_UPDATE_LOGS	BIT(8)  
#define SOF_DBG_PRINT_IPC_SUCCESS_LOGS		BIT(9)  
#define SOF_DBG_FORCE_NOCODEC			BIT(10)  
#define SOF_DBG_DUMP_IPC_MESSAGE_PAYLOAD	BIT(11)  
#define SOF_DBG_DSPLESS_MODE			BIT(15)  

 
#define SOF_DBG_DUMP_REGS		BIT(0)
#define SOF_DBG_DUMP_MBOX		BIT(1)
#define SOF_DBG_DUMP_TEXT		BIT(2)
#define SOF_DBG_DUMP_PCI		BIT(3)
 
#define SOF_DBG_DUMP_OPTIONAL		BIT(4)

 
bool sof_debug_check_flag(int mask);

 
#define SND_SOF_BARS	8

 
#define SND_SOF_SUSPEND_DELAY_MS	2000

 
#define DMA_BUF_SIZE_FOR_TRACE (PAGE_SIZE * 16)

#define SOF_IPC_DSP_REPLY		0
#define SOF_IPC_HOST_REPLY		1

 
#define SOF_DAI_STREAM(sname, scmin, scmax, srates, sfmt) \
	{.stream_name = sname, .channels_min = scmin, .channels_max = scmax, \
	 .rates = srates, .formats = sfmt}

#define SOF_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | \
	SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_FLOAT)

 
#define SOF_DSP_PRIMARY_CORE 0

 
#define SOF_MAX_DSP_NUM_CORES 8

struct sof_dsp_power_state {
	u32 state;
	u32 substate;  
};

 
enum sof_system_suspend_state {
	SOF_SUSPEND_NONE = 0,
	SOF_SUSPEND_S0IX,
	SOF_SUSPEND_S3,
	SOF_SUSPEND_S4,
	SOF_SUSPEND_S5,
};

enum sof_dfsentry_type {
	SOF_DFSENTRY_TYPE_IOMEM = 0,
	SOF_DFSENTRY_TYPE_BUF,
};

enum sof_debugfs_access_type {
	SOF_DEBUGFS_ACCESS_ALWAYS = 0,
	SOF_DEBUGFS_ACCESS_D0_ONLY,
};

struct sof_compr_stream {
	u64 copied_total;
	u32 sampling_rate;
	u16 channels;
	u16 sample_container_bytes;
	size_t posn_offset;
};

struct snd_sof_dev;
struct snd_sof_ipc_msg;
struct snd_sof_ipc;
struct snd_sof_debugfs_map;
struct snd_soc_tplg_ops;
struct snd_soc_component;
struct snd_sof_pdata;

 
struct snd_sof_platform_stream_params {
	u16 stream_tag;
	bool use_phy_address;
	u32 phy_addr;
	bool no_ipc_position;
	bool cont_update_posn;
};

 
struct sof_firmware {
	const struct firmware *fw;
	u32 payload_offset;
};

 
struct snd_sof_dsp_ops {

	 
	int (*probe)(struct snd_sof_dev *sof_dev);  
	int (*remove)(struct snd_sof_dev *sof_dev);  
	int (*shutdown)(struct snd_sof_dev *sof_dev);  

	 
	int (*run)(struct snd_sof_dev *sof_dev);  
	int (*stall)(struct snd_sof_dev *sof_dev, unsigned int core_mask);  
	int (*reset)(struct snd_sof_dev *sof_dev);  
	int (*core_get)(struct snd_sof_dev *sof_dev, int core);  
	int (*core_put)(struct snd_sof_dev *sof_dev, int core);  

	 
	void (*write8)(struct snd_sof_dev *sof_dev, void __iomem *addr,
		       u8 value);  
	u8 (*read8)(struct snd_sof_dev *sof_dev,
		    void __iomem *addr);  
	void (*write)(struct snd_sof_dev *sof_dev, void __iomem *addr,
		      u32 value);  
	u32 (*read)(struct snd_sof_dev *sof_dev,
		    void __iomem *addr);  
	void (*write64)(struct snd_sof_dev *sof_dev, void __iomem *addr,
			u64 value);  
	u64 (*read64)(struct snd_sof_dev *sof_dev,
		      void __iomem *addr);  

	 
	int (*block_read)(struct snd_sof_dev *sof_dev,
			  enum snd_sof_fw_blk_type type, u32 offset,
			  void *dest, size_t size);  
	int (*block_write)(struct snd_sof_dev *sof_dev,
			   enum snd_sof_fw_blk_type type, u32 offset,
			   void *src, size_t size);  

	 
	void (*mailbox_read)(struct snd_sof_dev *sof_dev,
			     u32 offset, void *dest,
			     size_t size);  
	void (*mailbox_write)(struct snd_sof_dev *sof_dev,
			      u32 offset, void *src,
			      size_t size);  

	 
	irqreturn_t (*irq_handler)(int irq, void *context);  
	irqreturn_t (*irq_thread)(int irq, void *context);  

	 
	int (*send_msg)(struct snd_sof_dev *sof_dev,
			struct snd_sof_ipc_msg *msg);  

	 
	int (*load_firmware)(struct snd_sof_dev *sof_dev);  
	int (*load_module)(struct snd_sof_dev *sof_dev,
			   struct snd_sof_mod_hdr *hdr);  

	 
	int (*pcm_open)(struct snd_sof_dev *sdev,
			struct snd_pcm_substream *substream);  
	 
	int (*pcm_close)(struct snd_sof_dev *sdev,
			 struct snd_pcm_substream *substream);  

	 
	int (*pcm_hw_params)(struct snd_sof_dev *sdev,
			     struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_sof_platform_stream_params *platform_params);  

	 
	int (*pcm_hw_free)(struct snd_sof_dev *sdev,
			   struct snd_pcm_substream *substream);  

	 
	int (*pcm_trigger)(struct snd_sof_dev *sdev,
			   struct snd_pcm_substream *substream,
			   int cmd);  

	 
	snd_pcm_uframes_t (*pcm_pointer)(struct snd_sof_dev *sdev,
					 struct snd_pcm_substream *substream);  

	 
	int (*pcm_ack)(struct snd_sof_dev *sdev, struct snd_pcm_substream *substream);  

	 
	u64 (*get_stream_position)(struct snd_sof_dev *sdev,
				   struct snd_soc_component *component,
				   struct snd_pcm_substream *substream);  

	 
	int (*ipc_msg_data)(struct snd_sof_dev *sdev,
			    struct snd_sof_pcm_stream *sps,
			    void *p, size_t sz);  

	 
	int (*set_stream_data_offset)(struct snd_sof_dev *sdev,
				      struct snd_sof_pcm_stream *sps,
				      size_t posn_offset);  

	 
	int (*pre_fw_run)(struct snd_sof_dev *sof_dev);  
	int (*post_fw_run)(struct snd_sof_dev *sof_dev);  

	 
	int (*parse_platform_ext_manifest)(struct snd_sof_dev *sof_dev,
					   const struct sof_ext_man_elem_header *hdr);

	 
	int (*suspend)(struct snd_sof_dev *sof_dev,
		       u32 target_state);  
	int (*resume)(struct snd_sof_dev *sof_dev);  
	int (*runtime_suspend)(struct snd_sof_dev *sof_dev);  
	int (*runtime_resume)(struct snd_sof_dev *sof_dev);  
	int (*runtime_idle)(struct snd_sof_dev *sof_dev);  
	int (*set_hw_params_upon_resume)(struct snd_sof_dev *sdev);  
	int (*set_power_state)(struct snd_sof_dev *sdev,
			       const struct sof_dsp_power_state *target_state);  

	 
	int (*set_clk)(struct snd_sof_dev *sof_dev, u32 freq);  

	 
	const struct snd_sof_debugfs_map *debug_map;  
	int debug_map_count;  
	void (*dbg_dump)(struct snd_sof_dev *sof_dev,
			 u32 flags);  
	void (*ipc_dump)(struct snd_sof_dev *sof_dev);  
	int (*debugfs_add_region_item)(struct snd_sof_dev *sdev,
				       enum snd_sof_fw_blk_type blk_type, u32 offset,
				       size_t size, const char *name,
				       enum sof_debugfs_access_type access_type);  

	 
	int (*trace_init)(struct snd_sof_dev *sdev,
			  struct snd_dma_buffer *dmatb,
			  struct sof_ipc_dma_trace_params_ext *dtrace_params);  
	int (*trace_release)(struct snd_sof_dev *sdev);  
	int (*trace_trigger)(struct snd_sof_dev *sdev,
			     int cmd);  

	 
	int (*get_bar_index)(struct snd_sof_dev *sdev,
			     u32 type);  
	int (*get_mailbox_offset)(struct snd_sof_dev *sdev); 
	int (*get_window_offset)(struct snd_sof_dev *sdev,
				 u32 id); 

	 
	int (*machine_register)(struct snd_sof_dev *sdev,
				void *pdata);  
	void (*machine_unregister)(struct snd_sof_dev *sdev,
				   void *pdata);  
	struct snd_soc_acpi_mach * (*machine_select)(struct snd_sof_dev *sdev);  
	void (*set_mach_params)(struct snd_soc_acpi_mach *mach,
				struct snd_sof_dev *sdev);  

	 
	int (*register_ipc_clients)(struct snd_sof_dev *sdev);  
	void (*unregister_ipc_clients)(struct snd_sof_dev *sdev);  

	 
	struct snd_soc_dai_driver *drv;
	int num_drv;

	 
	u32 hw_info;

	const struct dsp_arch_ops *dsp_arch_ops;
};

 
struct dsp_arch_ops {
	void (*dsp_oops)(struct snd_sof_dev *sdev, const char *level, void *oops);
	void (*dsp_stack)(struct snd_sof_dev *sdev, const char *level, void *oops,
			  u32 *stack, u32 stack_words);
};

#define sof_dsp_arch_ops(sdev) ((sdev)->pdata->desc->ops->dsp_arch_ops)

 
struct snd_sof_dfsentry {
	size_t size;
	size_t buf_data_size;   
	enum sof_dfsentry_type type;
	 
	enum sof_debugfs_access_type access_type;
#if IS_ENABLED(CONFIG_SND_SOC_SOF_DEBUG_ENABLE_DEBUGFS_CACHE)
	char *cache_buf;  
#endif
	struct snd_sof_dev *sdev;
	struct list_head list;   
	union {
		void __iomem *io_mem;
		void *buf;
	};
};

 
struct snd_sof_debugfs_map {
	const char *name;
	u32 bar;
	u32 offset;
	u32 size;
	 
	enum sof_debugfs_access_type access_type;
};

 
struct snd_sof_mailbox {
	u32 offset;
	size_t size;
};

 
struct snd_sof_ipc_msg {
	 
	void *msg_data;
	void *reply_data;
	size_t msg_size;
	size_t reply_size;
	int reply_error;

	 
	void *rx_data;

	wait_queue_head_t waitq;
	bool ipc_complete;
};

 
struct sof_ipc_fw_tracing_ops {
	int (*init)(struct snd_sof_dev *sdev);
	void (*free)(struct snd_sof_dev *sdev);
	void (*fw_crashed)(struct snd_sof_dev *sdev);
	void (*suspend)(struct snd_sof_dev *sdev, pm_message_t pm_state);
	int (*resume)(struct snd_sof_dev *sdev);
};

 
struct sof_ipc_pm_ops {
	int (*ctx_save)(struct snd_sof_dev *sdev);
	int (*ctx_restore)(struct snd_sof_dev *sdev);
	int (*set_core_state)(struct snd_sof_dev *sdev, int core_idx, bool on);
	int (*set_pm_gate)(struct snd_sof_dev *sdev, u32 flags);
};

 
struct sof_ipc_fw_loader_ops {
	int (*validate)(struct snd_sof_dev *sdev);
	size_t (*parse_ext_manifest)(struct snd_sof_dev *sdev);
	int (*load_fw_to_dsp)(struct snd_sof_dev *sdev);
};

struct sof_ipc_tplg_ops;
struct sof_ipc_pcm_ops;

 
struct sof_ipc_ops {
	const struct sof_ipc_tplg_ops *tplg;
	const struct sof_ipc_pm_ops *pm;
	const struct sof_ipc_pcm_ops *pcm;
	const struct sof_ipc_fw_loader_ops *fw_loader;
	const struct sof_ipc_fw_tracing_ops *fw_tracing;

	int (*init)(struct snd_sof_dev *sdev);
	void (*exit)(struct snd_sof_dev *sdev);
	int (*post_fw_boot)(struct snd_sof_dev *sdev);

	int (*tx_msg)(struct snd_sof_dev *sdev, void *msg_data, size_t msg_bytes,
		      void *reply_data, size_t reply_bytes, bool no_pm);
	int (*set_get_data)(struct snd_sof_dev *sdev, void *data, size_t data_bytes,
			    bool set);
	int (*get_reply)(struct snd_sof_dev *sdev);
	void (*rx_msg)(struct snd_sof_dev *sdev);
};

 
struct snd_sof_ipc {
	struct snd_sof_dev *sdev;

	 
	struct mutex tx_mutex;
	 
	bool disable_ipc_tx;

	 
	size_t max_payload_size;

	struct snd_sof_ipc_msg msg;

	 
	const struct sof_ipc_ops *ops;
};

 
#define sof_ipc_get_ops(sdev, ops_name)		\
		(((sdev)->ipc && (sdev)->ipc->ops) ? (sdev)->ipc->ops->ops_name : NULL)

 
struct snd_sof_dev {
	struct device *dev;
	spinlock_t ipc_lock;	 
	spinlock_t hw_lock;	 

	 
	bool dspless_mode_selected;

	 
	struct sof_firmware basefw;

	 
	struct snd_soc_component_driver plat_drv;

	 
	struct sof_dsp_power_state dsp_power_state;
	 
	struct mutex power_state_access;

	 
	enum sof_system_suspend_state system_suspend_target;

	 
	wait_queue_head_t boot_wait;
	enum sof_fw_state fw_state;
	bool first_boot;

	 
	struct work_struct probe_work;
	bool probe_completed;

	 
	struct snd_sof_pdata *pdata;

	 
	struct snd_sof_ipc *ipc;
	struct snd_sof_mailbox fw_info_box;	 
	struct snd_sof_mailbox dsp_box;		 
	struct snd_sof_mailbox host_box;	 
	struct snd_sof_mailbox stream_box;	 
	struct snd_sof_mailbox debug_box;	 
	struct snd_sof_ipc_msg *msg;
	int ipc_irq;
	u32 next_comp_id;  

	 
	void __iomem *bar[SND_SOF_BARS];	 
	int mmio_bar;
	int mailbox_bar;
	size_t dsp_oops_offset;

	 
	struct dentry *debugfs_root;
	struct list_head dfsentry_list;
	bool dbg_dump_printed;
	bool ipc_dump_printed;

	 
	struct sof_ipc_fw_ready fw_ready;
	struct sof_ipc_fw_version fw_version;
	struct sof_ipc_cc_version *cc_version;

	 
	struct snd_soc_tplg_ops *tplg_ops;
	struct list_head pcm_list;
	struct list_head kcontrol_list;
	struct list_head widget_list;
	struct list_head pipeline_list;
	struct list_head dai_list;
	struct list_head dai_link_list;
	struct list_head route_list;
	struct snd_soc_component *component;
	u32 enabled_cores_mask;  
	bool led_present;

	 
	struct sof_ipc_window *info_window;

	 
	int ipc_timeout;
	int boot_timeout;

	 
	bool fw_trace_is_supported;  
	void *fw_trace_data;  

	bool msi_enabled;

	 
	u32 num_cores;

	 
	int dsp_core_ref_count[SOF_MAX_DSP_NUM_CORES];

	 
	struct list_head ipc_client_list;

	 
	struct mutex ipc_client_mutex;

	 
	struct list_head ipc_rx_handler_list;

	 
	struct list_head fw_state_handler_list;

	 
	struct mutex client_event_handler_mutex;

	 
	bool mclk_id_override;
	u16  mclk_id_quirk;  

	void *private;			 
};

 

int snd_sof_device_probe(struct device *dev, struct snd_sof_pdata *plat_data);
int snd_sof_device_remove(struct device *dev);
int snd_sof_device_shutdown(struct device *dev);
bool snd_sof_device_probe_completed(struct device *dev);

int snd_sof_runtime_suspend(struct device *dev);
int snd_sof_runtime_resume(struct device *dev);
int snd_sof_runtime_idle(struct device *dev);
int snd_sof_resume(struct device *dev);
int snd_sof_suspend(struct device *dev);
int snd_sof_dsp_power_down_notify(struct snd_sof_dev *sdev);
int snd_sof_prepare(struct device *dev);
void snd_sof_complete(struct device *dev);

void snd_sof_new_platform_drv(struct snd_sof_dev *sdev);

 
extern struct snd_compress_ops sof_compressed_ops;

 
int snd_sof_load_firmware_raw(struct snd_sof_dev *sdev);
int snd_sof_load_firmware_memcpy(struct snd_sof_dev *sdev);
int snd_sof_run_firmware(struct snd_sof_dev *sdev);
void snd_sof_fw_unload(struct snd_sof_dev *sdev);

 
struct snd_sof_ipc *snd_sof_ipc_init(struct snd_sof_dev *sdev);
void snd_sof_ipc_free(struct snd_sof_dev *sdev);
void snd_sof_ipc_get_reply(struct snd_sof_dev *sdev);
void snd_sof_ipc_reply(struct snd_sof_dev *sdev, u32 msg_id);
static inline void snd_sof_ipc_msgs_rx(struct snd_sof_dev *sdev)
{
	sdev->ipc->ops->rx_msg(sdev);
}
int sof_ipc_tx_message(struct snd_sof_ipc *ipc, void *msg_data, size_t msg_bytes,
		       void *reply_data, size_t reply_bytes);
static inline int sof_ipc_tx_message_no_reply(struct snd_sof_ipc *ipc, void *msg_data,
					      size_t msg_bytes)
{
	return sof_ipc_tx_message(ipc, msg_data, msg_bytes, NULL, 0);
}
int sof_ipc_set_get_data(struct snd_sof_ipc *ipc, void *msg_data,
			 size_t msg_bytes, bool set);
int sof_ipc_tx_message_no_pm(struct snd_sof_ipc *ipc, void *msg_data, size_t msg_bytes,
			     void *reply_data, size_t reply_bytes);
static inline int sof_ipc_tx_message_no_pm_no_reply(struct snd_sof_ipc *ipc, void *msg_data,
						    size_t msg_bytes)
{
	return sof_ipc_tx_message_no_pm(ipc, msg_data, msg_bytes, NULL, 0);
}
int sof_ipc_send_msg(struct snd_sof_dev *sdev, void *msg_data, size_t msg_bytes,
		     size_t reply_bytes);

static inline void snd_sof_ipc_process_reply(struct snd_sof_dev *sdev, u32 msg_id)
{
	snd_sof_ipc_get_reply(sdev);
	snd_sof_ipc_reply(sdev, msg_id);
}

 
int snd_sof_dbg_init(struct snd_sof_dev *sdev);
void snd_sof_free_debug(struct snd_sof_dev *sdev);
int snd_sof_debugfs_buf_item(struct snd_sof_dev *sdev,
			     void *base, size_t size,
			     const char *name, mode_t mode);
void sof_print_oops_and_stack(struct snd_sof_dev *sdev, const char *level,
			      u32 panic_code, u32 tracep_code, void *oops,
			      struct sof_ipc_panic_info *panic_info,
			      void *stack, size_t stack_words);
void snd_sof_handle_fw_exception(struct snd_sof_dev *sdev, const char *msg);
int snd_sof_dbg_memory_info_init(struct snd_sof_dev *sdev);
int snd_sof_debugfs_add_region_item_iomem(struct snd_sof_dev *sdev,
		enum snd_sof_fw_blk_type blk_type, u32 offset, size_t size,
		const char *name, enum sof_debugfs_access_type access_type);
 
int sof_fw_trace_init(struct snd_sof_dev *sdev);
void sof_fw_trace_free(struct snd_sof_dev *sdev);
void sof_fw_trace_fw_crashed(struct snd_sof_dev *sdev);
void sof_fw_trace_suspend(struct snd_sof_dev *sdev, pm_message_t pm_state);
int sof_fw_trace_resume(struct snd_sof_dev *sdev);

 
static inline void sof_stack(struct snd_sof_dev *sdev, const char *level,
			     void *oops, u32 *stack, u32 stack_words)
{
		sof_dsp_arch_ops(sdev)->dsp_stack(sdev, level,  oops, stack,
						  stack_words);
}

static inline void sof_oops(struct snd_sof_dev *sdev, const char *level, void *oops)
{
	if (sof_dsp_arch_ops(sdev)->dsp_oops)
		sof_dsp_arch_ops(sdev)->dsp_oops(sdev, level, oops);
}

extern const struct dsp_arch_ops sof_xtensa_arch_ops;

 
void sof_set_fw_state(struct snd_sof_dev *sdev, enum sof_fw_state new_state);

 
void sof_io_write(struct snd_sof_dev *sdev, void __iomem *addr, u32 value);
void sof_io_write64(struct snd_sof_dev *sdev, void __iomem *addr, u64 value);
u32 sof_io_read(struct snd_sof_dev *sdev, void __iomem *addr);
u64 sof_io_read64(struct snd_sof_dev *sdev, void __iomem *addr);
void sof_mailbox_write(struct snd_sof_dev *sdev, u32 offset,
		       void *message, size_t bytes);
void sof_mailbox_read(struct snd_sof_dev *sdev, u32 offset,
		      void *message, size_t bytes);
int sof_block_write(struct snd_sof_dev *sdev, enum snd_sof_fw_blk_type blk_type,
		    u32 offset, void *src, size_t size);
int sof_block_read(struct snd_sof_dev *sdev, enum snd_sof_fw_blk_type blk_type,
		   u32 offset, void *dest, size_t size);

int sof_ipc_msg_data(struct snd_sof_dev *sdev,
		     struct snd_sof_pcm_stream *sps,
		     void *p, size_t sz);
int sof_set_stream_data_offset(struct snd_sof_dev *sdev,
			       struct snd_sof_pcm_stream *sps,
			       size_t posn_offset);

int sof_stream_pcm_open(struct snd_sof_dev *sdev,
			struct snd_pcm_substream *substream);
int sof_stream_pcm_close(struct snd_sof_dev *sdev,
			 struct snd_pcm_substream *substream);

int sof_machine_check(struct snd_sof_dev *sdev);

 
#if IS_ENABLED(CONFIG_SND_SOC_SOF_CLIENT)
int sof_client_dev_register(struct snd_sof_dev *sdev, const char *name, u32 id,
			    const void *data, size_t size);
void sof_client_dev_unregister(struct snd_sof_dev *sdev, const char *name, u32 id);
int sof_register_clients(struct snd_sof_dev *sdev);
void sof_unregister_clients(struct snd_sof_dev *sdev);
void sof_client_ipc_rx_dispatcher(struct snd_sof_dev *sdev, void *msg_buf);
void sof_client_fw_state_dispatcher(struct snd_sof_dev *sdev);
int sof_suspend_clients(struct snd_sof_dev *sdev, pm_message_t state);
int sof_resume_clients(struct snd_sof_dev *sdev);
#else  
static inline int sof_client_dev_register(struct snd_sof_dev *sdev, const char *name,
					  u32 id, const void *data, size_t size)
{
	return 0;
}

static inline void sof_client_dev_unregister(struct snd_sof_dev *sdev,
					     const char *name, u32 id)
{
}

static inline int sof_register_clients(struct snd_sof_dev *sdev)
{
	return 0;
}

static inline  void sof_unregister_clients(struct snd_sof_dev *sdev)
{
}

static inline void sof_client_ipc_rx_dispatcher(struct snd_sof_dev *sdev, void *msg_buf)
{
}

static inline void sof_client_fw_state_dispatcher(struct snd_sof_dev *sdev)
{
}

static inline int sof_suspend_clients(struct snd_sof_dev *sdev, pm_message_t state)
{
	return 0;
}

static inline int sof_resume_clients(struct snd_sof_dev *sdev)
{
	return 0;
}
#endif  

 
extern const struct sof_ipc_ops ipc3_ops;
extern const struct sof_ipc_ops ipc4_ops;

#endif
