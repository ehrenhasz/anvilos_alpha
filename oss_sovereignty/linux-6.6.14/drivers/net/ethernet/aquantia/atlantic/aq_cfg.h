 
 

 

#ifndef AQ_CFG_H
#define AQ_CFG_H

#define AQ_CFG_VECS_DEF   8U
#define AQ_CFG_TCS_DEF    1U

#define AQ_CFG_TXDS_DEF    4096U
#define AQ_CFG_RXDS_DEF    2048U

#define AQ_CFG_IS_POLLING_DEF 0U

#define AQ_CFG_FORCE_LEGACY_INT 0U

#define AQ_CFG_INTERRUPT_MODERATION_OFF		0
#define AQ_CFG_INTERRUPT_MODERATION_ON		1
#define AQ_CFG_INTERRUPT_MODERATION_AUTO	0xFFFFU

#define AQ_CFG_INTERRUPT_MODERATION_USEC_MAX (0x1FF * 2)

#define AQ_CFG_IRQ_MASK                      0x3FFU

#define AQ_CFG_VECS_MAX   8U
#define AQ_CFG_TCS_MAX    8U

#define AQ_CFG_TX_FRAME_MAX  (16U * 1024U)
#define AQ_CFG_RX_FRAME_MAX  (2U * 1024U)

#define AQ_CFG_TX_CLEAN_BUDGET 256U

#define AQ_CFG_RX_REFILL_THRES 32U

#define AQ_CFG_RX_HDR_SIZE 256U

#define AQ_CFG_RX_PAGEORDER 0U
#define AQ_CFG_XDP_PAGEORDER 2U

 
#define AQ_CFG_IS_LRO_DEF           1U

 
#define AQ_CFG_RSS_INDIRECTION_TABLE_MAX  64U
#define AQ_CFG_RSS_HASHKEY_SIZE           40U

#define AQ_CFG_IS_RSS_DEF           1U
#define AQ_CFG_NUM_RSS_QUEUES_DEF   AQ_CFG_VECS_DEF
#define AQ_CFG_RSS_BASE_CPU_NUM_DEF 0U

#define AQ_CFG_PCI_FUNC_MSIX_IRQS   9U
#define AQ_CFG_PCI_FUNC_PORTS       2U

#define AQ_CFG_SERVICE_TIMER_INTERVAL    (1 * HZ)
#define AQ_CFG_POLLING_TIMER_INTERVAL   ((unsigned int)(2 * HZ))

#define AQ_CFG_SKB_FRAGS_MAX   32U

 
#define AQ_CFG_RESTART_DESC_THRES   (AQ_CFG_SKB_FRAGS_MAX * 2)

 

#define AQ_CFG_FC_MODE AQ_NIC_FC_FULL

 
#define AQ_CFG_WOL_MODES WAKE_MAGIC

#define AQ_CFG_SPEED_MSK  0xFFFFU	 

#define AQ_CFG_IS_AUTONEG_DEF       1U
#define AQ_CFG_MTU_DEF              1514U

#define AQ_CFG_LOCK_TRYS   100U

#define AQ_CFG_DRV_AUTHOR      "Marvell"
#define AQ_CFG_DRV_DESC        "Marvell (Aquantia) Corporation(R) Network Driver"
#define AQ_CFG_DRV_NAME        "atlantic"

#endif  
