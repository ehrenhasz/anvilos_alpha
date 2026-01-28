#ifndef __CVMX_FPA_H__
#define __CVMX_FPA_H__
#include <linux/delay.h>
#include <asm/octeon/cvmx-address.h>
#include <asm/octeon/cvmx-fpa-defs.h>
#define CVMX_FPA_NUM_POOLS	8
#define CVMX_FPA_MIN_BLOCK_SIZE 128
#define CVMX_FPA_ALIGNMENT	128
typedef union {
	uint64_t u64;
	struct {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t scraddr:8;
		uint64_t len:8;
		uint64_t did:8;
		uint64_t addr:40;
#else
		uint64_t addr:40;
		uint64_t did:8;
		uint64_t len:8;
		uint64_t scraddr:8;
#endif
	} s;
} cvmx_fpa_iobdma_data_t;
typedef struct {
	const char *name;
	uint64_t size;
	void *base;
	uint64_t starting_element_count;
} cvmx_fpa_pool_info_t;
extern cvmx_fpa_pool_info_t cvmx_fpa_pool_info[CVMX_FPA_NUM_POOLS];
static inline const char *cvmx_fpa_get_name(uint64_t pool)
{
	return cvmx_fpa_pool_info[pool].name;
}
static inline void *cvmx_fpa_get_base(uint64_t pool)
{
	return cvmx_fpa_pool_info[pool].base;
}
static inline int cvmx_fpa_is_member(uint64_t pool, void *ptr)
{
	return ((ptr >= cvmx_fpa_pool_info[pool].base) &&
		((char *)ptr <
		 ((char *)(cvmx_fpa_pool_info[pool].base)) +
		 cvmx_fpa_pool_info[pool].size *
		 cvmx_fpa_pool_info[pool].starting_element_count));
}
static inline void cvmx_fpa_enable(void)
{
	union cvmx_fpa_ctl_status status;
	status.u64 = cvmx_read_csr(CVMX_FPA_CTL_STATUS);
	if (status.s.enb) {
		cvmx_dprintf
		    ("Warning: Enabling FPA when FPA already enabled.\n");
	}
	if (cvmx_octeon_is_pass1()) {
		union cvmx_fpa_fpfx_marks marks;
		int i;
		for (i = 1; i < 8; i++) {
			marks.u64 =
			    cvmx_read_csr(CVMX_FPA_FPF1_MARKS + (i - 1) * 8ull);
			marks.s.fpf_wr = 0xe0;
			cvmx_write_csr(CVMX_FPA_FPF1_MARKS + (i - 1) * 8ull,
				       marks.u64);
		}
		__delay(10);
	}
	status.u64 = 0;
	status.s.enb = 1;
	cvmx_write_csr(CVMX_FPA_CTL_STATUS, status.u64);
}
static inline void *cvmx_fpa_alloc(uint64_t pool)
{
	uint64_t address =
	    cvmx_read_csr(CVMX_ADDR_DID(CVMX_FULL_DID(CVMX_OCT_DID_FPA, pool)));
	if (address)
		return cvmx_phys_to_ptr(address);
	else
		return NULL;
}
static inline void cvmx_fpa_async_alloc(uint64_t scr_addr, uint64_t pool)
{
	cvmx_fpa_iobdma_data_t data;
	data.s.scraddr = scr_addr >> 3;
	data.s.len = 1;
	data.s.did = CVMX_FULL_DID(CVMX_OCT_DID_FPA, pool);
	data.s.addr = 0;
	cvmx_send_single(data.u64);
}
static inline void cvmx_fpa_free_nosync(void *ptr, uint64_t pool,
					uint64_t num_cache_lines)
{
	cvmx_addr_t newptr;
	newptr.u64 = cvmx_ptr_to_phys(ptr);
	newptr.sfilldidspace.didspace =
	    CVMX_ADDR_DIDSPACE(CVMX_FULL_DID(CVMX_OCT_DID_FPA, pool));
	barrier();
	cvmx_write_io(newptr.u64, num_cache_lines);
}
static inline void cvmx_fpa_free(void *ptr, uint64_t pool,
				 uint64_t num_cache_lines)
{
	cvmx_addr_t newptr;
	newptr.u64 = cvmx_ptr_to_phys(ptr);
	newptr.sfilldidspace.didspace =
	    CVMX_ADDR_DIDSPACE(CVMX_FULL_DID(CVMX_OCT_DID_FPA, pool));
	CVMX_SYNCWS;
	cvmx_write_io(newptr.u64, num_cache_lines);
}
extern uint64_t cvmx_fpa_shutdown_pool(uint64_t pool);
uint64_t cvmx_fpa_get_block_size(uint64_t pool);
#endif  
