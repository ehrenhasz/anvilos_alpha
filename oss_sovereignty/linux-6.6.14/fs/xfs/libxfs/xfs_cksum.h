#ifndef _XFS_CKSUM_H
#define _XFS_CKSUM_H 1
#define XFS_CRC_SEED	(~(uint32_t)0)
static inline uint32_t
xfs_start_cksum_safe(char *buffer, size_t length, unsigned long cksum_offset)
{
	uint32_t zero = 0;
	uint32_t crc;
	crc = crc32c(XFS_CRC_SEED, buffer, cksum_offset);
	crc = crc32c(crc, &zero, sizeof(__u32));
	return crc32c(crc, &buffer[cksum_offset + sizeof(__be32)],
		      length - (cksum_offset + sizeof(__be32)));
}
static inline uint32_t
xfs_start_cksum_update(char *buffer, size_t length, unsigned long cksum_offset)
{
	*(__le32 *)(buffer + cksum_offset) = 0;
	return crc32c(XFS_CRC_SEED, buffer, length);
}
static inline __le32
xfs_end_cksum(uint32_t crc)
{
	return ~cpu_to_le32(crc);
}
static inline void
xfs_update_cksum(char *buffer, size_t length, unsigned long cksum_offset)
{
	uint32_t crc = xfs_start_cksum_update(buffer, length, cksum_offset);
	*(__le32 *)(buffer + cksum_offset) = xfs_end_cksum(crc);
}
static inline int
xfs_verify_cksum(char *buffer, size_t length, unsigned long cksum_offset)
{
	uint32_t crc = xfs_start_cksum_safe(buffer, length, cksum_offset);
	return *(__le32 *)(buffer + cksum_offset) == xfs_end_cksum(crc);
}
#endif  
