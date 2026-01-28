

#ifndef _LIBFDISK_PRIVATE_H
#define _LIBFDISK_PRIVATE_H

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid.h>

#include "c.h"
#include "libfdisk.h"

#include "list.h"
#include "debug.h"
#include <stdio.h>
#include <stdarg.h>


#define LIBFDISK_DEBUG_HELP	(1 << 0)
#define LIBFDISK_DEBUG_INIT	(1 << 1)
#define LIBFDISK_DEBUG_CXT	(1 << 2)
#define LIBFDISK_DEBUG_LABEL    (1 << 3)
#define LIBFDISK_DEBUG_ASK      (1 << 4)
#define LIBFDISK_DEBUG_PART	(1 << 6)
#define LIBFDISK_DEBUG_PARTTYPE	(1 << 7)
#define LIBFDISK_DEBUG_TAB	(1 << 8)
#define LIBFDISK_DEBUG_SCRIPT	(1 << 9)
#define LIBFDISK_DEBUG_WIPE	(1 << 10)
#define LIBFDISK_DEBUG_ITEM	(1 << 11)
#define LIBFDISK_DEBUG_GPT	(1 << 12)
#define LIBFDISK_DEBUG_ALL	0xFFFF

UL_DEBUG_DECLARE_MASK(libfdisk);
#define DBG(m, x)	__UL_DBG(libfdisk, LIBFDISK_DEBUG_, m, x)
#define ON_DBG(m, x)	__UL_DBG_CALL(libfdisk, LIBFDISK_DEBUG_, m, x)
#define DBG_FLUSH	__UL_DBG_FLUSH(libfdisk, LIBFDISK_DEBUG_)

#define UL_DEBUG_CURRENT_MASK	UL_DEBUG_MASK(libfdisk)
#include "debugobj.h"


#define LIBFDISK_TEXTDOMAIN	PACKAGE
#define UL_TEXTDOMAIN_EXPLICIT	LIBFDISK_TEXTDOMAIN
#include "nls.h"


#ifdef TEST_PROGRAM
struct fdisk_test {
	const char	*name;
	int		(*body)(struct fdisk_test *ts, int argc, char *argv[]);
	const char	*usage;
};


extern int fdisk_run_test(struct fdisk_test *tests, int argc, char *argv[]);
#endif

#define FDISK_GPT_NPARTITIONS_DEFAULT	128


struct fdisk_iter {
        struct list_head        *p;		
        struct list_head        *head;		
	int			direction;	
};

#define IS_ITER_FORWARD(_i)	((_i)->direction == FDISK_ITER_FORWARD)
#define IS_ITER_BACKWARD(_i)	((_i)->direction == FDISK_ITER_BACKWARD)

#define FDISK_ITER_INIT(itr, list) \
	do { \
		(itr)->p = IS_ITER_FORWARD(itr) ? \
				(list)->next : (list)->prev; \
		(itr)->head = (list); \
	} while(0)

#define FDISK_ITER_ITERATE(itr, res, restype, member) \
	do { \
		res = list_entry((itr)->p, restype, member); \
		(itr)->p = IS_ITER_FORWARD(itr) ? \
				(itr)->p->next : (itr)->p->prev; \
	} while(0)


struct fdisk_parttype {
	unsigned int	code;		
	char		*name;		
	char		*typestr;	

	unsigned int	flags;		
	int		refcount;	
};

enum {
	FDISK_PARTTYPE_UNKNOWN		= (1 << 1),
	FDISK_PARTTYPE_INVISIBLE	= (1 << 2),
	FDISK_PARTTYPE_ALLOCATED	= (1 << 3)
};

#define fdisk_parttype_is_invisible(_x)	((_x) && ((_x)->flags & FDISK_PARTTYPE_INVISIBLE))
#define fdisk_parttype_is_allocated(_x)	((_x) && ((_x)->flags & FDISK_PARTTYPE_ALLOCATED))


struct fdisk_shortcut {
	const char	*shortcut;	
	const char	*alias;		
	const char	*data;		

	unsigned int    deprecated : 1;
};

struct fdisk_partition {
	int		refcount;		

	size_t		partno;			
	size_t		parent_partno;		

	fdisk_sector_t	start;			
	fdisk_sector_t	size;			

	int		movestart;		
	int		resize;			

	char		*name;			
	char		*uuid;			
	char		*attrs;			
	struct fdisk_parttype	*type;		

	char		*fstype;		
	char		*fsuuid;		
	char		*fslabel;		

	struct list_head	parts;		

	
	char		start_post;		
	char		end_post;		
	char		size_post;		

	uint64_t	fsize;			
	uint64_t	bsize;
	uint64_t	cpg;

	char		*start_chs;		
	char		*end_chs;		

	unsigned int	boot;			

	unsigned int	container : 1,			
			end_follow_default : 1,		
			freespace : 1,			
			partno_follow_default : 1,	
			size_explicit : 1,		
			start_follow_default : 1,	
			fs_probed : 1,			
			used : 1,			
			wholedisk : 1;			
};

enum {
	FDISK_MOVE_NONE = 0,
	FDISK_MOVE_DOWN = -1,
	FDISK_MOVE_UP = 1
};

enum {
	FDISK_RESIZE_NONE = 0,
	FDISK_RESIZE_REDUCE = -1,
	FDISK_RESIZE_ENLARGE = 1
};

#define FDISK_INIT_UNDEF(_x)	((_x) = (__typeof__(_x)) -1)
#define FDISK_IS_UNDEF(_x)	((_x) == (__typeof__(_x)) -1)

struct fdisk_table {
	struct list_head	parts;		
	int			refcount;
	size_t			nents;		
};


struct fdisk_geometry {
	unsigned int heads;
	fdisk_sector_t sectors;
	fdisk_sector_t cylinders;
};


struct fdisk_label_operations {
	
	int (*probe)(struct fdisk_context *cxt);
	
	int (*write)(struct fdisk_context *cxt);
	
	int (*verify)(struct fdisk_context *cxt);
	
	int (*create)(struct fdisk_context *cxt);
	
	int (*locate)(struct fdisk_context *cxt, int n, const char **name,
		      uint64_t *offset, size_t *size);
	
	int (*reorder)(struct fdisk_context *cxt);
	
	int (*get_item)(struct fdisk_context *cxt, struct fdisk_labelitem *item);
	
	int (*set_id)(struct fdisk_context *cxt, const char *str);


	
	int (*add_part)(struct fdisk_context *cxt, struct fdisk_partition *pa,
						size_t *partno);
	
	int (*del_part)(struct fdisk_context *cxt, size_t partnum);

	
	int (*get_part)(struct fdisk_context *cxt, size_t n,
						struct fdisk_partition *pa);
	
	int (*set_part)(struct fdisk_context *cxt, size_t n,
						struct fdisk_partition *pa);

	
	int (*part_is_used)(struct fdisk_context *cxt, size_t partnum);

	int (*part_toggle_flag)(struct fdisk_context *cxt, size_t i, unsigned long flag);

	
	int (*reset_alignment)(struct fdisk_context *cxt);

	
	void (*free)(struct fdisk_label *lb);

	
	void (*deinit)(struct fdisk_label *lb);
};


struct fdisk_field {
	int		id;		
	const char	*name;		
	double		width;		
	int		flags;		
};


enum {
	FDISK_FIELDFL_DETAIL	= (1 << 1),	
	FDISK_FIELDFL_EYECANDY	= (1 << 2),	
	FDISK_FIELDFL_NUMBER	= (1 << 3),	
};


struct fdisk_label {
	const char		*name;		
	enum fdisk_labeltype	id;		
	struct fdisk_parttype	*parttypes;	
	size_t			nparttypes;	

	const struct fdisk_shortcut *parttype_cuts;	
	size_t			nparttype_cuts;	

	size_t			nparts_max;	
	size_t			nparts_cur;	

	int			flags;		

	struct fdisk_geometry	geom_min;	
	struct fdisk_geometry	geom_max;	

	unsigned int		changed:1,	
				disabled:1;	

	const struct fdisk_field *fields;	
	size_t			nfields;

	const struct fdisk_label_operations *op;
};



enum {
	FDISK_LABEL_FL_REQUIRE_GEOMETRY = (1 << 2),
	FDISK_LABEL_FL_INCHARS_PARTNO   = (1 << 3)
};


extern struct fdisk_label *fdisk_new_gpt_label(struct fdisk_context *cxt);
extern struct fdisk_label *fdisk_new_dos_label(struct fdisk_context *cxt);
extern struct fdisk_label *fdisk_new_bsd_label(struct fdisk_context *cxt);
extern struct fdisk_label *fdisk_new_sgi_label(struct fdisk_context *cxt);
extern struct fdisk_label *fdisk_new_sun_label(struct fdisk_context *cxt);


struct ask_menuitem {
	char	key;
	const char	*name;
	const char	*desc;

	struct ask_menuitem *next;
};


struct fdisk_ask {
	int		type;		
	char		*query;

	int		refcount;

	union {
		
		struct ask_number {
			uint64_t	hig;		
			uint64_t	low;		
			uint64_t	dfl;		
			uint64_t	result;
			uint64_t	base;		
			uint64_t	unit;		
			const char	*range;		
			unsigned int	relative :1,
					inchars  :1,
					wrap_negative	:1;
		} num;
		
		struct ask_print {
			const char	*mesg;
			int		errnum;		
		} print;
		
		struct ask_yesno {
			int		result;		
		} yesno;
		
		struct ask_string {
			char		*result;	
		} str;
		
		struct ask_menu {
			int		dfl;		
			int		result;
			struct ask_menuitem *first;
		} menu;
	} data;
};

struct fdisk_context {
	int dev_fd;         
	char *dev_path;     
	char *dev_model;    
	struct stat dev_st; 

	int refcount;

	unsigned char *firstsector; 
	unsigned long firstsector_bufsz;


	
	unsigned long io_size;		
	unsigned long optimal_io_size;	
	unsigned long min_io_size;	
	unsigned long phy_sector_size;	
	unsigned long sector_size;	
	unsigned long alignment_offset;

	unsigned int readonly : 1,		
		     display_in_cyl_units : 1,	
		     display_details : 1,	
		     protect_bootbits : 1,	
		     pt_collision : 1,		
		     no_disalogs : 1,		
		     dev_model_probed : 1,	
		     is_priv : 1,		
		     is_excl : 1,		
		     listonly : 1;		

	char *collision;			
	struct list_head wipes;			

	int sizeunit;				

	
	unsigned long grain;		
	fdisk_sector_t first_lba;		
	fdisk_sector_t last_lba;		

	
	fdisk_sector_t total_sectors;	
	struct fdisk_geometry geom;

	
	struct fdisk_geometry user_geom;
	unsigned long user_pyh_sector;
	unsigned long user_log_sector;
	unsigned long user_grain;

	struct fdisk_label *label;	

	size_t nlabels;			
	struct fdisk_label *labels[8];	

	int	(*ask_cb)(struct fdisk_context *, struct fdisk_ask *, void *);	
	void	*ask_data;		

	struct fdisk_context	*parent;	
	struct fdisk_script	*script;	
};


enum {
	FDISK_DIFF_UNCHANGED = 0,
	FDISK_DIFF_REMOVED,
	FDISK_DIFF_ADDED,
	FDISK_DIFF_MOVED,
	FDISK_DIFF_RESIZED
};
extern int fdisk_diff_tables(struct fdisk_table *a, struct fdisk_table *b,
				struct fdisk_iter *itr,
				struct fdisk_partition **res, int *change);
extern void fdisk_debug_print_table(struct fdisk_table *tb);



extern int __fdisk_switch_label(struct fdisk_context *cxt,
				    struct fdisk_label *lb);
extern int fdisk_missing_geometry(struct fdisk_context *cxt);


fdisk_sector_t fdisk_scround(struct fdisk_context *cxt, fdisk_sector_t num);
fdisk_sector_t fdisk_cround(struct fdisk_context *cxt, fdisk_sector_t num);

extern int fdisk_discover_geometry(struct fdisk_context *cxt);
extern int fdisk_discover_topology(struct fdisk_context *cxt);

extern int fdisk_has_user_device_geometry(struct fdisk_context *cxt);
extern int fdisk_apply_user_device_properties(struct fdisk_context *cxt);
extern int fdisk_apply_label_device_properties(struct fdisk_context *cxt);
extern void fdisk_zeroize_device_properties(struct fdisk_context *cxt);


extern int fdisk_init_firstsector_buffer(struct fdisk_context *cxt,
			unsigned int protect_off, unsigned int protect_size);
extern int fdisk_read_firstsector(struct fdisk_context *cxt);


extern int fdisk_probe_labels(struct fdisk_context *cxt);
extern void fdisk_deinit_label(struct fdisk_label *lb);

struct fdisk_labelitem {
	int		refcount;	
	int		id;		
	char		type;		
	const char	*name;		

	union {
		char		*str;
		uint64_t	num64;
	} data;
};


#define FDISK_LABELITEM_INIT	{ .type = 0, .refcount = 0 }


struct fdisk_ask *fdisk_new_ask(void);
void fdisk_reset_ask(struct fdisk_ask *ask);
int fdisk_ask_set_query(struct fdisk_ask *ask, const char *str);
int fdisk_ask_set_type(struct fdisk_ask *ask, int type);
int fdisk_do_ask(struct fdisk_context *cxt, struct fdisk_ask *ask);
int fdisk_ask_number_set_range(struct fdisk_ask *ask, const char *range);
int fdisk_ask_number_set_default(struct fdisk_ask *ask, uint64_t dflt);
int fdisk_ask_number_set_low(struct fdisk_ask *ask, uint64_t low);
int fdisk_ask_number_set_high(struct fdisk_ask *ask, uint64_t high);
int fdisk_ask_number_set_base(struct fdisk_ask *ask, uint64_t base);
int fdisk_ask_number_set_unit(struct fdisk_ask *ask, uint64_t unit);
int fdisk_ask_number_is_relative(struct fdisk_ask *ask);
int fdisk_ask_number_set_wrap_negative(struct fdisk_ask *ask, int wrap_negative);
int fdisk_ask_menu_set_default(struct fdisk_ask *ask, int dfl);
int fdisk_ask_menu_add_item(struct fdisk_ask *ask, int key,
			const char *name, const char *desc);
int fdisk_ask_print_set_errno(struct fdisk_ask *ask, int errnum);
int fdisk_ask_print_set_mesg(struct fdisk_ask *ask, const char *mesg);
int fdisk_info_new_partition(
			struct fdisk_context *cxt,
			int num, fdisk_sector_t start, fdisk_sector_t stop,
			struct fdisk_parttype *t);


extern struct dos_partition *fdisk_dos_get_partition(
				struct fdisk_context *cxt,
				size_t i);


void fdisk_free_wipe_areas(struct fdisk_context *cxt);
int fdisk_set_wipe_area(struct fdisk_context *cxt, uint64_t start, uint64_t size, int enable);
int fdisk_do_wipe(struct fdisk_context *cxt);
int fdisk_has_wipe_area(struct fdisk_context *cxt, uint64_t start, uint64_t size);
int fdisk_check_collisions(struct fdisk_context *cxt);


const char *fdisk_label_translate_type_shortcut(const struct fdisk_label *lb, char *cut);

#endif 
