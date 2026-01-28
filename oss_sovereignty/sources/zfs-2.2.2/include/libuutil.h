


#ifndef	_LIBUUTIL_H
#define	_LIBUUTIL_H

#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef	__cplusplus
extern "C" {
#endif


#define	UU_DEFAULT		0


#define	UU_ERROR_NONE		0	
#define	UU_ERROR_INVALID_ARGUMENT 1	
#define	UU_ERROR_UNKNOWN_FLAG	2	
#define	UU_ERROR_NO_MEMORY	3	
#define	UU_ERROR_CALLBACK_FAILED 4	
#define	UU_ERROR_NOT_SUPPORTED	5	
#define	UU_ERROR_EMPTY		6	
#define	UU_ERROR_UNDERFLOW	7	
#define	UU_ERROR_OVERFLOW	8	
#define	UU_ERROR_INVALID_CHAR	9	
#define	UU_ERROR_INVALID_DIGIT	10	

#define	UU_ERROR_SYSTEM		99	
#define	UU_ERROR_UNKNOWN	100	


#define	UU_PROFILE_DEFAULT	0
#define	UU_PROFILE_LAUNCHER	1


uint32_t uu_error(void);
const char *uu_strerror(uint32_t);


#define	UU_NAME_DOMAIN		0x1	
#define	UU_NAME_PATH		0x2	

int uu_check_name(const char *, uint_t);


#define	UU_NELEM(a)	(sizeof (a) / sizeof ((a)[0]))

extern char *uu_msprintf(const char *format, ...)
    __attribute__((format(printf, 1, 2)));
extern void *uu_zalloc(size_t);
extern char *uu_strdup(const char *);
extern void uu_free(void *);

extern boolean_t uu_strcaseeq(const char *a, const char *b);
extern boolean_t uu_streq(const char *a, const char *b);
extern char *uu_strndup(const char *s, size_t n);
extern boolean_t uu_strbw(const char *a, const char *b);
extern void *uu_memdup(const void *buf, size_t sz);


typedef int uu_compare_fn_t(const void *__left, const void *__right,
    void *__private);


#define	UU_WALK_ROBUST		0x00000001	
#define	UU_WALK_REVERSE		0x00000002	

#define	UU_WALK_PREORDER	0x00000010	
#define	UU_WALK_POSTORDER	0x00000020	


#define	UU_WALK_ERROR		-1
#define	UU_WALK_NEXT		0
#define	UU_WALK_DONE		1


typedef int uu_walk_fn_t(void *_elem, void *_private);


typedef struct uu_list_pool uu_list_pool_t;
typedef struct uu_list uu_list_t;

typedef struct uu_list_node {
	uintptr_t uln_opaque[2];
} uu_list_node_t;

typedef struct uu_list_walk uu_list_walk_t;

typedef uintptr_t uu_list_index_t;


uu_list_pool_t *uu_list_pool_create(const char *, size_t, size_t,
    uu_compare_fn_t *, uint32_t);
#define	UU_LIST_POOL_DEBUG	0x00000001

void uu_list_pool_destroy(uu_list_pool_t *);


void uu_list_node_init(void *, uu_list_node_t *, uu_list_pool_t *);
void uu_list_node_fini(void *, uu_list_node_t *, uu_list_pool_t *);

uu_list_t *uu_list_create(uu_list_pool_t *, void *_parent, uint32_t);
#define	UU_LIST_DEBUG	0x00000001
#define	UU_LIST_SORTED	0x00000002	

void uu_list_destroy(uu_list_t *);	

size_t uu_list_numnodes(uu_list_t *);

void *uu_list_first(uu_list_t *);
void *uu_list_last(uu_list_t *);

void *uu_list_next(uu_list_t *, void *);
void *uu_list_prev(uu_list_t *, void *);

int uu_list_walk(uu_list_t *, uu_walk_fn_t *, void *, uint32_t);

uu_list_walk_t *uu_list_walk_start(uu_list_t *, uint32_t);
void *uu_list_walk_next(uu_list_walk_t *);
void uu_list_walk_end(uu_list_walk_t *);

void *uu_list_find(uu_list_t *, void *, void *, uu_list_index_t *);
void uu_list_insert(uu_list_t *, void *, uu_list_index_t);

void *uu_list_nearest_next(uu_list_t *, uu_list_index_t);
void *uu_list_nearest_prev(uu_list_t *, uu_list_index_t);

void *uu_list_teardown(uu_list_t *, void **);

void uu_list_remove(uu_list_t *, void *);


int uu_list_insert_before(uu_list_t *, void *_target, void *_elem);
int uu_list_insert_after(uu_list_t *, void *_target, void *_elem);


typedef struct uu_avl_pool uu_avl_pool_t;
typedef struct uu_avl uu_avl_t;

typedef struct uu_avl_node {
#ifdef _LP64
	uintptr_t uan_opaque[3];
#else
	uintptr_t uan_opaque[4];
#endif
} uu_avl_node_t;

typedef struct uu_avl_walk uu_avl_walk_t;

typedef uintptr_t uu_avl_index_t;


uu_avl_pool_t *uu_avl_pool_create(const char *, size_t, size_t,
    uu_compare_fn_t *, uint32_t);
#define	UU_AVL_POOL_DEBUG	0x00000001

void uu_avl_pool_destroy(uu_avl_pool_t *);


void uu_avl_node_init(void *, uu_avl_node_t *, uu_avl_pool_t *);
void uu_avl_node_fini(void *, uu_avl_node_t *, uu_avl_pool_t *);

uu_avl_t *uu_avl_create(uu_avl_pool_t *, void *_parent, uint32_t);
#define	UU_AVL_DEBUG	0x00000001

void uu_avl_destroy(uu_avl_t *);	

size_t uu_avl_numnodes(uu_avl_t *);

void *uu_avl_first(uu_avl_t *);
void *uu_avl_last(uu_avl_t *);

void *uu_avl_next(uu_avl_t *, void *);
void *uu_avl_prev(uu_avl_t *, void *);

int uu_avl_walk(uu_avl_t *, uu_walk_fn_t *, void *, uint32_t);

uu_avl_walk_t *uu_avl_walk_start(uu_avl_t *, uint32_t);
void *uu_avl_walk_next(uu_avl_walk_t *);
void uu_avl_walk_end(uu_avl_walk_t *);

void *uu_avl_find(uu_avl_t *, void *, void *, uu_avl_index_t *);
void uu_avl_insert(uu_avl_t *, void *, uu_avl_index_t);

void *uu_avl_nearest_next(uu_avl_t *, uu_avl_index_t);
void *uu_avl_nearest_prev(uu_avl_t *, uu_avl_index_t);

void *uu_avl_teardown(uu_avl_t *, void **);

void uu_avl_remove(uu_avl_t *, void *);

#ifdef	__cplusplus
}
#endif

#endif	
