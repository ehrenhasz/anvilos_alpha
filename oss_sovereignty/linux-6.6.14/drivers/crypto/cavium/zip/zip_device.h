#ifndef __ZIP_DEVICE_H__
#define __ZIP_DEVICE_H__
#include <linux/types.h>
#include "zip_main.h"
struct sg_info {
	union zip_zptr_s *gather;
	union zip_zptr_s *scatter;
	u64 scatter_buf_size;
	u64 gather_enable;
	u64 scatter_enable;
	u32 gbuf_cnt;
	u32 sbuf_cnt;
	u8 alloc_state;
};
struct zip_state {
	union zip_inst_s zip_cmd;
	union zip_zres_s result;
	union zip_zptr_s *ctx;
	union zip_zptr_s *history;
	struct sg_info   sginfo;
};
#define ZIP_CONTEXT_SIZE          2048
#define ZIP_INFLATE_HISTORY_SIZE  32768
#define ZIP_DEFLATE_HISTORY_SIZE  32768
#endif
