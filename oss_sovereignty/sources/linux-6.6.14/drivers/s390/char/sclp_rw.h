


#ifndef __SCLP_RW_H__
#define __SCLP_RW_H__

#include <linux/list.h>

struct mto {
	u16 length;
	u16 type;
	u16 line_type_flags;
	u8  alarm_control;
	u8  _reserved[3];
} __attribute__((packed));

struct go {
	u16 length;
	u16 type;
	u32 domid;
	u8  hhmmss_time[8];
	u8  th_time[3];
	u8  reserved_0;
	u8  dddyyyy_date[7];
	u8  _reserved_1;
	u16 general_msg_flags;
	u8  _reserved_2[10];
	u8  originating_system_name[8];
	u8  job_guest_name[8];
} __attribute__((packed));

struct mdb_header {
	u16 length;
	u16 type;
	u32 tag;
	u32 revision_code;
} __attribute__((packed));

struct mdb {
	struct mdb_header header;
	struct go go;
	struct mto mto;
} __attribute__((packed));

struct msg_buf {
	struct evbuf_header header;
	struct mdb mdb;
} __attribute__((packed));


#define NR_EMPTY_MSG_PER_SCCB ((PAGE_SIZE - sizeof(struct sclp_buffer) - \
			sizeof(struct sccb_header)) / sizeof(struct msg_buf))


struct sclp_buffer {
	struct list_head list;		
	struct sclp_req request;
	void *sccb;
	struct msg_buf *current_msg;
	char *current_line;
	int current_length;
	int retry_count;
	
	unsigned short columns;
	unsigned short htab;
	
	unsigned int char_sum;		
	unsigned int messages;		
	
	void (*callback)(struct sclp_buffer *, int);
};

int sclp_rw_init(void);
struct sclp_buffer *sclp_make_buffer(void *, unsigned short, unsigned short);
void *sclp_unmake_buffer(struct sclp_buffer *);
int sclp_buffer_space(struct sclp_buffer *);
int sclp_write(struct sclp_buffer *buffer, const unsigned char *, int);
int sclp_emit_buffer(struct sclp_buffer *,void (*)(struct sclp_buffer *,int));
unsigned int sclp_chars_in_buffer(struct sclp_buffer *);

#endif	
