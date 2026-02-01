
 

#include <keys/asymmetric-type.h>
#include <linux/user_namespace.h>
#include <linux/ima.h>
#include "ima.h"

 
void ima_post_key_create_or_update(struct key *keyring, struct key *key,
				   const void *payload, size_t payload_len,
				   unsigned long flags, bool create)
{
	bool queued = false;

	 
	if (key->type != &key_type_asymmetric)
		return;

	if (!payload || (payload_len == 0))
		return;

	if (ima_should_queue_key())
		queued = ima_queue_key(keyring, payload, payload_len);

	if (queued)
		return;

	 
	process_buffer_measurement(&nop_mnt_idmap, NULL, payload, payload_len,
				   keyring->description, KEY_CHECK, 0,
				   keyring->description, false, NULL, 0);
}
