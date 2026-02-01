
 

 

#include "ni_route_values.h"
#include "ni_route_values/all.h"

const struct family_route_values *const ni_all_route_values[] = {
	&ni_660x_route_values,
	&ni_eseries_route_values,
	&ni_mseries_route_values,
	NULL,
};
