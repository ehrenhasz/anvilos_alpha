#ifndef _HPIDSPCD_H_
#define _HPIDSPCD_H_
#include "hpi_internal.h"
struct code_header {
	u32 size;
	u32 type;
	u32 adapter;
	u32 version;
	u32 checksum;
};
compile_time_assert((sizeof(struct code_header) == 20), code_header_size);
struct dsp_code {
	struct code_header header;
	u32 block_length;
	u32 word_count;
	struct dsp_code_private *pvt;
};
short hpi_dsp_code_open(
	u32 adapter, void *pci_dev,
	struct dsp_code *ps_dsp_code,
	u32 *pos_error_code);
void hpi_dsp_code_close(struct dsp_code *ps_dsp_code);
void hpi_dsp_code_rewind(struct dsp_code *ps_dsp_code);
short hpi_dsp_code_read_word(struct dsp_code *ps_dsp_code,
	u32 *pword  
	);
short hpi_dsp_code_read_block(size_t words_requested,
	struct dsp_code *ps_dsp_code,
	u32 **ppblock);
#endif
