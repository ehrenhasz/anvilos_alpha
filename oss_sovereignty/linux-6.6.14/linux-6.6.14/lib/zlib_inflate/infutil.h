#ifndef _INFUTIL_H
#define _INFUTIL_H
#include <linux/zlib.h>
#ifdef CONFIG_ZLIB_DFLTCC
#include "../zlib_dfltcc/dfltcc.h"
#include <asm/page.h>
#endif
struct inflate_workspace {
	struct inflate_state inflate_state;
#ifdef CONFIG_ZLIB_DFLTCC
	struct dfltcc_state dfltcc_state;
	unsigned char working_window[(1 << MAX_WBITS) + PAGE_SIZE];
#else
	unsigned char working_window[(1 << MAX_WBITS)];
#endif
};
#ifdef CONFIG_ZLIB_DFLTCC
static_assert(offsetof(struct inflate_workspace, dfltcc_state) % 8 == 0);
#endif
#define WS(strm) ((struct inflate_workspace *)(strm->workspace))
#endif
