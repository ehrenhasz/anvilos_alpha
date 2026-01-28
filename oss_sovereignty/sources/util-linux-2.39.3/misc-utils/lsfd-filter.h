
#ifndef UTIL_LINUX_LSFD_FILTER_H
#define UTIL_LINUX_LSFD_FILTER_H

#include "libsmartcols.h"
#include <stdio.h>
#include <stdbool.h>

#define LSFD_FILTER_UNKNOWN_COL_ID -1

struct lsfd_filter;


struct lsfd_filter *lsfd_filter_new(const char *const expr, struct libscols_table *tb,
				      int ncols,
				      int (*column_name_to_id)(const char *, void *),
				      struct libscols_column *(*add_column_by_id)(struct libscols_table *, int, void*),
				      void *data);


const char *lsfd_filter_get_errmsg(struct lsfd_filter *filter);
void lsfd_filter_free(struct lsfd_filter *filter);
bool lsfd_filter_apply(struct lsfd_filter *filter, struct libscols_line *ln);


void lsfd_filter_dump(struct lsfd_filter *filter, FILE *stream);

#endif	
