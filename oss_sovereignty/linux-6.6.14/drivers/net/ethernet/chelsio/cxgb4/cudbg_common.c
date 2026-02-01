
 

#include "cxgb4.h"
#include "cudbg_if.h"
#include "cudbg_lib_common.h"

int cudbg_get_buff(struct cudbg_init *pdbg_init,
		   struct cudbg_buffer *pdbg_buff, u32 size,
		   struct cudbg_buffer *pin_buff)
{
	u32 offset;

	offset = pdbg_buff->offset;
	if (offset + size > pdbg_buff->size)
		return CUDBG_STATUS_NO_MEM;

	if (pdbg_init->compress_type != CUDBG_COMPRESSION_NONE) {
		if (size > pdbg_init->compress_buff_size)
			return CUDBG_STATUS_NO_MEM;

		pin_buff->data = (char *)pdbg_init->compress_buff;
		pin_buff->offset = 0;
		pin_buff->size = size;
		return 0;
	}

	pin_buff->data = (char *)pdbg_buff->data + offset;
	pin_buff->offset = offset;
	pin_buff->size = size;
	return 0;
}

void cudbg_put_buff(struct cudbg_init *pdbg_init,
		    struct cudbg_buffer *pin_buff)
{
	 
	if (pdbg_init->compress_type != CUDBG_COMPRESSION_NONE)
		memset(pdbg_init->compress_buff, 0,
		       pdbg_init->compress_buff_size);

	pin_buff->data = NULL;
	pin_buff->offset = 0;
	pin_buff->size = 0;
}

void cudbg_update_buff(struct cudbg_buffer *pin_buff,
		       struct cudbg_buffer *pout_buff)
{
	 
	pout_buff->offset += pin_buff->size;
}
