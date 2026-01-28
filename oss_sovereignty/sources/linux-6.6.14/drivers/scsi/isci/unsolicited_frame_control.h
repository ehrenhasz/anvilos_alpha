

#ifndef _SCIC_SDS_UNSOLICITED_FRAME_CONTROL_H_
#define _SCIC_SDS_UNSOLICITED_FRAME_CONTROL_H_

#include "isci.h"

#define SCU_UNSOLICITED_FRAME_HEADER_DATA_DWORDS 15


struct scu_unsolicited_frame_header {
	
	u32 iit_exists:1;

	
	u32 protocol_type:3;

	
	u32 is_address_frame:1;

	
	u32 connection_rate:4;

	u32 reserved:23;

	
	u32 data[SCU_UNSOLICITED_FRAME_HEADER_DATA_DWORDS];

};




enum unsolicited_frame_state {
	
	UNSOLICITED_FRAME_EMPTY,

	
	UNSOLICITED_FRAME_IN_USE,

	
	UNSOLICITED_FRAME_RELEASED,

	UNSOLICITED_FRAME_MAX_STATES
};


struct sci_unsolicited_frame {
	
	enum unsolicited_frame_state state;

	
	struct scu_unsolicited_frame_header *header;

	
	void *buffer;

};


struct sci_uf_header_array {
	
	struct scu_unsolicited_frame_header *array;

	
	dma_addr_t physical_address;

};


struct sci_uf_buffer_array {
	
	struct sci_unsolicited_frame array[SCU_MAX_UNSOLICITED_FRAMES];

	
	dma_addr_t physical_address;
};


struct sci_uf_address_table_array {
	
	u64 *array;

	
	dma_addr_t physical_address;

};


struct sci_unsolicited_frame_control {
	
	u32 get;

	
	struct sci_uf_header_array headers;

	
	struct sci_uf_buffer_array buffers;

	
	struct sci_uf_address_table_array address_table;

};

#define SCI_UFI_BUF_SIZE (SCU_MAX_UNSOLICITED_FRAMES * SCU_UNSOLICITED_FRAME_BUFFER_SIZE)
#define SCI_UFI_HDR_SIZE (SCU_MAX_UNSOLICITED_FRAMES * sizeof(struct scu_unsolicited_frame_header))
#define SCI_UFI_TOTAL_SIZE (SCI_UFI_BUF_SIZE + SCI_UFI_HDR_SIZE + SCU_MAX_UNSOLICITED_FRAMES * sizeof(u64))

struct isci_host;

void sci_unsolicited_frame_control_construct(struct isci_host *ihost);

enum sci_status sci_unsolicited_frame_control_get_header(
	struct sci_unsolicited_frame_control *uf_control,
	u32 frame_index,
	void **frame_header);

enum sci_status sci_unsolicited_frame_control_get_buffer(
	struct sci_unsolicited_frame_control *uf_control,
	u32 frame_index,
	void **frame_buffer);

bool sci_unsolicited_frame_control_release_frame(
	struct sci_unsolicited_frame_control *uf_control,
	u32 frame_index);

#endif 
