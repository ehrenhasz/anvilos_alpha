#include "libbb.h"
#include "volume_id.h"
PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN
#define dbg(...) ((void)0)
#define VOLUME_ID_VERSION		48
#define VOLUME_ID_LABEL_SIZE		64
#define VOLUME_ID_UUID_SIZE		36
#define VOLUME_ID_FORMAT_SIZE		32
#define VOLUME_ID_PARTITIONS_MAX	256
enum volume_id_usage {
	VOLUME_ID_UNUSED,
	VOLUME_ID_UNPROBED,
	VOLUME_ID_OTHER,
	VOLUME_ID_FILESYSTEM,
	VOLUME_ID_PARTITIONTABLE,
	VOLUME_ID_RAID,
	VOLUME_ID_DISKLABEL,
	VOLUME_ID_CRYPTO,
};
#ifdef UNUSED_PARTITION_CODE
struct volume_id_partition {
};
#endif
struct volume_id {
	int		fd;
	int		error;
	size_t		sbbuf_len;
	size_t		seekbuf_len;
	uint8_t		*sbbuf;
	uint8_t		*seekbuf;
	uint64_t	seekbuf_off;
#if ENABLE_FEATURE_BLKID_TYPE
	const char	*type;
#endif
#ifdef UNUSED_PARTITION_CODE
	struct volume_id_partition *partitions;
	size_t		partition_count;
#endif
	char		label[VOLUME_ID_LABEL_SIZE+1];
	char		uuid[VOLUME_ID_UUID_SIZE+1];
};
struct volume_id* FAST_FUNC volume_id_open_node(int fd);
int FAST_FUNC volume_id_probe_all(struct volume_id *id,   uint64_t size);
void FAST_FUNC free_volume_id(struct volume_id *id);
#define SB_BUFFER_SIZE				0x11000
#define SEEK_BUFFER_SIZE			0x10000
#if BB_LITTLE_ENDIAN
# define le16_to_cpu(x) (uint16_t)(x)
# define le32_to_cpu(x) (uint32_t)(x)
# define le64_to_cpu(x) (uint64_t)(x)
# define be16_to_cpu(x) (uint16_t)(bswap_16(x))
# define be32_to_cpu(x) (uint32_t)(bswap_32(x))
# define cpu_to_le16(x) (uint16_t)(x)
# define cpu_to_le32(x) (uint32_t)(x)
# define cpu_to_be32(x) (uint32_t)(bswap_32(x))
#else
# define le16_to_cpu(x) (uint16_t)(bswap_16(x))
# define le32_to_cpu(x) (uint32_t)(bswap_32(x))
# define le64_to_cpu(x) (uint64_t)(bb_bswap_64(x))
# define be16_to_cpu(x) (uint16_t)(x)
# define be32_to_cpu(x) (uint32_t)(x)
# define cpu_to_le16(x) (uint16_t)(bswap_16(x))
# define cpu_to_le32(x) (uint32_t)(bswap_32(x))
# define cpu_to_be32(x) (uint32_t)(x)
#endif
enum uuid_format {
	UUID_DOS = 0,		 
	UUID_NTFS = 1,		 
	UUID_DCE = 2,		 
	UUID_DCE_STRING = 3,	 
};
enum endian {
	LE = 0,
	BE = 1
};
void volume_id_set_unicode16(char *str, size_t len, const uint8_t *buf, enum endian endianess, size_t count);
void volume_id_set_label_string(struct volume_id *id, const uint8_t *buf, size_t count);
void volume_id_set_label_unicode16(struct volume_id *id, const uint8_t *buf, enum endian endianess, size_t count);
void volume_id_set_uuid(struct volume_id *id, const uint8_t *buf, enum uuid_format format);
void *volume_id_get_buffer(struct volume_id *id, uint64_t off, size_t len);
void volume_id_free_buffer(struct volume_id *id);
int FAST_FUNC volume_id_probe_linux_raid(struct volume_id *id  , uint64_t size);
int FAST_FUNC volume_id_probe_bcache(struct volume_id *id  );
int FAST_FUNC volume_id_probe_btrfs(struct volume_id *id  );
int FAST_FUNC volume_id_probe_cramfs(struct volume_id *id  );
int FAST_FUNC volume_id_probe_ext(struct volume_id *id  );
int FAST_FUNC volume_id_probe_vfat(struct volume_id *id  );
int FAST_FUNC volume_id_probe_hfs_hfsplus(struct volume_id *id  );
int FAST_FUNC volume_id_probe_iso9660(struct volume_id *id  );
int FAST_FUNC volume_id_probe_jfs(struct volume_id *id  );
int FAST_FUNC volume_id_probe_lfs(struct volume_id *id  );
int FAST_FUNC volume_id_probe_linux_swap(struct volume_id *id  );
int FAST_FUNC volume_id_probe_luks(struct volume_id *id  );
int FAST_FUNC volume_id_probe_minix(struct volume_id *id  );
int FAST_FUNC volume_id_probe_f2fs(struct volume_id *id  );
int FAST_FUNC volume_id_probe_nilfs(struct volume_id *id  );
int FAST_FUNC volume_id_probe_ntfs(struct volume_id *id  );
int FAST_FUNC volume_id_probe_exfat(struct volume_id *id  );
int FAST_FUNC volume_id_probe_ocfs2(struct volume_id *id  );
int FAST_FUNC volume_id_probe_reiserfs(struct volume_id *id  );
int FAST_FUNC volume_id_probe_romfs(struct volume_id *id  );
int FAST_FUNC volume_id_probe_squashfs(struct volume_id *id  );
int FAST_FUNC volume_id_probe_erofs(struct volume_id *id  );
int FAST_FUNC volume_id_probe_sysv(struct volume_id *id  );
int FAST_FUNC volume_id_probe_udf(struct volume_id *id  );
int FAST_FUNC volume_id_probe_xfs(struct volume_id *id  );
int FAST_FUNC volume_id_probe_ubifs(struct volume_id *id  );
POP_SAVED_FUNCTION_VISIBILITY
