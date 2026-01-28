struct irt_entry {
	u8 entry_type;
	u8 entry_length;
	u8 interrupt_type;
	u8 polarity_trigger;
	u8 src_bus_irq_devno;
	u8 src_bus_id;
	u8 src_seg_id;
	u8 dest_iosapic_intin;
	u64 dest_iosapic_addr;
};
#define IRT_IOSAPIC_TYPE   139
#define IRT_IOSAPIC_LENGTH 16
#define IRT_VECTORED_INTR  0
#define IRT_PO_MASK        0x3
#define IRT_ACTIVE_HI      1
#define IRT_ACTIVE_LO      3
#define IRT_EL_MASK        0x3
#define IRT_EL_SHIFT       2
#define IRT_EDGE_TRIG      1
#define IRT_LEVEL_TRIG     3
#define IRT_IRQ_MASK       0x3
#define IRT_DEV_MASK       0x1f
#define IRT_DEV_SHIFT      2
#define IRT_IRQ_DEVNO_MASK	((IRT_DEV_MASK << IRT_DEV_SHIFT) | IRT_IRQ_MASK)
#ifdef SUPPORT_MULTI_CELL
struct iosapic_irt {
        struct iosapic_irt *irt_next;   
        struct irt_entry *irt_base;              
        size_t  irte_count;             
        size_t  irte_size;              
};
#endif
struct vector_info {
	struct iosapic_info *iosapic;	 
	struct irt_entry *irte;		 
	__le32 __iomem *eoi_addr;	 
	__le32	eoi_data;		 
	int	txn_irq;		 
	ulong	txn_addr;		 
	u32	txn_data;		 
	u8	status;			 
	u8	irqline;		 
};
struct iosapic_info {
	struct iosapic_info *	isi_next;	 
	void __iomem *		addr;		 
	unsigned long		isi_hpa;	 
	struct vector_info *	isi_vector;	 
	int			isi_num_vectors;  
	int			isi_status;	 
	unsigned int		isi_version;	 
};
#ifdef __IA64__
struct local_sapic_info {
	struct local_sapic_info *lsi_next;       
	int                     *lsi_cpu_id;     
	unsigned long           *lsi_id_eid;     
	int                     *lsi_status;     
	void                    *lsi_private;    
};
struct sapic_info {
	struct sapic_info	*si_next;	 
	int                     si_cellid;       
	unsigned int            si_status;        
	char                    *si_pib_base;    
	local_sapic_info_t      *si_local_info;
	io_sapic_info_t         *si_io_info;
	extint_info_t           *si_extint_info; 
};
#endif
