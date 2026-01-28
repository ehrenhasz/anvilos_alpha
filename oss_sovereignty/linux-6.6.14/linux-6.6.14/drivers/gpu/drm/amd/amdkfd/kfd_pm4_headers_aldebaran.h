#ifndef PM4_MES_MAP_PROCESS_PER_DEBUG_VMID_DEFINED
#define PM4_MES_MAP_PROCESS_PER_DEBUG_VMID_DEFINED
struct pm4_mes_map_process_aldebaran {
	union {
		union PM4_MES_TYPE_3_HEADER header;	 
		uint32_t ordinal1;
	};
	union {
		struct {
			uint32_t pasid:16;	     
			uint32_t single_memops:1;    
			uint32_t reserved1:1;	     
			uint32_t debug_vmid:4;	     
			uint32_t new_debug:1;	     
			uint32_t tmz:1;		     
			uint32_t diq_enable:1;       
			uint32_t process_quantum:7;  
		} bitfields2;
		uint32_t ordinal2;
	};
	uint32_t vm_context_page_table_base_addr_lo32;
	uint32_t vm_context_page_table_base_addr_hi32;
	uint32_t sh_mem_bases;
	uint32_t sh_mem_config;
	uint32_t sq_shader_tba_lo;
	uint32_t sq_shader_tba_hi;
	uint32_t sq_shader_tma_lo;
	uint32_t sq_shader_tma_hi;
	uint32_t reserved6;
	uint32_t gds_addr_lo;
	uint32_t gds_addr_hi;
	union {
		struct {
			uint32_t num_gws:7;
			uint32_t sdma_enable:1;
			uint32_t num_oac:4;
			uint32_t gds_size_hi:4;
			uint32_t gds_size:6;
			uint32_t num_queues:10;
		} bitfields14;
		uint32_t ordinal14;
	};
	uint32_t spi_gdbg_per_vmid_cntl;
	uint32_t tcp_watch_cntl[4];
	uint32_t completion_signal_lo;
	uint32_t completion_signal_hi;
};
#endif
