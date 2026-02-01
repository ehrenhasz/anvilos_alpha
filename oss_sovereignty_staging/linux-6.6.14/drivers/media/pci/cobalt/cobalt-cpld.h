 
 

#ifndef COBALT_CPLD_H
#define COBALT_CPLD_H

#include "cobalt-driver.h"

void cobalt_cpld_status(struct cobalt *cobalt);
bool cobalt_cpld_set_freq(struct cobalt *cobalt, unsigned freq);

#endif
