#ifndef	_SYS_UUID_H
#define	_SYS_UUID_H
#ifdef	__cplusplus
extern "C" {
#endif
#include <sys/types.h>
#include <sys/byteorder.h>
typedef struct {
	uint8_t		nodeID[6];
} uuid_node_t;
typedef struct uuid {
	uint32_t	time_low;
	uint16_t	time_mid;
	uint16_t	time_hi_and_version;
	uint8_t		clock_seq_hi_and_reserved;
	uint8_t		clock_seq_low;
	uint8_t		node_addr[6];
} uuid_t;
#define	UUID_PRINTABLE_STRING_LENGTH 37
#define	UUID_LE_CONVERT(dest, src)					\
{									\
	(dest) = (src);							\
	(dest).time_low = LE_32((dest).time_low);			\
	(dest).time_mid = LE_16((dest).time_mid);			\
	(dest).time_hi_and_version = LE_16((dest).time_hi_and_version);	\
}
static __inline int
uuid_is_null(const caddr_t uuid)
{
	return (0);
}
#ifdef __cplusplus
}
#endif
#endif  
