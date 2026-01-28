#include <keys/asymmetric-type.h>
extern struct asymmetric_key_id *asymmetric_key_hex_to_key_id(const char *id);
extern int __asymmetric_key_hex_to_key_id(const char *id,
					  struct asymmetric_key_id *match_id,
					  size_t hexlen);
extern int asymmetric_key_eds_op(struct kernel_pkey_params *params,
				 const void *in, void *out);
