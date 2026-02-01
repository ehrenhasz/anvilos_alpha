 
 

#ifndef __LINUX_MTD_SPI_NOR_INTERNAL_H
#define __LINUX_MTD_SPI_NOR_INTERNAL_H

#include "sfdp.h"

#define SPI_NOR_MAX_ID_LEN	6

 
#define SPI_NOR_READID_OP(naddr, ndummy, buf, len)			\
	SPI_MEM_OP(SPI_MEM_OP_CMD(SPINOR_OP_RDID, 0),			\
		   SPI_MEM_OP_ADDR(naddr, 0, 0),			\
		   SPI_MEM_OP_DUMMY(ndummy, 0),				\
		   SPI_MEM_OP_DATA_IN(len, buf, 0))

#define SPI_NOR_WREN_OP							\
	SPI_MEM_OP(SPI_MEM_OP_CMD(SPINOR_OP_WREN, 0),			\
		   SPI_MEM_OP_NO_ADDR,					\
		   SPI_MEM_OP_NO_DUMMY,					\
		   SPI_MEM_OP_NO_DATA)

#define SPI_NOR_WRDI_OP							\
	SPI_MEM_OP(SPI_MEM_OP_CMD(SPINOR_OP_WRDI, 0),			\
		   SPI_MEM_OP_NO_ADDR,					\
		   SPI_MEM_OP_NO_DUMMY,					\
		   SPI_MEM_OP_NO_DATA)

#define SPI_NOR_RDSR_OP(buf)						\
	SPI_MEM_OP(SPI_MEM_OP_CMD(SPINOR_OP_RDSR, 0),			\
		   SPI_MEM_OP_NO_ADDR,					\
		   SPI_MEM_OP_NO_DUMMY,					\
		   SPI_MEM_OP_DATA_IN(1, buf, 0))

#define SPI_NOR_WRSR_OP(buf, len)					\
	SPI_MEM_OP(SPI_MEM_OP_CMD(SPINOR_OP_WRSR, 0),			\
		   SPI_MEM_OP_NO_ADDR,					\
		   SPI_MEM_OP_NO_DUMMY,					\
		   SPI_MEM_OP_DATA_OUT(len, buf, 0))

#define SPI_NOR_RDSR2_OP(buf)						\
	SPI_MEM_OP(SPI_MEM_OP_CMD(SPINOR_OP_RDSR2, 0),			\
		   SPI_MEM_OP_NO_ADDR,					\
		   SPI_MEM_OP_NO_DUMMY,					\
		   SPI_MEM_OP_DATA_OUT(1, buf, 0))

#define SPI_NOR_WRSR2_OP(buf)						\
	SPI_MEM_OP(SPI_MEM_OP_CMD(SPINOR_OP_WRSR2, 0),			\
		   SPI_MEM_OP_NO_ADDR,					\
		   SPI_MEM_OP_NO_DUMMY,					\
		   SPI_MEM_OP_DATA_OUT(1, buf, 0))

#define SPI_NOR_RDCR_OP(buf)						\
	SPI_MEM_OP(SPI_MEM_OP_CMD(SPINOR_OP_RDCR, 0),			\
		   SPI_MEM_OP_NO_ADDR,					\
		   SPI_MEM_OP_NO_DUMMY,					\
		   SPI_MEM_OP_DATA_IN(1, buf, 0))

#define SPI_NOR_EN4B_EX4B_OP(enable)					\
	SPI_MEM_OP(SPI_MEM_OP_CMD(enable ? SPINOR_OP_EN4B : SPINOR_OP_EX4B, 0),	\
		   SPI_MEM_OP_NO_ADDR,					\
		   SPI_MEM_OP_NO_DUMMY,					\
		   SPI_MEM_OP_NO_DATA)

#define SPI_NOR_BRWR_OP(buf)						\
	SPI_MEM_OP(SPI_MEM_OP_CMD(SPINOR_OP_BRWR, 0),			\
		   SPI_MEM_OP_NO_ADDR,					\
		   SPI_MEM_OP_NO_DUMMY,					\
		   SPI_MEM_OP_DATA_OUT(1, buf, 0))

#define SPI_NOR_GBULK_OP						\
	SPI_MEM_OP(SPI_MEM_OP_CMD(SPINOR_OP_GBULK, 0),			\
		   SPI_MEM_OP_NO_ADDR,					\
		   SPI_MEM_OP_NO_DUMMY,					\
		   SPI_MEM_OP_NO_DATA)

#define SPI_NOR_CHIP_ERASE_OP						\
	SPI_MEM_OP(SPI_MEM_OP_CMD(SPINOR_OP_CHIP_ERASE, 0),		\
		   SPI_MEM_OP_NO_ADDR,					\
		   SPI_MEM_OP_NO_DUMMY,					\
		   SPI_MEM_OP_NO_DATA)

#define SPI_NOR_SECTOR_ERASE_OP(opcode, addr_nbytes, addr)		\
	SPI_MEM_OP(SPI_MEM_OP_CMD(opcode, 0),				\
		   SPI_MEM_OP_ADDR(addr_nbytes, addr, 0),		\
		   SPI_MEM_OP_NO_DUMMY,					\
		   SPI_MEM_OP_NO_DATA)

#define SPI_NOR_READ_OP(opcode)						\
	SPI_MEM_OP(SPI_MEM_OP_CMD(opcode, 0),				\
		   SPI_MEM_OP_ADDR(3, 0, 0),				\
		   SPI_MEM_OP_DUMMY(1, 0),				\
		   SPI_MEM_OP_DATA_IN(2, NULL, 0))

#define SPI_NOR_PP_OP(opcode)						\
	SPI_MEM_OP(SPI_MEM_OP_CMD(opcode, 0),				\
		   SPI_MEM_OP_ADDR(3, 0, 0),				\
		   SPI_MEM_OP_NO_DUMMY,					\
		   SPI_MEM_OP_DATA_OUT(2, NULL, 0))

#define SPINOR_SRSTEN_OP						\
	SPI_MEM_OP(SPI_MEM_OP_CMD(SPINOR_OP_SRSTEN, 0),			\
		   SPI_MEM_OP_NO_DUMMY,					\
		   SPI_MEM_OP_NO_ADDR,					\
		   SPI_MEM_OP_NO_DATA)

#define SPINOR_SRST_OP							\
	SPI_MEM_OP(SPI_MEM_OP_CMD(SPINOR_OP_SRST, 0),			\
		   SPI_MEM_OP_NO_DUMMY,					\
		   SPI_MEM_OP_NO_ADDR,					\
		   SPI_MEM_OP_NO_DATA)

 
enum spi_nor_option_flags {
	SNOR_F_HAS_SR_TB	= BIT(0),
	SNOR_F_NO_OP_CHIP_ERASE	= BIT(1),
	SNOR_F_BROKEN_RESET	= BIT(2),
	SNOR_F_4B_OPCODES	= BIT(3),
	SNOR_F_HAS_4BAIT	= BIT(4),
	SNOR_F_HAS_LOCK		= BIT(5),
	SNOR_F_HAS_16BIT_SR	= BIT(6),
	SNOR_F_NO_READ_CR	= BIT(7),
	SNOR_F_HAS_SR_TB_BIT6	= BIT(8),
	SNOR_F_HAS_4BIT_BP      = BIT(9),
	SNOR_F_HAS_SR_BP3_BIT6  = BIT(10),
	SNOR_F_IO_MODE_EN_VOLATILE = BIT(11),
	SNOR_F_SOFT_RESET	= BIT(12),
	SNOR_F_SWP_IS_VOLATILE	= BIT(13),
	SNOR_F_RWW		= BIT(14),
	SNOR_F_ECC		= BIT(15),
	SNOR_F_NO_WP		= BIT(16),
};

struct spi_nor_read_command {
	u8			num_mode_clocks;
	u8			num_wait_states;
	u8			opcode;
	enum spi_nor_protocol	proto;
};

struct spi_nor_pp_command {
	u8			opcode;
	enum spi_nor_protocol	proto;
};

enum spi_nor_read_command_index {
	SNOR_CMD_READ,
	SNOR_CMD_READ_FAST,
	SNOR_CMD_READ_1_1_1_DTR,

	 
	SNOR_CMD_READ_1_1_2,
	SNOR_CMD_READ_1_2_2,
	SNOR_CMD_READ_2_2_2,
	SNOR_CMD_READ_1_2_2_DTR,

	 
	SNOR_CMD_READ_1_1_4,
	SNOR_CMD_READ_1_4_4,
	SNOR_CMD_READ_4_4_4,
	SNOR_CMD_READ_1_4_4_DTR,

	 
	SNOR_CMD_READ_1_1_8,
	SNOR_CMD_READ_1_8_8,
	SNOR_CMD_READ_8_8_8,
	SNOR_CMD_READ_1_8_8_DTR,
	SNOR_CMD_READ_8_8_8_DTR,

	SNOR_CMD_READ_MAX
};

enum spi_nor_pp_command_index {
	SNOR_CMD_PP,

	 
	SNOR_CMD_PP_1_1_4,
	SNOR_CMD_PP_1_4_4,
	SNOR_CMD_PP_4_4_4,

	 
	SNOR_CMD_PP_1_1_8,
	SNOR_CMD_PP_1_8_8,
	SNOR_CMD_PP_8_8_8,
	SNOR_CMD_PP_8_8_8_DTR,

	SNOR_CMD_PP_MAX
};

 
struct spi_nor_erase_type {
	u32	size;
	u32	size_shift;
	u32	size_mask;
	u8	opcode;
	u8	idx;
};

 
struct spi_nor_erase_command {
	struct list_head	list;
	u32			count;
	u32			size;
	u8			opcode;
};

 
struct spi_nor_erase_region {
	u64		offset;
	u64		size;
};

#define SNOR_ERASE_TYPE_MAX	4
#define SNOR_ERASE_TYPE_MASK	GENMASK_ULL(SNOR_ERASE_TYPE_MAX - 1, 0)

#define SNOR_LAST_REGION	BIT(4)
#define SNOR_OVERLAID_REGION	BIT(5)

#define SNOR_ERASE_FLAGS_MAX	6
#define SNOR_ERASE_FLAGS_MASK	GENMASK_ULL(SNOR_ERASE_FLAGS_MAX - 1, 0)

 
struct spi_nor_erase_map {
	struct spi_nor_erase_region	*regions;
	struct spi_nor_erase_region	uniform_region;
	struct spi_nor_erase_type	erase_type[SNOR_ERASE_TYPE_MAX];
	u8				uniform_erase_type;
};

 
struct spi_nor_locking_ops {
	int (*lock)(struct spi_nor *nor, loff_t ofs, uint64_t len);
	int (*unlock)(struct spi_nor *nor, loff_t ofs, uint64_t len);
	int (*is_locked)(struct spi_nor *nor, loff_t ofs, uint64_t len);
};

 
struct spi_nor_otp_organization {
	size_t len;
	loff_t base;
	loff_t offset;
	unsigned int n_regions;
};

 
struct spi_nor_otp_ops {
	int (*read)(struct spi_nor *nor, loff_t addr, size_t len, u8 *buf);
	int (*write)(struct spi_nor *nor, loff_t addr, size_t len,
		     const u8 *buf);
	int (*lock)(struct spi_nor *nor, unsigned int region);
	int (*erase)(struct spi_nor *nor, loff_t addr);
	int (*is_locked)(struct spi_nor *nor, unsigned int region);
};

 
struct spi_nor_otp {
	const struct spi_nor_otp_organization *org;
	const struct spi_nor_otp_ops *ops;
};

 
struct spi_nor_flash_parameter {
	u64				bank_size;
	u64				size;
	u32				writesize;
	u32				page_size;
	u8				addr_nbytes;
	u8				addr_mode_nbytes;
	u8				rdsr_dummy;
	u8				rdsr_addr_nbytes;
	u8				n_dice;
	u32				*vreg_offset;

	struct spi_nor_hwcaps		hwcaps;
	struct spi_nor_read_command	reads[SNOR_CMD_READ_MAX];
	struct spi_nor_pp_command	page_programs[SNOR_CMD_PP_MAX];

	struct spi_nor_erase_map        erase_map;
	struct spi_nor_otp		otp;

	int (*set_octal_dtr)(struct spi_nor *nor, bool enable);
	int (*quad_enable)(struct spi_nor *nor);
	int (*set_4byte_addr_mode)(struct spi_nor *nor, bool enable);
	u32 (*convert_addr)(struct spi_nor *nor, u32 addr);
	int (*setup)(struct spi_nor *nor, const struct spi_nor_hwcaps *hwcaps);
	int (*ready)(struct spi_nor *nor);

	const struct spi_nor_locking_ops *locking_ops;
	void *priv;
};

 
struct spi_nor_fixups {
	void (*default_init)(struct spi_nor *nor);
	int (*post_bfpt)(struct spi_nor *nor,
			 const struct sfdp_parameter_header *bfpt_header,
			 const struct sfdp_bfpt *bfpt);
	int (*post_sfdp)(struct spi_nor *nor);
	int (*late_init)(struct spi_nor *nor);
};

 
struct flash_info {
	char *name;
	u8 id[SPI_NOR_MAX_ID_LEN];
	u8 id_len;
	unsigned sector_size;
	u16 n_sectors;
	u16 page_size;
	u8 n_banks;
	u8 addr_nbytes;

	bool parse_sfdp;
	u16 flags;
#define SPI_NOR_HAS_LOCK		BIT(0)
#define SPI_NOR_HAS_TB			BIT(1)
#define SPI_NOR_TB_SR_BIT6		BIT(2)
#define SPI_NOR_4BIT_BP			BIT(3)
#define SPI_NOR_BP3_SR_BIT6		BIT(4)
#define SPI_NOR_SWP_IS_VOLATILE		BIT(5)
#define SPI_NOR_NO_ERASE		BIT(6)
#define NO_CHIP_ERASE			BIT(7)
#define SPI_NOR_NO_FR			BIT(8)
#define SPI_NOR_QUAD_PP			BIT(9)
#define SPI_NOR_RWW			BIT(10)

	u8 no_sfdp_flags;
#define SPI_NOR_SKIP_SFDP		BIT(0)
#define SECT_4K				BIT(1)
#define SPI_NOR_DUAL_READ		BIT(3)
#define SPI_NOR_QUAD_READ		BIT(4)
#define SPI_NOR_OCTAL_READ		BIT(5)
#define SPI_NOR_OCTAL_DTR_READ		BIT(6)
#define SPI_NOR_OCTAL_DTR_PP		BIT(7)

	u8 fixup_flags;
#define SPI_NOR_4B_OPCODES		BIT(0)
#define SPI_NOR_IO_MODE_EN_VOLATILE	BIT(1)

	u8 mfr_flags;

	const struct spi_nor_otp_organization otp_org;
	const struct spi_nor_fixups *fixups;
};

#define SPI_NOR_ID_2ITEMS(_id) ((_id) >> 8) & 0xff, (_id) & 0xff
#define SPI_NOR_ID_3ITEMS(_id) ((_id) >> 16) & 0xff, SPI_NOR_ID_2ITEMS(_id)

#define SPI_NOR_ID(_jedec_id, _ext_id)					\
	.id = { SPI_NOR_ID_3ITEMS(_jedec_id), SPI_NOR_ID_2ITEMS(_ext_id) }, \
	.id_len = !(_jedec_id) ? 0 : (3 + ((_ext_id) ? 2 : 0))

#define SPI_NOR_ID6(_jedec_id, _ext_id)					\
	.id = { SPI_NOR_ID_3ITEMS(_jedec_id), SPI_NOR_ID_3ITEMS(_ext_id) }, \
	.id_len = 6

#define SPI_NOR_GEOMETRY(_sector_size, _n_sectors, _n_banks)		\
	.sector_size = (_sector_size),					\
	.n_sectors = (_n_sectors),					\
	.page_size = 256,						\
	.n_banks = (_n_banks)

 
#define INFO(_jedec_id, _ext_id, _sector_size, _n_sectors)		\
	SPI_NOR_ID((_jedec_id), (_ext_id)),				\
	SPI_NOR_GEOMETRY((_sector_size), (_n_sectors), 1),

#define INFOB(_jedec_id, _ext_id, _sector_size, _n_sectors, _n_banks)	\
	SPI_NOR_ID((_jedec_id), (_ext_id)),				\
	SPI_NOR_GEOMETRY((_sector_size), (_n_sectors), (_n_banks)),

#define INFO6(_jedec_id, _ext_id, _sector_size, _n_sectors)		\
	SPI_NOR_ID6((_jedec_id), (_ext_id)),				\
	SPI_NOR_GEOMETRY((_sector_size), (_n_sectors), 1),

#define CAT25_INFO(_sector_size, _n_sectors, _page_size, _addr_nbytes)	\
		.sector_size = (_sector_size),				\
		.n_sectors = (_n_sectors),				\
		.page_size = (_page_size),				\
		.n_banks = 1,						\
		.addr_nbytes = (_addr_nbytes),				\
		.flags = SPI_NOR_NO_ERASE | SPI_NOR_NO_FR,		\

#define OTP_INFO(_len, _n_regions, _base, _offset)			\
		.otp_org = {						\
			.len = (_len),					\
			.base = (_base),				\
			.offset = (_offset),				\
			.n_regions = (_n_regions),			\
		},

#define PARSE_SFDP							\
	.parse_sfdp = true,						\

#define FLAGS(_flags)							\
		.flags = (_flags),					\

#define NO_SFDP_FLAGS(_no_sfdp_flags)					\
		.no_sfdp_flags = (_no_sfdp_flags),			\

#define FIXUP_FLAGS(_fixup_flags)					\
		.fixup_flags = (_fixup_flags),				\

#define MFR_FLAGS(_mfr_flags)						\
		.mfr_flags = (_mfr_flags),				\

 
struct spi_nor_manufacturer {
	const char *name;
	const struct flash_info *parts;
	unsigned int nparts;
	const struct spi_nor_fixups *fixups;
};

 
struct sfdp {
	size_t	num_dwords;
	u32	*dwords;
};

 
extern const struct spi_nor_manufacturer spi_nor_atmel;
extern const struct spi_nor_manufacturer spi_nor_catalyst;
extern const struct spi_nor_manufacturer spi_nor_eon;
extern const struct spi_nor_manufacturer spi_nor_esmt;
extern const struct spi_nor_manufacturer spi_nor_everspin;
extern const struct spi_nor_manufacturer spi_nor_fujitsu;
extern const struct spi_nor_manufacturer spi_nor_gigadevice;
extern const struct spi_nor_manufacturer spi_nor_intel;
extern const struct spi_nor_manufacturer spi_nor_issi;
extern const struct spi_nor_manufacturer spi_nor_macronix;
extern const struct spi_nor_manufacturer spi_nor_micron;
extern const struct spi_nor_manufacturer spi_nor_st;
extern const struct spi_nor_manufacturer spi_nor_spansion;
extern const struct spi_nor_manufacturer spi_nor_sst;
extern const struct spi_nor_manufacturer spi_nor_winbond;
extern const struct spi_nor_manufacturer spi_nor_xilinx;
extern const struct spi_nor_manufacturer spi_nor_xmc;

extern const struct attribute_group *spi_nor_sysfs_groups[];

void spi_nor_spimem_setup_op(const struct spi_nor *nor,
			     struct spi_mem_op *op,
			     const enum spi_nor_protocol proto);
int spi_nor_write_enable(struct spi_nor *nor);
int spi_nor_write_disable(struct spi_nor *nor);
int spi_nor_set_4byte_addr_mode_en4b_ex4b(struct spi_nor *nor, bool enable);
int spi_nor_set_4byte_addr_mode_wren_en4b_ex4b(struct spi_nor *nor,
					       bool enable);
int spi_nor_set_4byte_addr_mode_brwr(struct spi_nor *nor, bool enable);
int spi_nor_set_4byte_addr_mode(struct spi_nor *nor, bool enable);
int spi_nor_wait_till_ready(struct spi_nor *nor);
int spi_nor_global_block_unlock(struct spi_nor *nor);
int spi_nor_prep_and_lock(struct spi_nor *nor);
void spi_nor_unlock_and_unprep(struct spi_nor *nor);
int spi_nor_sr1_bit6_quad_enable(struct spi_nor *nor);
int spi_nor_sr2_bit1_quad_enable(struct spi_nor *nor);
int spi_nor_sr2_bit7_quad_enable(struct spi_nor *nor);
int spi_nor_read_id(struct spi_nor *nor, u8 naddr, u8 ndummy, u8 *id,
		    enum spi_nor_protocol reg_proto);
int spi_nor_read_sr(struct spi_nor *nor, u8 *sr);
int spi_nor_sr_ready(struct spi_nor *nor);
int spi_nor_read_cr(struct spi_nor *nor, u8 *cr);
int spi_nor_write_sr(struct spi_nor *nor, const u8 *sr, size_t len);
int spi_nor_write_sr_and_check(struct spi_nor *nor, u8 sr1);
int spi_nor_write_16bit_cr_and_check(struct spi_nor *nor, u8 cr);

ssize_t spi_nor_read_data(struct spi_nor *nor, loff_t from, size_t len,
			  u8 *buf);
ssize_t spi_nor_write_data(struct spi_nor *nor, loff_t to, size_t len,
			   const u8 *buf);
int spi_nor_read_any_reg(struct spi_nor *nor, struct spi_mem_op *op,
			 enum spi_nor_protocol proto);
int spi_nor_write_any_volatile_reg(struct spi_nor *nor, struct spi_mem_op *op,
				   enum spi_nor_protocol proto);
int spi_nor_erase_sector(struct spi_nor *nor, u32 addr);

int spi_nor_otp_read_secr(struct spi_nor *nor, loff_t addr, size_t len, u8 *buf);
int spi_nor_otp_write_secr(struct spi_nor *nor, loff_t addr, size_t len,
			   const u8 *buf);
int spi_nor_otp_erase_secr(struct spi_nor *nor, loff_t addr);
int spi_nor_otp_lock_sr2(struct spi_nor *nor, unsigned int region);
int spi_nor_otp_is_locked_sr2(struct spi_nor *nor, unsigned int region);

int spi_nor_hwcaps_read2cmd(u32 hwcaps);
int spi_nor_hwcaps_pp2cmd(u32 hwcaps);
u8 spi_nor_convert_3to4_read(u8 opcode);
void spi_nor_set_read_settings(struct spi_nor_read_command *read,
			       u8 num_mode_clocks,
			       u8 num_wait_states,
			       u8 opcode,
			       enum spi_nor_protocol proto);
void spi_nor_set_pp_settings(struct spi_nor_pp_command *pp, u8 opcode,
			     enum spi_nor_protocol proto);

void spi_nor_set_erase_type(struct spi_nor_erase_type *erase, u32 size,
			    u8 opcode);
void spi_nor_mask_erase_type(struct spi_nor_erase_type *erase);
struct spi_nor_erase_region *
spi_nor_region_next(struct spi_nor_erase_region *region);
void spi_nor_init_uniform_erase_map(struct spi_nor_erase_map *map,
				    u8 erase_mask, u64 flash_size);

int spi_nor_post_bfpt_fixups(struct spi_nor *nor,
			     const struct sfdp_parameter_header *bfpt_header,
			     const struct sfdp_bfpt *bfpt);

void spi_nor_init_default_locking_ops(struct spi_nor *nor);
void spi_nor_try_unlock_all(struct spi_nor *nor);
void spi_nor_set_mtd_locking_ops(struct spi_nor *nor);
void spi_nor_set_mtd_otp_ops(struct spi_nor *nor);

int spi_nor_controller_ops_read_reg(struct spi_nor *nor, u8 opcode,
				    u8 *buf, size_t len);
int spi_nor_controller_ops_write_reg(struct spi_nor *nor, u8 opcode,
				     const u8 *buf, size_t len);

int spi_nor_check_sfdp_signature(struct spi_nor *nor);
int spi_nor_parse_sfdp(struct spi_nor *nor);

static inline struct spi_nor *mtd_to_spi_nor(struct mtd_info *mtd)
{
	return container_of(mtd, struct spi_nor, mtd);
}

#ifdef CONFIG_DEBUG_FS
void spi_nor_debugfs_register(struct spi_nor *nor);
void spi_nor_debugfs_shutdown(void);
#else
static inline void spi_nor_debugfs_register(struct spi_nor *nor) {}
static inline void spi_nor_debugfs_shutdown(void) {}
#endif

#endif  
