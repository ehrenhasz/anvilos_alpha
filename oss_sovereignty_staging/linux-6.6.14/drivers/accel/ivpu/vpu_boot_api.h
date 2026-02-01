 
 

#ifndef VPU_BOOT_API_H
#define VPU_BOOT_API_H

 

 
#define VPU_BOOT_API_VER_MAJOR 3

 
#define VPU_BOOT_API_VER_MINOR 12

 
#define VPU_BOOT_API_VER_PATCH 2

 
#define VPU_BOOT_API_VER_INDEX 0
 

#pragma pack(push, 1)

 
#define VPU_FW_HEADER_SIZE    4096
#define VPU_FW_HEADER_VERSION 0x1
#define VPU_FW_VERSION_SIZE   32
#define VPU_FW_API_VER_NUM    16

struct vpu_firmware_header {
	u32 header_version;
	u32 image_format;
	u64 image_load_address;
	u32 image_size;
	u64 entry_point;
	u8 vpu_version[VPU_FW_VERSION_SIZE];
	u32 compression_type;
	u64 firmware_version_load_address;
	u32 firmware_version_size;
	u64 boot_params_load_address;
	u32 api_version[VPU_FW_API_VER_NUM];
	 
	u32 runtime_size;
	u32 shave_nn_fw_size;
};

 

#define VPU_BOOT_PLL_COUNT     3
#define VPU_BOOT_PLL_OUT_COUNT 4

 
#define VPU_BOOT_TYPE_COLDBOOT 0
#define VPU_BOOT_TYPE_WARMBOOT 1

 
#define VPU_BOOT_PARAMS_MAGIC 0x10000

 
#define VPU_SCHEDULING_MODE_OS 0
#define VPU_SCHEDULING_MODE_HW 1

enum VPU_BOOT_L2_CACHE_CFG_TYPE {
	VPU_BOOT_L2_CACHE_CFG_UPA = 0,
	VPU_BOOT_L2_CACHE_CFG_NN = 1,
	VPU_BOOT_L2_CACHE_CFG_NUM = 2
};

 
enum vpu_trace_destination {
	VPU_TRACE_DESTINATION_PIPEPRINT = 0x1,
	VPU_TRACE_DESTINATION_VERBOSE_TRACING = 0x2,
	VPU_TRACE_DESTINATION_NORTH_PEAK = 0x4,
};

 
#define VPU_TRACE_PROC_BIT_ARM	     0
#define VPU_TRACE_PROC_BIT_LRT	     1
#define VPU_TRACE_PROC_BIT_LNN	     2
#define VPU_TRACE_PROC_BIT_SHV_0     3
#define VPU_TRACE_PROC_BIT_SHV_1     4
#define VPU_TRACE_PROC_BIT_SHV_2     5
#define VPU_TRACE_PROC_BIT_SHV_3     6
#define VPU_TRACE_PROC_BIT_SHV_4     7
#define VPU_TRACE_PROC_BIT_SHV_5     8
#define VPU_TRACE_PROC_BIT_SHV_6     9
#define VPU_TRACE_PROC_BIT_SHV_7     10
#define VPU_TRACE_PROC_BIT_SHV_8     11
#define VPU_TRACE_PROC_BIT_SHV_9     12
#define VPU_TRACE_PROC_BIT_SHV_10    13
#define VPU_TRACE_PROC_BIT_SHV_11    14
#define VPU_TRACE_PROC_BIT_SHV_12    15
#define VPU_TRACE_PROC_BIT_SHV_13    16
#define VPU_TRACE_PROC_BIT_SHV_14    17
#define VPU_TRACE_PROC_BIT_SHV_15    18
#define VPU_TRACE_PROC_BIT_ACT_SHV_0 19
#define VPU_TRACE_PROC_BIT_ACT_SHV_1 20
#define VPU_TRACE_PROC_BIT_ACT_SHV_2 21
#define VPU_TRACE_PROC_BIT_ACT_SHV_3 22
#define VPU_TRACE_PROC_NO_OF_HW_DEVS 23

 
#define VPU_TRACE_PROC_BIT_KMB_FIRST VPU_TRACE_PROC_BIT_LRT
#define VPU_TRACE_PROC_BIT_KMB_LAST  VPU_TRACE_PROC_BIT_SHV_15

struct vpu_boot_l2_cache_config {
	u8 use;
	u8 cfg;
};

struct vpu_warm_boot_section {
	u32 src;
	u32 dst;
	u32 size;
	u32 core_id;
	u32 is_clear_op;
};

struct vpu_boot_params {
	u32 magic;
	u32 vpu_id;
	u32 vpu_count;
	u32 pad0[5];
	 
	u32 frequency;
	u32 pll[VPU_BOOT_PLL_COUNT][VPU_BOOT_PLL_OUT_COUNT];
	u32 perf_clk_frequency;
	u32 pad1[42];
	 
	u64 ipc_header_area_start;
	u32 ipc_header_area_size;
	u64 shared_region_base;
	u32 shared_region_size;
	u64 ipc_payload_area_start;
	u32 ipc_payload_area_size;
	u64 global_aliased_pio_base;
	u32 global_aliased_pio_size;
	u32 autoconfig;
	struct vpu_boot_l2_cache_config cache_defaults[VPU_BOOT_L2_CACHE_CFG_NUM];
	u64 global_memory_allocator_base;
	u32 global_memory_allocator_size;
	 
	u64 shave_nn_fw_base;
	u64 save_restore_ret_address;  
	u32 pad2[43];
	 
	s32 watchdog_irq_mss;
	s32 watchdog_irq_nce;
	 
	u32 host_to_vpu_irq;
	 
	u32 job_done_irq;
	 
	u32 mmu_update_request_irq;
	 
	u32 mmu_update_done_irq;
	 
	u32 set_power_level_irq;
	 
	u32 set_power_level_done_irq;
	 
	u32 set_vpu_idle_update_irq;
	 
	u32 metric_query_event_irq;
	 
	u32 metric_query_event_done_irq;
	 
	u32 preemption_done_irq;
	 
	u32 pad3[52];
	 
	u32 host_version_id;
	u32 si_stepping;
	u64 device_id;
	u64 feature_exclusion;
	u64 sku;
	 
	u32 min_freq_pll_ratio;
	 
	u32 max_freq_pll_ratio;
	 
	u32 default_trace_level;
	u32 boot_type;
	u64 punit_telemetry_sram_base;
	u64 punit_telemetry_sram_size;
	u32 vpu_telemetry_enable;
	u64 crit_tracing_buff_addr;
	u32 crit_tracing_buff_size;
	u64 verbose_tracing_buff_addr;
	u32 verbose_tracing_buff_size;
	u64 verbose_tracing_sw_component_mask;  
	 
	u32 trace_destination_mask;
	 
	u64 trace_hw_component_mask;
	 
	u64 tracing_buff_message_format_mask;
	u64 trace_reserved_1[2];
	 
	u32 temp_sensor_period_ms;
	 
	u32 pn_freq_pll_ratio;
	u32 pad4[28];
	 
	u32 warm_boot_sections_count;
	u32 warm_boot_start_address_reference;
	u32 warm_boot_section_info_address_offset;
	u32 pad5[13];
	 
	struct {
		 
		u64 vpu_active_state_requested;
		 
		u64 vpu_active_state_achieved;
		 
		u64 vpu_idle_state_requested;
		 
		u64 vpu_idle_state_achieved;
		 
		u64 vpu_standby_state_requested;
		 
		u64 vpu_standby_state_achieved;
	} power_states_timestamps;
	 
	u32 vpu_scheduling_mode;
	 
	u32 vpu_focus_present_timer_ms;
	 
	u32 pad6[738];
};

 

#define VPU_TRACING_BUFFER_CANARY (0xCAFECAFE)

 
#define VPU_TRACING_FORMAT_STRING 0
#define VPU_TRACING_FORMAT_MIPI	  2
 
struct vpu_tracing_buffer_header {
	 
	u32 host_canary_start;
	 
	u32 read_index;
	u32 pad_to_cache_line_size_0[14];
	 

	 
	u32 vpu_canary_start;
	 
	u32 write_index;
	 
	u32 wrap_count;
	 
	u32 reserved_0;
	 
	u32 size;
	 
	u16 header_version;
	 
	u16 header_size;
	 
	u32 format;
	 
	u32 alignment;  
	 
	char name[16];
	u32 pad_to_cache_line_size_1[4];
	 
};

#pragma pack(pop)

#endif
