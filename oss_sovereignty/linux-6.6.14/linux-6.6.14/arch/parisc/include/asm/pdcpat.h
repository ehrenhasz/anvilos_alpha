#ifndef __PARISC_PATPDC_H
#define __PARISC_PATPDC_H
#define PDC_PAT_CELL           	64L    
#define PDC_PAT_CELL_GET_NUMBER    0L    
#define PDC_PAT_CELL_GET_INFO      1L    
#define PDC_PAT_CELL_MODULE        2L    
#define PDC_PAT_CELL_SET_ATTENTION 9L    
#define PDC_PAT_CELL_NUMBER_TO_LOC 10L    
#define PDC_PAT_CELL_WALK_FABRIC   11L    
#define PDC_PAT_CELL_GET_RDT_SIZE  12L    
#define PDC_PAT_CELL_GET_RDT       13L    
#define PDC_PAT_CELL_GET_LOCAL_PDH_SZ 14L  
#define PDC_PAT_CELL_SET_LOCAL_PDH    15L   
#define PDC_PAT_CELL_GET_REMOTE_PDH_SZ 16L  
#define PDC_PAT_CELL_GET_REMOTE_PDH 17L  
#define PDC_PAT_CELL_GET_DBG_INFO   128L   
#define PDC_PAT_CELL_CHANGE_ALIAS   129L   
#define IO_VIEW      0UL
#define PA_VIEW      1UL
#define	PAT_ENTITY_CA	0	 
#define	PAT_ENTITY_PROC	1	 
#define	PAT_ENTITY_MEM	2	 
#define	PAT_ENTITY_SBA	3	 
#define	PAT_ENTITY_LBA	4	 
#define	PAT_ENTITY_PBC	5	 
#define	PAT_ENTITY_XBC	6	 
#define	PAT_ENTITY_RC	7	 
#define PAT_PBNUM           0          
#define PAT_LMMIO           1          
#define PAT_GMMIO           2          
#define PAT_NPIOP           3          
#define PAT_PIOP            4          
#define PAT_AHPA            5          
#define PAT_UFO             6          
#define PAT_GNIP            7          
#define PDC_PAT_CHASSIS_LOG		65L
#define PDC_PAT_CHASSIS_WRITE_LOG    	0L  
#define PDC_PAT_CHASSIS_READ_LOG     	1L  
#define PDC_PAT_COMPLEX			66L
#define PDC_PAT_CPU                	67L
#define PDC_PAT_CPU_INFO            	0L  
#define PDC_PAT_CPU_DELETE          	1L  
#define PDC_PAT_CPU_ADD             	2L  
#define PDC_PAT_CPU_GET_NUMBER      	3L  
#define PDC_PAT_CPU_GET_HPA         	4L  
#define PDC_PAT_CPU_STOP            	5L  
#define PDC_PAT_CPU_RENDEZVOUS      	6L  
#define PDC_PAT_CPU_GET_CLOCK_INFO  	7L  
#define PDC_PAT_CPU_GET_RENDEZVOUS_STATE 8L  
#define PDC_PAT_CPU_GET_PDC_ENTRYPOINT	11L  
#define PDC_PAT_CPU_PLUNGE_FABRIC 	128L  
#define PDC_PAT_CPU_UPDATE_CACHE_CLEANSING 129L  
#define PDC_PAT_EVENT              	68L
#define PDC_PAT_EVENT_GET_CAPS     	0L  
#define PDC_PAT_EVENT_SET_MODE     	1L  
#define PDC_PAT_EVENT_SCAN         	2L  
#define PDC_PAT_EVENT_HANDLE       	3L  
#define PDC_PAT_EVENT_GET_NB_CALL  	4L  
#define PDC_PAT_HPMC               70L
#define PDC_PAT_HPMC_RENDEZ_CPU     0L  
#define PDC_PAT_HPMC_SET_PARAMS     1L  
#define HPMC_SET_PARAMS_INTR 	    1L  
#define HPMC_SET_PARAMS_WAKE 	    2L  
#define PDC_PAT_IO                  71L
#define PDC_PAT_IO_GET_SLOT_STATUS   	5L  
#define PDC_PAT_IO_GET_LOC_FROM_HARDWARE 6L  
#define PDC_PAT_IO_GET_HARDWARE_FROM_LOC 7L  
#define PDC_PAT_IO_GET_PCI_CONFIG_FROM_HW 11L  
#define PDC_PAT_IO_GET_HW_FROM_PCI_CONFIG 12L  
#define PDC_PAT_IO_READ_HOST_BRIDGE_INFO 13L   
#define PDC_PAT_IO_CLEAR_HOST_BRIDGE_INFO 14L  
#define PDC_PAT_IO_GET_PCI_ROUTING_TABLE_SIZE 15L  
#define PDC_PAT_IO_GET_PCI_ROUTING_TABLE  16L  
#define PDC_PAT_IO_GET_HINT_TABLE_SIZE 	17L  
#define PDC_PAT_IO_GET_HINT_TABLE   	18L  
#define PDC_PAT_IO_PCI_CONFIG_READ  	19L  
#define PDC_PAT_IO_PCI_CONFIG_WRITE 	20L  
#define PDC_PAT_IO_GET_NUM_IO_SLOTS 	21L  
#define PDC_PAT_IO_GET_LOC_IO_SLOTS 	22L  
#define PDC_PAT_IO_BAY_STATUS_INFO  	28L  
#define PDC_PAT_IO_GET_PROC_VIEW        29L  
#define PDC_PAT_IO_PROG_SBA_DIR_RANGE   30L  
#define PDC_PAT_MEM            72L
#define PDC_PAT_MEM_PD_INFO     	0L  
#define PDC_PAT_MEM_PD_CLEAR    	1L  
#define PDC_PAT_MEM_PD_READ     	2L  
#define PDC_PAT_MEM_PD_RESET    	3L  
#define PDC_PAT_MEM_CELL_INFO   	5L  
#define PDC_PAT_MEM_CELL_CLEAR  	6L  
#define PDC_PAT_MEM_CELL_READ   	7L  
#define PDC_PAT_MEM_CELL_RESET  	8L  
#define PDC_PAT_MEM_SETGM		9L  
#define PDC_PAT_MEM_ADD_PAGE		10L  
#define PDC_PAT_MEM_ADDRESS		11L  
#define PDC_PAT_MEM_GET_TXT_SIZE   	12L  
#define PDC_PAT_MEM_GET_PD_TXT     	13L  
#define PDC_PAT_MEM_GET_CELL_TXT   	14L  
#define PDC_PAT_MEM_RD_STATE_INFO  	15L  
#define PDC_PAT_MEM_CLR_STATE_INFO 	16L  
#define PDC_PAT_MEM_CLEAN_RANGE    	128L  
#define PDC_PAT_MEM_GET_TBL_SIZE   	131L  
#define PDC_PAT_MEM_GET_TBL        	132L  
#define PDC_PAT_NVOLATILE	73L
#define PDC_PAT_NVOLATILE_READ		0L  
#define PDC_PAT_NVOLATILE_WRITE		1L  
#define PDC_PAT_NVOLATILE_GET_SIZE	2L  
#define PDC_PAT_NVOLATILE_VERIFY	3L  
#define PDC_PAT_NVOLATILE_INIT		4L  
#define PDC_PAT_PD		74L          
#define PDC_PAT_PD_GET_ADDR_MAP		0L   
#define PDC_PAT_PD_GET_PDC_INTERF_REV	1L   
#define PDC_PAT_CAPABILITY_BIT_PDC_SERIALIZE	(1UL << 0)
#define PDC_PAT_CAPABILITY_BIT_PDC_POLLING	(1UL << 1)
#define PDC_PAT_CAPABILITY_BIT_PDC_NBC		(1UL << 2)  
#define PDC_PAT_CAPABILITY_BIT_PDC_UFO		(1UL << 3)
#define PDC_PAT_CAPABILITY_BIT_PDC_IODC_32	(1UL << 4)
#define PDC_PAT_CAPABILITY_BIT_PDC_IODC_64	(1UL << 5)
#define PDC_PAT_CAPABILITY_BIT_PDC_HPMC_RENDEZ	(1UL << 6)
#define PDC_PAT_CAPABILITY_BIT_SIMULTANEOUS_PTLB (1UL << 7)
#define PAT_MEMORY_DESCRIPTOR		1   
#define PAT_MEMTYPE_MEMORY		0
#define PAT_MEMTYPE_FIRMWARE		4
#define PAT_MEMUSE_GENERAL		0
#define PAT_MEMUSE_GI			128
#define PAT_MEMUSE_GNI			129
#define PDC_PAT_REGISTER_TOC	75L
#define PDC_PAT_TOC_REGISTER_VECTOR	0L  
#define PDC_PAT_TOC_READ_VECTOR		1L  
#define PDC_PAT_SYSTEM_INFO	76L
#ifndef __ASSEMBLY__
#include <linux/types.h>
#ifdef CONFIG_64BIT
#define is_pdc_pat()	(PDC_TYPE_PAT == pdc_type)
extern int pdc_pat_get_irt_size(unsigned long *num_entries, unsigned long cell_num);
extern int pdc_pat_get_irt(void *r_addr, unsigned long cell_num);
#else	 
#define is_pdc_pat()	(0)
#define pdc_pat_get_irt_size(num_entries, cell_numn)	PDC_BAD_PROC
#define pdc_pat_get_irt(r_addr, cell_num)		PDC_BAD_PROC
#endif	 
struct pdc_pat_cell_num {
	unsigned long cell_num;
	unsigned long cell_loc;
};
struct pdc_pat_cpu_num {
	unsigned long cpu_num;
	unsigned long cpu_loc;
};
struct pdc_pat_mem_retinfo {  
	unsigned int ke;	 
	unsigned int current_pdt_entries:16;
	unsigned int max_pdt_entries:16;
	unsigned long Cs_bitmap;
	unsigned long Ic_bitmap;
	unsigned long good_mem;
	unsigned long first_dbe_loc;  
	unsigned long clear_time;  
};
struct pdc_pat_mem_cell_pdt_retinfo {  
	u64 reserved:32;
	u64 cs:1;		 
	u64 current_pdt_entries:15;
	u64 ic:1;		 
	u64 max_pdt_entries:15;
	unsigned long good_mem;
	unsigned long first_dbe_loc;  
	unsigned long clear_time;  
};
struct pdc_pat_mem_read_pd_retinfo {  
	unsigned long actual_count_bytes;
	unsigned long pdt_entries;
};
struct pdc_pat_mem_phys_mem_location {  
	u64 cabinet:8;
	u64 ign1:8;
	u64 ign2:8;
	u64 cell_slot:8;
	u64 ign3:8;
	u64 dimm_slot:8;  
	u64 ign4:8;
	u64 source:4;  
	u64 source_detail:4;  
};
struct pdc_pat_pd_addr_map_entry {
	unsigned char entry_type;        
	unsigned char reserve1[5];
	unsigned char memory_type;
	unsigned char memory_usage;
	unsigned long paddr;
	unsigned int  pages;             
	unsigned int  reserve2;
	unsigned long cell_map;
};
#define PAT_GET_CBA(value) ((value) & 0xfffffffffffff000UL)
#define PAT_GET_ENTITY(value)	(((value) >> 56) & 0xffUL)
#define PAT_GET_DVI(value)	(((value) >> 48) & 0xffUL)
#define PAT_GET_IOC(value)	(((value) >> 40) & 0xffUL)
#define PAT_GET_MOD_PAGES(value) ((value) & 0xffffffUL)
typedef struct pdc_pat_cell_info_rtn_block {
	unsigned long pdc_rev;
	unsigned long capabilities;  
	unsigned long reserved0[2];
	unsigned long cell_info;	 
	unsigned long cell_phys_location;
	unsigned long cpu_info;
	unsigned long cpu_speed;
	unsigned long io_chassis_phys_location;
	unsigned long cell_io_information;
	unsigned long reserved1[2];
	unsigned long io_slot_info_size;  
	struct {
		unsigned long header, info0, info1;
		unsigned long phys_loc, hw_path;
	} io_slot[16];
	unsigned long cell_mem_size;	 
	unsigned long cell_dimm_info_size;
	unsigned long dimm_info[16];
	unsigned long fabric_info_size;	 
	struct {			 
		unsigned long fabric_info_xbc_port;
		unsigned long rc_attached_to_xbc;
	} xbc[8*4];
} pdc_pat_cell_info_rtn_block_t;
struct pdc_pat_cell_mod_maddr_block {	 
	unsigned long cba;		 
	unsigned long mod_info;		 
	unsigned long mod_location;	 
	struct hardware_path mod_path;	 
	unsigned long mod[508];		 
} __attribute__((aligned(8))) ;
typedef struct pdc_pat_cell_mod_maddr_block pdc_pat_cell_mod_maddr_block_t;
extern int pdc_pat_get_PDC_entrypoint(unsigned long *pdc_entry);
extern int pdc_pat_chassis_send_log(unsigned long status, unsigned long data);
extern int pdc_pat_cell_get_number(struct pdc_pat_cell_num *cell_info);
extern int pdc_pat_cell_info(struct pdc_pat_cell_info_rtn_block *info,
		unsigned long *actcnt, unsigned long offset,
		unsigned long cell_number);
extern int pdc_pat_cell_module(unsigned long *actcnt, unsigned long ploc,
		unsigned long mod, unsigned long view_type, void *mem_addr);
extern int pdc_pat_cell_num_to_loc(void *, unsigned long);
extern int pdc_pat_cpu_get_number(struct pdc_pat_cpu_num *cpu_info, unsigned long hpa);
extern int pdc_pat_pd_get_addr_map(unsigned long *actual_len, void *mem_addr,
		unsigned long count, unsigned long offset);
extern int pdc_pat_pd_get_pdc_revisions(unsigned long *legacy_rev,
		unsigned long *pat_rev, unsigned long *pdc_cap);
extern int pdc_pat_io_pci_cfg_read(unsigned long pci_addr, int pci_size, u32 *val); 
extern int pdc_pat_io_pci_cfg_write(unsigned long pci_addr, int pci_size, u32 val); 
extern int pdc_pat_mem_pdt_info(struct pdc_pat_mem_retinfo *rinfo);
extern int pdc_pat_mem_pdt_cell_info(struct pdc_pat_mem_cell_pdt_retinfo *rinfo,
		unsigned long cell);
extern int pdc_pat_mem_read_cell_pdt(struct pdc_pat_mem_read_pd_retinfo *pret,
		unsigned long *pdt_entries_ptr, unsigned long max_entries);
extern int pdc_pat_mem_read_pd_pdt(struct pdc_pat_mem_read_pd_retinfo *pret,
		unsigned long *pdt_entries_ptr, unsigned long count,
		unsigned long offset);
extern int pdc_pat_mem_get_dimm_phys_location(
                struct pdc_pat_mem_phys_mem_location *pret,
                unsigned long phys_addr);
#endif  
#endif  
