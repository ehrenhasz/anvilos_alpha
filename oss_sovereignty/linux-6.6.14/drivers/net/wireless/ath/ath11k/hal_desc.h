 
 
#include "core.h"

#ifndef ATH11K_HAL_DESC_H
#define ATH11K_HAL_DESC_H

#define BUFFER_ADDR_INFO0_ADDR         GENMASK(31, 0)

#define BUFFER_ADDR_INFO1_ADDR         GENMASK(7, 0)
#define BUFFER_ADDR_INFO1_RET_BUF_MGR  GENMASK(10, 8)
#define BUFFER_ADDR_INFO1_SW_COOKIE    GENMASK(31, 11)

struct ath11k_buffer_addr {
	u32 info0;
	u32 info1;
} __packed;

 

enum hal_tlv_tag {
	HAL_MACTX_CBF_START                    =   0  ,
	HAL_PHYRX_DATA                         =   1  ,
	HAL_PHYRX_CBF_DATA_RESP                =   2  ,
	HAL_PHYRX_ABORT_REQUEST                =   3  ,
	HAL_PHYRX_USER_ABORT_NOTIFICATION      =   4  ,
	HAL_MACTX_DATA_RESP                    =   5  ,
	HAL_MACTX_CBF_DATA                     =   6  ,
	HAL_MACTX_CBF_DONE                     =   7  ,
	HAL_MACRX_CBF_READ_REQUEST             =   8  ,
	HAL_MACRX_CBF_DATA_REQUEST             =   9  ,
	HAL_MACRX_EXPECT_NDP_RECEPTION         =  10  ,
	HAL_MACRX_FREEZE_CAPTURE_CHANNEL       =  11  ,
	HAL_MACRX_NDP_TIMEOUT                  =  12  ,
	HAL_MACRX_ABORT_ACK                    =  13  ,
	HAL_MACRX_REQ_IMPLICIT_FB              =  14  ,
	HAL_MACRX_CHAIN_MASK                   =  15  ,
	HAL_MACRX_NAP_USER                     =  16  ,
	HAL_MACRX_ABORT_REQUEST                =  17  ,
	HAL_PHYTX_OTHER_TRANSMIT_INFO16        =  18  ,
	HAL_PHYTX_ABORT_ACK                    =  19  ,
	HAL_PHYTX_ABORT_REQUEST                =  20  ,
	HAL_PHYTX_PKT_END                      =  21  ,
	HAL_PHYTX_PPDU_HEADER_INFO_REQUEST     =  22  ,
	HAL_PHYTX_REQUEST_CTRL_INFO            =  23  ,
	HAL_PHYTX_DATA_REQUEST                 =  24  ,
	HAL_PHYTX_BF_CV_LOADING_DONE           =  25  ,
	HAL_PHYTX_NAP_ACK                      =  26  ,
	HAL_PHYTX_NAP_DONE                     =  27  ,
	HAL_PHYTX_OFF_ACK                      =  28  ,
	HAL_PHYTX_ON_ACK                       =  29  ,
	HAL_PHYTX_SYNTH_OFF_ACK                =  30  ,
	HAL_PHYTX_DEBUG16                      =  31  ,
	HAL_MACTX_ABORT_REQUEST                =  32  ,
	HAL_MACTX_ABORT_ACK                    =  33  ,
	HAL_MACTX_PKT_END                      =  34  ,
	HAL_MACTX_PRE_PHY_DESC                 =  35  ,
	HAL_MACTX_BF_PARAMS_COMMON             =  36  ,
	HAL_MACTX_BF_PARAMS_PER_USER           =  37  ,
	HAL_MACTX_PREFETCH_CV                  =  38  ,
	HAL_MACTX_USER_DESC_COMMON             =  39  ,
	HAL_MACTX_USER_DESC_PER_USER           =  40  ,
	HAL_EXAMPLE_USER_TLV_16                =  41  ,
	HAL_EXAMPLE_TLV_16                     =  42  ,
	HAL_MACTX_PHY_OFF                      =  43  ,
	HAL_MACTX_PHY_ON                       =  44  ,
	HAL_MACTX_SYNTH_OFF                    =  45  ,
	HAL_MACTX_EXPECT_CBF_COMMON            =  46  ,
	HAL_MACTX_EXPECT_CBF_PER_USER          =  47  ,
	HAL_MACTX_PHY_DESC                     =  48  ,
	HAL_MACTX_L_SIG_A                      =  49  ,
	HAL_MACTX_L_SIG_B                      =  50  ,
	HAL_MACTX_HT_SIG                       =  51  ,
	HAL_MACTX_VHT_SIG_A                    =  52  ,
	HAL_MACTX_VHT_SIG_B_SU20               =  53  ,
	HAL_MACTX_VHT_SIG_B_SU40               =  54  ,
	HAL_MACTX_VHT_SIG_B_SU80               =  55  ,
	HAL_MACTX_VHT_SIG_B_SU160              =  56  ,
	HAL_MACTX_VHT_SIG_B_MU20               =  57  ,
	HAL_MACTX_VHT_SIG_B_MU40               =  58  ,
	HAL_MACTX_VHT_SIG_B_MU80               =  59  ,
	HAL_MACTX_VHT_SIG_B_MU160              =  60  ,
	HAL_MACTX_SERVICE                      =  61  ,
	HAL_MACTX_HE_SIG_A_SU                  =  62  ,
	HAL_MACTX_HE_SIG_A_MU_DL               =  63  ,
	HAL_MACTX_HE_SIG_A_MU_UL               =  64  ,
	HAL_MACTX_HE_SIG_B1_MU                 =  65  ,
	HAL_MACTX_HE_SIG_B2_MU                 =  66  ,
	HAL_MACTX_HE_SIG_B2_OFDMA              =  67  ,
	HAL_MACTX_DELETE_CV                    =  68  ,
	HAL_MACTX_MU_UPLINK_COMMON             =  69  ,
	HAL_MACTX_MU_UPLINK_USER_SETUP         =  70  ,
	HAL_MACTX_OTHER_TRANSMIT_INFO          =  71  ,
	HAL_MACTX_PHY_NAP                      =  72  ,
	HAL_MACTX_DEBUG                        =  73  ,
	HAL_PHYRX_ABORT_ACK                    =  74  ,
	HAL_PHYRX_GENERATED_CBF_DETAILS        =  75  ,
	HAL_PHYRX_RSSI_LEGACY                  =  76  ,
	HAL_PHYRX_RSSI_HT                      =  77  ,
	HAL_PHYRX_USER_INFO                    =  78  ,
	HAL_PHYRX_PKT_END                      =  79  ,
	HAL_PHYRX_DEBUG                        =  80  ,
	HAL_PHYRX_CBF_TRANSFER_DONE            =  81  ,
	HAL_PHYRX_CBF_TRANSFER_ABORT           =  82  ,
	HAL_PHYRX_L_SIG_A                      =  83  ,
	HAL_PHYRX_L_SIG_B                      =  84  ,
	HAL_PHYRX_HT_SIG                       =  85  ,
	HAL_PHYRX_VHT_SIG_A                    =  86  ,
	HAL_PHYRX_VHT_SIG_B_SU20               =  87  ,
	HAL_PHYRX_VHT_SIG_B_SU40               =  88  ,
	HAL_PHYRX_VHT_SIG_B_SU80               =  89  ,
	HAL_PHYRX_VHT_SIG_B_SU160              =  90  ,
	HAL_PHYRX_VHT_SIG_B_MU20               =  91  ,
	HAL_PHYRX_VHT_SIG_B_MU40               =  92  ,
	HAL_PHYRX_VHT_SIG_B_MU80               =  93  ,
	HAL_PHYRX_VHT_SIG_B_MU160              =  94  ,
	HAL_PHYRX_HE_SIG_A_SU                  =  95  ,
	HAL_PHYRX_HE_SIG_A_MU_DL               =  96  ,
	HAL_PHYRX_HE_SIG_A_MU_UL               =  97  ,
	HAL_PHYRX_HE_SIG_B1_MU                 =  98  ,
	HAL_PHYRX_HE_SIG_B2_MU                 =  99  ,
	HAL_PHYRX_HE_SIG_B2_OFDMA              = 100  ,
	HAL_PHYRX_OTHER_RECEIVE_INFO           = 101  ,
	HAL_PHYRX_COMMON_USER_INFO             = 102  ,
	HAL_PHYRX_DATA_DONE                    = 103  ,
	HAL_RECEIVE_RSSI_INFO                  = 104  ,
	HAL_RECEIVE_USER_INFO                  = 105  ,
	HAL_MIMO_CONTROL_INFO                  = 106  ,
	HAL_RX_LOCATION_INFO                   = 107  ,
	HAL_COEX_TX_REQ                        = 108  ,
	HAL_DUMMY                              = 109  ,
	HAL_RX_TIMING_OFFSET_INFO              = 110  ,
	HAL_EXAMPLE_TLV_32_NAME                = 111  ,
	HAL_MPDU_LIMIT                         = 112  ,
	HAL_NA_LENGTH_END                      = 113  ,
	HAL_OLE_BUF_STATUS                     = 114  ,
	HAL_PCU_PPDU_SETUP_DONE                = 115  ,
	HAL_PCU_PPDU_SETUP_END                 = 116  ,
	HAL_PCU_PPDU_SETUP_INIT                = 117  ,
	HAL_PCU_PPDU_SETUP_START               = 118  ,
	HAL_PDG_FES_SETUP                      = 119  ,
	HAL_PDG_RESPONSE                       = 120  ,
	HAL_PDG_TX_REQ                         = 121  ,
	HAL_SCH_WAIT_INSTR                     = 122  ,
	HAL_SCHEDULER_TLV                      = 123  ,
	HAL_TQM_FLOW_EMPTY_STATUS              = 124  ,
	HAL_TQM_FLOW_NOT_EMPTY_STATUS          = 125  ,
	HAL_TQM_GEN_MPDU_LENGTH_LIST           = 126  ,
	HAL_TQM_GEN_MPDU_LENGTH_LIST_STATUS    = 127  ,
	HAL_TQM_GEN_MPDUS                      = 128  ,
	HAL_TQM_GEN_MPDUS_STATUS               = 129  ,
	HAL_TQM_REMOVE_MPDU                    = 130  ,
	HAL_TQM_REMOVE_MPDU_STATUS             = 131  ,
	HAL_TQM_REMOVE_MSDU                    = 132  ,
	HAL_TQM_REMOVE_MSDU_STATUS             = 133  ,
	HAL_TQM_UPDATE_TX_MPDU_COUNT           = 134  ,
	HAL_TQM_WRITE_CMD                      = 135  ,
	HAL_OFDMA_TRIGGER_DETAILS              = 136  ,
	HAL_TX_DATA                            = 137  ,
	HAL_TX_FES_SETUP                       = 138  ,
	HAL_RX_PACKET                          = 139  ,
	HAL_EXPECTED_RESPONSE                  = 140  ,
	HAL_TX_MPDU_END                        = 141  ,
	HAL_TX_MPDU_START                      = 142  ,
	HAL_TX_MSDU_END                        = 143  ,
	HAL_TX_MSDU_START                      = 144  ,
	HAL_TX_SW_MODE_SETUP                   = 145  ,
	HAL_TXPCU_BUFFER_STATUS                = 146  ,
	HAL_TXPCU_USER_BUFFER_STATUS           = 147  ,
	HAL_DATA_TO_TIME_CONFIG                = 148  ,
	HAL_EXAMPLE_USER_TLV_32                = 149  ,
	HAL_MPDU_INFO                          = 150  ,
	HAL_PDG_USER_SETUP                     = 151  ,
	HAL_TX_11AH_SETUP                      = 152  ,
	HAL_REO_UPDATE_RX_REO_QUEUE_STATUS     = 153  ,
	HAL_TX_PEER_ENTRY                      = 154  ,
	HAL_TX_RAW_OR_NATIVE_FRAME_SETUP       = 155  ,
	HAL_EXAMPLE_STRUCT_NAME                = 156  ,
	HAL_PCU_PPDU_SETUP_END_INFO            = 157  ,
	HAL_PPDU_RATE_SETTING                  = 158  ,
	HAL_PROT_RATE_SETTING                  = 159  ,
	HAL_RX_MPDU_DETAILS                    = 160  ,
	HAL_EXAMPLE_USER_TLV_42                = 161  ,
	HAL_RX_MSDU_LINK                       = 162  ,
	HAL_RX_REO_QUEUE                       = 163  ,
	HAL_ADDR_SEARCH_ENTRY                  = 164  ,
	HAL_SCHEDULER_CMD                      = 165  ,
	HAL_TX_FLUSH                           = 166  ,
	HAL_TQM_ENTRANCE_RING                  = 167  ,
	HAL_TX_DATA_WORD                       = 168  ,
	HAL_TX_MPDU_DETAILS                    = 169  ,
	HAL_TX_MPDU_LINK                       = 170  ,
	HAL_TX_MPDU_LINK_PTR                   = 171  ,
	HAL_TX_MPDU_QUEUE_HEAD                 = 172  ,
	HAL_TX_MPDU_QUEUE_EXT                  = 173  ,
	HAL_TX_MPDU_QUEUE_EXT_PTR              = 174  ,
	HAL_TX_MSDU_DETAILS                    = 175  ,
	HAL_TX_MSDU_EXTENSION                  = 176  ,
	HAL_TX_MSDU_FLOW                       = 177  ,
	HAL_TX_MSDU_LINK                       = 178  ,
	HAL_TX_MSDU_LINK_ENTRY_PTR             = 179  ,
	HAL_RESPONSE_RATE_SETTING              = 180  ,
	HAL_TXPCU_BUFFER_BASICS                = 181  ,
	HAL_UNIFORM_DESCRIPTOR_HEADER          = 182  ,
	HAL_UNIFORM_TQM_CMD_HEADER             = 183  ,
	HAL_UNIFORM_TQM_STATUS_HEADER          = 184  ,
	HAL_USER_RATE_SETTING                  = 185  ,
	HAL_WBM_BUFFER_RING                    = 186  ,
	HAL_WBM_LINK_DESCRIPTOR_RING           = 187  ,
	HAL_WBM_RELEASE_RING                   = 188  ,
	HAL_TX_FLUSH_REQ                       = 189  ,
	HAL_RX_MSDU_DETAILS                    = 190  ,
	HAL_TQM_WRITE_CMD_STATUS               = 191  ,
	HAL_TQM_GET_MPDU_QUEUE_STATS           = 192  ,
	HAL_TQM_GET_MSDU_FLOW_STATS            = 193  ,
	HAL_EXAMPLE_USER_CTLV_32               = 194  ,
	HAL_TX_FES_STATUS_START                = 195  ,
	HAL_TX_FES_STATUS_USER_PPDU            = 196  ,
	HAL_TX_FES_STATUS_USER_RESPONSE        = 197  ,
	HAL_TX_FES_STATUS_END                  = 198  ,
	HAL_RX_TRIG_INFO                       = 199  ,
	HAL_RXPCU_TX_SETUP_CLEAR               = 200  ,
	HAL_RX_FRAME_BITMAP_REQ                = 201  ,
	HAL_RX_FRAME_BITMAP_ACK                = 202  ,
	HAL_COEX_RX_STATUS                     = 203  ,
	HAL_RX_START_PARAM                     = 204  ,
	HAL_RX_PPDU_START                      = 205  ,
	HAL_RX_PPDU_END                        = 206  ,
	HAL_RX_MPDU_START                      = 207  ,
	HAL_RX_MPDU_END                        = 208  ,
	HAL_RX_MSDU_START                      = 209  ,
	HAL_RX_MSDU_END                        = 210  ,
	HAL_RX_ATTENTION                       = 211  ,
	HAL_RECEIVED_RESPONSE_INFO             = 212  ,
	HAL_RX_PHY_SLEEP                       = 213  ,
	HAL_RX_HEADER                          = 214  ,
	HAL_RX_PEER_ENTRY                      = 215  ,
	HAL_RX_FLUSH                           = 216  ,
	HAL_RX_RESPONSE_REQUIRED_INFO          = 217  ,
	HAL_RX_FRAMELESS_BAR_DETAILS           = 218  ,
	HAL_TQM_GET_MPDU_QUEUE_STATS_STATUS    = 219  ,
	HAL_TQM_GET_MSDU_FLOW_STATS_STATUS     = 220  ,
	HAL_TX_CBF_INFO                        = 221  ,
	HAL_PCU_PPDU_SETUP_USER                = 222  ,
	HAL_RX_MPDU_PCU_START                  = 223  ,
	HAL_RX_PM_INFO                         = 224  ,
	HAL_RX_USER_PPDU_END                   = 225  ,
	HAL_RX_PRE_PPDU_START                  = 226  ,
	HAL_RX_PREAMBLE                        = 227  ,
	HAL_TX_FES_SETUP_COMPLETE              = 228  ,
	HAL_TX_LAST_MPDU_FETCHED               = 229  ,
	HAL_TXDMA_STOP_REQUEST                 = 230  ,
	HAL_RXPCU_SETUP                        = 231  ,
	HAL_RXPCU_USER_SETUP                   = 232  ,
	HAL_TX_FES_STATUS_ACK_OR_BA            = 233  ,
	HAL_TQM_ACKED_MPDU                     = 234  ,
	HAL_COEX_TX_RESP                       = 235  ,
	HAL_COEX_TX_STATUS                     = 236  ,
	HAL_MACTX_COEX_PHY_CTRL                = 237  ,
	HAL_COEX_STATUS_BROADCAST              = 238  ,
	HAL_RESPONSE_START_STATUS              = 239  ,
	HAL_RESPONSE_END_STATUS                = 240  ,
	HAL_CRYPTO_STATUS                      = 241  ,
	HAL_RECEIVED_TRIGGER_INFO              = 242  ,
	HAL_REO_ENTRANCE_RING                  = 243  ,
	HAL_RX_MPDU_LINK                       = 244  ,
	HAL_COEX_TX_STOP_CTRL                  = 245  ,
	HAL_RX_PPDU_ACK_REPORT                 = 246  ,
	HAL_RX_PPDU_NO_ACK_REPORT              = 247  ,
	HAL_SCH_COEX_STATUS                    = 248  ,
	HAL_SCHEDULER_COMMAND_STATUS           = 249  ,
	HAL_SCHEDULER_RX_PPDU_NO_RESPONSE_STATUS = 250  ,
	HAL_TX_FES_STATUS_PROT                 = 251  ,
	HAL_TX_FES_STATUS_START_PPDU           = 252  ,
	HAL_TX_FES_STATUS_START_PROT           = 253  ,
	HAL_TXPCU_PHYTX_DEBUG32                = 254  ,
	HAL_TXPCU_PHYTX_OTHER_TRANSMIT_INFO32  = 255  ,
	HAL_TX_MPDU_COUNT_TRANSFER_END         = 256  ,
	HAL_WHO_ANCHOR_OFFSET                  = 257  ,
	HAL_WHO_ANCHOR_VALUE                   = 258  ,
	HAL_WHO_CCE_INFO                       = 259  ,
	HAL_WHO_COMMIT                         = 260  ,
	HAL_WHO_COMMIT_DONE                    = 261  ,
	HAL_WHO_FLUSH                          = 262  ,
	HAL_WHO_L2_LLC                         = 263  ,
	HAL_WHO_L2_PAYLOAD                     = 264  ,
	HAL_WHO_L3_CHECKSUM                    = 265  ,
	HAL_WHO_L3_INFO                        = 266  ,
	HAL_WHO_L4_CHECKSUM                    = 267  ,
	HAL_WHO_L4_INFO                        = 268  ,
	HAL_WHO_MSDU                           = 269  ,
	HAL_WHO_MSDU_MISC                      = 270  ,
	HAL_WHO_PACKET_DATA                    = 271  ,
	HAL_WHO_PACKET_HDR                     = 272  ,
	HAL_WHO_PPDU_END                       = 273  ,
	HAL_WHO_PPDU_START                     = 274  ,
	HAL_WHO_TSO                            = 275  ,
	HAL_WHO_WMAC_HEADER_PV0                = 276  ,
	HAL_WHO_WMAC_HEADER_PV1                = 277  ,
	HAL_WHO_WMAC_IV                        = 278  ,
	HAL_MPDU_INFO_END                      = 279  ,
	HAL_MPDU_INFO_BITMAP                   = 280  ,
	HAL_TX_QUEUE_EXTENSION                 = 281  ,
	HAL_RX_PEER_ENTRY_DETAILS              = 282  ,
	HAL_RX_REO_QUEUE_REFERENCE             = 283  ,
	HAL_RX_REO_QUEUE_EXT                   = 284  ,
	HAL_SCHEDULER_SELFGEN_RESPONSE_STATUS  = 285  ,
	HAL_TQM_UPDATE_TX_MPDU_COUNT_STATUS    = 286  ,
	HAL_TQM_ACKED_MPDU_STATUS              = 287  ,
	HAL_TQM_ADD_MSDU_STATUS                = 288  ,
	HAL_RX_MPDU_LINK_PTR                   = 289  ,
	HAL_REO_DESTINATION_RING               = 290  ,
	HAL_TQM_LIST_GEN_DONE                  = 291  ,
	HAL_WHO_TERMINATE                      = 292  ,
	HAL_TX_LAST_MPDU_END                   = 293  ,
	HAL_TX_CV_DATA                         = 294  ,
	HAL_TCL_ENTRANCE_FROM_PPE_RING         = 295  ,
	HAL_PPDU_TX_END                        = 296  ,
	HAL_PROT_TX_END                        = 297  ,
	HAL_PDG_RESPONSE_RATE_SETTING          = 298  ,
	HAL_MPDU_INFO_GLOBAL_END               = 299  ,
	HAL_TQM_SCH_INSTR_GLOBAL_END           = 300  ,
	HAL_RX_PPDU_END_USER_STATS             = 301  ,
	HAL_RX_PPDU_END_USER_STATS_EXT         = 302  ,
	HAL_NO_ACK_REPORT                      = 303  ,
	HAL_ACK_REPORT                         = 304  ,
	HAL_UNIFORM_REO_CMD_HEADER             = 305  ,
	HAL_REO_GET_QUEUE_STATS                = 306  ,
	HAL_REO_FLUSH_QUEUE                    = 307  ,
	HAL_REO_FLUSH_CACHE                    = 308  ,
	HAL_REO_UNBLOCK_CACHE                  = 309  ,
	HAL_UNIFORM_REO_STATUS_HEADER          = 310  ,
	HAL_REO_GET_QUEUE_STATS_STATUS         = 311  ,
	HAL_REO_FLUSH_QUEUE_STATUS             = 312  ,
	HAL_REO_FLUSH_CACHE_STATUS             = 313  ,
	HAL_REO_UNBLOCK_CACHE_STATUS           = 314  ,
	HAL_TQM_FLUSH_CACHE                    = 315  ,
	HAL_TQM_UNBLOCK_CACHE                  = 316  ,
	HAL_TQM_FLUSH_CACHE_STATUS             = 317  ,
	HAL_TQM_UNBLOCK_CACHE_STATUS           = 318  ,
	HAL_RX_PPDU_END_STATUS_DONE            = 319  ,
	HAL_RX_STATUS_BUFFER_DONE              = 320  ,
	HAL_BUFFER_ADDR_INFO                   = 321  ,
	HAL_RX_MSDU_DESC_INFO                  = 322  ,
	HAL_RX_MPDU_DESC_INFO                  = 323  ,
	HAL_TCL_DATA_CMD                       = 324  ,
	HAL_TCL_GSE_CMD                        = 325  ,
	HAL_TCL_EXIT_BASE                      = 326  ,
	HAL_TCL_COMPACT_EXIT_RING              = 327  ,
	HAL_TCL_REGULAR_EXIT_RING              = 328  ,
	HAL_TCL_EXTENDED_EXIT_RING             = 329  ,
	HAL_UPLINK_COMMON_INFO                 = 330  ,
	HAL_UPLINK_USER_SETUP_INFO             = 331  ,
	HAL_TX_DATA_SYNC                       = 332  ,
	HAL_PHYRX_CBF_READ_REQUEST_ACK         = 333  ,
	HAL_TCL_STATUS_RING                    = 334  ,
	HAL_TQM_GET_MPDU_HEAD_INFO             = 335  ,
	HAL_TQM_SYNC_CMD                       = 336  ,
	HAL_TQM_GET_MPDU_HEAD_INFO_STATUS      = 337  ,
	HAL_TQM_SYNC_CMD_STATUS                = 338  ,
	HAL_TQM_THRESHOLD_DROP_NOTIFICATION_STATUS = 339  ,
	HAL_TQM_DESCRIPTOR_THRESHOLD_REACHED_STATUS = 340  ,
	HAL_REO_FLUSH_TIMEOUT_LIST             = 341  ,
	HAL_REO_FLUSH_TIMEOUT_LIST_STATUS      = 342  ,
	HAL_REO_TO_PPE_RING                    = 343  ,
	HAL_RX_MPDU_INFO                       = 344  ,
	HAL_REO_DESCRIPTOR_THRESHOLD_REACHED_STATUS = 345  ,
	HAL_SCHEDULER_RX_SIFS_RESPONSE_TRIGGER_STATUS = 346  ,
	HAL_EXAMPLE_USER_TLV_32_NAME           = 347  ,
	HAL_RX_PPDU_START_USER_INFO            = 348  ,
	HAL_RX_RXPCU_CLASSIFICATION_OVERVIEW   = 349  ,
	HAL_RX_RING_MASK                       = 350  ,
	HAL_WHO_CLASSIFY_INFO                  = 351  ,
	HAL_TXPT_CLASSIFY_INFO                 = 352  ,
	HAL_RXPT_CLASSIFY_INFO                 = 353  ,
	HAL_TX_FLOW_SEARCH_ENTRY               = 354  ,
	HAL_RX_FLOW_SEARCH_ENTRY               = 355  ,
	HAL_RECEIVED_TRIGGER_INFO_DETAILS      = 356  ,
	HAL_COEX_MAC_NAP                       = 357  ,
	HAL_MACRX_ABORT_REQUEST_INFO           = 358  ,
	HAL_MACTX_ABORT_REQUEST_INFO           = 359  ,
	HAL_PHYRX_ABORT_REQUEST_INFO           = 360  ,
	HAL_PHYTX_ABORT_REQUEST_INFO           = 361  ,
	HAL_RXPCU_PPDU_END_INFO                = 362  ,
	HAL_WHO_MESH_CONTROL                   = 363  ,
	HAL_L_SIG_A_INFO                       = 364  ,
	HAL_L_SIG_B_INFO                       = 365  ,
	HAL_HT_SIG_INFO                        = 366  ,
	HAL_VHT_SIG_A_INFO                     = 367  ,
	HAL_VHT_SIG_B_SU20_INFO                = 368  ,
	HAL_VHT_SIG_B_SU40_INFO                = 369  ,
	HAL_VHT_SIG_B_SU80_INFO                = 370  ,
	HAL_VHT_SIG_B_SU160_INFO               = 371  ,
	HAL_VHT_SIG_B_MU20_INFO                = 372  ,
	HAL_VHT_SIG_B_MU40_INFO                = 373  ,
	HAL_VHT_SIG_B_MU80_INFO                = 374  ,
	HAL_VHT_SIG_B_MU160_INFO               = 375  ,
	HAL_SERVICE_INFO                       = 376  ,
	HAL_HE_SIG_A_SU_INFO                   = 377  ,
	HAL_HE_SIG_A_MU_DL_INFO                = 378  ,
	HAL_HE_SIG_A_MU_UL_INFO                = 379  ,
	HAL_HE_SIG_B1_MU_INFO                  = 380  ,
	HAL_HE_SIG_B2_MU_INFO                  = 381  ,
	HAL_HE_SIG_B2_OFDMA_INFO               = 382  ,
	HAL_PDG_SW_MODE_BW_START               = 383  ,
	HAL_PDG_SW_MODE_BW_END                 = 384  ,
	HAL_PDG_WAIT_FOR_MAC_REQUEST           = 385  ,
	HAL_PDG_WAIT_FOR_PHY_REQUEST           = 386  ,
	HAL_SCHEDULER_END                      = 387  ,
	HAL_PEER_TABLE_ENTRY                   = 388  ,
	HAL_SW_PEER_INFO                       = 389  ,
	HAL_RXOLE_CCE_CLASSIFY_INFO            = 390  ,
	HAL_TCL_CCE_CLASSIFY_INFO              = 391  ,
	HAL_RXOLE_CCE_INFO                     = 392  ,
	HAL_TCL_CCE_INFO                       = 393  ,
	HAL_TCL_CCE_SUPERRULE                  = 394  ,
	HAL_CCE_RULE                           = 395  ,
	HAL_RX_PPDU_START_DROPPED              = 396  ,
	HAL_RX_PPDU_END_DROPPED                = 397  ,
	HAL_RX_PPDU_END_STATUS_DONE_DROPPED    = 398  ,
	HAL_RX_MPDU_START_DROPPED              = 399  ,
	HAL_RX_MSDU_START_DROPPED              = 400  ,
	HAL_RX_MSDU_END_DROPPED                = 401  ,
	HAL_RX_MPDU_END_DROPPED                = 402  ,
	HAL_RX_ATTENTION_DROPPED               = 403  ,
	HAL_TXPCU_USER_SETUP                   = 404  ,
	HAL_RXPCU_USER_SETUP_EXT               = 405  ,
	HAL_CE_SRC_DESC                        = 406  ,
	HAL_CE_STAT_DESC                       = 407  ,
	HAL_RXOLE_CCE_SUPERRULE                = 408  ,
	HAL_TX_RATE_STATS_INFO                 = 409  ,
	HAL_CMD_PART_0_END                     = 410  ,
	HAL_MACTX_SYNTH_ON                     = 411  ,
	HAL_SCH_CRITICAL_TLV_REFERENCE         = 412  ,
	HAL_TQM_MPDU_GLOBAL_START              = 413  ,
	HAL_EXAMPLE_TLV_32                     = 414  ,
	HAL_TQM_UPDATE_TX_MSDU_FLOW            = 415  ,
	HAL_TQM_UPDATE_TX_MPDU_QUEUE_HEAD      = 416  ,
	HAL_TQM_UPDATE_TX_MSDU_FLOW_STATUS     = 417  ,
	HAL_TQM_UPDATE_TX_MPDU_QUEUE_HEAD_STATUS = 418  ,
	HAL_REO_UPDATE_RX_REO_QUEUE            = 419  ,
	HAL_CE_DST_DESC			       = 420  ,
	HAL_TLV_BASE                           = 511  ,
};

#define HAL_TLV_HDR_TAG		GENMASK(9, 1)
#define HAL_TLV_HDR_LEN		GENMASK(25, 10)
#define HAL_TLV_USR_ID		GENMASK(31, 26)

#define HAL_TLV_ALIGN	4

struct hal_tlv_hdr {
	u32 tl;
	u8 value[];
} __packed;

#define RX_MPDU_DESC_INFO0_MSDU_COUNT		GENMASK(7, 0)
#define RX_MPDU_DESC_INFO0_SEQ_NUM		GENMASK(19, 8)
#define RX_MPDU_DESC_INFO0_FRAG_FLAG		BIT(20)
#define RX_MPDU_DESC_INFO0_MPDU_RETRY		BIT(21)
#define RX_MPDU_DESC_INFO0_AMPDU_FLAG		BIT(22)
#define RX_MPDU_DESC_INFO0_BAR_FRAME		BIT(23)
#define RX_MPDU_DESC_INFO0_VALID_PN		BIT(24)
#define RX_MPDU_DESC_INFO0_VALID_SA		BIT(25)
#define RX_MPDU_DESC_INFO0_SA_IDX_TIMEOUT	BIT(26)
#define RX_MPDU_DESC_INFO0_VALID_DA		BIT(27)
#define RX_MPDU_DESC_INFO0_DA_MCBC		BIT(28)
#define RX_MPDU_DESC_INFO0_DA_IDX_TIMEOUT	BIT(29)
#define RX_MPDU_DESC_INFO0_RAW_MPDU		BIT(30)

#define RX_MPDU_DESC_META_DATA_PEER_ID		GENMASK(15, 0)

struct rx_mpdu_desc {
	u32 info0;  
	u32 meta_data;
} __packed;

 

enum hal_rx_msdu_desc_reo_dest_ind {
	HAL_RX_MSDU_DESC_REO_DEST_IND_TCL,
	HAL_RX_MSDU_DESC_REO_DEST_IND_SW1,
	HAL_RX_MSDU_DESC_REO_DEST_IND_SW2,
	HAL_RX_MSDU_DESC_REO_DEST_IND_SW3,
	HAL_RX_MSDU_DESC_REO_DEST_IND_SW4,
	HAL_RX_MSDU_DESC_REO_DEST_IND_RELEASE,
	HAL_RX_MSDU_DESC_REO_DEST_IND_FW,
};

#define RX_MSDU_DESC_INFO0_FIRST_MSDU_IN_MPDU	BIT(0)
#define RX_MSDU_DESC_INFO0_LAST_MSDU_IN_MPDU	BIT(1)
#define RX_MSDU_DESC_INFO0_MSDU_CONTINUATION	BIT(2)
#define RX_MSDU_DESC_INFO0_MSDU_LENGTH		GENMASK(16, 3)
#define RX_MSDU_DESC_INFO0_REO_DEST_IND		GENMASK(21, 17)
#define RX_MSDU_DESC_INFO0_MSDU_DROP		BIT(22)
#define RX_MSDU_DESC_INFO0_VALID_SA		BIT(23)
#define RX_MSDU_DESC_INFO0_SA_IDX_TIMEOUT	BIT(24)
#define RX_MSDU_DESC_INFO0_VALID_DA		BIT(25)
#define RX_MSDU_DESC_INFO0_DA_MCBC		BIT(26)
#define RX_MSDU_DESC_INFO0_DA_IDX_TIMEOUT	BIT(27)

#define HAL_RX_MSDU_PKT_LENGTH_GET(val)		\
	(FIELD_GET(RX_MSDU_DESC_INFO0_MSDU_LENGTH, (val)))

struct rx_msdu_desc {
	u32 info0;
	u32 rsvd0;
} __packed;

 

enum hal_reo_dest_ring_buffer_type {
	HAL_REO_DEST_RING_BUFFER_TYPE_MSDU,
	HAL_REO_DEST_RING_BUFFER_TYPE_LINK_DESC,
};

enum hal_reo_dest_ring_push_reason {
	HAL_REO_DEST_RING_PUSH_REASON_ERR_DETECTED,
	HAL_REO_DEST_RING_PUSH_REASON_ROUTING_INSTRUCTION,
};

enum hal_reo_dest_ring_error_code {
	HAL_REO_DEST_RING_ERROR_CODE_DESC_ADDR_ZERO,
	HAL_REO_DEST_RING_ERROR_CODE_DESC_INVALID,
	HAL_REO_DEST_RING_ERROR_CODE_AMPDU_IN_NON_BA,
	HAL_REO_DEST_RING_ERROR_CODE_NON_BA_DUPLICATE,
	HAL_REO_DEST_RING_ERROR_CODE_BA_DUPLICATE,
	HAL_REO_DEST_RING_ERROR_CODE_FRAME_2K_JUMP,
	HAL_REO_DEST_RING_ERROR_CODE_BAR_2K_JUMP,
	HAL_REO_DEST_RING_ERROR_CODE_FRAME_OOR,
	HAL_REO_DEST_RING_ERROR_CODE_BAR_OOR,
	HAL_REO_DEST_RING_ERROR_CODE_NO_BA_SESSION,
	HAL_REO_DEST_RING_ERROR_CODE_FRAME_SN_EQUALS_SSN,
	HAL_REO_DEST_RING_ERROR_CODE_PN_CHECK_FAILED,
	HAL_REO_DEST_RING_ERROR_CODE_2K_ERR_FLAG_SET,
	HAL_REO_DEST_RING_ERROR_CODE_PN_ERR_FLAG_SET,
	HAL_REO_DEST_RING_ERROR_CODE_DESC_BLOCKED,
	HAL_REO_DEST_RING_ERROR_CODE_MAX,
};

#define HAL_REO_DEST_RING_INFO0_QUEUE_ADDR_HI		GENMASK(7, 0)
#define HAL_REO_DEST_RING_INFO0_BUFFER_TYPE		BIT(8)
#define HAL_REO_DEST_RING_INFO0_PUSH_REASON		GENMASK(10, 9)
#define HAL_REO_DEST_RING_INFO0_ERROR_CODE		GENMASK(15, 11)
#define HAL_REO_DEST_RING_INFO0_RX_QUEUE_NUM		GENMASK(31, 16)

#define HAL_REO_DEST_RING_INFO1_REORDER_INFO_VALID	BIT(0)
#define HAL_REO_DEST_RING_INFO1_REORDER_OPCODE		GENMASK(4, 1)
#define HAL_REO_DEST_RING_INFO1_REORDER_SLOT_IDX	GENMASK(12, 5)

#define HAL_REO_DEST_RING_INFO2_RING_ID			GENMASK(27, 20)
#define HAL_REO_DEST_RING_INFO2_LOOPING_COUNT		GENMASK(31, 28)

struct hal_reo_dest_ring {
	struct ath11k_buffer_addr buf_addr_info;
	struct rx_mpdu_desc rx_mpdu_info;
	struct rx_msdu_desc rx_msdu_info;
	u32 queue_addr_lo;
	u32 info0;  
	u32 info1;  
	u32 rsvd0;
	u32 rsvd1;
	u32 rsvd2;
	u32 rsvd3;
	u32 rsvd4;
	u32 rsvd5;
	u32 info2;  
} __packed;

 

enum hal_reo_entr_rxdma_ecode {
	HAL_REO_ENTR_RING_RXDMA_ECODE_OVERFLOW_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_MPDU_LEN_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_FCS_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_DECRYPT_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_TKIP_MIC_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_UNECRYPTED_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_MSDU_LEN_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_MSDU_LIMIT_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_WIFI_PARSE_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_AMSDU_PARSE_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_SA_TIMEOUT_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_DA_TIMEOUT_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_FLOW_TIMEOUT_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_FLUSH_REQUEST_ERR,
	HAL_REO_ENTR_RING_RXDMA_ECODE_MAX,
};

#define HAL_REO_ENTR_RING_INFO0_QUEUE_ADDR_HI		GENMASK(7, 0)
#define HAL_REO_ENTR_RING_INFO0_MPDU_BYTE_COUNT		GENMASK(21, 8)
#define HAL_REO_ENTR_RING_INFO0_DEST_IND		GENMASK(26, 22)
#define HAL_REO_ENTR_RING_INFO0_FRAMELESS_BAR		BIT(27)

#define HAL_REO_ENTR_RING_INFO1_RXDMA_PUSH_REASON	GENMASK(1, 0)
#define HAL_REO_ENTR_RING_INFO1_RXDMA_ERROR_CODE	GENMASK(6, 2)

struct hal_reo_entrance_ring {
	struct ath11k_buffer_addr buf_addr_info;
	struct rx_mpdu_desc rx_mpdu_info;
	u32 queue_addr_lo;
	u32 info0;  
	u32 info1;  
	u32 info2;  

} __packed;

 

#define HAL_SW_MON_RING_INFO0_RXDMA_PUSH_REASON	GENMASK(1, 0)
#define HAL_SW_MON_RING_INFO0_RXDMA_ERROR_CODE	GENMASK(6, 2)
#define HAL_SW_MON_RING_INFO0_MPDU_FRAG_NUMBER	GENMASK(10, 7)
#define HAL_SW_MON_RING_INFO0_FRAMELESS_BAR	BIT(11)
#define HAL_SW_MON_RING_INFO0_STATUS_BUF_CNT	GENMASK(15, 12)
#define HAL_SW_MON_RING_INFO0_END_OF_PPDU	BIT(16)

#define HAL_SW_MON_RING_INFO1_PHY_PPDU_ID	GENMASK(15, 0)
#define HAL_SW_MON_RING_INFO1_RING_ID		GENMASK(27, 20)
#define HAL_SW_MON_RING_INFO1_LOOPING_COUNT	GENMASK(31, 28)

struct hal_sw_monitor_ring {
	struct ath11k_buffer_addr buf_addr_info;
	struct rx_mpdu_desc rx_mpdu_info;
	struct ath11k_buffer_addr status_buf_addr_info;
	u32 info0;
	u32 info1;
} __packed;

#define HAL_REO_CMD_HDR_INFO0_CMD_NUMBER	GENMASK(15, 0)
#define HAL_REO_CMD_HDR_INFO0_STATUS_REQUIRED	BIT(16)

struct hal_reo_cmd_hdr {
	u32 info0;
} __packed;

#define HAL_REO_GET_QUEUE_STATS_INFO0_QUEUE_ADDR_HI	GENMASK(7, 0)
#define HAL_REO_GET_QUEUE_STATS_INFO0_CLEAR_STATS	BIT(8)

struct hal_reo_get_queue_stats {
	struct hal_reo_cmd_hdr cmd;
	u32 queue_addr_lo;
	u32 info0;
	u32 rsvd0[6];
} __packed;

 

#define HAL_REO_FLUSH_QUEUE_INFO0_DESC_ADDR_HI		GENMASK(7, 0)
#define HAL_REO_FLUSH_QUEUE_INFO0_BLOCK_DESC_ADDR	BIT(8)
#define HAL_REO_FLUSH_QUEUE_INFO0_BLOCK_RESRC_IDX	GENMASK(10, 9)

struct hal_reo_flush_queue {
	struct hal_reo_cmd_hdr cmd;
	u32 desc_addr_lo;
	u32 info0;
	u32 rsvd0[6];
} __packed;

#define HAL_REO_FLUSH_CACHE_INFO0_CACHE_ADDR_HI		GENMASK(7, 0)
#define HAL_REO_FLUSH_CACHE_INFO0_FWD_ALL_MPDUS		BIT(8)
#define HAL_REO_FLUSH_CACHE_INFO0_RELEASE_BLOCK_IDX	BIT(9)
#define HAL_REO_FLUSH_CACHE_INFO0_BLOCK_RESRC_IDX	GENMASK(11, 10)
#define HAL_REO_FLUSH_CACHE_INFO0_FLUSH_WO_INVALIDATE	BIT(12)
#define HAL_REO_FLUSH_CACHE_INFO0_BLOCK_CACHE_USAGE	BIT(13)
#define HAL_REO_FLUSH_CACHE_INFO0_FLUSH_ALL		BIT(14)

struct hal_reo_flush_cache {
	struct hal_reo_cmd_hdr cmd;
	u32 cache_addr_lo;
	u32 info0;
	u32 rsvd0[6];
} __packed;

#define HAL_TCL_DATA_CMD_INFO0_DESC_TYPE	BIT(0)
#define HAL_TCL_DATA_CMD_INFO0_EPD		BIT(1)
#define HAL_TCL_DATA_CMD_INFO0_ENCAP_TYPE	GENMASK(3, 2)
#define HAL_TCL_DATA_CMD_INFO0_ENCRYPT_TYPE	GENMASK(7, 4)
#define HAL_TCL_DATA_CMD_INFO0_SRC_BUF_SWAP	BIT(8)
#define HAL_TCL_DATA_CMD_INFO0_LNK_META_SWAP	BIT(9)
#define HAL_TCL_DATA_CMD_INFO0_SEARCH_TYPE	GENMASK(13, 12)
#define HAL_TCL_DATA_CMD_INFO0_ADDR_EN		GENMASK(15, 14)
#define HAL_TCL_DATA_CMD_INFO0_CMD_NUM		GENMASK(31, 16)

#define HAL_TCL_DATA_CMD_INFO1_DATA_LEN		GENMASK(15, 0)
#define HAL_TCL_DATA_CMD_INFO1_IP4_CKSUM_EN	BIT(16)
#define HAL_TCL_DATA_CMD_INFO1_UDP4_CKSUM_EN	BIT(17)
#define HAL_TCL_DATA_CMD_INFO1_UDP6_CKSUM_EN	BIT(18)
#define HAL_TCL_DATA_CMD_INFO1_TCP4_CKSUM_EN	BIT(19)
#define HAL_TCL_DATA_CMD_INFO1_TCP6_CKSUM_EN	BIT(20)
#define HAL_TCL_DATA_CMD_INFO1_TO_FW		BIT(21)
#define HAL_TCL_DATA_CMD_INFO1_PKT_OFFSET	GENMASK(31, 23)

#define HAL_TCL_DATA_CMD_INFO2_BUF_TIMESTAMP		GENMASK(18, 0)
#define HAL_TCL_DATA_CMD_INFO2_BUF_T_VALID		BIT(19)
#define HAL_IPQ8074_TCL_DATA_CMD_INFO2_MESH_ENABLE	BIT(20)
#define HAL_TCL_DATA_CMD_INFO2_TID_OVERWRITE		BIT(21)
#define HAL_TCL_DATA_CMD_INFO2_TID			GENMASK(25, 22)
#define HAL_TCL_DATA_CMD_INFO2_LMAC_ID			GENMASK(27, 26)

#define HAL_TCL_DATA_CMD_INFO3_DSCP_TID_TABLE_IDX	GENMASK(5, 0)
#define HAL_TCL_DATA_CMD_INFO3_SEARCH_INDEX		GENMASK(25, 6)
#define HAL_TCL_DATA_CMD_INFO3_CACHE_SET_NUM		GENMASK(29, 26)
#define HAL_QCN9074_TCL_DATA_CMD_INFO3_MESH_ENABLE	GENMASK(31, 30)

#define HAL_TCL_DATA_CMD_INFO4_RING_ID			GENMASK(27, 20)
#define HAL_TCL_DATA_CMD_INFO4_LOOPING_COUNT		GENMASK(31, 28)

enum hal_encrypt_type {
	HAL_ENCRYPT_TYPE_WEP_40,
	HAL_ENCRYPT_TYPE_WEP_104,
	HAL_ENCRYPT_TYPE_TKIP_NO_MIC,
	HAL_ENCRYPT_TYPE_WEP_128,
	HAL_ENCRYPT_TYPE_TKIP_MIC,
	HAL_ENCRYPT_TYPE_WAPI,
	HAL_ENCRYPT_TYPE_CCMP_128,
	HAL_ENCRYPT_TYPE_OPEN,
	HAL_ENCRYPT_TYPE_CCMP_256,
	HAL_ENCRYPT_TYPE_GCMP_128,
	HAL_ENCRYPT_TYPE_AES_GCMP_256,
	HAL_ENCRYPT_TYPE_WAPI_GCM_SM4,
};

enum hal_tcl_encap_type {
	HAL_TCL_ENCAP_TYPE_RAW,
	HAL_TCL_ENCAP_TYPE_NATIVE_WIFI,
	HAL_TCL_ENCAP_TYPE_ETHERNET,
	HAL_TCL_ENCAP_TYPE_802_3 = 3,
};

enum hal_tcl_desc_type {
	HAL_TCL_DESC_TYPE_BUFFER,
	HAL_TCL_DESC_TYPE_EXT_DESC,
};

enum hal_wbm_htt_tx_comp_status {
	HAL_WBM_REL_HTT_TX_COMP_STATUS_OK,
	HAL_WBM_REL_HTT_TX_COMP_STATUS_DROP,
	HAL_WBM_REL_HTT_TX_COMP_STATUS_TTL,
	HAL_WBM_REL_HTT_TX_COMP_STATUS_REINJ,
	HAL_WBM_REL_HTT_TX_COMP_STATUS_INSPECT,
	HAL_WBM_REL_HTT_TX_COMP_STATUS_MEC_NOTIFY,
};

struct hal_tcl_data_cmd {
	struct ath11k_buffer_addr buf_addr_info;
	u32 info0;
	u32 info1;
	u32 info2;
	u32 info3;
	u32 info4;
} __packed;

 

#define HAL_TCL_DESC_LEN sizeof(struct hal_tcl_data_cmd)

enum hal_tcl_gse_ctrl {
	HAL_TCL_GSE_CTRL_RD_STAT,
	HAL_TCL_GSE_CTRL_SRCH_DIS,
	HAL_TCL_GSE_CTRL_WR_BK_SINGLE,
	HAL_TCL_GSE_CTRL_WR_BK_ALL,
	HAL_TCL_GSE_CTRL_INVAL_SINGLE,
	HAL_TCL_GSE_CTRL_INVAL_ALL,
	HAL_TCL_GSE_CTRL_WR_BK_INVAL_SINGLE,
	HAL_TCL_GSE_CTRL_WR_BK_INVAL_ALL,
	HAL_TCL_GSE_CTRL_CLR_STAT_SINGLE,
};

 

#define HAL_TCL_GSE_CMD_INFO0_CTRL_BUF_ADDR_HI		GENMASK(7, 0)
#define HAL_TCL_GSE_CMD_INFO0_GSE_CTRL			GENMASK(11, 8)
#define HAL_TCL_GSE_CMD_INFO0_GSE_SEL			BIT(12)
#define HAL_TCL_GSE_CMD_INFO0_STATUS_DEST_RING_ID	BIT(13)
#define HAL_TCL_GSE_CMD_INFO0_SWAP			BIT(14)

#define HAL_TCL_GSE_CMD_INFO1_RING_ID			GENMASK(27, 20)
#define HAL_TCL_GSE_CMD_INFO1_LOOPING_COUNT		GENMASK(31, 28)

struct hal_tcl_gse_cmd {
	u32 ctrl_buf_addr_lo;
	u32 info0;
	u32 meta_data[2];
	u32 rsvd0[2];
	u32 info1;
} __packed;

 

enum hal_tcl_cache_op_res {
	HAL_TCL_CACHE_OP_RES_DONE,
	HAL_TCL_CACHE_OP_RES_NOT_FOUND,
	HAL_TCL_CACHE_OP_RES_TIMEOUT,
};

#define HAL_TCL_STATUS_RING_INFO0_GSE_CTRL		GENMASK(3, 0)
#define HAL_TCL_STATUS_RING_INFO0_GSE_SEL		BIT(4)
#define HAL_TCL_STATUS_RING_INFO0_CACHE_OP_RES		GENMASK(6, 5)
#define HAL_TCL_STATUS_RING_INFO0_MSDU_CNT		GENMASK(31, 8)

#define HAL_TCL_STATUS_RING_INFO1_HASH_IDX		GENMASK(19, 0)

#define HAL_TCL_STATUS_RING_INFO2_RING_ID		GENMASK(27, 20)
#define HAL_TCL_STATUS_RING_INFO2_LOOPING_COUNT		GENMASK(31, 28)

struct hal_tcl_status_ring {
	u32 info0;
	u32 msdu_byte_count;
	u32 msdu_timestamp;
	u32 meta_data[2];
	u32 info1;
	u32 rsvd0;
	u32 info2;
} __packed;

 

#define HAL_CE_SRC_DESC_ADDR_INFO_ADDR_HI	GENMASK(7, 0)
#define HAL_CE_SRC_DESC_ADDR_INFO_HASH_EN	BIT(8)
#define HAL_CE_SRC_DESC_ADDR_INFO_BYTE_SWAP	BIT(9)
#define HAL_CE_SRC_DESC_ADDR_INFO_DEST_SWAP	BIT(10)
#define HAL_CE_SRC_DESC_ADDR_INFO_GATHER	BIT(11)
#define HAL_CE_SRC_DESC_ADDR_INFO_LEN		GENMASK(31, 16)

#define HAL_CE_SRC_DESC_META_INFO_DATA		GENMASK(15, 0)

#define HAL_CE_SRC_DESC_FLAGS_RING_ID		GENMASK(27, 20)
#define HAL_CE_SRC_DESC_FLAGS_LOOP_CNT		HAL_SRNG_DESC_LOOP_CNT

struct hal_ce_srng_src_desc {
	u32 buffer_addr_low;
	u32 buffer_addr_info;  
	u32 meta_info;  
	u32 flags;  
} __packed;

 

#define HAL_CE_DEST_DESC_ADDR_INFO_ADDR_HI		GENMASK(7, 0)
#define HAL_CE_DEST_DESC_ADDR_INFO_RING_ID		GENMASK(27, 20)
#define HAL_CE_DEST_DESC_ADDR_INFO_LOOP_CNT		HAL_SRNG_DESC_LOOP_CNT

struct hal_ce_srng_dest_desc {
	u32 buffer_addr_low;
	u32 buffer_addr_info;  
} __packed;

 

#define HAL_CE_DST_STATUS_DESC_FLAGS_HASH_EN		BIT(8)
#define HAL_CE_DST_STATUS_DESC_FLAGS_BYTE_SWAP		BIT(9)
#define HAL_CE_DST_STATUS_DESC_FLAGS_DEST_SWAP		BIT(10)
#define HAL_CE_DST_STATUS_DESC_FLAGS_GATHER		BIT(11)
#define HAL_CE_DST_STATUS_DESC_FLAGS_LEN		GENMASK(31, 16)

#define HAL_CE_DST_STATUS_DESC_META_INFO_DATA		GENMASK(15, 0)
#define HAL_CE_DST_STATUS_DESC_META_INFO_RING_ID	GENMASK(27, 20)
#define HAL_CE_DST_STATUS_DESC_META_INFO_LOOP_CNT	HAL_SRNG_DESC_LOOP_CNT

struct hal_ce_srng_dst_status_desc {
	u32 flags;  
	u32 toeplitz_hash0;
	u32 toeplitz_hash1;
	u32 meta_info;  
} __packed;

 

#define HAL_TX_RATE_STATS_INFO0_VALID		BIT(0)
#define HAL_TX_RATE_STATS_INFO0_BW		GENMASK(2, 1)
#define HAL_TX_RATE_STATS_INFO0_PKT_TYPE	GENMASK(6, 3)
#define HAL_TX_RATE_STATS_INFO0_STBC		BIT(7)
#define HAL_TX_RATE_STATS_INFO0_LDPC		BIT(8)
#define HAL_TX_RATE_STATS_INFO0_SGI		GENMASK(10, 9)
#define HAL_TX_RATE_STATS_INFO0_MCS		GENMASK(14, 11)
#define HAL_TX_RATE_STATS_INFO0_OFDMA_TX	BIT(15)
#define HAL_TX_RATE_STATS_INFO0_TONES_IN_RU	GENMASK(27, 16)

enum hal_tx_rate_stats_bw {
	HAL_TX_RATE_STATS_BW_20,
	HAL_TX_RATE_STATS_BW_40,
	HAL_TX_RATE_STATS_BW_80,
	HAL_TX_RATE_STATS_BW_160,
};

enum hal_tx_rate_stats_pkt_type {
	HAL_TX_RATE_STATS_PKT_TYPE_11A,
	HAL_TX_RATE_STATS_PKT_TYPE_11B,
	HAL_TX_RATE_STATS_PKT_TYPE_11N,
	HAL_TX_RATE_STATS_PKT_TYPE_11AC,
	HAL_TX_RATE_STATS_PKT_TYPE_11AX,
};

enum hal_tx_rate_stats_sgi {
	HAL_TX_RATE_STATS_SGI_08US,
	HAL_TX_RATE_STATS_SGI_04US,
	HAL_TX_RATE_STATS_SGI_16US,
	HAL_TX_RATE_STATS_SGI_32US,
};

struct hal_tx_rate_stats {
	u32 info0;
	u32 tsf;
} __packed;

struct hal_wbm_link_desc {
	struct ath11k_buffer_addr buf_addr_info;
} __packed;

 

enum hal_wbm_rel_src_module {
	HAL_WBM_REL_SRC_MODULE_TQM,
	HAL_WBM_REL_SRC_MODULE_RXDMA,
	HAL_WBM_REL_SRC_MODULE_REO,
	HAL_WBM_REL_SRC_MODULE_FW,
	HAL_WBM_REL_SRC_MODULE_SW,
};

enum hal_wbm_rel_desc_type {
	HAL_WBM_REL_DESC_TYPE_REL_MSDU,
	HAL_WBM_REL_DESC_TYPE_MSDU_LINK,
	HAL_WBM_REL_DESC_TYPE_MPDU_LINK,
	HAL_WBM_REL_DESC_TYPE_MSDU_EXT,
	HAL_WBM_REL_DESC_TYPE_QUEUE_EXT,
};

 

enum hal_wbm_rel_bm_act {
	HAL_WBM_REL_BM_ACT_PUT_IN_IDLE,
	HAL_WBM_REL_BM_ACT_REL_MSDU,
};

 

#define HAL_WBM_RELEASE_INFO0_REL_SRC_MODULE		GENMASK(2, 0)
#define HAL_WBM_RELEASE_INFO0_BM_ACTION			GENMASK(5, 3)
#define HAL_WBM_RELEASE_INFO0_DESC_TYPE			GENMASK(8, 6)
#define HAL_WBM_RELEASE_INFO0_FIRST_MSDU_IDX		GENMASK(12, 9)
#define HAL_WBM_RELEASE_INFO0_TQM_RELEASE_REASON	GENMASK(16, 13)
#define HAL_WBM_RELEASE_INFO0_RXDMA_PUSH_REASON		GENMASK(18, 17)
#define HAL_WBM_RELEASE_INFO0_RXDMA_ERROR_CODE		GENMASK(23, 19)
#define HAL_WBM_RELEASE_INFO0_REO_PUSH_REASON		GENMASK(25, 24)
#define HAL_WBM_RELEASE_INFO0_REO_ERROR_CODE		GENMASK(30, 26)
#define HAL_WBM_RELEASE_INFO0_WBM_INTERNAL_ERROR	BIT(31)

#define HAL_WBM_RELEASE_INFO1_TQM_STATUS_NUMBER		GENMASK(23, 0)
#define HAL_WBM_RELEASE_INFO1_TRANSMIT_COUNT		GENMASK(30, 24)

#define HAL_WBM_RELEASE_INFO2_ACK_FRAME_RSSI		GENMASK(7, 0)
#define HAL_WBM_RELEASE_INFO2_SW_REL_DETAILS_VALID	BIT(8)
#define HAL_WBM_RELEASE_INFO2_FIRST_MSDU		BIT(9)
#define HAL_WBM_RELEASE_INFO2_LAST_MSDU			BIT(10)
#define HAL_WBM_RELEASE_INFO2_MSDU_IN_AMSDU		BIT(11)
#define HAL_WBM_RELEASE_INFO2_FW_TX_NOTIF_FRAME		BIT(12)
#define HAL_WBM_RELEASE_INFO2_BUFFER_TIMESTAMP		GENMASK(31, 13)

#define HAL_WBM_RELEASE_INFO3_PEER_ID			GENMASK(15, 0)
#define HAL_WBM_RELEASE_INFO3_TID			GENMASK(19, 16)
#define HAL_WBM_RELEASE_INFO3_RING_ID			GENMASK(27, 20)
#define HAL_WBM_RELEASE_INFO3_LOOPING_COUNT		GENMASK(31, 28)

#define HAL_WBM_REL_HTT_TX_COMP_INFO0_STATUS		GENMASK(12, 9)
#define HAL_WBM_REL_HTT_TX_COMP_INFO0_REINJ_REASON	GENMASK(16, 13)
#define HAL_WBM_REL_HTT_TX_COMP_INFO0_EXP_FRAME		BIT(17)

struct hal_wbm_release_ring {
	struct ath11k_buffer_addr buf_addr_info;
	u32 info0;
	u32 info1;
	u32 info2;
	struct hal_tx_rate_stats rate_stats;
	u32 info3;
} __packed;

 

 
enum hal_wbm_tqm_rel_reason {
	HAL_WBM_TQM_REL_REASON_FRAME_ACKED,
	HAL_WBM_TQM_REL_REASON_CMD_REMOVE_MPDU,
	HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX,
	HAL_WBM_TQM_REL_REASON_CMD_REMOVE_NOTX,
	HAL_WBM_TQM_REL_REASON_CMD_REMOVE_AGED_FRAMES,
	HAL_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON1,
	HAL_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON2,
	HAL_WBM_TQM_REL_REASON_CMD_REMOVE_RESEAON3,
};

struct hal_wbm_buffer_ring {
	struct ath11k_buffer_addr buf_addr_info;
};

enum hal_desc_owner {
	HAL_DESC_OWNER_WBM,
	HAL_DESC_OWNER_SW,
	HAL_DESC_OWNER_TQM,
	HAL_DESC_OWNER_RXDMA,
	HAL_DESC_OWNER_REO,
	HAL_DESC_OWNER_SWITCH,
};

enum hal_desc_buf_type {
	HAL_DESC_BUF_TYPE_TX_MSDU_LINK,
	HAL_DESC_BUF_TYPE_TX_MPDU_LINK,
	HAL_DESC_BUF_TYPE_TX_MPDU_QUEUE_HEAD,
	HAL_DESC_BUF_TYPE_TX_MPDU_QUEUE_EXT,
	HAL_DESC_BUF_TYPE_TX_FLOW,
	HAL_DESC_BUF_TYPE_TX_BUFFER,
	HAL_DESC_BUF_TYPE_RX_MSDU_LINK,
	HAL_DESC_BUF_TYPE_RX_MPDU_LINK,
	HAL_DESC_BUF_TYPE_RX_REO_QUEUE,
	HAL_DESC_BUF_TYPE_RX_REO_QUEUE_EXT,
	HAL_DESC_BUF_TYPE_RX_BUFFER,
	HAL_DESC_BUF_TYPE_IDLE_LINK,
};

#define HAL_DESC_REO_OWNED		4
#define HAL_DESC_REO_QUEUE_DESC		8
#define HAL_DESC_REO_QUEUE_EXT_DESC	9
#define HAL_DESC_REO_NON_QOS_TID	16

#define HAL_DESC_HDR_INFO0_OWNER	GENMASK(3, 0)
#define HAL_DESC_HDR_INFO0_BUF_TYPE	GENMASK(7, 4)
#define HAL_DESC_HDR_INFO0_DBG_RESERVED	GENMASK(31, 8)

struct hal_desc_header {
	u32 info0;
} __packed;

struct hal_rx_mpdu_link_ptr {
	struct ath11k_buffer_addr addr_info;
} __packed;

struct hal_rx_msdu_details {
	struct ath11k_buffer_addr buf_addr_info;
	struct rx_msdu_desc rx_msdu_info;
} __packed;

#define HAL_RX_MSDU_LNK_INFO0_RX_QUEUE_NUMBER		GENMASK(15, 0)
#define HAL_RX_MSDU_LNK_INFO0_FIRST_MSDU_LNK		BIT(16)

struct hal_rx_msdu_link {
	struct hal_desc_header desc_hdr;
	struct ath11k_buffer_addr buf_addr_info;
	u32 info0;
	u32 pn[4];
	struct hal_rx_msdu_details msdu_link[6];
} __packed;

struct hal_rx_reo_queue_ext {
	struct hal_desc_header desc_hdr;
	u32 rsvd;
	struct hal_rx_mpdu_link_ptr mpdu_link[15];
} __packed;

 

enum hal_rx_reo_queue_pn_size {
	HAL_RX_REO_QUEUE_PN_SIZE_24,
	HAL_RX_REO_QUEUE_PN_SIZE_48,
	HAL_RX_REO_QUEUE_PN_SIZE_128,
};

#define HAL_RX_REO_QUEUE_RX_QUEUE_NUMBER		GENMASK(15, 0)

#define HAL_RX_REO_QUEUE_INFO0_VLD			BIT(0)
#define HAL_RX_REO_QUEUE_INFO0_ASSOC_LNK_DESC_COUNTER	GENMASK(2, 1)
#define HAL_RX_REO_QUEUE_INFO0_DIS_DUP_DETECTION	BIT(3)
#define HAL_RX_REO_QUEUE_INFO0_SOFT_REORDER_EN		BIT(4)
#define HAL_RX_REO_QUEUE_INFO0_AC			GENMASK(6, 5)
#define HAL_RX_REO_QUEUE_INFO0_BAR			BIT(7)
#define HAL_RX_REO_QUEUE_INFO0_RETRY			BIT(8)
#define HAL_RX_REO_QUEUE_INFO0_CHECK_2K_MODE		BIT(9)
#define HAL_RX_REO_QUEUE_INFO0_OOR_MODE			BIT(10)
#define HAL_RX_REO_QUEUE_INFO0_BA_WINDOW_SIZE		GENMASK(18, 11)
#define HAL_RX_REO_QUEUE_INFO0_PN_CHECK			BIT(19)
#define HAL_RX_REO_QUEUE_INFO0_EVEN_PN			BIT(20)
#define HAL_RX_REO_QUEUE_INFO0_UNEVEN_PN		BIT(21)
#define HAL_RX_REO_QUEUE_INFO0_PN_HANDLE_ENABLE		BIT(22)
#define HAL_RX_REO_QUEUE_INFO0_PN_SIZE			GENMASK(24, 23)
#define HAL_RX_REO_QUEUE_INFO0_IGNORE_AMPDU_FLG		BIT(25)

#define HAL_RX_REO_QUEUE_INFO1_SVLD			BIT(0)
#define HAL_RX_REO_QUEUE_INFO1_SSN			GENMASK(12, 1)
#define HAL_RX_REO_QUEUE_INFO1_CURRENT_IDX		GENMASK(20, 13)
#define HAL_RX_REO_QUEUE_INFO1_SEQ_2K_ERR		BIT(21)
#define HAL_RX_REO_QUEUE_INFO1_PN_ERR			BIT(22)
#define HAL_RX_REO_QUEUE_INFO1_PN_VALID			BIT(31)

#define HAL_RX_REO_QUEUE_INFO2_MPDU_COUNT		GENMASK(6, 0)
#define HAL_RX_REO_QUEUE_INFO2_MSDU_COUNT		(31, 7)

#define HAL_RX_REO_QUEUE_INFO3_TIMEOUT_COUNT		GENMASK(9, 4)
#define HAL_RX_REO_QUEUE_INFO3_FWD_DUE_TO_BAR_CNT	GENMASK(15, 10)
#define HAL_RX_REO_QUEUE_INFO3_DUPLICATE_COUNT		GENMASK(31, 16)

#define HAL_RX_REO_QUEUE_INFO4_FRAME_IN_ORD_COUNT	GENMASK(23, 0)
#define HAL_RX_REO_QUEUE_INFO4_BAR_RECVD_COUNT		GENMASK(31, 24)

#define HAL_RX_REO_QUEUE_INFO5_LATE_RX_MPDU_COUNT	GENMASK(11, 0)
#define HAL_RX_REO_QUEUE_INFO5_WINDOW_JUMP_2K		GENMASK(15, 12)
#define HAL_RX_REO_QUEUE_INFO5_HOLE_COUNT		GENMASK(31, 16)

struct hal_rx_reo_queue {
	struct hal_desc_header desc_hdr;
	u32 rx_queue_num;
	u32 info0;
	u32 info1;
	u32 pn[4];
	u32 last_rx_enqueue_timestamp;
	u32 last_rx_dequeue_timestamp;
	u32 next_aging_queue[2];
	u32 prev_aging_queue[2];
	u32 rx_bitmap[8];
	u32 info2;
	u32 info3;
	u32 info4;
	u32 processed_mpdus;
	u32 processed_msdus;
	u32 processed_total_bytes;
	u32 info5;
	u32 rsvd[3];
	struct hal_rx_reo_queue_ext ext_desc[];
} __packed;

 

#define HAL_REO_UPD_RX_QUEUE_INFO0_QUEUE_ADDR_HI		GENMASK(7, 0)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_RX_QUEUE_NUM		BIT(8)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_VLD			BIT(9)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_ASSOC_LNK_DESC_CNT	BIT(10)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_DIS_DUP_DETECTION	BIT(11)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_SOFT_REORDER_EN		BIT(12)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_AC			BIT(13)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_BAR			BIT(14)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_RETRY			BIT(15)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_CHECK_2K_MODE		BIT(16)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_OOR_MODE			BIT(17)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_BA_WINDOW_SIZE		BIT(18)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN_CHECK			BIT(19)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_EVEN_PN			BIT(20)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_UNEVEN_PN		BIT(21)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN_HANDLE_ENABLE		BIT(22)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN_SIZE			BIT(23)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_IGNORE_AMPDU_FLG		BIT(24)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_SVLD			BIT(25)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_SSN			BIT(26)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_SEQ_2K_ERR		BIT(27)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN_ERR			BIT(28)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN_VALID			BIT(29)
#define HAL_REO_UPD_RX_QUEUE_INFO0_UPD_PN			BIT(30)

#define HAL_REO_UPD_RX_QUEUE_INFO1_RX_QUEUE_NUMBER		GENMASK(15, 0)
#define HAL_REO_UPD_RX_QUEUE_INFO1_VLD				BIT(16)
#define HAL_REO_UPD_RX_QUEUE_INFO1_ASSOC_LNK_DESC_COUNTER	GENMASK(18, 17)
#define HAL_REO_UPD_RX_QUEUE_INFO1_DIS_DUP_DETECTION		BIT(19)
#define HAL_REO_UPD_RX_QUEUE_INFO1_SOFT_REORDER_EN		BIT(20)
#define HAL_REO_UPD_RX_QUEUE_INFO1_AC				GENMASK(22, 21)
#define HAL_REO_UPD_RX_QUEUE_INFO1_BAR				BIT(23)
#define HAL_REO_UPD_RX_QUEUE_INFO1_RETRY			BIT(24)
#define HAL_REO_UPD_RX_QUEUE_INFO1_CHECK_2K_MODE		BIT(25)
#define HAL_REO_UPD_RX_QUEUE_INFO1_OOR_MODE			BIT(26)
#define HAL_REO_UPD_RX_QUEUE_INFO1_PN_CHECK			BIT(27)
#define HAL_REO_UPD_RX_QUEUE_INFO1_EVEN_PN			BIT(28)
#define HAL_REO_UPD_RX_QUEUE_INFO1_UNEVEN_PN			BIT(29)
#define HAL_REO_UPD_RX_QUEUE_INFO1_PN_HANDLE_ENABLE		BIT(30)
#define HAL_REO_UPD_RX_QUEUE_INFO1_IGNORE_AMPDU_FLG		BIT(31)

#define HAL_REO_UPD_RX_QUEUE_INFO2_BA_WINDOW_SIZE		GENMASK(7, 0)
#define HAL_REO_UPD_RX_QUEUE_INFO2_PN_SIZE			GENMASK(9, 8)
#define HAL_REO_UPD_RX_QUEUE_INFO2_SVLD				BIT(10)
#define HAL_REO_UPD_RX_QUEUE_INFO2_SSN				GENMASK(22, 11)
#define HAL_REO_UPD_RX_QUEUE_INFO2_SEQ_2K_ERR			BIT(23)
#define HAL_REO_UPD_RX_QUEUE_INFO2_PN_ERR			BIT(24)
#define HAL_REO_UPD_RX_QUEUE_INFO2_PN_VALID			BIT(25)

struct hal_reo_update_rx_queue {
	struct hal_reo_cmd_hdr cmd;
	u32 queue_addr_lo;
	u32 info0;
	u32 info1;
	u32 info2;
	u32 pn[4];
} __packed;

#define HAL_REO_UNBLOCK_CACHE_INFO0_UNBLK_CACHE		BIT(0)
#define HAL_REO_UNBLOCK_CACHE_INFO0_RESOURCE_IDX	GENMASK(2, 1)

struct hal_reo_unblock_cache {
	struct hal_reo_cmd_hdr cmd;
	u32 info0;
	u32 rsvd[7];
} __packed;

enum hal_reo_exec_status {
	HAL_REO_EXEC_STATUS_SUCCESS,
	HAL_REO_EXEC_STATUS_BLOCKED,
	HAL_REO_EXEC_STATUS_FAILED,
	HAL_REO_EXEC_STATUS_RESOURCE_BLOCKED,
};

#define HAL_REO_STATUS_HDR_INFO0_STATUS_NUM	GENMASK(15, 0)
#define HAL_REO_STATUS_HDR_INFO0_EXEC_TIME	GENMASK(25, 16)
#define HAL_REO_STATUS_HDR_INFO0_EXEC_STATUS	GENMASK(27, 26)

struct hal_reo_status_hdr {
	u32 info0;
	u32 timestamp;
} __packed;

 
#define HAL_REO_GET_QUEUE_STATS_STATUS_INFO0_SSN		GENMASK(11, 0)
#define HAL_REO_GET_QUEUE_STATS_STATUS_INFO0_CUR_IDX		GENMASK(19, 12)

#define HAL_REO_GET_QUEUE_STATS_STATUS_INFO1_MPDU_COUNT		GENMASK(6, 0)
#define HAL_REO_GET_QUEUE_STATS_STATUS_INFO1_MSDU_COUNT		GENMASK(31, 7)

#define HAL_REO_GET_QUEUE_STATS_STATUS_INFO2_TIMEOUT_COUNT	GENMASK(9, 4)
#define HAL_REO_GET_QUEUE_STATS_STATUS_INFO2_FDTB_COUNT		GENMASK(15, 10)
#define HAL_REO_GET_QUEUE_STATS_STATUS_INFO2_DUPLICATE_COUNT	GENMASK(31, 16)

#define HAL_REO_GET_QUEUE_STATS_STATUS_INFO3_FIO_COUNT		GENMASK(23, 0)
#define HAL_REO_GET_QUEUE_STATS_STATUS_INFO3_BAR_RCVD_CNT	GENMASK(31, 24)

#define HAL_REO_GET_QUEUE_STATS_STATUS_INFO4_LATE_RX_MPDU	GENMASK(11, 0)
#define HAL_REO_GET_QUEUE_STATS_STATUS_INFO4_WINDOW_JMP2K	GENMASK(15, 12)
#define HAL_REO_GET_QUEUE_STATS_STATUS_INFO4_HOLE_COUNT		GENMASK(31, 16)

#define HAL_REO_GET_QUEUE_STATS_STATUS_INFO5_LOOPING_CNT	GENMASK(31, 28)

struct hal_reo_get_queue_stats_status {
	struct hal_reo_status_hdr hdr;
	u32 info0;
	u32 pn[4];
	u32 last_rx_enqueue_timestamp;
	u32 last_rx_dequeue_timestamp;
	u32 rx_bitmap[8];
	u32 info1;
	u32 info2;
	u32 info3;
	u32 num_mpdu_frames;
	u32 num_msdu_frames;
	u32 total_bytes;
	u32 info4;
	u32 info5;
} __packed;

 

#define HAL_REO_STATUS_LOOP_CNT			GENMASK(31, 28)

#define HAL_REO_FLUSH_QUEUE_INFO0_ERR_DETECTED	BIT(0)
#define HAL_REO_FLUSH_QUEUE_INFO0_RSVD		GENMASK(31, 1)
#define HAL_REO_FLUSH_QUEUE_INFO1_RSVD		GENMASK(27, 0)

struct hal_reo_flush_queue_status {
	struct hal_reo_status_hdr hdr;
	u32 info0;
	u32 rsvd0[21];
	u32 info1;
} __packed;

 

#define HAL_REO_FLUSH_CACHE_STATUS_INFO0_IS_ERR			BIT(0)
#define HAL_REO_FLUSH_CACHE_STATUS_INFO0_BLOCK_ERR_CODE		GENMASK(2, 1)
#define HAL_REO_FLUSH_CACHE_STATUS_INFO0_FLUSH_STATUS_HIT	BIT(8)
#define HAL_REO_FLUSH_CACHE_STATUS_INFO0_FLUSH_DESC_TYPE	GENMASK(11, 9)
#define HAL_REO_FLUSH_CACHE_STATUS_INFO0_FLUSH_CLIENT_ID	GENMASK(15, 12)
#define HAL_REO_FLUSH_CACHE_STATUS_INFO0_FLUSH_ERR		GENMASK(17, 16)
#define HAL_REO_FLUSH_CACHE_STATUS_INFO0_FLUSH_COUNT		GENMASK(25, 18)

struct hal_reo_flush_cache_status {
	struct hal_reo_status_hdr hdr;
	u32 info0;
	u32 rsvd0[21];
	u32 info1;
} __packed;

 

#define HAL_REO_UNBLOCK_CACHE_STATUS_INFO0_IS_ERR	BIT(0)
#define HAL_REO_UNBLOCK_CACHE_STATUS_INFO0_TYPE		BIT(1)

struct hal_reo_unblock_cache_status {
	struct hal_reo_status_hdr hdr;
	u32 info0;
	u32 rsvd0[21];
	u32 info1;
} __packed;

 

#define HAL_REO_FLUSH_TIMEOUT_STATUS_INFO0_IS_ERR		BIT(0)
#define HAL_REO_FLUSH_TIMEOUT_STATUS_INFO0_LIST_EMPTY		BIT(1)

#define HAL_REO_FLUSH_TIMEOUT_STATUS_INFO1_REL_DESC_COUNT	GENMASK(15, 0)
#define HAL_REO_FLUSH_TIMEOUT_STATUS_INFO1_FWD_BUF_COUNT	GENMASK(31, 16)

struct hal_reo_flush_timeout_list_status {
	struct hal_reo_status_hdr hdr;
	u32 info0;
	u32 info1;
	u32 rsvd0[20];
	u32 info2;
} __packed;

 

#define HAL_REO_DESC_THRESH_STATUS_INFO0_THRESH_INDEX		GENMASK(1, 0)
#define HAL_REO_DESC_THRESH_STATUS_INFO1_LINK_DESC_COUNTER0	GENMASK(23, 0)
#define HAL_REO_DESC_THRESH_STATUS_INFO2_LINK_DESC_COUNTER1	GENMASK(23, 0)
#define HAL_REO_DESC_THRESH_STATUS_INFO3_LINK_DESC_COUNTER2	GENMASK(23, 0)
#define HAL_REO_DESC_THRESH_STATUS_INFO4_LINK_DESC_COUNTER_SUM	GENMASK(25, 0)

struct hal_reo_desc_thresh_reached_status {
	struct hal_reo_status_hdr hdr;
	u32 info0;
	u32 info1;
	u32 info2;
	u32 info3;
	u32 info4;
	u32 rsvd0[17];
	u32 info5;
} __packed;

 

#endif  
