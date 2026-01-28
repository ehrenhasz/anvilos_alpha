#ifndef _COMEDI_DRIVERS_NI_ROUTINT_NI_ROUTE_VALUES_H
#define _COMEDI_DRIVERS_NI_ROUTINT_NI_ROUTE_VALUES_H
#include <linux/comedi.h>
#include <linux/types.h>
#define B(x)	((x) - NI_NAMES_BASE)
#define V(x)	(((x) & 0x7f) | 0x80)
#ifndef NI_ROUTE_VALUE_EXTERNAL_CONVERSION
	#define I(x)	V(x)
	#define U(x)	0x0
	typedef u8 register_type;
#else
	#define I(x)	(((x) & 0x7f) | 0x100)
	#define U(x)	(((x) & 0x7f) | 0x200)
	#define MARKED_V(x)	(((x) & 0x80) != 0)
	#define MARKED_I(x)	(((x) & 0x100) != 0)
	#define MARKED_U(x)	(((x) & 0x200) != 0)
	typedef u16 register_type;
#endif
#define UNMARK(x)	((x) & 0x7f)
#define Gi_SRC(val, subsel)	((val) | ((subsel) << 6))
struct family_route_values {
	const char *family;
	const register_type register_values[NI_NUM_NAMES][NI_NUM_NAMES];
};
extern const struct family_route_values *const ni_all_route_values[];
#endif  
