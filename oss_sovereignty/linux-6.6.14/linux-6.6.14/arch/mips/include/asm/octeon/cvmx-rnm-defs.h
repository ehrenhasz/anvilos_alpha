#ifndef __CVMX_RNM_DEFS_H__
#define __CVMX_RNM_DEFS_H__
#define CVMX_RNM_BIST_STATUS (CVMX_ADD_IO_SEG(0x0001180040000008ull))
#define CVMX_RNM_CTL_STATUS (CVMX_ADD_IO_SEG(0x0001180040000000ull))
#define CVMX_RNM_EER_DBG (CVMX_ADD_IO_SEG(0x0001180040000018ull))
#define CVMX_RNM_EER_KEY (CVMX_ADD_IO_SEG(0x0001180040000010ull))
#define CVMX_RNM_SERIAL_NUM (CVMX_ADD_IO_SEG(0x0001180040000020ull))
union cvmx_rnm_bist_status {
	uint64_t u64;
	struct cvmx_rnm_bist_status_s {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t reserved_2_63:62;
		uint64_t rrc:1;
		uint64_t mem:1;
#else
		uint64_t mem:1;
		uint64_t rrc:1;
		uint64_t reserved_2_63:62;
#endif
	} s;
};
union cvmx_rnm_ctl_status {
	uint64_t u64;
	struct cvmx_rnm_ctl_status_s {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t reserved_12_63:52;
		uint64_t dis_mak:1;
		uint64_t eer_lck:1;
		uint64_t eer_val:1;
		uint64_t ent_sel:4;
		uint64_t exp_ent:1;
		uint64_t rng_rst:1;
		uint64_t rnm_rst:1;
		uint64_t rng_en:1;
		uint64_t ent_en:1;
#else
		uint64_t ent_en:1;
		uint64_t rng_en:1;
		uint64_t rnm_rst:1;
		uint64_t rng_rst:1;
		uint64_t exp_ent:1;
		uint64_t ent_sel:4;
		uint64_t eer_val:1;
		uint64_t eer_lck:1;
		uint64_t dis_mak:1;
		uint64_t reserved_12_63:52;
#endif
	} s;
	struct cvmx_rnm_ctl_status_cn30xx {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t reserved_4_63:60;
		uint64_t rng_rst:1;
		uint64_t rnm_rst:1;
		uint64_t rng_en:1;
		uint64_t ent_en:1;
#else
		uint64_t ent_en:1;
		uint64_t rng_en:1;
		uint64_t rnm_rst:1;
		uint64_t rng_rst:1;
		uint64_t reserved_4_63:60;
#endif
	} cn30xx;
	struct cvmx_rnm_ctl_status_cn50xx {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t reserved_9_63:55;
		uint64_t ent_sel:4;
		uint64_t exp_ent:1;
		uint64_t rng_rst:1;
		uint64_t rnm_rst:1;
		uint64_t rng_en:1;
		uint64_t ent_en:1;
#else
		uint64_t ent_en:1;
		uint64_t rng_en:1;
		uint64_t rnm_rst:1;
		uint64_t rng_rst:1;
		uint64_t exp_ent:1;
		uint64_t ent_sel:4;
		uint64_t reserved_9_63:55;
#endif
	} cn50xx;
	struct cvmx_rnm_ctl_status_cn63xx {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t reserved_11_63:53;
		uint64_t eer_lck:1;
		uint64_t eer_val:1;
		uint64_t ent_sel:4;
		uint64_t exp_ent:1;
		uint64_t rng_rst:1;
		uint64_t rnm_rst:1;
		uint64_t rng_en:1;
		uint64_t ent_en:1;
#else
		uint64_t ent_en:1;
		uint64_t rng_en:1;
		uint64_t rnm_rst:1;
		uint64_t rng_rst:1;
		uint64_t exp_ent:1;
		uint64_t ent_sel:4;
		uint64_t eer_val:1;
		uint64_t eer_lck:1;
		uint64_t reserved_11_63:53;
#endif
	} cn63xx;
};
union cvmx_rnm_eer_dbg {
	uint64_t u64;
	struct cvmx_rnm_eer_dbg_s {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t dat:64;
#else
		uint64_t dat:64;
#endif
	} s;
};
union cvmx_rnm_eer_key {
	uint64_t u64;
	struct cvmx_rnm_eer_key_s {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t key:64;
#else
		uint64_t key:64;
#endif
	} s;
};
union cvmx_rnm_serial_num {
	uint64_t u64;
	struct cvmx_rnm_serial_num_s {
#ifdef __BIG_ENDIAN_BITFIELD
		uint64_t dat:64;
#else
		uint64_t dat:64;
#endif
	} s;
};
#endif
