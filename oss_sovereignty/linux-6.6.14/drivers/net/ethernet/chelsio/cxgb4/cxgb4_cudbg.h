#ifndef __CXGB4_CUDBG_H__
#define __CXGB4_CUDBG_H__
#include "cudbg_if.h"
#include "cudbg_lib_common.h"
#include "cudbg_entity.h"
#include "cudbg_lib.h"
#define CUDBG_DUMP_BUFF_SIZE (32 * 1024 * 1024)  
#define CUDBG_COMPRESS_BUFF_SIZE (4 * 1024 * 1024)  
typedef int (*cudbg_collect_callback_t)(struct cudbg_init *pdbg_init,
					struct cudbg_buffer *dbg_buff,
					struct cudbg_error *cudbg_err);
struct cxgb4_collect_entity {
	enum cudbg_dbg_entity_type entity;
	cudbg_collect_callback_t collect_cb;
};
enum CXGB4_ETHTOOL_DUMP_FLAGS {
	CXGB4_ETH_DUMP_NONE = ETH_FW_DUMP_DISABLE,
	CXGB4_ETH_DUMP_MEM = (1 << 0),  
	CXGB4_ETH_DUMP_HW = (1 << 1),  
	CXGB4_ETH_DUMP_FLASH = (1 << 2),  
};
#define CXGB4_ETH_DUMP_ALL (CXGB4_ETH_DUMP_MEM | CXGB4_ETH_DUMP_HW)
u32 cxgb4_get_dump_length(struct adapter *adap, u32 flag);
int cxgb4_cudbg_collect(struct adapter *adap, void *buf, u32 *buf_size,
			u32 flag);
void cxgb4_init_ethtool_dump(struct adapter *adapter);
int cxgb4_cudbg_vmcore_add_dump(struct adapter *adap);
#endif  
