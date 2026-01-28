
#ifndef LIBFDT_H
#define LIBFDT_H


#include "libfdt_env.h"
#include "fdt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FDT_FIRST_SUPPORTED_VERSION	0x02
#define FDT_LAST_COMPATIBLE_VERSION 0x10
#define FDT_LAST_SUPPORTED_VERSION	0x11


#define FDT_ERR_NOTFOUND	1
	
#define FDT_ERR_EXISTS		2
	
#define FDT_ERR_NOSPACE		3
	


#define FDT_ERR_BADOFFSET	4
	
#define FDT_ERR_BADPATH		5
	
#define FDT_ERR_BADPHANDLE	6
	
#define FDT_ERR_BADSTATE	7
	


#define FDT_ERR_TRUNCATED	8
	
#define FDT_ERR_BADMAGIC	9
	
#define FDT_ERR_BADVERSION	10
	
#define FDT_ERR_BADSTRUCTURE	11
	
#define FDT_ERR_BADLAYOUT	12
	


#define FDT_ERR_INTERNAL	13
	


#define FDT_ERR_BADNCELLS	14
	

#define FDT_ERR_BADVALUE	15
	

#define FDT_ERR_BADOVERLAY	16
	

#define FDT_ERR_NOPHANDLES	17
	

#define FDT_ERR_BADFLAGS	18
	

#define FDT_ERR_ALIGNMENT	19
	

#define FDT_ERR_MAX		19


#define FDT_MAX_PHANDLE 0xfffffffe
	





#ifndef SWIG 
const void *fdt_offset_ptr(const void *fdt, int offset, unsigned int checklen);
#endif
static inline void *fdt_offset_ptr_w(void *fdt, int offset, int checklen)
{
	return (void *)(uintptr_t)fdt_offset_ptr(fdt, offset, checklen);
}

uint32_t fdt_next_tag(const void *fdt, int offset, int *nextoffset);


static inline uint16_t fdt16_ld(const fdt16_t *p)
{
	const uint8_t *bp = (const uint8_t *)p;

	return ((uint16_t)bp[0] << 8) | bp[1];
}

static inline uint32_t fdt32_ld(const fdt32_t *p)
{
	const uint8_t *bp = (const uint8_t *)p;

	return ((uint32_t)bp[0] << 24)
		| ((uint32_t)bp[1] << 16)
		| ((uint32_t)bp[2] << 8)
		| bp[3];
}

static inline void fdt32_st(void *property, uint32_t value)
{
	uint8_t *bp = (uint8_t *)property;

	bp[0] = value >> 24;
	bp[1] = (value >> 16) & 0xff;
	bp[2] = (value >> 8) & 0xff;
	bp[3] = value & 0xff;
}

static inline uint64_t fdt64_ld(const fdt64_t *p)
{
	const uint8_t *bp = (const uint8_t *)p;

	return ((uint64_t)bp[0] << 56)
		| ((uint64_t)bp[1] << 48)
		| ((uint64_t)bp[2] << 40)
		| ((uint64_t)bp[3] << 32)
		| ((uint64_t)bp[4] << 24)
		| ((uint64_t)bp[5] << 16)
		| ((uint64_t)bp[6] << 8)
		| bp[7];
}

static inline void fdt64_st(void *property, uint64_t value)
{
	uint8_t *bp = (uint8_t *)property;

	bp[0] = value >> 56;
	bp[1] = (value >> 48) & 0xff;
	bp[2] = (value >> 40) & 0xff;
	bp[3] = (value >> 32) & 0xff;
	bp[4] = (value >> 24) & 0xff;
	bp[5] = (value >> 16) & 0xff;
	bp[6] = (value >> 8) & 0xff;
	bp[7] = value & 0xff;
}





int fdt_next_node(const void *fdt, int offset, int *depth);


int fdt_first_subnode(const void *fdt, int offset);


int fdt_next_subnode(const void *fdt, int offset);


#define fdt_for_each_subnode(node, fdt, parent)		\
	for (node = fdt_first_subnode(fdt, parent);	\
	     node >= 0;					\
	     node = fdt_next_subnode(fdt, node))




#define fdt_get_header(fdt, field) \
	(fdt32_ld(&((const struct fdt_header *)(fdt))->field))
#define fdt_magic(fdt)			(fdt_get_header(fdt, magic))
#define fdt_totalsize(fdt)		(fdt_get_header(fdt, totalsize))
#define fdt_off_dt_struct(fdt)		(fdt_get_header(fdt, off_dt_struct))
#define fdt_off_dt_strings(fdt)		(fdt_get_header(fdt, off_dt_strings))
#define fdt_off_mem_rsvmap(fdt)		(fdt_get_header(fdt, off_mem_rsvmap))
#define fdt_version(fdt)		(fdt_get_header(fdt, version))
#define fdt_last_comp_version(fdt)	(fdt_get_header(fdt, last_comp_version))
#define fdt_boot_cpuid_phys(fdt)	(fdt_get_header(fdt, boot_cpuid_phys))
#define fdt_size_dt_strings(fdt)	(fdt_get_header(fdt, size_dt_strings))
#define fdt_size_dt_struct(fdt)		(fdt_get_header(fdt, size_dt_struct))

#define fdt_set_hdr_(name) \
	static inline void fdt_set_##name(void *fdt, uint32_t val) \
	{ \
		struct fdt_header *fdth = (struct fdt_header *)fdt; \
		fdth->name = cpu_to_fdt32(val); \
	}
fdt_set_hdr_(magic);
fdt_set_hdr_(totalsize);
fdt_set_hdr_(off_dt_struct);
fdt_set_hdr_(off_dt_strings);
fdt_set_hdr_(off_mem_rsvmap);
fdt_set_hdr_(version);
fdt_set_hdr_(last_comp_version);
fdt_set_hdr_(boot_cpuid_phys);
fdt_set_hdr_(size_dt_strings);
fdt_set_hdr_(size_dt_struct);
#undef fdt_set_hdr_


size_t fdt_header_size(const void *fdt);


size_t fdt_header_size_(uint32_t version);


int fdt_check_header(const void *fdt);


int fdt_move(const void *fdt, void *buf, int bufsize);





int fdt_check_full(const void *fdt, size_t bufsize);


const char *fdt_get_string(const void *fdt, int stroffset, int *lenp);


const char *fdt_string(const void *fdt, int stroffset);


int fdt_find_max_phandle(const void *fdt, uint32_t *phandle);


static inline uint32_t fdt_get_max_phandle(const void *fdt)
{
	uint32_t phandle;
	int err;

	err = fdt_find_max_phandle(fdt, &phandle);
	if (err < 0)
		return (uint32_t)-1;

	return phandle;
}


int fdt_generate_phandle(const void *fdt, uint32_t *phandle);


int fdt_num_mem_rsv(const void *fdt);


int fdt_get_mem_rsv(const void *fdt, int n, uint64_t *address, uint64_t *size);


#ifndef SWIG 
int fdt_subnode_offset_namelen(const void *fdt, int parentoffset,
			       const char *name, int namelen);
#endif

int fdt_subnode_offset(const void *fdt, int parentoffset, const char *name);


#ifndef SWIG 
int fdt_path_offset_namelen(const void *fdt, const char *path, int namelen);
#endif


int fdt_path_offset(const void *fdt, const char *path);


const char *fdt_get_name(const void *fdt, int nodeoffset, int *lenp);


int fdt_first_property_offset(const void *fdt, int nodeoffset);


int fdt_next_property_offset(const void *fdt, int offset);


#define fdt_for_each_property_offset(property, fdt, node)	\
	for (property = fdt_first_property_offset(fdt, node);	\
	     property >= 0;					\
	     property = fdt_next_property_offset(fdt, property))


const struct fdt_property *fdt_get_property_by_offset(const void *fdt,
						      int offset,
						      int *lenp);
static inline struct fdt_property *fdt_get_property_by_offset_w(void *fdt,
								int offset,
								int *lenp)
{
	return (struct fdt_property *)(uintptr_t)
		fdt_get_property_by_offset(fdt, offset, lenp);
}


#ifndef SWIG 
const struct fdt_property *fdt_get_property_namelen(const void *fdt,
						    int nodeoffset,
						    const char *name,
						    int namelen, int *lenp);
#endif


const struct fdt_property *fdt_get_property(const void *fdt, int nodeoffset,
					    const char *name, int *lenp);
static inline struct fdt_property *fdt_get_property_w(void *fdt, int nodeoffset,
						      const char *name,
						      int *lenp)
{
	return (struct fdt_property *)(uintptr_t)
		fdt_get_property(fdt, nodeoffset, name, lenp);
}


#ifndef SWIG 
const void *fdt_getprop_by_offset(const void *fdt, int offset,
				  const char **namep, int *lenp);
#endif


#ifndef SWIG 
const void *fdt_getprop_namelen(const void *fdt, int nodeoffset,
				const char *name, int namelen, int *lenp);
static inline void *fdt_getprop_namelen_w(void *fdt, int nodeoffset,
					  const char *name, int namelen,
					  int *lenp)
{
	return (void *)(uintptr_t)fdt_getprop_namelen(fdt, nodeoffset, name,
						      namelen, lenp);
}
#endif


const void *fdt_getprop(const void *fdt, int nodeoffset,
			const char *name, int *lenp);
static inline void *fdt_getprop_w(void *fdt, int nodeoffset,
				  const char *name, int *lenp)
{
	return (void *)(uintptr_t)fdt_getprop(fdt, nodeoffset, name, lenp);
}


uint32_t fdt_get_phandle(const void *fdt, int nodeoffset);


#ifndef SWIG 
const char *fdt_get_alias_namelen(const void *fdt,
				  const char *name, int namelen);
#endif


const char *fdt_get_alias(const void *fdt, const char *name);


int fdt_get_path(const void *fdt, int nodeoffset, char *buf, int buflen);


int fdt_supernode_atdepth_offset(const void *fdt, int nodeoffset,
				 int supernodedepth, int *nodedepth);


int fdt_node_depth(const void *fdt, int nodeoffset);


int fdt_parent_offset(const void *fdt, int nodeoffset);


int fdt_node_offset_by_prop_value(const void *fdt, int startoffset,
				  const char *propname,
				  const void *propval, int proplen);


int fdt_node_offset_by_phandle(const void *fdt, uint32_t phandle);


int fdt_node_check_compatible(const void *fdt, int nodeoffset,
			      const char *compatible);


int fdt_node_offset_by_compatible(const void *fdt, int startoffset,
				  const char *compatible);


int fdt_stringlist_contains(const char *strlist, int listlen, const char *str);


int fdt_stringlist_count(const void *fdt, int nodeoffset, const char *property);


int fdt_stringlist_search(const void *fdt, int nodeoffset, const char *property,
			  const char *string);


const char *fdt_stringlist_get(const void *fdt, int nodeoffset,
			       const char *property, int index,
			       int *lenp);






#define FDT_MAX_NCELLS		4


int fdt_address_cells(const void *fdt, int nodeoffset);


int fdt_size_cells(const void *fdt, int nodeoffset);







#ifndef SWIG 
int fdt_setprop_inplace_namelen_partial(void *fdt, int nodeoffset,
					const char *name, int namelen,
					uint32_t idx, const void *val,
					int len);
#endif


#ifndef SWIG 
int fdt_setprop_inplace(void *fdt, int nodeoffset, const char *name,
			const void *val, int len);
#endif


static inline int fdt_setprop_inplace_u32(void *fdt, int nodeoffset,
					  const char *name, uint32_t val)
{
	fdt32_t tmp = cpu_to_fdt32(val);
	return fdt_setprop_inplace(fdt, nodeoffset, name, &tmp, sizeof(tmp));
}


static inline int fdt_setprop_inplace_u64(void *fdt, int nodeoffset,
					  const char *name, uint64_t val)
{
	fdt64_t tmp = cpu_to_fdt64(val);
	return fdt_setprop_inplace(fdt, nodeoffset, name, &tmp, sizeof(tmp));
}


static inline int fdt_setprop_inplace_cell(void *fdt, int nodeoffset,
					   const char *name, uint32_t val)
{
	return fdt_setprop_inplace_u32(fdt, nodeoffset, name, val);
}


int fdt_nop_property(void *fdt, int nodeoffset, const char *name);


int fdt_nop_node(void *fdt, int nodeoffset);






#define FDT_CREATE_FLAG_NO_NAME_DEDUP 0x1
	

#define FDT_CREATE_FLAGS_ALL	(FDT_CREATE_FLAG_NO_NAME_DEDUP)


int fdt_create_with_flags(void *buf, int bufsize, uint32_t flags);


int fdt_create(void *buf, int bufsize);

int fdt_resize(void *fdt, void *buf, int bufsize);
int fdt_add_reservemap_entry(void *fdt, uint64_t addr, uint64_t size);
int fdt_finish_reservemap(void *fdt);
int fdt_begin_node(void *fdt, const char *name);
int fdt_property(void *fdt, const char *name, const void *val, int len);
static inline int fdt_property_u32(void *fdt, const char *name, uint32_t val)
{
	fdt32_t tmp = cpu_to_fdt32(val);
	return fdt_property(fdt, name, &tmp, sizeof(tmp));
}
static inline int fdt_property_u64(void *fdt, const char *name, uint64_t val)
{
	fdt64_t tmp = cpu_to_fdt64(val);
	return fdt_property(fdt, name, &tmp, sizeof(tmp));
}

#ifndef SWIG 
static inline int fdt_property_cell(void *fdt, const char *name, uint32_t val)
{
	return fdt_property_u32(fdt, name, val);
}
#endif


int fdt_property_placeholder(void *fdt, const char *name, int len, void **valp);

#define fdt_property_string(fdt, name, str) \
	fdt_property(fdt, name, str, strlen(str)+1)
int fdt_end_node(void *fdt);
int fdt_finish(void *fdt);





int fdt_create_empty_tree(void *buf, int bufsize);
int fdt_open_into(const void *fdt, void *buf, int bufsize);
int fdt_pack(void *fdt);


int fdt_add_mem_rsv(void *fdt, uint64_t address, uint64_t size);


int fdt_del_mem_rsv(void *fdt, int n);


int fdt_set_name(void *fdt, int nodeoffset, const char *name);


int fdt_setprop(void *fdt, int nodeoffset, const char *name,
		const void *val, int len);


int fdt_setprop_placeholder(void *fdt, int nodeoffset, const char *name,
			    int len, void **prop_data);


static inline int fdt_setprop_u32(void *fdt, int nodeoffset, const char *name,
				  uint32_t val)
{
	fdt32_t tmp = cpu_to_fdt32(val);
	return fdt_setprop(fdt, nodeoffset, name, &tmp, sizeof(tmp));
}


static inline int fdt_setprop_u64(void *fdt, int nodeoffset, const char *name,
				  uint64_t val)
{
	fdt64_t tmp = cpu_to_fdt64(val);
	return fdt_setprop(fdt, nodeoffset, name, &tmp, sizeof(tmp));
}


static inline int fdt_setprop_cell(void *fdt, int nodeoffset, const char *name,
				   uint32_t val)
{
	return fdt_setprop_u32(fdt, nodeoffset, name, val);
}


#define fdt_setprop_string(fdt, nodeoffset, name, str) \
	fdt_setprop((fdt), (nodeoffset), (name), (str), strlen(str)+1)



#define fdt_setprop_empty(fdt, nodeoffset, name) \
	fdt_setprop((fdt), (nodeoffset), (name), NULL, 0)


int fdt_appendprop(void *fdt, int nodeoffset, const char *name,
		   const void *val, int len);


static inline int fdt_appendprop_u32(void *fdt, int nodeoffset,
				     const char *name, uint32_t val)
{
	fdt32_t tmp = cpu_to_fdt32(val);
	return fdt_appendprop(fdt, nodeoffset, name, &tmp, sizeof(tmp));
}


static inline int fdt_appendprop_u64(void *fdt, int nodeoffset,
				     const char *name, uint64_t val)
{
	fdt64_t tmp = cpu_to_fdt64(val);
	return fdt_appendprop(fdt, nodeoffset, name, &tmp, sizeof(tmp));
}


static inline int fdt_appendprop_cell(void *fdt, int nodeoffset,
				      const char *name, uint32_t val)
{
	return fdt_appendprop_u32(fdt, nodeoffset, name, val);
}


#define fdt_appendprop_string(fdt, nodeoffset, name, str) \
	fdt_appendprop((fdt), (nodeoffset), (name), (str), strlen(str)+1)


int fdt_appendprop_addrrange(void *fdt, int parent, int nodeoffset,
			     const char *name, uint64_t addr, uint64_t size);


int fdt_delprop(void *fdt, int nodeoffset, const char *name);


#ifndef SWIG 
int fdt_add_subnode_namelen(void *fdt, int parentoffset,
			    const char *name, int namelen);
#endif


int fdt_add_subnode(void *fdt, int parentoffset, const char *name);


int fdt_del_node(void *fdt, int nodeoffset);


int fdt_overlay_apply(void *fdt, void *fdto);


int fdt_overlay_target_offset(const void *fdt, const void *fdto,
			      int fragment_offset, char const **pathp);





const char *fdt_strerror(int errval);

#ifdef __cplusplus
}
#endif

#endif 
