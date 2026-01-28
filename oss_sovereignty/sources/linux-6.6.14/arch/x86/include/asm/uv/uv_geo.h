

#ifndef _ASM_UV_GEO_H
#define _ASM_UV_GEO_H




#define GEOID_SIZE	8


struct geo_common_s {
	unsigned char type;		
	unsigned char blade;
	unsigned char slot;		
	unsigned char upos;
	unsigned char rack;
};


struct geo_node_s {
	struct geo_common_s common;		
};

struct geo_rtr_s {
	struct geo_common_s common;		
};

struct geo_iocntl_s {
	struct geo_common_s common;		
};

struct geo_pcicard_s {
	struct geo_iocntl_s common;
	char bus;				
	char slot;				
};


struct geo_cpu_s {
	struct geo_node_s node;
	unsigned char	socket:4,	
			thread:4;
	unsigned char	core;
};

struct geo_mem_s {
	struct geo_node_s node;
	char membus;			
	char memslot;			
};

union geoid_u {
	struct geo_common_s common;
	struct geo_node_s node;
	struct geo_iocntl_s iocntl;
	struct geo_pcicard_s pcicard;
	struct geo_rtr_s rtr;
	struct geo_cpu_s cpu;
	struct geo_mem_s mem;
	char padsize[GEOID_SIZE];
};



#define GEO_MAX_LEN	48

#define GEO_TYPE_INVALID	0
#define GEO_TYPE_MODULE		1
#define GEO_TYPE_NODE		2
#define GEO_TYPE_RTR		3
#define GEO_TYPE_IOCNTL		4
#define GEO_TYPE_IOCARD		5
#define GEO_TYPE_CPU		6
#define GEO_TYPE_MEM		7
#define GEO_TYPE_MAX		(GEO_TYPE_MEM+1)

static inline int geo_rack(union geoid_u g)
{
	return (g.common.type == GEO_TYPE_INVALID) ?
		-1 : g.common.rack;
}

static inline int geo_slot(union geoid_u g)
{
	return (g.common.type == GEO_TYPE_INVALID) ?
		-1 : g.common.upos;
}

static inline int geo_blade(union geoid_u g)
{
	return (g.common.type == GEO_TYPE_INVALID) ?
		-1 : g.common.blade * 2 + g.common.slot;
}

#endif 
