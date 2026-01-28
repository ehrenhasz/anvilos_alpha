

#ifndef IOSM_IPC_MMIO_H
#define IOSM_IPC_MMIO_H


#define IOSM_CP_VERSION 0x0100UL


#define DL_AGGR BIT(9)


#define UL_AGGR BIT(8)


#define UL_FLOW_CREDIT BIT(21)


enum ipc_mem_device_ipc_state {
	IPC_MEM_DEVICE_IPC_UNINIT,
	IPC_MEM_DEVICE_IPC_INIT,
	IPC_MEM_DEVICE_IPC_RUNNING,
	IPC_MEM_DEVICE_IPC_RECOVERY,
	IPC_MEM_DEVICE_IPC_ERROR,
	IPC_MEM_DEVICE_IPC_DONT_CARE,
	IPC_MEM_DEVICE_IPC_INVALID = -1
};


enum rom_exit_code {
	IMEM_ROM_EXIT_OPEN_EXT = 0x01,
	IMEM_ROM_EXIT_OPEN_MEM = 0x02,
	IMEM_ROM_EXIT_CERT_EXT = 0x10,
	IMEM_ROM_EXIT_CERT_MEM = 0x20,
	IMEM_ROM_EXIT_FAIL = 0xFF
};


enum ipc_mem_exec_stage {
	IPC_MEM_EXEC_STAGE_RUN = 0x600DF00D,
	IPC_MEM_EXEC_STAGE_CRASH = 0x8BADF00D,
	IPC_MEM_EXEC_STAGE_CD_READY = 0xBADC0DED,
	IPC_MEM_EXEC_STAGE_BOOT = 0xFEEDB007,
	IPC_MEM_EXEC_STAGE_PSI = 0xFEEDBEEF,
	IPC_MEM_EXEC_STAGE_EBL = 0xFEEDCAFE,
	IPC_MEM_EXEC_STAGE_INVALID = 0xFFFFFFFF
};


struct mmio_offset {
	int exec_stage;
	int chip_info;
	int rom_exit_code;
	int psi_address;
	int psi_size;
	int ipc_status;
	int context_info;
	int ap_win_base;
	int ap_win_end;
	int cp_version;
	int cp_capability;
};


struct iosm_mmio {
	unsigned char __iomem *base;
	struct device *dev;
	struct mmio_offset offset;
	phys_addr_t context_info_addr;
	unsigned int chip_info_version;
	unsigned int chip_info_size;
	u32 mux_protocol;
	u8 has_ul_flow_credit:1,
	   has_slp_no_prot:1,
	   has_mcr_support:1;
};


struct iosm_mmio *ipc_mmio_init(void __iomem *mmio_addr, struct device *dev);


void ipc_mmio_set_psi_addr_and_size(struct iosm_mmio *ipc_mmio, dma_addr_t addr,
				    u32 size);


void ipc_mmio_set_contex_info_addr(struct iosm_mmio *ipc_mmio,
				   phys_addr_t addr);


int ipc_mmio_get_cp_version(struct iosm_mmio *ipc_mmio);


enum rom_exit_code ipc_mmio_get_rom_exit_code(struct iosm_mmio *ipc_mmio);


enum ipc_mem_exec_stage ipc_mmio_get_exec_stage(struct iosm_mmio *ipc_mmio);


enum ipc_mem_device_ipc_state
ipc_mmio_get_ipc_state(struct iosm_mmio *ipc_mmio);


void ipc_mmio_copy_chip_info(struct iosm_mmio *ipc_mmio, void *dest,
			     size_t size);


void ipc_mmio_config(struct iosm_mmio *ipc_mmio);


void ipc_mmio_update_cp_capability(struct iosm_mmio *ipc_mmio);

#endif
