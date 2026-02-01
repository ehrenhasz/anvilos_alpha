 

#include <sys/vdev_draid.h>

static inline uint64_t rotl(const uint64_t x, int k)
{
	return (x << k) | (x >> (64 - k));
}

uint64_t
vdev_draid_rand(uint64_t *s)
{
	const uint64_t s0 = s[0];
	uint64_t s1 = s[1];
	const uint64_t result = rotl(s0 + s1, 17) + s0;

	s1 ^= s0;
	s[0] = rotl(s0, 49) ^ s1 ^ (s1 << 21); 
	s[1] = rotl(s1, 28); 

	return (result);
}
