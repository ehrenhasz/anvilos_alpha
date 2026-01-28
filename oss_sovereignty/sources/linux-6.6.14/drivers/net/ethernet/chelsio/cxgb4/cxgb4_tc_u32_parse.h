

#ifndef __CXGB4_TC_U32_PARSE_H
#define __CXGB4_TC_U32_PARSE_H

struct cxgb4_match_field {
	int off; 
	
	int (*val)(struct ch_filter_specification *f, __be32 val, __be32 mask);
};


static inline int cxgb4_fill_ipv4_tos(struct ch_filter_specification *f,
				      __be32 val, __be32 mask)
{
	f->val.tos  = (ntohl(val)  >> 16) & 0x000000FF;
	f->mask.tos = (ntohl(mask) >> 16) & 0x000000FF;

	return 0;
}

static inline int cxgb4_fill_ipv4_frag(struct ch_filter_specification *f,
				       __be32 val, __be32 mask)
{
	u32 mask_val;
	u8 frag_val;

	frag_val = (ntohl(val) >> 13) & 0x00000007;
	mask_val = ntohl(mask) & 0x0000FFFF;

	if (frag_val == 0x1 && mask_val != 0x3FFF) { 
		f->val.frag = 1;
		f->mask.frag = 1;
	} else if (frag_val == 0x2 && mask_val != 0x3FFF) { 
		f->val.frag = 0;
		f->mask.frag = 1;
	} else {
		return -EINVAL;
	}

	return 0;
}

static inline int cxgb4_fill_ipv4_proto(struct ch_filter_specification *f,
					__be32 val, __be32 mask)
{
	f->val.proto  = (ntohl(val)  >> 16) & 0x000000FF;
	f->mask.proto = (ntohl(mask) >> 16) & 0x000000FF;

	return 0;
}

static inline int cxgb4_fill_ipv4_src_ip(struct ch_filter_specification *f,
					 __be32 val, __be32 mask)
{
	memcpy(&f->val.fip[0],  &val,  sizeof(u32));
	memcpy(&f->mask.fip[0], &mask, sizeof(u32));

	return 0;
}

static inline int cxgb4_fill_ipv4_dst_ip(struct ch_filter_specification *f,
					 __be32 val, __be32 mask)
{
	memcpy(&f->val.lip[0],  &val,  sizeof(u32));
	memcpy(&f->mask.lip[0], &mask, sizeof(u32));

	return 0;
}

static const struct cxgb4_match_field cxgb4_ipv4_fields[] = {
	{ .off = 0,  .val = cxgb4_fill_ipv4_tos },
	{ .off = 4,  .val = cxgb4_fill_ipv4_frag },
	{ .off = 8,  .val = cxgb4_fill_ipv4_proto },
	{ .off = 12, .val = cxgb4_fill_ipv4_src_ip },
	{ .off = 16, .val = cxgb4_fill_ipv4_dst_ip },
	{ .val = NULL }
};


static inline int cxgb4_fill_ipv6_tos(struct ch_filter_specification *f,
				      __be32 val, __be32 mask)
{
	f->val.tos  = (ntohl(val)  >> 20) & 0x000000FF;
	f->mask.tos = (ntohl(mask) >> 20) & 0x000000FF;

	return 0;
}

static inline int cxgb4_fill_ipv6_proto(struct ch_filter_specification *f,
					__be32 val, __be32 mask)
{
	f->val.proto  = (ntohl(val)  >> 8) & 0x000000FF;
	f->mask.proto = (ntohl(mask) >> 8) & 0x000000FF;

	return 0;
}

static inline int cxgb4_fill_ipv6_src_ip0(struct ch_filter_specification *f,
					  __be32 val, __be32 mask)
{
	memcpy(&f->val.fip[0],  &val,  sizeof(u32));
	memcpy(&f->mask.fip[0], &mask, sizeof(u32));

	return 0;
}

static inline int cxgb4_fill_ipv6_src_ip1(struct ch_filter_specification *f,
					  __be32 val, __be32 mask)
{
	memcpy(&f->val.fip[4],  &val,  sizeof(u32));
	memcpy(&f->mask.fip[4], &mask, sizeof(u32));

	return 0;
}

static inline int cxgb4_fill_ipv6_src_ip2(struct ch_filter_specification *f,
					  __be32 val, __be32 mask)
{
	memcpy(&f->val.fip[8],  &val,  sizeof(u32));
	memcpy(&f->mask.fip[8], &mask, sizeof(u32));

	return 0;
}

static inline int cxgb4_fill_ipv6_src_ip3(struct ch_filter_specification *f,
					  __be32 val, __be32 mask)
{
	memcpy(&f->val.fip[12],  &val,  sizeof(u32));
	memcpy(&f->mask.fip[12], &mask, sizeof(u32));

	return 0;
}

static inline int cxgb4_fill_ipv6_dst_ip0(struct ch_filter_specification *f,
					  __be32 val, __be32 mask)
{
	memcpy(&f->val.lip[0],  &val,  sizeof(u32));
	memcpy(&f->mask.lip[0], &mask, sizeof(u32));

	return 0;
}

static inline int cxgb4_fill_ipv6_dst_ip1(struct ch_filter_specification *f,
					  __be32 val, __be32 mask)
{
	memcpy(&f->val.lip[4],  &val,  sizeof(u32));
	memcpy(&f->mask.lip[4], &mask, sizeof(u32));

	return 0;
}

static inline int cxgb4_fill_ipv6_dst_ip2(struct ch_filter_specification *f,
					  __be32 val, __be32 mask)
{
	memcpy(&f->val.lip[8],  &val,  sizeof(u32));
	memcpy(&f->mask.lip[8], &mask, sizeof(u32));

	return 0;
}

static inline int cxgb4_fill_ipv6_dst_ip3(struct ch_filter_specification *f,
					  __be32 val, __be32 mask)
{
	memcpy(&f->val.lip[12],  &val,  sizeof(u32));
	memcpy(&f->mask.lip[12], &mask, sizeof(u32));

	return 0;
}

static const struct cxgb4_match_field cxgb4_ipv6_fields[] = {
	{ .off = 0,  .val = cxgb4_fill_ipv6_tos },
	{ .off = 4,  .val = cxgb4_fill_ipv6_proto },
	{ .off = 8,  .val = cxgb4_fill_ipv6_src_ip0 },
	{ .off = 12, .val = cxgb4_fill_ipv6_src_ip1 },
	{ .off = 16, .val = cxgb4_fill_ipv6_src_ip2 },
	{ .off = 20, .val = cxgb4_fill_ipv6_src_ip3 },
	{ .off = 24, .val = cxgb4_fill_ipv6_dst_ip0 },
	{ .off = 28, .val = cxgb4_fill_ipv6_dst_ip1 },
	{ .off = 32, .val = cxgb4_fill_ipv6_dst_ip2 },
	{ .off = 36, .val = cxgb4_fill_ipv6_dst_ip3 },
	{ .val = NULL }
};


static inline int cxgb4_fill_l4_ports(struct ch_filter_specification *f,
				      __be32 val, __be32 mask)
{
	f->val.fport  = ntohl(val)  >> 16;
	f->mask.fport = ntohl(mask) >> 16;
	f->val.lport  = ntohl(val)  & 0x0000FFFF;
	f->mask.lport = ntohl(mask) & 0x0000FFFF;

	return 0;
};

static const struct cxgb4_match_field cxgb4_tcp_fields[] = {
	{ .off = 0, .val = cxgb4_fill_l4_ports },
	{ .val = NULL }
};

static const struct cxgb4_match_field cxgb4_udp_fields[] = {
	{ .off = 0, .val = cxgb4_fill_l4_ports },
	{ .val = NULL }
};

struct cxgb4_next_header {
	
	struct tc_u32_sel sel;
	struct tc_u32_key key;
	
	const struct cxgb4_match_field *jump;
};


static const struct cxgb4_next_header cxgb4_ipv4_jumps[] = {
	{
		
		.sel = {
			.off = 0,
			.offoff = 0,
			.offshift = 6,
			.offmask = cpu_to_be16(0x0f00),
		},
		.key = {
			.off = 8,
			.val = cpu_to_be32(0x00060000),
			.mask = cpu_to_be32(0x00ff0000),
		},
		.jump = cxgb4_tcp_fields,
	},
	{
		
		.sel = {
			.off = 0,
			.offoff = 0,
			.offshift = 6,
			.offmask = cpu_to_be16(0x0f00),
		},
		.key = {
			.off = 8,
			.val = cpu_to_be32(0x00110000),
			.mask = cpu_to_be32(0x00ff0000),
		},
		.jump = cxgb4_udp_fields,
	},
	{ .jump = NULL },
};


static const struct cxgb4_next_header cxgb4_ipv6_jumps[] = {
	{
		
		.sel = {
			.off = 40,
			.offoff = 0,
			.offshift = 0,
			.offmask = 0,
		},
		.key = {
			.off = 4,
			.val = cpu_to_be32(0x00000600),
			.mask = cpu_to_be32(0x0000ff00),
		},
		.jump = cxgb4_tcp_fields,
	},
	{
		
		.sel = {
			.off = 40,
			.offoff = 0,
			.offshift = 0,
			.offmask = 0,
		},
		.key = {
			.off = 4,
			.val = cpu_to_be32(0x00001100),
			.mask = cpu_to_be32(0x0000ff00),
		},
		.jump = cxgb4_udp_fields,
	},
	{ .jump = NULL },
};

struct cxgb4_link {
	const struct cxgb4_match_field *match_field;  
	struct ch_filter_specification fs; 
	u32 link_handle;         
	unsigned long *tid_map;  
};

struct cxgb4_tc_u32_table {
	unsigned int size;          
	struct cxgb4_link table[]; 
};
#endif 
