#ifndef _DISCOVERY_H_
#define _DISCOVERY_H_
#define PSP_HEADER_SIZE                 256
#define BINARY_SIGNATURE                0x28211407
#define DISCOVERY_TABLE_SIGNATURE       0x53445049
#define GC_TABLE_ID                     0x4347
#define HARVEST_TABLE_SIGNATURE         0x56524148
#define VCN_INFO_TABLE_ID               0x004E4356
#define MALL_INFO_TABLE_ID              0x4C4C414D
typedef enum
{
	IP_DISCOVERY = 0,
	GC,
	HARVEST_INFO,
	VCN_INFO,
	MALL_INFO,
	RESERVED_1,
	TOTAL_TABLES = 6
} table;
#pragma pack(1)
typedef struct table_info
{
	uint16_t offset;    
	uint16_t checksum;  
	uint16_t size;      
	uint16_t padding;
} table_info;
typedef struct binary_header
{
	uint32_t binary_signature;  
	uint16_t version_major;
	uint16_t version_minor;
	uint16_t binary_checksum;   
	uint16_t binary_size;       
	table_info table_list[TOTAL_TABLES];
} binary_header;
typedef struct die_info
{
	uint16_t die_id;
	uint16_t die_offset;  
} die_info;
typedef struct ip_discovery_header
{
	uint32_t signature;     
	uint16_t version;       
	uint16_t size;          
	uint32_t id;            
	uint16_t num_dies;      
	die_info die_info[16];  
	union {
		uint16_t padding[1];	 
		struct {		 
			uint8_t base_addr_64_bit : 1;  
			uint8_t reserved : 7;
			uint8_t reserved2;
		};
	};
} ip_discovery_header;
typedef struct ip
{
	uint16_t hw_id;            
	uint8_t number_instance;   
	uint8_t num_base_address;  
	uint8_t major;             
	uint8_t minor;             
	uint8_t revision;          
#if defined(__BIG_ENDIAN)
	uint8_t reserved : 4;      
	uint8_t harvest : 4;       
#else
	uint8_t harvest : 4;       
	uint8_t reserved : 4;      
#endif
	uint32_t base_address[];  
} ip;
typedef struct ip_v3
{
	uint16_t hw_id;                          
	uint8_t instance_number;                 
	uint8_t num_base_address;                
	uint8_t major;                           
	uint8_t minor;                           
	uint8_t revision;                        
#if defined(__BIG_ENDIAN)
	uint8_t variant : 4;                     
	uint8_t sub_revision : 4;                
#else
	uint8_t sub_revision : 4;                
	uint8_t variant : 4;                     
#endif
	uint32_t base_address[];		 
} ip_v3;
typedef struct ip_v4 {
	uint16_t hw_id;                          
	uint8_t instance_number;                 
	uint8_t num_base_address;                
	uint8_t major;                           
	uint8_t minor;                           
	uint8_t revision;                        
#if defined(LITTLEENDIAN_CPU)
	uint8_t sub_revision : 4;                
	uint8_t variant : 4;                     
#elif defined(BIGENDIAN_CPU)
	uint8_t variant : 4;                     
	uint8_t sub_revision : 4;                
#endif
	union {
		DECLARE_FLEX_ARRAY(uint32_t, base_address);	 
		DECLARE_FLEX_ARRAY(uint64_t, base_address_64);	 
	} __packed;
} ip_v4;
typedef struct die_header
{
	uint16_t die_id;
	uint16_t num_ips;
} die_header;
typedef struct ip_structure
{
	ip_discovery_header* header;
	struct die
	{
		die_header *die_header;
		union
		{
			ip *ip_list;
			ip_v3 *ip_v3_list;
			ip_v4 *ip_v4_list;
		};                                   
	} die;
} ip_structure;
struct gpu_info_header {
	uint32_t table_id;       
	uint16_t version_major;  
	uint16_t version_minor;  
	uint32_t size;           
};
struct gc_info_v1_0 {
	struct gpu_info_header header;
	uint32_t gc_num_se;
	uint32_t gc_num_wgp0_per_sa;
	uint32_t gc_num_wgp1_per_sa;
	uint32_t gc_num_rb_per_se;
	uint32_t gc_num_gl2c;
	uint32_t gc_num_gprs;
	uint32_t gc_num_max_gs_thds;
	uint32_t gc_gs_table_depth;
	uint32_t gc_gsprim_buff_depth;
	uint32_t gc_parameter_cache_depth;
	uint32_t gc_double_offchip_lds_buffer;
	uint32_t gc_wave_size;
	uint32_t gc_max_waves_per_simd;
	uint32_t gc_max_scratch_slots_per_cu;
	uint32_t gc_lds_size;
	uint32_t gc_num_sc_per_se;
	uint32_t gc_num_sa_per_se;
	uint32_t gc_num_packer_per_sc;
	uint32_t gc_num_gl2a;
};
struct gc_info_v1_1 {
	struct gpu_info_header header;
	uint32_t gc_num_se;
	uint32_t gc_num_wgp0_per_sa;
	uint32_t gc_num_wgp1_per_sa;
	uint32_t gc_num_rb_per_se;
	uint32_t gc_num_gl2c;
	uint32_t gc_num_gprs;
	uint32_t gc_num_max_gs_thds;
	uint32_t gc_gs_table_depth;
	uint32_t gc_gsprim_buff_depth;
	uint32_t gc_parameter_cache_depth;
	uint32_t gc_double_offchip_lds_buffer;
	uint32_t gc_wave_size;
	uint32_t gc_max_waves_per_simd;
	uint32_t gc_max_scratch_slots_per_cu;
	uint32_t gc_lds_size;
	uint32_t gc_num_sc_per_se;
	uint32_t gc_num_sa_per_se;
	uint32_t gc_num_packer_per_sc;
	uint32_t gc_num_gl2a;
	uint32_t gc_num_tcp_per_sa;
	uint32_t gc_num_sdp_interface;
	uint32_t gc_num_tcps;
};
struct gc_info_v1_2 {
	struct gpu_info_header header;
	uint32_t gc_num_se;
	uint32_t gc_num_wgp0_per_sa;
	uint32_t gc_num_wgp1_per_sa;
	uint32_t gc_num_rb_per_se;
	uint32_t gc_num_gl2c;
	uint32_t gc_num_gprs;
	uint32_t gc_num_max_gs_thds;
	uint32_t gc_gs_table_depth;
	uint32_t gc_gsprim_buff_depth;
	uint32_t gc_parameter_cache_depth;
	uint32_t gc_double_offchip_lds_buffer;
	uint32_t gc_wave_size;
	uint32_t gc_max_waves_per_simd;
	uint32_t gc_max_scratch_slots_per_cu;
	uint32_t gc_lds_size;
	uint32_t gc_num_sc_per_se;
	uint32_t gc_num_sa_per_se;
	uint32_t gc_num_packer_per_sc;
	uint32_t gc_num_gl2a;
	uint32_t gc_num_tcp_per_sa;
	uint32_t gc_num_sdp_interface;
	uint32_t gc_num_tcps;
	uint32_t gc_num_tcp_per_wpg;
	uint32_t gc_tcp_l1_size;
	uint32_t gc_num_sqc_per_wgp;
	uint32_t gc_l1_instruction_cache_size_per_sqc;
	uint32_t gc_l1_data_cache_size_per_sqc;
	uint32_t gc_gl1c_per_sa;
	uint32_t gc_gl1c_size_per_instance;
	uint32_t gc_gl2c_per_gpu;
};
struct gc_info_v2_0 {
	struct gpu_info_header header;
	uint32_t gc_num_se;
	uint32_t gc_num_cu_per_sh;
	uint32_t gc_num_sh_per_se;
	uint32_t gc_num_rb_per_se;
	uint32_t gc_num_tccs;
	uint32_t gc_num_gprs;
	uint32_t gc_num_max_gs_thds;
	uint32_t gc_gs_table_depth;
	uint32_t gc_gsprim_buff_depth;
	uint32_t gc_parameter_cache_depth;
	uint32_t gc_double_offchip_lds_buffer;
	uint32_t gc_wave_size;
	uint32_t gc_max_waves_per_simd;
	uint32_t gc_max_scratch_slots_per_cu;
	uint32_t gc_lds_size;
	uint32_t gc_num_sc_per_se;
	uint32_t gc_num_packer_per_sc;
};
struct gc_info_v2_1 {
	struct gpu_info_header header;
	uint32_t gc_num_se;
	uint32_t gc_num_cu_per_sh;
	uint32_t gc_num_sh_per_se;
	uint32_t gc_num_rb_per_se;
	uint32_t gc_num_tccs;
	uint32_t gc_num_gprs;
	uint32_t gc_num_max_gs_thds;
	uint32_t gc_gs_table_depth;
	uint32_t gc_gsprim_buff_depth;
	uint32_t gc_parameter_cache_depth;
	uint32_t gc_double_offchip_lds_buffer;
	uint32_t gc_wave_size;
	uint32_t gc_max_waves_per_simd;
	uint32_t gc_max_scratch_slots_per_cu;
	uint32_t gc_lds_size;
	uint32_t gc_num_sc_per_se;
	uint32_t gc_num_packer_per_sc;
	uint32_t gc_num_tcp_per_sh;
	uint32_t gc_tcp_size_per_cu;
	uint32_t gc_num_sdp_interface;
	uint32_t gc_num_cu_per_sqc;
	uint32_t gc_instruction_cache_size_per_sqc;
	uint32_t gc_scalar_data_cache_size_per_sqc;
	uint32_t gc_tcc_size;
};
typedef struct harvest_info_header {
	uint32_t signature;  
	uint32_t version;    
} harvest_info_header;
typedef struct harvest_info {
	uint16_t hw_id;           
	uint8_t number_instance;  
	uint8_t reserved;         
} harvest_info;
typedef struct harvest_table {
	harvest_info_header header;
	harvest_info list[32];
} harvest_table;
struct mall_info_header {
	uint32_t table_id;  
	uint16_t version_major;  
	uint16_t version_minor;  
	uint32_t size_bytes;  
};
struct mall_info_v1_0 {
	struct mall_info_header header;
	uint32_t mall_size_per_m;
	uint32_t m_s_present;
	uint32_t m_half_use;
	uint32_t m_mall_config;
	uint32_t reserved[5];
};
struct mall_info_v2_0 {
	struct mall_info_header header;
	uint32_t mall_size_per_umc;
	uint32_t reserved[8];
};
#define VCN_INFO_TABLE_MAX_NUM_INSTANCES 4
struct vcn_info_header {
    uint32_t table_id;  
    uint16_t version_major;  
    uint16_t version_minor;  
    uint32_t size_bytes;  
};
struct vcn_instance_info_v1_0
{
	uint32_t instance_num;  
	union _fuse_data {
		struct {
			uint32_t av1_disabled : 1;
			uint32_t vp9_disabled : 1;
			uint32_t hevc_disabled : 1;
			uint32_t h264_disabled : 1;
			uint32_t reserved : 28;
		} bits;
		uint32_t all_bits;
	} fuse_data;
	uint32_t reserved[2];
};
struct vcn_info_v1_0 {
	struct vcn_info_header header;
	uint32_t num_of_instances;  
	struct vcn_instance_info_v1_0 instance_info[VCN_INFO_TABLE_MAX_NUM_INSTANCES];
	uint32_t reserved[4];
};
#pragma pack()
#endif
