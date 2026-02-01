 
#ifndef __PERF_STRFILTER_H
#define __PERF_STRFILTER_H
 

#include <linux/list.h>
#include <stdbool.h>

 
struct strfilter_node {
	struct strfilter_node *l;	 
	struct strfilter_node *r;	 
	const char *p;		 
};

 
struct strfilter {
	struct strfilter_node *root;
};

 
struct strfilter *strfilter__new(const char *rules, const char **err);

 
int strfilter__or(struct strfilter *filter,
		  const char *rules, const char **err);

 
int strfilter__and(struct strfilter *filter,
		   const char *rules, const char **err);

 
bool strfilter__compare(struct strfilter *filter, const char *str);

 
void strfilter__delete(struct strfilter *filter);

 
char *strfilter__string(struct strfilter *filter);

#endif
