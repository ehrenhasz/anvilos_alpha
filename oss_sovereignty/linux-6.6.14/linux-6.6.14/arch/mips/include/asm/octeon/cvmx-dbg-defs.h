#ifndef __CVMX_DBG_DEFS_H__
#define __CVMX_DBG_DEFS_H__
#define CVMX_DBG_DATA (CVMX_ADD_IO_SEG(0x00011F00000001E8ull))
union cvmx_dbg_data {
	uint64_t u64;
	struct cvmx_dbg_data_s {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t reserved_23_63:41;
		uint64_t c_mul:5;
		uint64_t dsel_ext:1;
		uint64_t data:17;
#else
		uint64_t data:17;
		uint64_t dsel_ext:1;
		uint64_t c_mul:5;
		uint64_t reserved_23_63:41;
#endif
	} s;
	struct cvmx_dbg_data_cn30xx {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t reserved_31_63:33;
		uint64_t pll_mul:3;
		uint64_t reserved_23_27:5;
		uint64_t c_mul:5;
		uint64_t dsel_ext:1;
		uint64_t data:17;
#else
		uint64_t data:17;
		uint64_t dsel_ext:1;
		uint64_t c_mul:5;
		uint64_t reserved_23_27:5;
		uint64_t pll_mul:3;
		uint64_t reserved_31_63:33;
#endif
	} cn30xx;
	struct cvmx_dbg_data_cn38xx {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t reserved_29_63:35;
		uint64_t d_mul:4;
		uint64_t dclk_mul2:1;
		uint64_t cclk_div2:1;
		uint64_t c_mul:5;
		uint64_t dsel_ext:1;
		uint64_t data:17;
#else
		uint64_t data:17;
		uint64_t dsel_ext:1;
		uint64_t c_mul:5;
		uint64_t cclk_div2:1;
		uint64_t dclk_mul2:1;
		uint64_t d_mul:4;
		uint64_t reserved_29_63:35;
#endif
	} cn38xx;
	struct cvmx_dbg_data_cn58xx {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t reserved_29_63:35;
		uint64_t rem:6;
		uint64_t c_mul:5;
		uint64_t dsel_ext:1;
		uint64_t data:17;
#else
		uint64_t data:17;
		uint64_t dsel_ext:1;
		uint64_t c_mul:5;
		uint64_t rem:6;
		uint64_t reserved_29_63:35;
#endif
	} cn58xx;
};
#endif
