 

 

#if !defined (_ALIAS_H_)
#define _ALIAS_H_

#include "stdc.h"

#include "hashlib.h"

typedef struct alias {
  char *name;
  char *value;
  char flags;
} alias_t;

 
#define AL_EXPANDNEXT		0x1
#define AL_BEINGEXPANDED	0x2

 
extern HASH_TABLE *aliases;

extern void initialize_aliases PARAMS((void));

 
extern alias_t *find_alias PARAMS((char *));

 
extern char *get_alias_value PARAMS((char *));

 
extern void add_alias PARAMS((char *, char *));

 
extern int remove_alias PARAMS((char *));

 
extern void delete_all_aliases PARAMS((void));

 
extern alias_t **all_aliases PARAMS((void));

 
extern char *alias_expand_word PARAMS((char *));

 
extern char *alias_expand PARAMS((char *));

 
extern void clear_string_list_expander PARAMS((alias_t *));

#endif  
