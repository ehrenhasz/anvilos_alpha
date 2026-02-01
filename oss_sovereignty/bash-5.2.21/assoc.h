 

 

#ifndef _ASSOC_H_
#define _ASSOC_H_

#include "stdc.h"
#include "hashlib.h"

#define ASSOC_HASH_BUCKETS	1024

#define assoc_empty(h)		((h)->nentries == 0)
#define assoc_num_elements(h)	((h)->nentries)

#define assoc_create(n)		(hash_create((n)))

#define assoc_copy(h)		(hash_copy((h), 0))

#define assoc_walk(h, f)	(hash_walk((h), (f))

extern void assoc_dispose PARAMS((HASH_TABLE *));
extern void assoc_flush PARAMS((HASH_TABLE *));

extern int assoc_insert PARAMS((HASH_TABLE *, char *, char *));
extern PTR_T assoc_replace PARAMS((HASH_TABLE *, char *, char *));
extern void assoc_remove PARAMS((HASH_TABLE *, char *));

extern char *assoc_reference PARAMS((HASH_TABLE *, char *));

extern char *assoc_subrange PARAMS((HASH_TABLE *, arrayind_t, arrayind_t, int, int, int));
extern char *assoc_patsub PARAMS((HASH_TABLE *, char *, char *, int));
extern char *assoc_modcase PARAMS((HASH_TABLE *, char *, int, int));

extern HASH_TABLE *assoc_quote PARAMS((HASH_TABLE *));
extern HASH_TABLE *assoc_quote_escapes PARAMS((HASH_TABLE *));
extern HASH_TABLE *assoc_dequote PARAMS((HASH_TABLE *));
extern HASH_TABLE *assoc_dequote_escapes PARAMS((HASH_TABLE *));
extern HASH_TABLE *assoc_remove_quoted_nulls PARAMS((HASH_TABLE *));

extern char *assoc_to_kvpair PARAMS((HASH_TABLE *, int));
extern char *assoc_to_assign PARAMS((HASH_TABLE *, int));

extern WORD_LIST *assoc_to_word_list PARAMS((HASH_TABLE *));
extern WORD_LIST *assoc_keys_to_word_list PARAMS((HASH_TABLE *));
extern WORD_LIST *assoc_to_kvpair_list PARAMS((HASH_TABLE *));

extern char *assoc_to_string PARAMS((HASH_TABLE *, char *, int));
#endif  
