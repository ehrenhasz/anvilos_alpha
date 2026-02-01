 
 

#ifndef __NET_TI_ICSSG_SWITCH_MAP_H
#define __NET_TI_ICSSG_SWITCH_MAP_H

 

 
#define NUMBER_OF_FDB_BUCKET_ENTRIES            (4)

 
#define SIZE_OF_FDB                             (2048)

#define FW_LINK_SPEED_1G                           (0x00)
#define FW_LINK_SPEED_100M                         (0x01)
#define FW_LINK_SPEED_10M                          (0x02)
#define FW_LINK_SPEED_HD                           (0x80)

 
#define FDB_AGEING_TIMEOUT_OFFSET                          0x0014

 
#define HOST_PORT_DF_VLAN_OFFSET                           0x001C

 
#define EMAC_ICSSG_SWITCH_PORT0_DEFAULT_VLAN_OFFSET        HOST_PORT_DF_VLAN_OFFSET

 
#define P1_PORT_DF_VLAN_OFFSET                             0x0020

 
#define EMAC_ICSSG_SWITCH_PORT1_DEFAULT_VLAN_OFFSET        P1_PORT_DF_VLAN_OFFSET

 
#define P2_PORT_DF_VLAN_OFFSET                             0x0024

 
#define EMAC_ICSSG_SWITCH_PORT2_DEFAULT_VLAN_OFFSET        P2_PORT_DF_VLAN_OFFSET

 
#define VLAN_STATIC_REG_TABLE_OFFSET                       0x0100

 
#define EMAC_ICSSG_SWITCH_DEFAULT_VLAN_TABLE_OFFSET        VLAN_STATIC_REG_TABLE_OFFSET

 
#define PORT_DESC0_HI                                      0x2104

 
#define PORT_DESC0_LO                                      0x2F6C

 
#define PORT_DESC1_HI                                      0x3DD4

 
#define PORT_DESC1_LO                                      0x4C3C

 
#define HOST_DESC0_HI                                      0x5AA4

 
#define HOST_DESC0_LO                                      0x5F0C

 
#define HOST_DESC1_HI                                      0x6374

 
#define HOST_DESC1_LO                                      0x67DC

 
#define HOST_SPPD0                                         0x7AAC

 
#define HOST_SPPD1                                         0x7EAC

 
#define TIMESYNC_FW_WC_CYCLECOUNT_OFFSET                   0x83EC

 
#define TIMESYNC_FW_WC_HI_ROLLOVER_COUNT_OFFSET            0x83F4

 
#define TIMESYNC_FW_WC_COUNT_HI_SW_OFFSET_OFFSET           0x83F8

 
#define TIMESYNC_FW_WC_SETCLOCK_DESC_OFFSET                0x83FC

 
#define TIMESYNC_FW_WC_SYNCOUT_REDUCTION_FACTOR_OFFSET     0x843C

 
#define TIMESYNC_FW_WC_SYNCOUT_REDUCTION_COUNT_OFFSET      0x8440

 
#define TIMESYNC_FW_WC_SYNCOUT_START_TIME_CYCLECOUNT_OFFSET 0x8444

 
#define TIMESYNC_FW_WC_ISOM_PIN_SIGNAL_EN_OFFSET           0x844C

 
#define TIMESYNC_FW_ST_SYNCOUT_PERIOD_OFFSET               0x8450

 
#define TIMESYNC_FW_WC_PKTTXDELAY_P1_OFFSET                0x8454

 
#define TIMESYNC_FW_WC_PKTTXDELAY_P2_OFFSET                0x8458

 
#define TIMESYNC_FW_SIG_PNFW_OFFSET                        0x845C

 
#define TIMESYNC_FW_SIG_TIMESYNCFW_OFFSET                  0x8460

 
#define TAS_CONFIG_CHANGE_TIME                             0x000C

 
#define TAS_CONFIG_CHANGE_ERROR_COUNTER                    0x0014

 
#define TAS_CONFIG_PENDING                                 0x0018

 
#define TAS_CONFIG_CHANGE                                  0x0019

 
#define TAS_ADMIN_LIST_LENGTH                              0x001A

 
#define TAS_ACTIVE_LIST_INDEX                              0x001B

 
#define TAS_ADMIN_CYCLE_TIME                               0x001C

 
#define TAS_CONFIG_CHANGE_CYCLE_COUNT                      0x0020

 
#define PSI_L_REGULAR_FLOW_ID_BASE_OFFSET                  0x0024

 
#define EMAC_ICSSG_SWITCH_PSI_L_REGULAR_FLOW_ID_BASE_OFFSET PSI_L_REGULAR_FLOW_ID_BASE_OFFSET

 
#define PSI_L_MGMT_FLOW_ID_OFFSET                          0x0026

 
#define EMAC_ICSSG_SWITCH_PSI_L_MGMT_FLOW_ID_BASE_OFFSET   PSI_L_MGMT_FLOW_ID_OFFSET

 
#define SPL_PKT_DEFAULT_PRIORITY                           0x0028

 
#define EXPRESS_PRE_EMPTIVE_Q_MASK                         0x0029

 
#define QUEUE_NUM_UNTAGGED                                 0x002A

 
#define PORT_Q_PRIORITY_REGEN_OFFSET                       0x002C

 
#define EXPRESS_PRE_EMPTIVE_Q_MAP                          0x0034

 
#define PORT_Q_PRIORITY_MAPPING_OFFSET                     0x003C

 
#define PORT_LINK_SPEED_OFFSET                             0x00A8

 
#define TAS_GATE_MASK_LIST0                                0x0100

 
#define TAS_GATE_MASK_LIST1                                0x0350

 
#define PRE_EMPTION_ENABLE_TX                              0x05A0

 
#define PRE_EMPTION_ACTIVE_TX                              0x05A1

 
#define PRE_EMPTION_ENABLE_VERIFY                          0x05A2

 
#define PRE_EMPTION_VERIFY_STATUS                          0x05A3

 
#define PRE_EMPTION_ADD_FRAG_SIZE_REMOTE                   0x05A4

 
#define PRE_EMPTION_ADD_FRAG_SIZE_LOCAL                    0x05A6

 
#define PRE_EMPTION_VERIFY_TIME                            0x05A8

 
#define MGR_R30_CMD_OFFSET                                 0x05AC

 
#define BUFFER_POOL_0_ADDR_OFFSET                          0x05BC

 
#define HOST_RX_Q_PRE_CONTEXT_OFFSET                       0x0684

 
#define FDB_CMD_BUFFER                                     0x0894

 
#define TAS_QUEUE_MAX_SDU_LIST                             0x08FA

 
#define HD_RAND_SEED_OFFSET                                0x0934

 
#define HOST_RX_Q_EXP_CONTEXT_OFFSET                       0x0940

 
#define PA_STAT_32b_START_OFFSET                           0x0080

#endif  
