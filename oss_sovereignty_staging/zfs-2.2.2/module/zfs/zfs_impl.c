 

 

#include <sys/zio_checksum.h>
#include <sys/zfs_context.h>
#include <sys/zfs_impl.h>

#include <sys/blake3.h>
#include <sys/sha2.h>

 
const zfs_impl_t *impl_ops[] = {
	&zfs_blake3_ops,
	&zfs_sha256_ops,
	&zfs_sha512_ops,
	NULL
};

 
const zfs_impl_t *
zfs_impl_get_ops(const char *algo)
{
	const zfs_impl_t **ops = impl_ops;

	if (!algo || !*algo)
		return (*ops);

	for (; *ops; ops++) {
		if (strcmp(algo, (*ops)->name) == 0)
			break;
	}

	ASSERT3P(ops, !=, NULL);
	return (*ops);
}
