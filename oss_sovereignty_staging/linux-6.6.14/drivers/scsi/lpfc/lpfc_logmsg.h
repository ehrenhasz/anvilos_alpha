 

#define LOG_ELS		0x00000001	 
#define LOG_DISCOVERY	0x00000002	 
#define LOG_MBOX	0x00000004	 
#define LOG_INIT	0x00000008	 
#define LOG_LINK_EVENT	0x00000010	 
#define LOG_IP		0x00000020	 
#define LOG_FCP		0x00000040	 
#define LOG_NODE	0x00000080	 
#define LOG_TEMP	0x00000100	 
#define LOG_BG		0x00000200	 
#define LOG_MISC	0x00000400	 
#define LOG_SLI		0x00000800	 
#define LOG_FCP_ERROR	0x00001000	 
#define LOG_LIBDFC	0x00002000	 
#define LOG_VPORT	0x00004000	 
#define LOG_LDS_EVENT	0x00008000	 
#define LOG_EVENT	0x00010000	 
#define LOG_FIP		0x00020000	 
#define LOG_FCP_UNDER	0x00040000	 
#define LOG_SCSI_CMD	0x00080000	 
#define LOG_NVME	0x00100000	 
#define LOG_NVME_DISC   0x00200000       
#define LOG_NVME_ABTS   0x00400000       
#define LOG_NVME_IOERR  0x00800000       
#define LOG_RSVD1	0x01000000	 
#define LOG_RSVD2	0x02000000	 
#define LOG_CGN_MGMT    0x04000000	 
#define LOG_TRACE_EVENT 0x80000000	 
#define LOG_ALL_MSG	0x7fffffff	 

void lpfc_dmp_dbg(struct lpfc_hba *phba);
void lpfc_dbg_print(struct lpfc_hba *phba, const char *fmt, ...);

 
#define lpfc_vlog_msg(vport, level, mask, fmt, arg...) \
{ if (((mask) & (vport)->cfg_log_verbose) || (level[1] <= '5')) \
	dev_printk(level, &((vport)->phba->pcidev)->dev, "%d:(%d):" \
		   fmt, (vport)->phba->brd_no, vport->vpi, ##arg); }

#define lpfc_log_msg(phba, level, mask, fmt, arg...) \
do { \
	{ uint32_t log_verbose = (phba)->pport ? \
				 (phba)->pport->cfg_log_verbose : \
				 (phba)->cfg_log_verbose; \
	if (((mask) & log_verbose) || (level[1] <= '5')) \
		dev_printk(level, &((phba)->pcidev)->dev, "%d:" \
			   fmt, phba->brd_no, ##arg); \
	} \
} while (0)

#define lpfc_printf_vlog(vport, level, mask, fmt, arg...) \
do { \
	{ if (((mask) & (vport)->cfg_log_verbose) || (level[1] <= '3')) { \
		if ((mask) & LOG_TRACE_EVENT && !(vport)->cfg_log_verbose) \
			lpfc_dmp_dbg((vport)->phba); \
		dev_printk(level, &((vport)->phba->pcidev)->dev, "%d:(%d):" \
			   fmt, (vport)->phba->brd_no, vport->vpi, ##arg);  \
		} else if (!(vport)->cfg_log_verbose) \
			lpfc_dbg_print((vport)->phba, "%d:(%d):" fmt, \
				(vport)->phba->brd_no, (vport)->vpi, ##arg); \
	} \
} while (0)

#define lpfc_printf_log(phba, level, mask, fmt, arg...) \
do { \
	{ uint32_t log_verbose = (phba)->pport ? \
				 (phba)->pport->cfg_log_verbose : \
				 (phba)->cfg_log_verbose; \
	if (((mask) & log_verbose) || (level[1] <= '3')) { \
		if ((mask) & LOG_TRACE_EVENT && !log_verbose) \
			lpfc_dmp_dbg(phba); \
		dev_printk(level, &((phba)->pcidev)->dev, "%d:" \
			fmt, phba->brd_no, ##arg); \
	} else if (!log_verbose)\
		lpfc_dbg_print(phba, "%d:" fmt, phba->brd_no, ##arg); \
	} \
} while (0)
