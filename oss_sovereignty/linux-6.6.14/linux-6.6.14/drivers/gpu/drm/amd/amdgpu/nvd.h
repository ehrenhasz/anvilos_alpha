#ifndef NVD_H
#define NVD_H
#define	PACKET_TYPE0	0
#define	PACKET_TYPE1	1
#define	PACKET_TYPE2	2
#define	PACKET_TYPE3	3
#define CP_PACKET_GET_TYPE(h) (((h) >> 30) & 3)
#define CP_PACKET_GET_COUNT(h) (((h) >> 16) & 0x3FFF)
#define CP_PACKET0_GET_REG(h) ((h) & 0xFFFF)
#define CP_PACKET3_GET_OPCODE(h) (((h) >> 8) & 0xFF)
#define PACKET0(reg, n)	((PACKET_TYPE0 << 30) |				\
			 ((reg) & 0xFFFF) |			\
			 ((n) & 0x3FFF) << 16)
#define CP_PACKET2			0x80000000
#define		PACKET2_PAD_SHIFT		0
#define		PACKET2_PAD_MASK		(0x3fffffff << 0)
#define PACKET2(v)	(CP_PACKET2 | REG_SET(PACKET2_PAD, (v)))
#define PACKET3(op, n)	((PACKET_TYPE3 << 30) |				\
			 (((op) & 0xFF) << 8) |				\
			 ((n) & 0x3FFF) << 16)
#define PACKET3_COMPUTE(op, n) (PACKET3(op, n) | 1 << 1)
#define	PACKET3_NOP					0x10
#define	PACKET3_SET_BASE				0x11
#define		PACKET3_BASE_INDEX(x)                  ((x) << 0)
#define			CE_PARTITION_BASE		3
#define	PACKET3_CLEAR_STATE				0x12
#define	PACKET3_INDEX_BUFFER_SIZE			0x13
#define	PACKET3_DISPATCH_DIRECT				0x15
#define	PACKET3_DISPATCH_INDIRECT			0x16
#define	PACKET3_INDIRECT_BUFFER_END			0x17
#define	PACKET3_INDIRECT_BUFFER_CNST_END		0x19
#define	PACKET3_ATOMIC_GDS				0x1D
#define	PACKET3_ATOMIC_MEM				0x1E
#define	PACKET3_OCCLUSION_QUERY				0x1F
#define	PACKET3_SET_PREDICATION				0x20
#define	PACKET3_REG_RMW					0x21
#define	PACKET3_COND_EXEC				0x22
#define	PACKET3_PRED_EXEC				0x23
#define	PACKET3_DRAW_INDIRECT				0x24
#define	PACKET3_DRAW_INDEX_INDIRECT			0x25
#define	PACKET3_INDEX_BASE				0x26
#define	PACKET3_DRAW_INDEX_2				0x27
#define	PACKET3_CONTEXT_CONTROL				0x28
#define	PACKET3_INDEX_TYPE				0x2A
#define	PACKET3_DRAW_INDIRECT_MULTI			0x2C
#define	PACKET3_DRAW_INDEX_AUTO				0x2D
#define	PACKET3_NUM_INSTANCES				0x2F
#define	PACKET3_DRAW_INDEX_MULTI_AUTO			0x30
#define	PACKET3_INDIRECT_BUFFER_PRIV			0x32
#define	PACKET3_INDIRECT_BUFFER_CNST			0x33
#define	PACKET3_COND_INDIRECT_BUFFER_CNST		0x33
#define	PACKET3_STRMOUT_BUFFER_UPDATE			0x34
#define	PACKET3_DRAW_INDEX_OFFSET_2			0x35
#define	PACKET3_DRAW_PREAMBLE				0x36
#define	PACKET3_WRITE_DATA				0x37
#define		WRITE_DATA_DST_SEL(x)                   ((x) << 8)
#define		WR_ONE_ADDR                             (1 << 16)
#define		WR_CONFIRM                              (1 << 20)
#define		WRITE_DATA_CACHE_POLICY(x)              ((x) << 25)
#define		WRITE_DATA_ENGINE_SEL(x)                ((x) << 30)
#define	PACKET3_DRAW_INDEX_INDIRECT_MULTI		0x38
#define	PACKET3_MEM_SEMAPHORE				0x39
#              define PACKET3_SEM_USE_MAILBOX       (0x1 << 16)
#              define PACKET3_SEM_SEL_SIGNAL_TYPE   (0x1 << 20)  
#              define PACKET3_SEM_SEL_SIGNAL	    (0x6 << 29)
#              define PACKET3_SEM_SEL_WAIT	    (0x7 << 29)
#define	PACKET3_DRAW_INDEX_MULTI_INST			0x3A
#define	PACKET3_COPY_DW					0x3B
#define	PACKET3_WAIT_REG_MEM				0x3C
#define		WAIT_REG_MEM_FUNCTION(x)                ((x) << 0)
#define		WAIT_REG_MEM_MEM_SPACE(x)               ((x) << 4)
#define		WAIT_REG_MEM_OPERATION(x)               ((x) << 6)
#define		WAIT_REG_MEM_ENGINE(x)                  ((x) << 8)
#define	PACKET3_INDIRECT_BUFFER				0x3F
#define		INDIRECT_BUFFER_VALID                   (1 << 23)
#define		INDIRECT_BUFFER_CACHE_POLICY(x)         ((x) << 28)
#define		INDIRECT_BUFFER_PRE_ENB(x)		((x) << 21)
#define		INDIRECT_BUFFER_PRE_RESUME(x)           ((x) << 30)
#define	PACKET3_COND_INDIRECT_BUFFER			0x3F
#define	PACKET3_COPY_DATA				0x40
#define	PACKET3_CP_DMA					0x41
#define	PACKET3_PFP_SYNC_ME				0x42
#define	PACKET3_SURFACE_SYNC				0x43
#define	PACKET3_ME_INITIALIZE				0x44
#define	PACKET3_COND_WRITE				0x45
#define	PACKET3_EVENT_WRITE				0x46
#define		EVENT_TYPE(x)                           ((x) << 0)
#define		EVENT_INDEX(x)                          ((x) << 8)
#define	PACKET3_EVENT_WRITE_EOP				0x47
#define	PACKET3_EVENT_WRITE_EOS				0x48
#define	PACKET3_RELEASE_MEM				0x49
#define		PACKET3_RELEASE_MEM_EVENT_TYPE(x)	((x) << 0)
#define		PACKET3_RELEASE_MEM_EVENT_INDEX(x)	((x) << 8)
#define		PACKET3_RELEASE_MEM_GCR_GLM_WB		(1 << 12)
#define		PACKET3_RELEASE_MEM_GCR_GLM_INV		(1 << 13)
#define		PACKET3_RELEASE_MEM_GCR_GLV_INV		(1 << 14)
#define		PACKET3_RELEASE_MEM_GCR_GL1_INV		(1 << 15)
#define		PACKET3_RELEASE_MEM_GCR_GL2_US		(1 << 16)
#define		PACKET3_RELEASE_MEM_GCR_GL2_RANGE	(1 << 17)
#define		PACKET3_RELEASE_MEM_GCR_GL2_DISCARD	(1 << 19)
#define		PACKET3_RELEASE_MEM_GCR_GL2_INV		(1 << 20)
#define		PACKET3_RELEASE_MEM_GCR_GL2_WB		(1 << 21)
#define		PACKET3_RELEASE_MEM_GCR_SEQ		(1 << 22)
#define		PACKET3_RELEASE_MEM_CACHE_POLICY(x)	((x) << 25)
#define		PACKET3_RELEASE_MEM_EXECUTE		(1 << 28)
#define		PACKET3_RELEASE_MEM_DATA_SEL(x)		((x) << 29)
#define		PACKET3_RELEASE_MEM_INT_SEL(x)		((x) << 24)
#define		PACKET3_RELEASE_MEM_DST_SEL(x)		((x) << 16)
#define	PACKET3_PREAMBLE_CNTL				0x4A
#              define PACKET3_PREAMBLE_BEGIN_CLEAR_STATE     (2 << 28)
#              define PACKET3_PREAMBLE_END_CLEAR_STATE       (3 << 28)
#define	PACKET3_DMA_DATA				0x50
#              define PACKET3_DMA_DATA_ENGINE(x)     ((x) << 0)
#              define PACKET3_DMA_DATA_SRC_CACHE_POLICY(x) ((x) << 13)
#              define PACKET3_DMA_DATA_DST_SEL(x)  ((x) << 20)
#              define PACKET3_DMA_DATA_DST_CACHE_POLICY(x) ((x) << 25)
#              define PACKET3_DMA_DATA_SRC_SEL(x)  ((x) << 29)
#              define PACKET3_DMA_DATA_CP_SYNC     (1 << 31)
#              define PACKET3_DMA_DATA_CMD_SAS     (1 << 26)
#              define PACKET3_DMA_DATA_CMD_DAS     (1 << 27)
#              define PACKET3_DMA_DATA_CMD_SAIC    (1 << 28)
#              define PACKET3_DMA_DATA_CMD_DAIC    (1 << 29)
#              define PACKET3_DMA_DATA_CMD_RAW_WAIT  (1 << 30)
#define	PACKET3_CONTEXT_REG_RMW				0x51
#define	PACKET3_GFX_CNTX_UPDATE				0x52
#define	PACKET3_BLK_CNTX_UPDATE				0x53
#define	PACKET3_INCR_UPDT_STATE				0x55
#define	PACKET3_ACQUIRE_MEM				0x58
#define 	PACKET3_ACQUIRE_MEM_GCR_CNTL_GLI_INV(x) ((x) << 0)
#define 	PACKET3_ACQUIRE_MEM_GCR_CNTL_GL1_RANGE(x) ((x) << 2)
#define 	PACKET3_ACQUIRE_MEM_GCR_CNTL_GLM_WB(x) ((x) << 4)
#define 	PACKET3_ACQUIRE_MEM_GCR_CNTL_GLM_INV(x) ((x) << 5)
#define 	PACKET3_ACQUIRE_MEM_GCR_CNTL_GLK_WB(x) ((x) << 6)
#define 	PACKET3_ACQUIRE_MEM_GCR_CNTL_GLK_INV(x) ((x) << 7)
#define 	PACKET3_ACQUIRE_MEM_GCR_CNTL_GLV_INV(x) ((x) << 8)
#define 	PACKET3_ACQUIRE_MEM_GCR_CNTL_GL1_INV(x) ((x) << 9)
#define 	PACKET3_ACQUIRE_MEM_GCR_CNTL_GL2_US(x) ((x) << 10)
#define 	PACKET3_ACQUIRE_MEM_GCR_CNTL_GL2_RANGE(x) ((x) << 11)
#define 	PACKET3_ACQUIRE_MEM_GCR_CNTL_GL2_DISCARD(x)  ((x) << 13)
#define 	PACKET3_ACQUIRE_MEM_GCR_CNTL_GL2_INV(x) ((x) << 14)
#define 	PACKET3_ACQUIRE_MEM_GCR_CNTL_GL2_WB(x) ((x) << 15)
#define 	PACKET3_ACQUIRE_MEM_GCR_CNTL_SEQ(x) ((x) << 16)
#define 	PACKET3_ACQUIRE_MEM_GCR_RANGE_IS_PA  (1 << 18)
#define	PACKET3_REWIND					0x59
#define	PACKET3_INTERRUPT				0x5A
#define	PACKET3_GEN_PDEPTE				0x5B
#define	PACKET3_INDIRECT_BUFFER_PASID			0x5C
#define	PACKET3_PRIME_UTCL2				0x5D
#define	PACKET3_LOAD_UCONFIG_REG			0x5E
#define	PACKET3_LOAD_SH_REG				0x5F
#define	PACKET3_LOAD_CONFIG_REG				0x60
#define	PACKET3_LOAD_CONTEXT_REG			0x61
#define	PACKET3_LOAD_COMPUTE_STATE			0x62
#define	PACKET3_LOAD_SH_REG_INDEX			0x63
#define	PACKET3_SET_CONFIG_REG				0x68
#define		PACKET3_SET_CONFIG_REG_START			0x00002000
#define		PACKET3_SET_CONFIG_REG_END			0x00002c00
#define	PACKET3_SET_CONTEXT_REG				0x69
#define		PACKET3_SET_CONTEXT_REG_START			0x0000a000
#define		PACKET3_SET_CONTEXT_REG_END			0x0000a400
#define	PACKET3_SET_CONTEXT_REG_INDEX			0x6A
#define	PACKET3_SET_VGPR_REG_DI_MULTI			0x71
#define	PACKET3_SET_SH_REG_DI				0x72
#define	PACKET3_SET_CONTEXT_REG_INDIRECT		0x73
#define	PACKET3_SET_SH_REG_DI_MULTI			0x74
#define	PACKET3_GFX_PIPE_LOCK				0x75
#define	PACKET3_SET_SH_REG				0x76
#define		PACKET3_SET_SH_REG_START			0x00002c00
#define		PACKET3_SET_SH_REG_END				0x00003000
#define	PACKET3_SET_SH_REG_OFFSET			0x77
#define	PACKET3_SET_QUEUE_REG				0x78
#define	PACKET3_SET_UCONFIG_REG				0x79
#define		PACKET3_SET_UCONFIG_REG_START			0x0000c000
#define		PACKET3_SET_UCONFIG_REG_END			0x0000c400
#define	PACKET3_SET_UCONFIG_REG_INDEX			0x7A
#define	PACKET3_FORWARD_HEADER				0x7C
#define	PACKET3_SCRATCH_RAM_WRITE			0x7D
#define	PACKET3_SCRATCH_RAM_READ			0x7E
#define	PACKET3_LOAD_CONST_RAM				0x80
#define	PACKET3_WRITE_CONST_RAM				0x81
#define	PACKET3_DUMP_CONST_RAM				0x83
#define	PACKET3_INCREMENT_CE_COUNTER			0x84
#define	PACKET3_INCREMENT_DE_COUNTER			0x85
#define	PACKET3_WAIT_ON_CE_COUNTER			0x86
#define	PACKET3_WAIT_ON_DE_COUNTER_DIFF			0x88
#define	PACKET3_SWITCH_BUFFER				0x8B
#define	PACKET3_DISPATCH_DRAW_PREAMBLE			0x8C
#define	PACKET3_DISPATCH_DRAW_PREAMBLE_ACE		0x8C
#define	PACKET3_DISPATCH_DRAW				0x8D
#define	PACKET3_DISPATCH_DRAW_ACE			0x8D
#define	PACKET3_GET_LOD_STATS				0x8E
#define	PACKET3_DRAW_MULTI_PREAMBLE			0x8F
#define	PACKET3_FRAME_CONTROL				0x90
#			define FRAME_TMZ	(1 << 0)
#			define FRAME_CMD(x) ((x) << 28)
#define	PACKET3_INDEX_ATTRIBUTES_INDIRECT		0x91
#define	PACKET3_WAIT_REG_MEM64				0x93
#define	PACKET3_COND_PREEMPT				0x94
#define	PACKET3_HDP_FLUSH				0x95
#define	PACKET3_COPY_DATA_RB				0x96
#define	PACKET3_INVALIDATE_TLBS				0x98
#              define PACKET3_INVALIDATE_TLBS_DST_SEL(x)     ((x) << 0)
#              define PACKET3_INVALIDATE_TLBS_ALL_HUB(x)     ((x) << 4)
#              define PACKET3_INVALIDATE_TLBS_PASID(x)       ((x) << 5)
#define	PACKET3_AQL_PACKET				0x99
#define	PACKET3_DMA_DATA_FILL_MULTI			0x9A
#define	PACKET3_SET_SH_REG_INDEX			0x9B
#define	PACKET3_DRAW_INDIRECT_COUNT_MULTI		0x9C
#define	PACKET3_DRAW_INDEX_INDIRECT_COUNT_MULTI		0x9D
#define	PACKET3_DUMP_CONST_RAM_OFFSET			0x9E
#define	PACKET3_LOAD_CONTEXT_REG_INDEX			0x9F
#define	PACKET3_SET_RESOURCES				0xA0
#              define PACKET3_SET_RESOURCES_VMID_MASK(x)     ((x) << 0)
#              define PACKET3_SET_RESOURCES_UNMAP_LATENTY(x) ((x) << 16)
#              define PACKET3_SET_RESOURCES_QUEUE_TYPE(x)    ((x) << 29)
#define PACKET3_MAP_PROCESS				0xA1
#define PACKET3_MAP_QUEUES				0xA2
#              define PACKET3_MAP_QUEUES_QUEUE_SEL(x)       ((x) << 4)
#              define PACKET3_MAP_QUEUES_VMID(x)            ((x) << 8)
#              define PACKET3_MAP_QUEUES_QUEUE(x)           ((x) << 13)
#              define PACKET3_MAP_QUEUES_PIPE(x)            ((x) << 16)
#              define PACKET3_MAP_QUEUES_ME(x)              ((x) << 18)
#              define PACKET3_MAP_QUEUES_QUEUE_TYPE(x)      ((x) << 21)
#              define PACKET3_MAP_QUEUES_ALLOC_FORMAT(x)    ((x) << 24)
#              define PACKET3_MAP_QUEUES_ENGINE_SEL(x)      ((x) << 26)
#              define PACKET3_MAP_QUEUES_NUM_QUEUES(x)      ((x) << 29)
#              define PACKET3_MAP_QUEUES_CHECK_DISABLE(x)   ((x) << 1)
#              define PACKET3_MAP_QUEUES_DOORBELL_OFFSET(x) ((x) << 2)
#define	PACKET3_UNMAP_QUEUES				0xA3
#              define PACKET3_UNMAP_QUEUES_ACTION(x)           ((x) << 0)
#              define PACKET3_UNMAP_QUEUES_QUEUE_SEL(x)        ((x) << 4)
#              define PACKET3_UNMAP_QUEUES_ENGINE_SEL(x)       ((x) << 26)
#              define PACKET3_UNMAP_QUEUES_NUM_QUEUES(x)       ((x) << 29)
#              define PACKET3_UNMAP_QUEUES_PASID(x)            ((x) << 0)
#              define PACKET3_UNMAP_QUEUES_DOORBELL_OFFSET0(x) ((x) << 2)
#              define PACKET3_UNMAP_QUEUES_DOORBELL_OFFSET1(x) ((x) << 2)
#              define PACKET3_UNMAP_QUEUES_RB_WPTR(x)          ((x) << 0)
#              define PACKET3_UNMAP_QUEUES_DOORBELL_OFFSET2(x) ((x) << 2)
#              define PACKET3_UNMAP_QUEUES_DOORBELL_OFFSET3(x) ((x) << 2)
#define	PACKET3_QUERY_STATUS				0xA4
#              define PACKET3_QUERY_STATUS_CONTEXT_ID(x)       ((x) << 0)
#              define PACKET3_QUERY_STATUS_INTERRUPT_SEL(x)    ((x) << 28)
#              define PACKET3_QUERY_STATUS_COMMAND(x)          ((x) << 30)
#              define PACKET3_QUERY_STATUS_PASID(x)            ((x) << 0)
#              define PACKET3_QUERY_STATUS_DOORBELL_OFFSET(x)  ((x) << 2)
#              define PACKET3_QUERY_STATUS_ENG_SEL(x)          ((x) << 25)
#define	PACKET3_RUN_LIST				0xA5
#define	PACKET3_MAP_PROCESS_VM				0xA6
#define	PACKET3_SET_Q_PREEMPTION_MODE			0xF0
#              define PACKET3_SET_Q_PREEMPTION_MODE_IB_VMID(x)  ((x) << 0)
#              define PACKET3_SET_Q_PREEMPTION_MODE_INIT_SHADOW_MEM    (1 << 0)
#endif
