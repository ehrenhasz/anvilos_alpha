
#ifndef UTIL_LINUX_LSFD_COUNTER_H
#define UTIL_LINUX_LSFD_COUNTER_H

#include "libsmartcols.h"
#include "lsfd-filter.h"
#include <stdbool.h>

struct lsfd_counter;


struct lsfd_counter *lsfd_counter_new(const char *const name, struct lsfd_filter *filter);
void lsfd_counter_free(struct lsfd_counter *counter);

bool lsfd_counter_accumulate(struct lsfd_counter *counter, struct libscols_line *ln);

const char *lsfd_counter_name(struct lsfd_counter *counter);
size_t lsfd_counter_value(struct lsfd_counter *counter);

#endif	
