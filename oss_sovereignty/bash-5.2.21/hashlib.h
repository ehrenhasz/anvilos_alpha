 

 

#if !defined (_HASHLIB_H_)
#define _HASHLIB_H_

#include "stdc.h"

#ifndef PTR_T
#  ifdef __STDC__
#    define PTR_T void *
#  else
#    define PTR_T char *
#  endif
#endif

typedef struct bucket_contents {
  struct bucket_contents *next;	 
  char *key;			 
  PTR_T data;			 
  unsigned int khash;		 
  int times_found;		 
} BUCKET_CONTENTS;

typedef struct hash_table {
  BUCKET_CONTENTS **bucket_array;	 
  int nbuckets;			 
  int nentries;			 
} HASH_TABLE;

typedef int hash_wfunc PARAMS((BUCKET_CONTENTS *));

 
extern HASH_TABLE *hash_create PARAMS((int));
extern HASH_TABLE *hash_copy PARAMS((HASH_TABLE *, sh_string_func_t *));
extern void hash_flush PARAMS((HASH_TABLE *, sh_free_func_t *));
extern void hash_dispose PARAMS((HASH_TABLE *));
extern void hash_walk PARAMS((HASH_TABLE *, hash_wfunc *));

 
extern int hash_bucket PARAMS((const char *, HASH_TABLE *));
extern int hash_size PARAMS((HASH_TABLE *));

 
extern BUCKET_CONTENTS *hash_search PARAMS((const char *, HASH_TABLE *, int));
extern BUCKET_CONTENTS *hash_insert PARAMS((char *, HASH_TABLE *, int));
extern BUCKET_CONTENTS *hash_remove PARAMS((const char *, HASH_TABLE *, int));

 
extern unsigned int hash_string PARAMS((const char *));

 
#define hash_items(bucket, table) \
	((table && (bucket < table->nbuckets)) ?  \
		table->bucket_array[bucket] : \
		(BUCKET_CONTENTS *)NULL)

 
#define DEFAULT_HASH_BUCKETS 128	 

#define HASH_ENTRIES(ht)	((ht) ? (ht)->nentries : 0)

 
#define HASH_NOSRCH	0x01
#define HASH_CREATE	0x02

#if !defined (NULL)
#  if defined (__STDC__)
#    define NULL ((void *) 0)
#  else
#    define NULL 0x0
#  endif  
#endif  

#endif  
