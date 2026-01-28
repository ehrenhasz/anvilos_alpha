#ifndef _UAPI_PARISC_PDC_H
#define _UAPI_PARISC_PDC_H
#define PDC_WARN		  3	 
#define PDC_REQ_ERR_1		  2	 
#define PDC_REQ_ERR_0		  1	 
#define PDC_OK			  0	 
#define PDC_BAD_PROC		 -1	 
#define PDC_BAD_OPTION		 -2	 
#define PDC_ERROR		 -3	 
#define PDC_NE_MOD		 -5	 
#define PDC_NE_CELL_MOD		 -7	 
#define PDC_NE_BOOTDEV		 -9	 
#define PDC_INVALID_ARG		-10	 
#define PDC_BUS_POW_WARN	-12	 
#define PDC_NOT_NARROW		-17	 
#define PDC_POW_FAIL	1		 
#define PDC_POW_FAIL_PREPARE	0	 
#define PDC_CHASSIS	2		 
#define PDC_CHASSIS_DISP	0	 
#define PDC_CHASSIS_WARN	1	 
#define PDC_CHASSIS_DISPWARN	2	 
#define PDC_RETURN_CHASSIS_INFO 128	 
#define PDC_PIM         3                
#define PDC_PIM_HPMC            0        
#define PDC_PIM_RETURN_SIZE     1        
#define PDC_PIM_LPMC            2        
#define PDC_PIM_SOFT_BOOT       3        
#define PDC_PIM_TOC             4        
#define PDC_MODEL	4		 
#define PDC_MODEL_INFO		0	 
#define PDC_MODEL_BOOTID	1	 
#define PDC_MODEL_VERSIONS	2	 
#define PDC_MODEL_SYSMODEL	3	 
#define PDC_MODEL_ENSPEC	4	 
#define PDC_MODEL_DISPEC	5	 
#define PDC_MODEL_CPU_ID	6	 
#define PDC_MODEL_CAPABILITIES	7	 
#define  PDC_MODEL_OS64			(1 << 0)
#define  PDC_MODEL_OS32			(1 << 1)
#define  PDC_MODEL_IOPDIR_FDC		(1 << 2)
#define  PDC_MODEL_NVA_MASK		(3 << 4)
#define  PDC_MODEL_NVA_SUPPORTED	(0 << 4)
#define  PDC_MODEL_NVA_SLOW		(1 << 4)
#define  PDC_MODEL_NVA_UNSUPPORTED	(3 << 4)
#define PDC_MODEL_GET_BOOT__OP	8	 
#define PDC_MODEL_SET_BOOT__OP	9	 
#define PDC_MODEL_GET_PLATFORM_INFO 10	 
#define PDC_MODEL_GET_INSTALL_KERNEL 11	 
#define PA89_INSTRUCTION_SET	0x4	 
#define PA90_INSTRUCTION_SET	0x8
#define PDC_CACHE	5		 
#define PDC_CACHE_INFO		0	 
#define PDC_CACHE_SET_COH	1	 
#define PDC_CACHE_RET_SPID	2	 
#define PDC_HPA		6		 
#define PDC_HPA_PROCESSOR	0
#define PDC_HPA_MODULES		1
#define PDC_COPROC	7		 
#define PDC_COPROC_CFG		0	 
#define PDC_IODC	8		 
#define PDC_IODC_READ		0	 
#define PDC_IODC_RI_DATA_BYTES	0	 
#define PDC_IODC_RI_INIT	3	 
#define PDC_IODC_RI_IO		4	 
#define PDC_IODC_RI_SPA		5	 
#define PDC_IODC_RI_CONFIG	6	 
#define PDC_IODC_RI_TEST	8	 
#define PDC_IODC_RI_TLB		9	 
#define PDC_IODC_NINIT		2	 
#define PDC_IODC_DINIT		3	 
#define PDC_IODC_MEMERR		4	 
#define PDC_IODC_INDEX_DATA	0	 
#define PDC_IODC_BUS_ERROR	-4	 
#define PDC_IODC_INVALID_INDEX	-5	 
#define PDC_IODC_COUNT		-6	 
#define PDC_TOD		9		 
#define PDC_TOD_READ		0	 
#define PDC_TOD_WRITE		1	 
#define PDC_TOD_CALIBRATE	2	 
#define PDC_STABLE	10		 
#define PDC_STABLE_READ		0
#define PDC_STABLE_WRITE	1
#define PDC_STABLE_RETURN_SIZE	2
#define PDC_STABLE_VERIFY_CONTENTS 3
#define PDC_STABLE_INITIALIZE	4
#define PDC_NVOLATILE	11		 
#define PDC_NVOLATILE_READ	0
#define PDC_NVOLATILE_WRITE	1
#define PDC_NVOLATILE_RETURN_SIZE 2
#define PDC_NVOLATILE_VERIFY_CONTENTS 3
#define PDC_NVOLATILE_INITIALIZE 4
#define PDC_ADD_VALID	12		 
#define PDC_ADD_VALID_VERIFY	0	 
#define PDC_DEBUG	14		 
#define PDC_INSTR	15		 
#define PDC_PROC	16		 
#define PDC_CONFIG	17		 
#define PDC_CONFIG_DECONFIG	0
#define PDC_CONFIG_DRECONFIG	1
#define PDC_CONFIG_DRETURN_CONFIG 2
#define PDC_BLOCK_TLB	18		 
#define PDC_BTLB_INFO		0	 
#define PDC_BTLB_INSERT		1	 
#define PDC_BTLB_PURGE		2	 
#define PDC_BTLB_PURGE_ALL	3	 
#define PDC_TLB		19		 
#define PDC_TLB_INFO		0	 
#define PDC_TLB_SETUP		1	 
#define PDC_MEM		20		 
#define PDC_MEM_MEMINFO		0	 
#define PDC_MEM_ADD_PAGE	1	 
#define PDC_MEM_CLEAR_PDT	2	 
#define PDC_MEM_READ_PDT	3	 
#define PDC_MEM_RESET_CLEAR	4	 
#define PDC_MEM_GOODMEM		5	 
#define PDC_MEM_TABLE		128	 
#define PDC_MEM_RETURN_ADDRESS_TABLE	PDC_MEM_TABLE
#define PDC_MEM_GET_MEMORY_SYSTEM_TABLES_SIZE	131
#define PDC_MEM_GET_MEMORY_SYSTEM_TABLES	132
#define PDC_MEM_GET_PHYSICAL_LOCATION_FROM_MEMORY_ADDRESS 133
#define PDC_MEM_RET_SBE_REPLACED	5	 
#define PDC_MEM_RET_DUPLICATE_ENTRY	4
#define PDC_MEM_RET_BUF_SIZE_SMALL	1
#define PDC_MEM_RET_PDT_FULL		-11
#define PDC_MEM_RET_INVALID_PHYSICAL_LOCATION ~0ULL
#define PDC_PSW		21		 
#define PDC_PSW_MASK		0	 
#define PDC_PSW_GET_DEFAULTS	1	 
#define PDC_PSW_SET_DEFAULTS	2	 
#define PDC_PSW_ENDIAN_BIT	1	 
#define PDC_PSW_WIDE_BIT	2	 
#define PDC_SYSTEM_MAP	22		 
#define PDC_FIND_MODULE 	0
#define PDC_FIND_ADDRESS	1
#define PDC_TRANSLATE_PATH	2
#define PDC_SOFT_POWER	23		 
#define PDC_SOFT_POWER_INFO	0	 
#define PDC_SOFT_POWER_ENABLE	1	 
#define PDC_ALLOC	24		 
#define PDC_CRASH_PREP	25		 
#define PDC_CRASH_DUMP		0	 
#define PDC_CRASH_LOG_CEC_ERROR 1	 
#define PDC_SCSI_PARMS	26		 
#define PDC_SCSI_GET_PARMS	0	 
#define PDC_SCSI_SET_PARMS	1	 
#define PDC_MEM_MAP	128		 
#define PDC_MEM_MAP_HPA		0	 
#define PDC_EEPROM	129		 
#define PDC_EEPROM_READ_WORD	0
#define PDC_EEPROM_WRITE_WORD	1
#define PDC_EEPROM_READ_BYTE	2
#define PDC_EEPROM_WRITE_BYTE	3
#define PDC_EEPROM_EEPROM_PASSWORD -1000
#define PDC_NVM		130		 
#define PDC_NVM_READ_WORD	0
#define PDC_NVM_WRITE_WORD	1
#define PDC_NVM_READ_BYTE	2
#define PDC_NVM_WRITE_BYTE	3
#define PDC_SEED_ERROR	132		 
#define PDC_IO		135		 
#define PDC_IO_READ_AND_CLEAR_ERRORS	0
#define PDC_IO_RESET			1
#define PDC_IO_RESET_DEVICES		2
#define PDC_IO_USB_SUSPEND	0xC000000000000000
#define PDC_IO_EEPROM_IO_ERR_TABLE_FULL	-5	 
#define PDC_IO_NO_SUSPEND		-6	 
#define PDC_BROADCAST_RESET 136		 
#define PDC_DO_RESET		0	 
#define PDC_DO_FIRM_TEST_RESET	1	 
#define PDC_BR_RECONFIGURATION	2	 
#define PDC_FIRM_TEST_MAGIC	0xab9ec36fUL     
#define PDC_LAN_STATION_ID 138		 
#define PDC_LAN_STATION_ID_READ	0	 
#define	PDC_LAN_STATION_ID_SIZE	6
#define PDC_CHECK_RANGES 139		 
#define PDC_NV_SECTIONS	141		 
#define PDC_PERFORMANCE	142		 
#define PDC_SYSTEM_INFO	143		 
#define PDC_SYSINFO_RETURN_INFO_SIZE	0
#define PDC_SYSINFO_RRETURN_SYS_INFO	1
#define PDC_SYSINFO_RRETURN_ERRORS	2
#define PDC_SYSINFO_RRETURN_WARNINGS	3
#define PDC_SYSINFO_RETURN_REVISIONS	4
#define PDC_SYSINFO_RRETURN_DIAGNOSE	5
#define PDC_SYSINFO_RRETURN_HV_DIAGNOSE	1005
#define PDC_RDR		144		 
#define PDC_RDR_READ_BUFFER	0
#define PDC_RDR_READ_SINGLE	1
#define PDC_RDR_WRITE_SINGLE	2
#define PDC_INTRIGUE	145 		 
#define PDC_INTRIGUE_WRITE_BUFFER 	 0
#define PDC_INTRIGUE_GET_SCRATCH_BUFSIZE 1
#define PDC_INTRIGUE_START_CPU_COUNTERS	 2
#define PDC_INTRIGUE_STOP_CPU_COUNTERS	 3
#define PDC_STI		146 		 
#define PDC_PCI_INDEX	147
#define PDC_PCI_INTERFACE_INFO		0
#define PDC_PCI_SLOT_INFO		1
#define PDC_PCI_INFLIGHT_BYTES		2
#define PDC_PCI_READ_CONFIG		3
#define PDC_PCI_WRITE_CONFIG		4
#define PDC_PCI_READ_PCI_IO		5
#define PDC_PCI_WRITE_PCI_IO		6
#define PDC_PCI_READ_CONFIG_DELAY	7
#define PDC_PCI_UPDATE_CONFIG_DELAY	8
#define PDC_PCI_PCI_PATH_TO_PCI_HPA	9
#define PDC_PCI_PCI_HPA_TO_PCI_PATH	10
#define PDC_PCI_PCI_PATH_TO_PCI_BUS	11
#define PDC_PCI_PCI_RESERVED		12
#define PDC_PCI_PCI_INT_ROUTE_SIZE	13
#define PDC_PCI_GET_INT_TBL_SIZE	PDC_PCI_PCI_INT_ROUTE_SIZE
#define PDC_PCI_PCI_INT_ROUTE		14
#define PDC_PCI_GET_INT_TBL		PDC_PCI_PCI_INT_ROUTE
#define PDC_PCI_READ_MON_TYPE		15
#define PDC_PCI_WRITE_MON_TYPE		16
#define PDC_RELOCATE	149		 
#define PDC_RELOCATE_GET_RELOCINFO	0
#define PDC_RELOCATE_CHECKSUM		1
#define PDC_RELOCATE_RELOCATE		2
#define PDC_INITIATOR	163
#define PDC_GET_INITIATOR	0
#define PDC_SET_INITIATOR	1
#define PDC_DELETE_INITIATOR	2
#define PDC_RETURN_TABLE_SIZE	3
#define PDC_RETURN_TABLE	4
#define PDC_LINK	165 		 
#define PDC_LINK_PCI_ENTRY_POINTS	0   
#define PDC_LINK_USB_ENTRY_POINTS	1   
#define	CL_NULL		0	 
#define	CL_RANDOM	1	 
#define	CL_SEQU		2	 
#define	CL_DUPLEX	7	 
#define	CL_KEYBD	8	 
#define	CL_DISPL	9	 
#define	CL_FC		10	 
#define ENTRY_INIT_SRCH_FRST	2
#define ENTRY_INIT_SRCH_NEXT	3
#define ENTRY_INIT_MOD_DEV	4
#define ENTRY_INIT_DEV		5
#define ENTRY_INIT_MOD		6
#define ENTRY_INIT_MSG		9
#define ENTRY_IO_BOOTIN		0
#define ENTRY_IO_BOOTOUT	1
#define ENTRY_IO_CIN		2
#define ENTRY_IO_COUT		3
#define ENTRY_IO_CLOSE		4
#define ENTRY_IO_GETMSG		9
#define ENTRY_IO_BBLOCK_IN	16
#define ENTRY_IO_BBLOCK_OUT	17
#define OS_ID_NONE		0	 
#define OS_ID_HPUX		1	 
#define OS_ID_MPEXL		2	 
#define OS_ID_OSF		3	 
#define OS_ID_HPRT		4	 
#define OS_ID_NOVEL		5	 
#define OS_ID_LINUX		6	 
#define OSTAT_OFF		0
#define OSTAT_FLT		1
#define OSTAT_TEST		2
#define OSTAT_INIT		3
#define OSTAT_SHUT		4
#define OSTAT_WARN		5
#define OSTAT_RUN		6
#define OSTAT_ON		7
#define BOOT_CONSOLE_HPA_OFFSET  0x3c0
#define BOOT_CONSOLE_SPA_OFFSET  0x3c4
#define BOOT_CONSOLE_PATH_OFFSET 0x3a8
#define NUM_PDC_RESULT	32
#if !defined(__ASSEMBLY__)
#define	PF_AUTOBOOT	0x80
#define	PF_AUTOSEARCH	0x40
#define	PF_TIMER	0x0F
struct hardware_path {
	unsigned char flags;	 
	signed   char bc[6];	 
	signed   char mod;	 
};
struct pdc_module_path {	 
	struct hardware_path path;
	unsigned int layers[6];  
} __attribute__((aligned(8)));
struct pz_device {
	struct pdc_module_path dp;	 
	unsigned int hpa;	 
	unsigned int spa;	 
	unsigned int iodc_io;	 
	short	pad;		 
	unsigned short cl_class; 
} __attribute__((aligned(8))) ;
struct zeropage {
	unsigned int	vec_special;		 
	unsigned int	vec_pow_fail;  
	unsigned int	vec_toc;
	unsigned int	vec_toclen;
	unsigned int vec_rendz;
	int	vec_pow_fail_flen;
	int	vec_pad0[3];
	unsigned int vec_toc_hi;
	int	vec_pad1[6];
	int	pad0[112];               
	int	pad1[84];
	int	memc_cont;		 
	int	memc_phsize;		 
	int	memc_adsize;		 
	unsigned int mem_pdc_hi;	 
	unsigned int mem_booterr[8];	 
	unsigned int mem_free;		 
	unsigned int mem_hpa;		 
	unsigned int mem_pdc;		 
	unsigned int mem_10msec;	 
	unsigned int imm_hpa;		 
	int	imm_soft_boot;		 
	unsigned int	imm_spa_size;		 
	unsigned int	imm_max_mem;		 
	struct pz_device mem_cons;	 
	struct pz_device mem_boot;	 
	struct pz_device mem_kbd;	 
	int	pad430[116];
	unsigned int pad600[1];
	unsigned int proc_sti;		 
	unsigned int pad608[126];
};
struct pdc_chassis_info {        
	unsigned long actcnt;    
	unsigned long maxcnt;    
};
struct pdc_coproc_cfg {          
        unsigned long ccr_functional;
        unsigned long ccr_present;
        unsigned long revision;
        unsigned long model;
};
struct pdc_model {		 
	unsigned long hversion;
	unsigned long sversion;
	unsigned long hw_id;
	unsigned long boot_id;
	unsigned long sw_id;
	unsigned long sw_cap;
	unsigned long arch_rev;
	unsigned long pot_key;
	unsigned long curr_key;
	unsigned long width;	 
};
struct pdc_cache_cf {		 
    unsigned long
#ifdef __LP64__
		cc_padW:32,
#endif
		cc_alias: 4,	 
		cc_block: 4,	 
		cc_line	: 3,	 
		cc_shift: 2,	 
		cc_wt	: 1,	 
		cc_sh	: 2,	 
		cc_cst  : 3,	 
		cc_pad1 : 10,	 
		cc_hv   : 3;	 
};
struct pdc_tlb_cf {		 
    unsigned long tc_pad0:12,	 
#ifdef __LP64__
		tc_padW:32,
#endif
		tc_sh	: 2,	 
		tc_hv   : 1,	 
		tc_page : 1,	 
		tc_cst  : 3,	 
		tc_aid  : 5,	 
		tc_sr   : 8;	 
};
struct pdc_cache_info {		 
	unsigned long	ic_size;	 
	struct pdc_cache_cf ic_conf;	 
	unsigned long	ic_base;	 
	unsigned long	ic_stride;
	unsigned long	ic_count;
	unsigned long	ic_loop;
	unsigned long	dc_size;	 
	struct pdc_cache_cf dc_conf;	 
	unsigned long	dc_base;	 
	unsigned long	dc_stride;
	unsigned long	dc_count;
	unsigned long	dc_loop;
	unsigned long	it_size;	 
	struct pdc_tlb_cf it_conf;	 
	unsigned long	it_sp_base;
	unsigned long	it_sp_stride;
	unsigned long	it_sp_count;
	unsigned long	it_off_base;
	unsigned long	it_off_stride;
	unsigned long	it_off_count;
	unsigned long	it_loop;
	unsigned long	dt_size;	 
	struct pdc_tlb_cf dt_conf;	 
	unsigned long	dt_sp_base;
	unsigned long	dt_sp_stride;
	unsigned long	dt_sp_count;
	unsigned long	dt_off_base;
	unsigned long	dt_off_stride;
	unsigned long	dt_off_count;
	unsigned long	dt_loop;
};
struct pdc_iodc {      
	unsigned char   hversion_model;
	unsigned char 	hversion;
	unsigned char 	spa;
	unsigned char 	type;
	unsigned int	sversion_rev:4;
	unsigned int	sversion_model:19;
	unsigned int	sversion_opt:8;
	unsigned char	rev;
	unsigned char	dep;
	unsigned char	features;
	unsigned char	pad1;
	unsigned int	checksum:16;
	unsigned int	length:16;
	unsigned int    pad[15];
} __attribute__((aligned(8))) ;
struct pdc_btlb_info_range {
	unsigned char res00;
	unsigned char num_i;
	unsigned char num_d;
	unsigned char num_comb;
};
struct pdc_btlb_info {	 
	unsigned int min_size;	 
	unsigned int max_size;	 
	struct pdc_btlb_info_range fixed_range_info;
	struct pdc_btlb_info_range variable_range_info;
};
struct pdc_mem_retinfo {  
	unsigned long pdt_size;
	unsigned long pdt_entries;
	unsigned long pdt_status;
	unsigned long first_dbe_loc;
	unsigned long good_mem;
};
struct pdc_mem_read_pdt {  
	unsigned long pdt_entries;
};
#ifdef __LP64__
struct pdc_memory_table_raddr {  
	unsigned long entries_returned;
	unsigned long entries_total;
};
struct pdc_memory_table {        
	unsigned long paddr;
	unsigned int  pages;
	unsigned int  reserved;
};
#endif  
struct pdc_system_map_mod_info {  
	unsigned long mod_addr;
	unsigned long mod_pgs;
	unsigned long add_addrs;
};
struct pdc_system_map_addr_info {  
	unsigned long mod_addr;
	unsigned long mod_pgs;
};
struct pdc_initiator {  
	int host_id;
	int factor;
	int width;
	int mode;
};
struct pdc_memory_map {		 
	unsigned long hpa;	 
	unsigned long more_pgs;	 
};
struct pdc_tod {
	unsigned long tod_sec;
	unsigned long tod_usec;
};
struct pdc_hpmc_pim_11 {  
	unsigned int gr[32];
	unsigned int cr[32];
	unsigned int sr[8];
	unsigned int iasq_back;
	unsigned int iaoq_back;
	unsigned int check_type;
	unsigned int cpu_state;
	unsigned int rsvd1;
	unsigned int cache_check;
	unsigned int tlb_check;
	unsigned int bus_check;
	unsigned int assists_check;
	unsigned int rsvd2;
	unsigned int assist_state;
	unsigned int responder_addr;
	unsigned int requestor_addr;
	unsigned int path_info;
	unsigned long long fr[32];
};
struct pdc_hpmc_pim_20 {  
	unsigned long long gr[32];
	unsigned long long cr[32];
	unsigned long long sr[8];
	unsigned long long iasq_back;
	unsigned long long iaoq_back;
	unsigned int check_type;
	unsigned int cpu_state;
	unsigned int cache_check;
	unsigned int tlb_check;
	unsigned int bus_check;
	unsigned int assists_check;
	unsigned int assist_state;
	unsigned int path_info;
	unsigned long long responder_addr;
	unsigned long long requestor_addr;
	unsigned long long fr[32];
};
struct pim_cpu_state_cf {
	union {
	unsigned int
		iqv : 1,	 
		iqf : 1,	 
		ipv : 1,	 
		grv : 1,	 
		crv : 1,	 
		srv : 1,	 
		trv : 1,	 
		pad : 24,	 
		td  : 1;	 
	unsigned int val;
	};
};
struct pdc_toc_pim_11 {
	unsigned int gr[32];
	unsigned int cr[32];
	unsigned int sr[8];
	unsigned int iasq_back;
	unsigned int iaoq_back;
	unsigned int check_type;
	struct pim_cpu_state_cf cpu_state;
};
struct pdc_toc_pim_20 {
	unsigned long long gr[32];
	unsigned long long cr[32];
	unsigned long long sr[8];
	unsigned long long iasq_back;
	unsigned long long iaoq_back;
	unsigned int check_type;
	struct pim_cpu_state_cf cpu_state;
};
#endif  
#endif  
