#ifndef __CVMX_PACKET_H__
#define __CVMX_PACKET_H__
union cvmx_buf_ptr {
	void *ptr;
	uint64_t u64;
	struct {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t i:1;
		uint64_t back:4;
		uint64_t pool:3;
		uint64_t size:16;
		uint64_t addr:40;
#else
	        uint64_t addr:40;
	        uint64_t size:16;
	        uint64_t pool:3;
	        uint64_t back:4;
	        uint64_t i:1;
#endif
	} s;
};
#endif  
