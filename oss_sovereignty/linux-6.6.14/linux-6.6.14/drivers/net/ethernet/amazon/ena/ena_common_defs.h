#ifndef _ENA_COMMON_H_
#define _ENA_COMMON_H_
#define ENA_COMMON_SPEC_VERSION_MAJOR        2
#define ENA_COMMON_SPEC_VERSION_MINOR        0
struct ena_common_mem_addr {
	u32 mem_addr_low;
	u16 mem_addr_high;
	u16 reserved16;
};
#endif  
