#ifndef __CVMX_BOOTMEM_H__
#define __CVMX_BOOTMEM_H__
#define CVMX_BOOTMEM_NAME_LEN 128
#define CVMX_BOOTMEM_NUM_NAMED_BLOCKS 64
#define CVMX_BOOTMEM_ALIGNMENT_SIZE	(16ull)
#define CVMX_BOOTMEM_FLAG_END_ALLOC    (1 << 0)
#define CVMX_BOOTMEM_FLAG_NO_LOCKING   (1 << 1)
struct cvmx_bootmem_block_header {
	uint64_t next_block_addr;
	uint64_t size;
};
struct cvmx_bootmem_named_block_desc {
	uint64_t base_addr;
	uint64_t size;
	char name[CVMX_BOOTMEM_NAME_LEN];
};
#define CVMX_BOOTMEM_DESC_MAJ_VER   3
#define CVMX_BOOTMEM_DESC_MIN_VER   0
struct cvmx_bootmem_desc {
#if defined(__BIG_ENDIAN_BITFIELD) || defined(CVMX_BUILD_FOR_LINUX_HOST)
	uint32_t lock;
	uint32_t flags;
	uint64_t head_addr;
	uint32_t major_version;
	uint32_t minor_version;
	uint64_t app_data_addr;
	uint64_t app_data_size;
	uint32_t named_block_num_blocks;
	uint32_t named_block_name_len;
	uint64_t named_block_array_addr;
#else                            
	uint32_t flags;
	uint32_t lock;
	uint64_t head_addr;
	uint32_t minor_version;
	uint32_t major_version;
	uint64_t app_data_addr;
	uint64_t app_data_size;
	uint32_t named_block_name_len;
	uint32_t named_block_num_blocks;
	uint64_t named_block_array_addr;
#endif
};
extern int cvmx_bootmem_init(void *mem_desc_ptr);
extern void *cvmx_bootmem_alloc_address(uint64_t size, uint64_t address,
					uint64_t alignment);
extern void *cvmx_bootmem_alloc_named(uint64_t size, uint64_t alignment,
				      char *name);
extern void *cvmx_bootmem_alloc_named_range(uint64_t size, uint64_t min_addr,
					    uint64_t max_addr, uint64_t align,
					    char *name);
void *cvmx_bootmem_alloc_named_range_once(uint64_t size,
					  uint64_t min_addr,
					  uint64_t max_addr,
					  uint64_t align,
					  char *name,
					  void (*init) (void *));
extern int cvmx_bootmem_free_named(char *name);
struct cvmx_bootmem_named_block_desc *cvmx_bootmem_find_named_block(char *name);
int64_t cvmx_bootmem_phy_alloc(uint64_t req_size, uint64_t address_min,
			       uint64_t address_max, uint64_t alignment,
			       uint32_t flags);
int64_t cvmx_bootmem_phy_named_block_alloc(uint64_t size, uint64_t min_addr,
					   uint64_t max_addr,
					   uint64_t alignment,
					   char *name, uint32_t flags);
int __cvmx_bootmem_phy_free(uint64_t phy_addr, uint64_t size, uint32_t flags);
void cvmx_bootmem_lock(void);
void cvmx_bootmem_unlock(void);
extern struct cvmx_bootmem_desc *cvmx_bootmem_get_desc(void);
#endif  
