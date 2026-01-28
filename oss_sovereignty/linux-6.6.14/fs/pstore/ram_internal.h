#include <linux/pstore_ram.h>
#define PRZ_FLAG_NO_LOCK	BIT(0)
#define PRZ_FLAG_ZAP_OLD	BIT(1)
struct persistent_ram_zone {
	phys_addr_t paddr;
	size_t size;
	void *vaddr;
	char *label;
	enum pstore_type_id type;
	u32 flags;
	raw_spinlock_t buffer_lock;
	struct persistent_ram_buffer *buffer;
	size_t buffer_size;
	char *par_buffer;
	char *par_header;
	struct rs_control *rs_decoder;
	int corrected_bytes;
	int bad_blocks;
	struct persistent_ram_ecc_info ecc_info;
	char *old_log;
	size_t old_log_size;
};
struct persistent_ram_zone *persistent_ram_new(phys_addr_t start, size_t size,
			u32 sig, struct persistent_ram_ecc_info *ecc_info,
			unsigned int memtype, u32 flags, char *label);
void persistent_ram_free(struct persistent_ram_zone **_prz);
void persistent_ram_zap(struct persistent_ram_zone *prz);
int persistent_ram_write(struct persistent_ram_zone *prz, const void *s,
			 unsigned int count);
int persistent_ram_write_user(struct persistent_ram_zone *prz,
			      const void __user *s, unsigned int count);
void persistent_ram_save_old(struct persistent_ram_zone *prz);
size_t persistent_ram_old_size(struct persistent_ram_zone *prz);
void *persistent_ram_old(struct persistent_ram_zone *prz);
void persistent_ram_free_old(struct persistent_ram_zone *prz);
ssize_t persistent_ram_ecc_string(struct persistent_ram_zone *prz,
	char *str, size_t len);
