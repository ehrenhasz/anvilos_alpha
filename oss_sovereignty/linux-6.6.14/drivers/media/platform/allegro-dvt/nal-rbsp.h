 
 

#ifndef __NAL_RBSP_H__
#define __NAL_RBSP_H__

#include <linux/kernel.h>
#include <linux/types.h>

struct rbsp;

struct nal_rbsp_ops {
	int (*rbsp_bit)(struct rbsp *rbsp, int *val);
	int (*rbsp_bits)(struct rbsp *rbsp, int n, unsigned int *val);
	int (*rbsp_uev)(struct rbsp *rbsp, unsigned int *val);
	int (*rbsp_sev)(struct rbsp *rbsp, int *val);
};

 
struct rbsp {
	u8 *data;
	size_t size;
	unsigned int pos;
	unsigned int num_consecutive_zeros;
	struct nal_rbsp_ops *ops;
	int error;
};

extern struct nal_rbsp_ops write;
extern struct nal_rbsp_ops read;

void rbsp_init(struct rbsp *rbsp, void *addr, size_t size,
	       struct nal_rbsp_ops *ops);
void rbsp_unsupported(struct rbsp *rbsp);

void rbsp_bit(struct rbsp *rbsp, int *value);
void rbsp_bits(struct rbsp *rbsp, int n, int *value);
void rbsp_uev(struct rbsp *rbsp, unsigned int *value);
void rbsp_sev(struct rbsp *rbsp, int *value);

void rbsp_trailing_bits(struct rbsp *rbsp);

#endif  
