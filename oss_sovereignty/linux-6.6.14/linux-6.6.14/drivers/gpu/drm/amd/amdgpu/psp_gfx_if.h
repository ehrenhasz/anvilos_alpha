#ifndef _PSP_TEE_GFX_IF_H_
#define _PSP_TEE_GFX_IF_H_
#define PSP_GFX_CMD_BUF_VERSION     0x00000001
#define GFX_CMD_STATUS_MASK         0x0000FFFF
#define GFX_CMD_ID_MASK             0x000F0000
#define GFX_CMD_RESERVED_MASK       0x7FF00000
#define GFX_CMD_RESPONSE_MASK       0x80000000
#define C2PMSG_CMD_GFX_USB_PD_FW_VER 0x2000000
enum psp_gfx_crtl_cmd_id
{
    GFX_CTRL_CMD_ID_INIT_RBI_RING   = 0x00010000,    
    GFX_CTRL_CMD_ID_INIT_GPCOM_RING = 0x00020000,    
    GFX_CTRL_CMD_ID_DESTROY_RINGS   = 0x00030000,    
    GFX_CTRL_CMD_ID_CAN_INIT_RINGS  = 0x00040000,    
    GFX_CTRL_CMD_ID_ENABLE_INT      = 0x00050000,    
    GFX_CTRL_CMD_ID_DISABLE_INT     = 0x00060000,    
    GFX_CTRL_CMD_ID_MODE1_RST       = 0x00070000,    
    GFX_CTRL_CMD_ID_GBR_IH_SET      = 0x00080000,    
    GFX_CTRL_CMD_ID_CONSUME_CMD     = 0x00090000,    
    GFX_CTRL_CMD_ID_DESTROY_GPCOM_RING = 0x000C0000,  
    GFX_CTRL_CMD_ID_MAX             = 0x000F0000,    
};
struct psp_gfx_ctrl
{
    volatile uint32_t   cmd_resp;          
    volatile uint32_t   rbi_wptr;          
    volatile uint32_t   rbi_rptr;          
    volatile uint32_t   gpcom_wptr;        
    volatile uint32_t   gpcom_rptr;        
    volatile uint32_t   ring_addr_lo;      
    volatile uint32_t   ring_addr_hi;      
    volatile uint32_t   ring_buf_size;     
};
#define GFX_FLAG_RESPONSE               0x80000000
enum psp_gfx_cmd_id
{
    GFX_CMD_ID_LOAD_TA            = 0x00000001,    
    GFX_CMD_ID_UNLOAD_TA          = 0x00000002,    
    GFX_CMD_ID_INVOKE_CMD         = 0x00000003,    
    GFX_CMD_ID_LOAD_ASD           = 0x00000004,    
    GFX_CMD_ID_SETUP_TMR          = 0x00000005,    
    GFX_CMD_ID_LOAD_IP_FW         = 0x00000006,    
    GFX_CMD_ID_DESTROY_TMR        = 0x00000007,    
    GFX_CMD_ID_SAVE_RESTORE       = 0x00000008,    
    GFX_CMD_ID_SETUP_VMR          = 0x00000009,    
    GFX_CMD_ID_DESTROY_VMR        = 0x0000000A,    
    GFX_CMD_ID_PROG_REG           = 0x0000000B,    
    GFX_CMD_ID_GET_FW_ATTESTATION = 0x0000000F,    
    GFX_CMD_ID_LOAD_TOC           = 0x00000020,    
    GFX_CMD_ID_AUTOLOAD_RLC       = 0x00000021,    
    GFX_CMD_ID_BOOT_CFG           = 0x00000022,    
    GFX_CMD_ID_SRIOV_SPATIAL_PART = 0x00000027,    
};
enum psp_gfx_boot_config_cmd
{
    BOOTCFG_CMD_SET         = 1,  
    BOOTCFG_CMD_GET         = 2,  
    BOOTCFG_CMD_INVALIDATE  = 3   
};
enum psp_gfx_boot_config
{
    BOOT_CONFIG_GECC = 0x1,
};
struct psp_gfx_cmd_load_ta
{
    uint32_t        app_phy_addr_lo;         
    uint32_t        app_phy_addr_hi;         
    uint32_t        app_len;                 
    uint32_t        cmd_buf_phy_addr_lo;     
    uint32_t        cmd_buf_phy_addr_hi;     
    uint32_t        cmd_buf_len;             
};
struct psp_gfx_cmd_unload_ta
{
    uint32_t        session_id;           
};
struct psp_gfx_buf_desc
{
    uint32_t        buf_phy_addr_lo;        
    uint32_t        buf_phy_addr_hi;        
    uint32_t        buf_size;               
};
#define GFX_BUF_MAX_DESC        64
struct psp_gfx_buf_list
{
    uint32_t                num_desc;                     
    uint32_t                total_size;                   
    struct psp_gfx_buf_desc buf_desc[GFX_BUF_MAX_DESC];   
};
struct psp_gfx_cmd_invoke_cmd
{
    uint32_t                session_id;            
    uint32_t                ta_cmd_id;             
    struct psp_gfx_buf_list buf;                   
};
struct psp_gfx_cmd_setup_tmr
{
    uint32_t        buf_phy_addr_lo;        
    uint32_t        buf_phy_addr_hi;        
    uint32_t        buf_size;               
    union {
	struct {
		uint32_t	sriov_enabled:1;  
		uint32_t	virt_phy_addr:1;  
		uint32_t	reserved:30;
	} bitfield;
	uint32_t        tmr_flags;
    };
    uint32_t        system_phy_addr_lo;         
    uint32_t        system_phy_addr_hi;         
};
enum psp_gfx_fw_type {
	GFX_FW_TYPE_NONE        = 0,     
	GFX_FW_TYPE_CP_ME       = 1,     
	GFX_FW_TYPE_CP_PFP      = 2,     
	GFX_FW_TYPE_CP_CE       = 3,     
	GFX_FW_TYPE_CP_MEC      = 4,     
	GFX_FW_TYPE_CP_MEC_ME1  = 5,     
	GFX_FW_TYPE_CP_MEC_ME2  = 6,     
	GFX_FW_TYPE_RLC_V       = 7,     
	GFX_FW_TYPE_RLC_G       = 8,     
	GFX_FW_TYPE_SDMA0       = 9,     
	GFX_FW_TYPE_SDMA1       = 10,    
	GFX_FW_TYPE_DMCU_ERAM   = 11,    
	GFX_FW_TYPE_DMCU_ISR    = 12,    
	GFX_FW_TYPE_VCN         = 13,    
	GFX_FW_TYPE_UVD         = 14,    
	GFX_FW_TYPE_VCE         = 15,    
	GFX_FW_TYPE_ISP         = 16,    
	GFX_FW_TYPE_ACP         = 17,    
	GFX_FW_TYPE_SMU         = 18,    
	GFX_FW_TYPE_MMSCH       = 19,    
	GFX_FW_TYPE_RLC_RESTORE_LIST_GPM_MEM        = 20,    
	GFX_FW_TYPE_RLC_RESTORE_LIST_SRM_MEM        = 21,    
	GFX_FW_TYPE_RLC_RESTORE_LIST_SRM_CNTL       = 22,    
	GFX_FW_TYPE_UVD1        = 23,    
	GFX_FW_TYPE_TOC         = 24,    
	GFX_FW_TYPE_RLC_P                           = 25,    
	GFX_FW_TYPE_RLC_IRAM                        = 26,    
	GFX_FW_TYPE_GLOBAL_TAP_DELAYS               = 27,    
	GFX_FW_TYPE_SE0_TAP_DELAYS                  = 28,    
	GFX_FW_TYPE_SE1_TAP_DELAYS                  = 29,    
	GFX_FW_TYPE_GLOBAL_SE0_SE1_SKEW_DELAYS      = 30,    
	GFX_FW_TYPE_SDMA0_JT                        = 31,    
	GFX_FW_TYPE_SDMA1_JT                        = 32,    
	GFX_FW_TYPE_CP_MES                          = 33,    
	GFX_FW_TYPE_MES_STACK                       = 34,    
	GFX_FW_TYPE_RLC_SRM_DRAM_SR                 = 35,    
	GFX_FW_TYPE_RLCG_SCRATCH_SR                 = 36,    
	GFX_FW_TYPE_RLCP_SCRATCH_SR                 = 37,    
	GFX_FW_TYPE_RLCV_SCRATCH_SR                 = 38,    
	GFX_FW_TYPE_RLX6_DRAM_SR                    = 39,    
	GFX_FW_TYPE_SDMA0_PG_CONTEXT                = 40,    
	GFX_FW_TYPE_SDMA1_PG_CONTEXT                = 41,    
	GFX_FW_TYPE_GLOBAL_MUX_SELECT_RAM           = 42,    
	GFX_FW_TYPE_SE0_MUX_SELECT_RAM              = 43,    
	GFX_FW_TYPE_SE1_MUX_SELECT_RAM              = 44,    
	GFX_FW_TYPE_ACCUM_CTRL_RAM                  = 45,    
	GFX_FW_TYPE_RLCP_CAM                        = 46,    
	GFX_FW_TYPE_RLC_SPP_CAM_EXT                 = 47,    
	GFX_FW_TYPE_RLC_DRAM_BOOT                   = 48,    
	GFX_FW_TYPE_VCN0_RAM                        = 49,    
	GFX_FW_TYPE_VCN1_RAM                        = 50,    
	GFX_FW_TYPE_DMUB                            = 51,    
	GFX_FW_TYPE_SDMA2                           = 52,    
	GFX_FW_TYPE_SDMA3                           = 53,    
	GFX_FW_TYPE_SDMA4                           = 54,    
	GFX_FW_TYPE_SDMA5                           = 55,    
	GFX_FW_TYPE_SDMA6                           = 56,    
	GFX_FW_TYPE_SDMA7                           = 57,    
	GFX_FW_TYPE_VCN1                            = 58,    
	GFX_FW_TYPE_CAP                             = 62,    
	GFX_FW_TYPE_SE2_TAP_DELAYS                  = 65,    
	GFX_FW_TYPE_SE3_TAP_DELAYS                  = 66,    
	GFX_FW_TYPE_REG_LIST                        = 67,    
	GFX_FW_TYPE_IMU_I                           = 68,    
	GFX_FW_TYPE_IMU_D                           = 69,    
	GFX_FW_TYPE_LSDMA                           = 70,    
	GFX_FW_TYPE_SDMA_UCODE_TH0                  = 71,    
	GFX_FW_TYPE_SDMA_UCODE_TH1                  = 72,    
	GFX_FW_TYPE_PPTABLE                         = 73,    
	GFX_FW_TYPE_DISCRETE_USB4                   = 74,    
	GFX_FW_TYPE_TA                              = 75,    
	GFX_FW_TYPE_RS64_MES                        = 76,    
	GFX_FW_TYPE_RS64_MES_STACK                  = 77,    
	GFX_FW_TYPE_RS64_KIQ                        = 78,    
	GFX_FW_TYPE_RS64_KIQ_STACK                  = 79,    
	GFX_FW_TYPE_ISP_DATA                        = 80,    
	GFX_FW_TYPE_CP_MES_KIQ                      = 81,    
	GFX_FW_TYPE_MES_KIQ_STACK                   = 82,    
	GFX_FW_TYPE_UMSCH_DATA                      = 83,    
	GFX_FW_TYPE_UMSCH_UCODE                     = 84,    
	GFX_FW_TYPE_UMSCH_CMD_BUFFER                = 85,    
	GFX_FW_TYPE_USB_DP_COMBO_PHY                = 86,    
	GFX_FW_TYPE_RS64_PFP                        = 87,    
	GFX_FW_TYPE_RS64_ME                         = 88,    
	GFX_FW_TYPE_RS64_MEC                        = 89,    
	GFX_FW_TYPE_RS64_PFP_P0_STACK               = 90,    
	GFX_FW_TYPE_RS64_PFP_P1_STACK               = 91,    
	GFX_FW_TYPE_RS64_ME_P0_STACK                = 92,    
	GFX_FW_TYPE_RS64_ME_P1_STACK                = 93,    
	GFX_FW_TYPE_RS64_MEC_P0_STACK               = 94,    
	GFX_FW_TYPE_RS64_MEC_P1_STACK               = 95,    
	GFX_FW_TYPE_RS64_MEC_P2_STACK               = 96,    
	GFX_FW_TYPE_RS64_MEC_P3_STACK               = 97,    
	GFX_FW_TYPE_MAX
};
struct psp_gfx_cmd_load_ip_fw
{
    uint32_t                fw_phy_addr_lo;     
    uint32_t                fw_phy_addr_hi;     
    uint32_t                fw_size;            
    enum psp_gfx_fw_type    fw_type;            
};
struct psp_gfx_cmd_save_restore_ip_fw
{
    uint32_t                save_fw;               
    uint32_t                save_restore_addr_lo;  
    uint32_t                save_restore_addr_hi;  
    uint32_t                buf_size;              
    enum psp_gfx_fw_type    fw_type;               
};
struct psp_gfx_cmd_reg_prog {
	uint32_t	reg_value;
	uint32_t	reg_id;
};
struct psp_gfx_cmd_load_toc
{
    uint32_t        toc_phy_addr_lo;         
    uint32_t        toc_phy_addr_hi;         
    uint32_t        toc_size;                
};
struct psp_gfx_cmd_boot_cfg
{
    uint32_t                        timestamp;             
    enum psp_gfx_boot_config_cmd    sub_cmd;               
    uint32_t                        boot_config;           
    uint32_t                        boot_config_valid;     
};
struct psp_gfx_cmd_sriov_spatial_part {
	uint32_t mode;
	uint32_t override_ips;
	uint32_t override_xcds_avail;
	uint32_t override_this_aid;
};
union psp_gfx_commands
{
    struct psp_gfx_cmd_load_ta          cmd_load_ta;
    struct psp_gfx_cmd_unload_ta        cmd_unload_ta;
    struct psp_gfx_cmd_invoke_cmd       cmd_invoke_cmd;
    struct psp_gfx_cmd_setup_tmr        cmd_setup_tmr;
    struct psp_gfx_cmd_load_ip_fw       cmd_load_ip_fw;
    struct psp_gfx_cmd_save_restore_ip_fw cmd_save_restore_ip_fw;
    struct psp_gfx_cmd_reg_prog       cmd_setup_reg_prog;
    struct psp_gfx_cmd_setup_tmr        cmd_setup_vmr;
    struct psp_gfx_cmd_load_toc         cmd_load_toc;
    struct psp_gfx_cmd_boot_cfg         boot_cfg;
    struct psp_gfx_cmd_sriov_spatial_part cmd_spatial_part;
};
struct psp_gfx_uresp_reserved
{
    uint32_t reserved[8];
};
struct psp_gfx_uresp_fwar_db_info
{
    uint32_t fwar_db_addr_lo;
    uint32_t fwar_db_addr_hi;
};
struct psp_gfx_uresp_bootcfg {
	uint32_t boot_cfg;	 
};
union psp_gfx_uresp {
	struct psp_gfx_uresp_reserved		reserved;
	struct psp_gfx_uresp_bootcfg		boot_cfg;
	struct psp_gfx_uresp_fwar_db_info	fwar_db_info;
};
struct psp_gfx_resp
{
    uint32_t	status;		 
    uint32_t	session_id;	 
    uint32_t	fw_addr_lo;	 
    uint32_t	fw_addr_hi;	 
    uint32_t	tmr_size;	 
    uint32_t	reserved[11];
    union psp_gfx_uresp uresp;       
};
struct psp_gfx_cmd_resp
{
    uint32_t        buf_size;            
    uint32_t        buf_version;         
    uint32_t        cmd_id;              
    uint32_t        resp_buf_addr_lo;    
    uint32_t        resp_buf_addr_hi;    
    uint32_t        resp_offset;         
    uint32_t        resp_buf_size;       
    union psp_gfx_commands  cmd;         
    uint8_t         reserved_1[864 - sizeof(union psp_gfx_commands) - 28];
    struct psp_gfx_resp     resp;        
    uint8_t         reserved_2[1024 - 864 - sizeof(struct psp_gfx_resp)];
};
#define FRAME_TYPE_DESTROY          1    
struct psp_gfx_rb_frame
{
    uint32_t    cmd_buf_addr_lo;     
    uint32_t    cmd_buf_addr_hi;     
    uint32_t    cmd_buf_size;        
    uint32_t    fence_addr_lo;       
    uint32_t    fence_addr_hi;       
    uint32_t    fence_value;         
    uint32_t    sid_lo;              
    uint32_t    sid_hi;              
    uint8_t     vmid;                
    uint8_t     frame_type;          
    uint8_t     reserved1[2];        
    uint32_t    reserved2[7];        
};
#define PSP_ERR_UNKNOWN_COMMAND 0x00000100
enum tee_error_code {
    TEE_SUCCESS                         = 0x00000000,
    TEE_ERROR_NOT_SUPPORTED             = 0xFFFF000A,
};
#endif  
