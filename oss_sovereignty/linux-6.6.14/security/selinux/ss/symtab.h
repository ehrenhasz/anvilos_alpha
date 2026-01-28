#ifndef _SS_SYMTAB_H_
#define _SS_SYMTAB_H_
#include "hashtab.h"
struct symtab {
	struct hashtab table;	 
	u32 nprim;		 
};
int symtab_init(struct symtab *s, u32 size);
int symtab_insert(struct symtab *s, char *name, void *datum);
void *symtab_search(struct symtab *s, const char *name);
#endif	 
