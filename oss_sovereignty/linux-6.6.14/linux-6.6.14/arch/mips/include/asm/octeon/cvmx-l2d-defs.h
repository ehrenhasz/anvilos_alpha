#ifndef __CVMX_L2D_DEFS_H__
#define __CVMX_L2D_DEFS_H__
#define CVMX_L2D_ERR	(CVMX_ADD_IO_SEG(0x0001180080000010ull))
#define CVMX_L2D_FUS3	(CVMX_ADD_IO_SEG(0x00011800800007B8ull))
union cvmx_l2d_err {
	uint64_t u64;
	struct cvmx_l2d_err_s {
		__BITFIELD_FIELD(uint64_t reserved_6_63:58,
		__BITFIELD_FIELD(uint64_t bmhclsel:1,
		__BITFIELD_FIELD(uint64_t ded_err:1,
		__BITFIELD_FIELD(uint64_t sec_err:1,
		__BITFIELD_FIELD(uint64_t ded_intena:1,
		__BITFIELD_FIELD(uint64_t sec_intena:1,
		__BITFIELD_FIELD(uint64_t ecc_ena:1,
		;)))))))
	} s;
};
union cvmx_l2d_fus3 {
	uint64_t u64;
	struct cvmx_l2d_fus3_s {
		__BITFIELD_FIELD(uint64_t reserved_40_63:24,
		__BITFIELD_FIELD(uint64_t ema_ctl:3,
		__BITFIELD_FIELD(uint64_t reserved_34_36:3,
		__BITFIELD_FIELD(uint64_t q3fus:34,
		;))))
	} s;
};
#endif
