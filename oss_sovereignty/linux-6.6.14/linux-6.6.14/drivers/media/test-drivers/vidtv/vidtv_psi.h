#ifndef VIDTV_PSI_H
#define VIDTV_PSI_H
#include <linux/types.h>
#define PAT_LEN_UNTIL_LAST_SECTION_NUMBER 5
#define PMT_LEN_UNTIL_PROGRAM_INFO_LENGTH 9
#define SDT_LEN_UNTIL_RESERVED_FOR_FUTURE_USE 8
#define NIT_LEN_UNTIL_NETWORK_DESCRIPTOR_LEN 7
#define EIT_LEN_UNTIL_LAST_TABLE_ID 11
#define MAX_SECTION_LEN 1021
#define EIT_MAX_SECTION_LEN 4093  
#define VIDTV_PAT_PID 0  
#define VIDTV_SDT_PID 0x0011  
#define VIDTV_NIT_PID 0x0010  
#define VIDTV_EIT_PID 0x0012  
enum vidtv_psi_descriptors {
	REGISTRATION_DESCRIPTOR	= 0x05,  
	NETWORK_NAME_DESCRIPTOR = 0x40,  
	SERVICE_LIST_DESCRIPTOR = 0x41,  
	SERVICE_DESCRIPTOR = 0x48,  
	SHORT_EVENT_DESCRIPTOR = 0x4d,  
};
enum vidtv_psi_stream_types {
	STREAM_PRIVATE_DATA = 0x06,  
};
struct vidtv_psi_desc {
	struct vidtv_psi_desc *next;
	u8 type;
	u8 length;
	u8 data[];
} __packed;
struct vidtv_psi_desc_service {
	struct vidtv_psi_desc *next;
	u8 type;
	u8 length;
	u8 service_type;
	u8 provider_name_len;
	char *provider_name;
	u8 service_name_len;
	char *service_name;
} __packed;
struct vidtv_psi_desc_registration {
	struct vidtv_psi_desc *next;
	u8 type;
	u8 length;
	__be32 format_id;
	u8 additional_identification_info[];
} __packed;
struct vidtv_psi_desc_network_name {
	struct vidtv_psi_desc *next;
	u8 type;
	u8 length;
	char *network_name;
} __packed;
struct vidtv_psi_desc_service_list_entry {
	__be16 service_id;
	u8 service_type;
	struct vidtv_psi_desc_service_list_entry *next;
} __packed;
struct vidtv_psi_desc_service_list {
	struct vidtv_psi_desc *next;
	u8 type;
	u8 length;
	struct vidtv_psi_desc_service_list_entry *service_list;
} __packed;
struct vidtv_psi_desc_short_event {
	struct vidtv_psi_desc *next;
	u8 type;
	u8 length;
	char *iso_language_code;
	u8 event_name_len;
	char *event_name;
	u8 text_len;
	char *text;
} __packed;
struct vidtv_psi_desc_short_event
*vidtv_psi_short_event_desc_init(struct vidtv_psi_desc *head,
				 char *iso_language_code,
				 char *event_name,
				 char *text);
struct vidtv_psi_table_header {
	u8  table_id;
	__be16 bitfield;  
	__be16 id;  
	u8  current_next:1;
	u8  version:5;
	u8  one2:2;
	u8  section_id;	 
	u8  last_section;  
} __packed;
struct vidtv_psi_table_pat_program {
	__be16 service_id;
	__be16 bitfield;  
	struct vidtv_psi_table_pat_program *next;
} __packed;
struct vidtv_psi_table_pat {
	struct vidtv_psi_table_header header;
	u16 num_pat;
	u16 num_pmt;
	struct vidtv_psi_table_pat_program *program;
} __packed;
struct vidtv_psi_table_sdt_service {
	__be16 service_id;
	u8 EIT_present_following:1;
	u8 EIT_schedule:1;
	u8 reserved:6;
	__be16 bitfield;  
	struct vidtv_psi_desc *descriptor;
	struct vidtv_psi_table_sdt_service *next;
} __packed;
struct vidtv_psi_table_sdt {
	struct vidtv_psi_table_header header;
	__be16 network_id;  
	u8  reserved;
	struct vidtv_psi_table_sdt_service *service;
} __packed;
enum service_running_status {
	RUNNING = 0x4,
};
enum service_type {
	DIGITAL_TELEVISION_SERVICE = 0x1,
	DIGITAL_RADIO_SOUND_SERVICE = 0X2,
};
struct vidtv_psi_table_pmt_stream {
	u8 type;
	__be16 bitfield;  
	__be16 bitfield2;  
	struct vidtv_psi_desc *descriptor;
	struct vidtv_psi_table_pmt_stream *next;
} __packed;
struct vidtv_psi_table_pmt {
	struct vidtv_psi_table_header header;
	__be16 bitfield;  
	__be16 bitfield2;  
	struct vidtv_psi_desc *descriptor;
	struct vidtv_psi_table_pmt_stream *stream;
} __packed;
struct psi_write_args {
	void *dest_buf;
	void *from;
	size_t len;
	u32 dest_offset;
	u16 pid;
	bool new_psi_section;
	u8 *continuity_counter;
	bool is_crc;
	u32 dest_buf_sz;
	u32 *crc;
};
struct desc_write_args {
	void *dest_buf;
	u32 dest_offset;
	struct vidtv_psi_desc *desc;
	u16 pid;
	u8 *continuity_counter;
	u32 dest_buf_sz;
	u32 *crc;
};
struct crc32_write_args {
	void *dest_buf;
	u32 dest_offset;
	__be32 crc;
	u16 pid;
	u8 *continuity_counter;
	u32 dest_buf_sz;
};
struct header_write_args {
	void *dest_buf;
	u32 dest_offset;
	struct vidtv_psi_table_header *h;
	u16 pid;
	u8 *continuity_counter;
	u32 dest_buf_sz;
	u32 *crc;
};
struct vidtv_psi_desc_service *vidtv_psi_service_desc_init(struct vidtv_psi_desc *head,
							   enum service_type service_type,
							   char *service_name,
							   char *provider_name);
struct vidtv_psi_desc_registration
*vidtv_psi_registration_desc_init(struct vidtv_psi_desc *head,
				  __be32 format_id,
				  u8 *additional_ident_info,
				  u32 additional_info_len);
struct vidtv_psi_desc_network_name
*vidtv_psi_network_name_desc_init(struct vidtv_psi_desc *head, char *network_name);
struct vidtv_psi_desc_service_list
*vidtv_psi_service_list_desc_init(struct vidtv_psi_desc *head,
				  struct vidtv_psi_desc_service_list_entry *entry);
struct vidtv_psi_table_pat_program
*vidtv_psi_pat_program_init(struct vidtv_psi_table_pat_program *head,
			    u16 service_id,
			    u16 program_map_pid);
struct vidtv_psi_table_pmt_stream*
vidtv_psi_pmt_stream_init(struct vidtv_psi_table_pmt_stream *head,
			  enum vidtv_psi_stream_types stream_type,
			  u16 es_pid);
struct vidtv_psi_table_pat *vidtv_psi_pat_table_init(u16 transport_stream_id);
struct vidtv_psi_table_pmt *vidtv_psi_pmt_table_init(u16 program_number,
						     u16 pcr_pid);
struct vidtv_psi_table_sdt *vidtv_psi_sdt_table_init(u16 network_id,
						     u16 transport_stream_id);
struct vidtv_psi_table_sdt_service*
vidtv_psi_sdt_service_init(struct vidtv_psi_table_sdt_service *head,
			   u16 service_id,
			   bool eit_schedule,
			   bool eit_present_following);
void
vidtv_psi_desc_destroy(struct vidtv_psi_desc *desc);
void
vidtv_psi_pat_program_destroy(struct vidtv_psi_table_pat_program *p);
void
vidtv_psi_pat_table_destroy(struct vidtv_psi_table_pat *p);
void
vidtv_psi_pmt_stream_destroy(struct vidtv_psi_table_pmt_stream *s);
void
vidtv_psi_pmt_table_destroy(struct vidtv_psi_table_pmt *pmt);
void
vidtv_psi_sdt_table_destroy(struct vidtv_psi_table_sdt *sdt);
void
vidtv_psi_sdt_service_destroy(struct vidtv_psi_table_sdt_service *service);
void
vidtv_psi_sdt_service_assign(struct vidtv_psi_table_sdt *sdt,
			     struct vidtv_psi_table_sdt_service *service);
void vidtv_psi_desc_assign(struct vidtv_psi_desc **to,
			   struct vidtv_psi_desc *desc);
void vidtv_pmt_desc_assign(struct vidtv_psi_table_pmt *pmt,
			   struct vidtv_psi_desc **to,
			   struct vidtv_psi_desc *desc);
void vidtv_sdt_desc_assign(struct vidtv_psi_table_sdt *sdt,
			   struct vidtv_psi_desc **to,
			   struct vidtv_psi_desc *desc);
void vidtv_psi_pat_program_assign(struct vidtv_psi_table_pat *pat,
				  struct vidtv_psi_table_pat_program *p);
void vidtv_psi_pmt_stream_assign(struct vidtv_psi_table_pmt *pmt,
				 struct vidtv_psi_table_pmt_stream *s);
struct vidtv_psi_desc *vidtv_psi_desc_clone(struct vidtv_psi_desc *desc);
struct vidtv_psi_table_pmt**
vidtv_psi_pmt_create_sec_for_each_pat_entry(struct vidtv_psi_table_pat *pat, u16 pcr_pid);
u16 vidtv_psi_pmt_get_pid(struct vidtv_psi_table_pmt *section,
			  struct vidtv_psi_table_pat *pat);
void vidtv_psi_pat_table_update_sec_len(struct vidtv_psi_table_pat *pat);
void vidtv_psi_pmt_table_update_sec_len(struct vidtv_psi_table_pmt *pmt);
void vidtv_psi_sdt_table_update_sec_len(struct vidtv_psi_table_sdt *sdt);
struct vidtv_psi_pat_write_args {
	char *buf;
	u32 offset;
	struct vidtv_psi_table_pat *pat;
	u32 buf_sz;
	u8 *continuity_counter;
};
u32 vidtv_psi_pat_write_into(struct vidtv_psi_pat_write_args *args);
struct vidtv_psi_sdt_write_args {
	char *buf;
	u32 offset;
	struct vidtv_psi_table_sdt *sdt;
	u32 buf_sz;
	u8 *continuity_counter;
};
u32 vidtv_psi_sdt_write_into(struct vidtv_psi_sdt_write_args *args);
struct vidtv_psi_pmt_write_args {
	char *buf;
	u32 offset;
	struct vidtv_psi_table_pmt *pmt;
	u16 pid;
	u32 buf_sz;
	u8 *continuity_counter;
	u16 pcr_pid;
};
u32 vidtv_psi_pmt_write_into(struct vidtv_psi_pmt_write_args *args);
struct vidtv_psi_table_pmt *vidtv_psi_find_pmt_sec(struct vidtv_psi_table_pmt **pmt_sections,
						   u16 nsections,
						   u16 program_num);
u16 vidtv_psi_get_pat_program_pid(struct vidtv_psi_table_pat_program *p);
u16 vidtv_psi_pmt_stream_get_elem_pid(struct vidtv_psi_table_pmt_stream *s);
struct vidtv_psi_table_transport {
	__be16 transport_id;
	__be16 network_id;
	__be16 bitfield;  
	struct vidtv_psi_desc *descriptor;
	struct vidtv_psi_table_transport *next;
} __packed;
struct vidtv_psi_table_nit {
	struct vidtv_psi_table_header header;
	__be16 bitfield;  
	struct vidtv_psi_desc *descriptor;
	__be16 bitfield2;  
	struct vidtv_psi_table_transport *transport;
} __packed;
struct vidtv_psi_table_nit
*vidtv_psi_nit_table_init(u16 network_id,
			  u16 transport_stream_id,
			  char *network_name,
			  struct vidtv_psi_desc_service_list_entry *service_list);
struct vidtv_psi_nit_write_args {
	char *buf;
	u32 offset;
	struct vidtv_psi_table_nit *nit;
	u32 buf_sz;
	u8 *continuity_counter;
};
u32 vidtv_psi_nit_write_into(struct vidtv_psi_nit_write_args *args);
void vidtv_psi_nit_table_destroy(struct vidtv_psi_table_nit *nit);
struct vidtv_psi_table_eit_event {
	__be16 event_id;
	u8 start_time[5];
	u8 duration[3];
	__be16 bitfield;  
	struct vidtv_psi_desc *descriptor;
	struct vidtv_psi_table_eit_event *next;
} __packed;
struct vidtv_psi_table_eit {
	struct vidtv_psi_table_header header;
	__be16 transport_id;
	__be16 network_id;
	u8 last_segment;
	u8 last_table_id;
	struct vidtv_psi_table_eit_event *event;
} __packed;
struct vidtv_psi_table_eit
*vidtv_psi_eit_table_init(u16 network_id,
			  u16 transport_stream_id,
			  __be16 service_id);
struct vidtv_psi_eit_write_args {
	char *buf;
	u32 offset;
	struct vidtv_psi_table_eit *eit;
	u32 buf_sz;
	u8 *continuity_counter;
};
u32 vidtv_psi_eit_write_into(struct vidtv_psi_eit_write_args *args);
void vidtv_psi_eit_table_destroy(struct vidtv_psi_table_eit *eit);
void vidtv_psi_eit_table_update_sec_len(struct vidtv_psi_table_eit *eit);
void vidtv_psi_eit_event_assign(struct vidtv_psi_table_eit *eit,
				struct vidtv_psi_table_eit_event *e);
struct vidtv_psi_table_eit_event
*vidtv_psi_eit_event_init(struct vidtv_psi_table_eit_event *head, u16 event_id);
void vidtv_psi_eit_event_destroy(struct vidtv_psi_table_eit_event *e);
#endif  
