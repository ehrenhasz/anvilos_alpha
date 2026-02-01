







#include <linux/types.h>

#define CREATE_TRACE_POINTS
#include "trace.h"

#define BYTES_PER_LINE 16
#define MAX_CHUNK_SIZE ((PAGE_SIZE - 150)  	\
			/ (2 * BYTES_PER_LINE + 4)  	\
			* BYTES_PER_LINE)

void trace_avs_msg_payload(const void *data, size_t size)
{
	size_t remaining = size;
	size_t offset = 0;

	while (remaining > 0) {
		u32 chunk;

		chunk = min_t(size_t, remaining, MAX_CHUNK_SIZE);
		trace_avs_ipc_msg_payload(data, chunk, offset, size);

		remaining -= chunk;
		offset += chunk;
	}
}
