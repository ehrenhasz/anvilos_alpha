 

#ifndef AMDGPU_DOORBELL_H
#define AMDGPU_DOORBELL_H

 
struct amdgpu_doorbell {
	 
	resource_size_t		base;
	resource_size_t		size;

	 
	u32 num_kernel_doorbells;

	 
	struct amdgpu_bo *kernel_doorbells;

	 
	uint32_t *cpu_addr;
};

 
struct amdgpu_doorbell_index {
	uint32_t kiq;
	uint32_t mec_ring0;
	uint32_t mec_ring1;
	uint32_t mec_ring2;
	uint32_t mec_ring3;
	uint32_t mec_ring4;
	uint32_t mec_ring5;
	uint32_t mec_ring6;
	uint32_t mec_ring7;
	uint32_t userqueue_start;
	uint32_t userqueue_end;
	uint32_t gfx_ring0;
	uint32_t gfx_ring1;
	uint32_t gfx_userqueue_start;
	uint32_t gfx_userqueue_end;
	uint32_t sdma_engine[16];
	uint32_t mes_ring0;
	uint32_t mes_ring1;
	uint32_t ih;
	union {
		struct {
			uint32_t vcn_ring0_1;
			uint32_t vcn_ring2_3;
			uint32_t vcn_ring4_5;
			uint32_t vcn_ring6_7;
		} vcn;
		struct {
			uint32_t uvd_ring0_1;
			uint32_t uvd_ring2_3;
			uint32_t uvd_ring4_5;
			uint32_t uvd_ring6_7;
			uint32_t vce_ring0_1;
			uint32_t vce_ring2_3;
			uint32_t vce_ring4_5;
			uint32_t vce_ring6_7;
		} uvd_vce;
	};
	uint32_t first_non_cp;
	uint32_t last_non_cp;
	uint32_t max_assignment;
	 
	uint32_t sdma_doorbell_range;
	 
	uint32_t xcc_doorbell_range;
};

enum AMDGPU_DOORBELL_ASSIGNMENT {
	AMDGPU_DOORBELL_KIQ                     = 0x000,
	AMDGPU_DOORBELL_HIQ                     = 0x001,
	AMDGPU_DOORBELL_DIQ                     = 0x002,
	AMDGPU_DOORBELL_MEC_RING0               = 0x010,
	AMDGPU_DOORBELL_MEC_RING1               = 0x011,
	AMDGPU_DOORBELL_MEC_RING2               = 0x012,
	AMDGPU_DOORBELL_MEC_RING3               = 0x013,
	AMDGPU_DOORBELL_MEC_RING4               = 0x014,
	AMDGPU_DOORBELL_MEC_RING5               = 0x015,
	AMDGPU_DOORBELL_MEC_RING6               = 0x016,
	AMDGPU_DOORBELL_MEC_RING7               = 0x017,
	AMDGPU_DOORBELL_GFX_RING0               = 0x020,
	AMDGPU_DOORBELL_sDMA_ENGINE0            = 0x1E0,
	AMDGPU_DOORBELL_sDMA_ENGINE1            = 0x1E1,
	AMDGPU_DOORBELL_IH                      = 0x1E8,
	AMDGPU_DOORBELL_MAX_ASSIGNMENT          = 0x3FF,
	AMDGPU_DOORBELL_INVALID                 = 0xFFFF
};

enum AMDGPU_VEGA20_DOORBELL_ASSIGNMENT {

	 
	AMDGPU_VEGA20_DOORBELL_KIQ                     = 0x000,
	AMDGPU_VEGA20_DOORBELL_HIQ                     = 0x001,
	AMDGPU_VEGA20_DOORBELL_DIQ                     = 0x002,
	AMDGPU_VEGA20_DOORBELL_MEC_RING0               = 0x003,
	AMDGPU_VEGA20_DOORBELL_MEC_RING1               = 0x004,
	AMDGPU_VEGA20_DOORBELL_MEC_RING2               = 0x005,
	AMDGPU_VEGA20_DOORBELL_MEC_RING3               = 0x006,
	AMDGPU_VEGA20_DOORBELL_MEC_RING4               = 0x007,
	AMDGPU_VEGA20_DOORBELL_MEC_RING5               = 0x008,
	AMDGPU_VEGA20_DOORBELL_MEC_RING6               = 0x009,
	AMDGPU_VEGA20_DOORBELL_MEC_RING7               = 0x00A,
	AMDGPU_VEGA20_DOORBELL_USERQUEUE_START	       = 0x00B,
	AMDGPU_VEGA20_DOORBELL_USERQUEUE_END	       = 0x08A,
	AMDGPU_VEGA20_DOORBELL_GFX_RING0               = 0x08B,
	 
	AMDGPU_VEGA20_DOORBELL_sDMA_ENGINE0            = 0x100,
	AMDGPU_VEGA20_DOORBELL_sDMA_ENGINE1            = 0x10A,
	AMDGPU_VEGA20_DOORBELL_sDMA_ENGINE2            = 0x114,
	AMDGPU_VEGA20_DOORBELL_sDMA_ENGINE3            = 0x11E,
	AMDGPU_VEGA20_DOORBELL_sDMA_ENGINE4            = 0x128,
	AMDGPU_VEGA20_DOORBELL_sDMA_ENGINE5            = 0x132,
	AMDGPU_VEGA20_DOORBELL_sDMA_ENGINE6            = 0x13C,
	AMDGPU_VEGA20_DOORBELL_sDMA_ENGINE7            = 0x146,
	 
	AMDGPU_VEGA20_DOORBELL_IH                      = 0x178,
	 
	AMDGPU_VEGA20_DOORBELL64_VCN0_1                  = 0x188,  
	AMDGPU_VEGA20_DOORBELL64_VCN2_3                  = 0x189,
	AMDGPU_VEGA20_DOORBELL64_VCN4_5                  = 0x18A,
	AMDGPU_VEGA20_DOORBELL64_VCN6_7                  = 0x18B,

	AMDGPU_VEGA20_DOORBELL64_VCN8_9                  = 0x18C,  
	AMDGPU_VEGA20_DOORBELL64_VCNa_b                  = 0x18D,
	AMDGPU_VEGA20_DOORBELL64_VCNc_d                  = 0x18E,
	AMDGPU_VEGA20_DOORBELL64_VCNe_f                  = 0x18F,

	AMDGPU_VEGA20_DOORBELL64_UVD_RING0_1             = 0x188,
	AMDGPU_VEGA20_DOORBELL64_UVD_RING2_3             = 0x189,
	AMDGPU_VEGA20_DOORBELL64_UVD_RING4_5             = 0x18A,
	AMDGPU_VEGA20_DOORBELL64_UVD_RING6_7             = 0x18B,

	AMDGPU_VEGA20_DOORBELL64_VCE_RING0_1             = 0x18C,
	AMDGPU_VEGA20_DOORBELL64_VCE_RING2_3             = 0x18D,
	AMDGPU_VEGA20_DOORBELL64_VCE_RING4_5             = 0x18E,
	AMDGPU_VEGA20_DOORBELL64_VCE_RING6_7             = 0x18F,

	AMDGPU_VEGA20_DOORBELL64_FIRST_NON_CP            = AMDGPU_VEGA20_DOORBELL_sDMA_ENGINE0,
	AMDGPU_VEGA20_DOORBELL64_LAST_NON_CP             = AMDGPU_VEGA20_DOORBELL64_VCE_RING6_7,

	 
	AMDGPU_VEGA20_DOORBELL_XCC1_KIQ_START             = 0x190,
	 
	AMDGPU_VEGA20_DOORBELL_XCC1_MEC_RING0_START       = 0x197,

	 
	AMDGPU_VEGA20_DOORBELL_AID1_sDMA_START           = 0x1D0,

	AMDGPU_VEGA20_DOORBELL_MAX_ASSIGNMENT            = 0x1F7,
	AMDGPU_VEGA20_DOORBELL_INVALID                   = 0xFFFF
};

enum AMDGPU_NAVI10_DOORBELL_ASSIGNMENT {

	 
	AMDGPU_NAVI10_DOORBELL_KIQ			= 0x000,
	AMDGPU_NAVI10_DOORBELL_HIQ			= 0x001,
	AMDGPU_NAVI10_DOORBELL_DIQ			= 0x002,
	AMDGPU_NAVI10_DOORBELL_MEC_RING0		= 0x003,
	AMDGPU_NAVI10_DOORBELL_MEC_RING1		= 0x004,
	AMDGPU_NAVI10_DOORBELL_MEC_RING2		= 0x005,
	AMDGPU_NAVI10_DOORBELL_MEC_RING3		= 0x006,
	AMDGPU_NAVI10_DOORBELL_MEC_RING4		= 0x007,
	AMDGPU_NAVI10_DOORBELL_MEC_RING5		= 0x008,
	AMDGPU_NAVI10_DOORBELL_MEC_RING6		= 0x009,
	AMDGPU_NAVI10_DOORBELL_MEC_RING7		= 0x00A,
	AMDGPU_NAVI10_DOORBELL_MES_RING0	        = 0x00B,
	AMDGPU_NAVI10_DOORBELL_MES_RING1		= 0x00C,
	AMDGPU_NAVI10_DOORBELL_USERQUEUE_START		= 0x00D,
	AMDGPU_NAVI10_DOORBELL_USERQUEUE_END		= 0x08A,
	AMDGPU_NAVI10_DOORBELL_GFX_RING0		= 0x08B,
	AMDGPU_NAVI10_DOORBELL_GFX_RING1		= 0x08C,
	AMDGPU_NAVI10_DOORBELL_GFX_USERQUEUE_START	= 0x08D,
	AMDGPU_NAVI10_DOORBELL_GFX_USERQUEUE_END	= 0x0FF,

	 
	AMDGPU_NAVI10_DOORBELL_sDMA_ENGINE0		= 0x100,
	AMDGPU_NAVI10_DOORBELL_sDMA_ENGINE1		= 0x10A,
	AMDGPU_NAVI10_DOORBELL_sDMA_ENGINE2		= 0x114,
	AMDGPU_NAVI10_DOORBELL_sDMA_ENGINE3		= 0x11E,
	 
	AMDGPU_NAVI10_DOORBELL_IH			= 0x178,
	 
	AMDGPU_NAVI10_DOORBELL64_VCN0_1			= 0x188,  
	AMDGPU_NAVI10_DOORBELL64_VCN2_3			= 0x189,
	AMDGPU_NAVI10_DOORBELL64_VCN4_5			= 0x18A,
	AMDGPU_NAVI10_DOORBELL64_VCN6_7			= 0x18B,

	AMDGPU_NAVI10_DOORBELL64_VCN8_9			= 0x18C,
	AMDGPU_NAVI10_DOORBELL64_VCNa_b			= 0x18D,
	AMDGPU_NAVI10_DOORBELL64_VCNc_d			= 0x18E,
	AMDGPU_NAVI10_DOORBELL64_VCNe_f			= 0x18F,

	AMDGPU_NAVI10_DOORBELL64_FIRST_NON_CP		= AMDGPU_NAVI10_DOORBELL_sDMA_ENGINE0,
	AMDGPU_NAVI10_DOORBELL64_LAST_NON_CP		= AMDGPU_NAVI10_DOORBELL64_VCNe_f,

	AMDGPU_NAVI10_DOORBELL_MAX_ASSIGNMENT		= 0x18F,
	AMDGPU_NAVI10_DOORBELL_INVALID			= 0xFFFF
};

 
enum AMDGPU_DOORBELL64_ASSIGNMENT {
	 


	 
	AMDGPU_DOORBELL64_KIQ                     = 0x00,

	 
	AMDGPU_DOORBELL64_HIQ                     = 0x01,
	AMDGPU_DOORBELL64_DIQ                     = 0x02,

	 
	AMDGPU_DOORBELL64_MEC_RING0               = 0x03,
	AMDGPU_DOORBELL64_MEC_RING1               = 0x04,
	AMDGPU_DOORBELL64_MEC_RING2               = 0x05,
	AMDGPU_DOORBELL64_MEC_RING3               = 0x06,
	AMDGPU_DOORBELL64_MEC_RING4               = 0x07,
	AMDGPU_DOORBELL64_MEC_RING5               = 0x08,
	AMDGPU_DOORBELL64_MEC_RING6               = 0x09,
	AMDGPU_DOORBELL64_MEC_RING7               = 0x0a,

	 
	AMDGPU_DOORBELL64_USERQUEUE_START         = 0x0b,
	AMDGPU_DOORBELL64_USERQUEUE_END           = 0x8a,

	 
	AMDGPU_DOORBELL64_GFX_RING0               = 0x8b,

	 

	 
	AMDGPU_DOORBELL64_sDMA_ENGINE0            = 0xF0,
	AMDGPU_DOORBELL64_sDMA_HI_PRI_ENGINE0     = 0xF1,
	AMDGPU_DOORBELL64_sDMA_ENGINE1            = 0xF2,
	AMDGPU_DOORBELL64_sDMA_HI_PRI_ENGINE1     = 0xF3,

	 
	AMDGPU_DOORBELL64_IH                      = 0xF4,   
	AMDGPU_DOORBELL64_IH_RING1                = 0xF5,   
	AMDGPU_DOORBELL64_IH_RING2                = 0xF6,   

	 
	AMDGPU_DOORBELL64_VCN0_1                  = 0xF8,  
	AMDGPU_DOORBELL64_VCN2_3                  = 0xF9,
	AMDGPU_DOORBELL64_VCN4_5                  = 0xFA,
	AMDGPU_DOORBELL64_VCN6_7                  = 0xFB,

	 
	AMDGPU_DOORBELL64_UVD_RING0_1             = 0xF8,
	AMDGPU_DOORBELL64_UVD_RING2_3             = 0xF9,
	AMDGPU_DOORBELL64_UVD_RING4_5             = 0xFA,
	AMDGPU_DOORBELL64_UVD_RING6_7             = 0xFB,

	AMDGPU_DOORBELL64_VCE_RING0_1             = 0xFC,
	AMDGPU_DOORBELL64_VCE_RING2_3             = 0xFD,
	AMDGPU_DOORBELL64_VCE_RING4_5             = 0xFE,
	AMDGPU_DOORBELL64_VCE_RING6_7             = 0xFF,

	AMDGPU_DOORBELL64_FIRST_NON_CP            = AMDGPU_DOORBELL64_sDMA_ENGINE0,
	AMDGPU_DOORBELL64_LAST_NON_CP             = AMDGPU_DOORBELL64_VCE_RING6_7,

	AMDGPU_DOORBELL64_MAX_ASSIGNMENT          = 0xFF,
	AMDGPU_DOORBELL64_INVALID                 = 0xFFFF
};

enum AMDGPU_DOORBELL_ASSIGNMENT_LAYOUT1 {

	 

	 
	AMDGPU_DOORBELL_LAYOUT1_KIQ_START		= 0x000,
	AMDGPU_DOORBELL_LAYOUT1_HIQ			= 0x001,
	AMDGPU_DOORBELL_LAYOUT1_DIQ			= 0x002,
	 
	AMDGPU_DOORBELL_LAYOUT1_MEC_RING_START		= 0x008,
	AMDGPU_DOORBELL_LAYOUT1_MEC_RING_END		= 0x00F,
	AMDGPU_DOORBELL_LAYOUT1_USERQUEUE_START		= 0x010,
	AMDGPU_DOORBELL_LAYOUT1_USERQUEUE_END		= 0x01F,
	AMDGPU_DOORBELL_LAYOUT1_XCC_RANGE		= 0x020,

	 
	AMDGPU_DOORBELL_LAYOUT1_sDMA_ENGINE_START	= 0x100,
	AMDGPU_DOORBELL_LAYOUT1_sDMA_ENGINE_END		= 0x19F,
	 
	AMDGPU_DOORBELL_LAYOUT1_IH                      = 0x1A0,
	 
	AMDGPU_DOORBELL_LAYOUT1_VCN_START               = 0x1B0,
	AMDGPU_DOORBELL_LAYOUT1_VCN_END                 = 0x1E8,

	AMDGPU_DOORBELL_LAYOUT1_FIRST_NON_CP		= AMDGPU_DOORBELL_LAYOUT1_sDMA_ENGINE_START,
	AMDGPU_DOORBELL_LAYOUT1_LAST_NON_CP		= AMDGPU_DOORBELL_LAYOUT1_VCN_END,

	AMDGPU_DOORBELL_LAYOUT1_MAX_ASSIGNMENT          = 0x1E8,
	AMDGPU_DOORBELL_LAYOUT1_INVALID                 = 0xFFFF
};

u32 amdgpu_mm_rdoorbell(struct amdgpu_device *adev, u32 index);
void amdgpu_mm_wdoorbell(struct amdgpu_device *adev, u32 index, u32 v);
u64 amdgpu_mm_rdoorbell64(struct amdgpu_device *adev, u32 index);
void amdgpu_mm_wdoorbell64(struct amdgpu_device *adev, u32 index, u64 v);

 
int amdgpu_doorbell_init(struct amdgpu_device *adev);
void amdgpu_doorbell_fini(struct amdgpu_device *adev);
int amdgpu_doorbell_create_kernel_doorbells(struct amdgpu_device *adev);
uint32_t amdgpu_doorbell_index_on_bar(struct amdgpu_device *adev,
				      struct amdgpu_bo *db_bo,
				      uint32_t doorbell_index,
				      uint32_t db_size);

#define RDOORBELL32(index) amdgpu_mm_rdoorbell(adev, (index))
#define WDOORBELL32(index, v) amdgpu_mm_wdoorbell(adev, (index), (v))
#define RDOORBELL64(index) amdgpu_mm_rdoorbell64(adev, (index))
#define WDOORBELL64(index, v) amdgpu_mm_wdoorbell64(adev, (index), (v))

#endif
