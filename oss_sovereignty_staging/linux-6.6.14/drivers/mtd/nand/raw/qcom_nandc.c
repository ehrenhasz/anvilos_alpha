
 
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/dma/qcom_adm.h>
#include <linux/dma/qcom_bam_dma.h>
#include <linux/module.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/rawnand.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

 
#define	NAND_FLASH_CMD			0x00
#define	NAND_ADDR0			0x04
#define	NAND_ADDR1			0x08
#define	NAND_FLASH_CHIP_SELECT		0x0c
#define	NAND_EXEC_CMD			0x10
#define	NAND_FLASH_STATUS		0x14
#define	NAND_BUFFER_STATUS		0x18
#define	NAND_DEV0_CFG0			0x20
#define	NAND_DEV0_CFG1			0x24
#define	NAND_DEV0_ECC_CFG		0x28
#define	NAND_AUTO_STATUS_EN		0x2c
#define	NAND_DEV1_CFG0			0x30
#define	NAND_DEV1_CFG1			0x34
#define	NAND_READ_ID			0x40
#define	NAND_READ_STATUS		0x44
#define	NAND_DEV_CMD0			0xa0
#define	NAND_DEV_CMD1			0xa4
#define	NAND_DEV_CMD2			0xa8
#define	NAND_DEV_CMD_VLD		0xac
#define	SFLASHC_BURST_CFG		0xe0
#define	NAND_ERASED_CW_DETECT_CFG	0xe8
#define	NAND_ERASED_CW_DETECT_STATUS	0xec
#define	NAND_EBI2_ECC_BUF_CFG		0xf0
#define	FLASH_BUF_ACC			0x100

#define	NAND_CTRL			0xf00
#define	NAND_VERSION			0xf08
#define	NAND_READ_LOCATION_0		0xf20
#define	NAND_READ_LOCATION_1		0xf24
#define	NAND_READ_LOCATION_2		0xf28
#define	NAND_READ_LOCATION_3		0xf2c
#define	NAND_READ_LOCATION_LAST_CW_0	0xf40
#define	NAND_READ_LOCATION_LAST_CW_1	0xf44
#define	NAND_READ_LOCATION_LAST_CW_2	0xf48
#define	NAND_READ_LOCATION_LAST_CW_3	0xf4c

 
#define	NAND_DEV_CMD1_RESTORE		0xdead
#define	NAND_DEV_CMD_VLD_RESTORE	0xbeef

 
#define	PAGE_ACC			BIT(4)
#define	LAST_PAGE			BIT(5)

 
#define	NAND_DEV_SEL			0
#define	DM_EN				BIT(2)

 
#define	FS_OP_ERR			BIT(4)
#define	FS_READY_BSY_N			BIT(5)
#define	FS_MPU_ERR			BIT(8)
#define	FS_DEVICE_STS_ERR		BIT(16)
#define	FS_DEVICE_WP			BIT(23)

 
#define	BS_UNCORRECTABLE_BIT		BIT(8)
#define	BS_CORRECTABLE_ERR_MSK		0x1f

 
#define	DISABLE_STATUS_AFTER_WRITE	4
#define	CW_PER_PAGE			6
#define	UD_SIZE_BYTES			9
#define	UD_SIZE_BYTES_MASK		GENMASK(18, 9)
#define	ECC_PARITY_SIZE_BYTES_RS	19
#define	SPARE_SIZE_BYTES		23
#define	SPARE_SIZE_BYTES_MASK		GENMASK(26, 23)
#define	NUM_ADDR_CYCLES			27
#define	STATUS_BFR_READ			30
#define	SET_RD_MODE_AFTER_STATUS	31

 
#define	DEV0_CFG1_ECC_DISABLE		0
#define	WIDE_FLASH			1
#define	NAND_RECOVERY_CYCLES		2
#define	CS_ACTIVE_BSY			5
#define	BAD_BLOCK_BYTE_NUM		6
#define	BAD_BLOCK_IN_SPARE_AREA		16
#define	WR_RD_BSY_GAP			17
#define	ENABLE_BCH_ECC			27

 
#define	ECC_CFG_ECC_DISABLE		0
#define	ECC_SW_RESET			1
#define	ECC_MODE			4
#define	ECC_PARITY_SIZE_BYTES_BCH	8
#define	ECC_NUM_DATA_BYTES		16
#define	ECC_NUM_DATA_BYTES_MASK		GENMASK(25, 16)
#define	ECC_FORCE_CLK_OPEN		30

 
#define	READ_ADDR			0

 
#define	READ_START_VLD			BIT(0)
#define	READ_STOP_VLD			BIT(1)
#define	WRITE_START_VLD			BIT(2)
#define	ERASE_START_VLD			BIT(3)
#define	SEQ_READ_START_VLD		BIT(4)

 
#define	NUM_STEPS			0

 
#define	ERASED_CW_ECC_MASK		1
#define	AUTO_DETECT_RES			0
#define	MASK_ECC			BIT(ERASED_CW_ECC_MASK)
#define	RESET_ERASED_DET		BIT(AUTO_DETECT_RES)
#define	ACTIVE_ERASED_DET		(0 << AUTO_DETECT_RES)
#define	CLR_ERASED_PAGE_DET		(RESET_ERASED_DET | MASK_ECC)
#define	SET_ERASED_PAGE_DET		(ACTIVE_ERASED_DET | MASK_ECC)

 
#define	PAGE_ALL_ERASED			BIT(7)
#define	CODEWORD_ALL_ERASED		BIT(6)
#define	PAGE_ERASED			BIT(5)
#define	CODEWORD_ERASED			BIT(4)
#define	ERASED_PAGE			(PAGE_ALL_ERASED | PAGE_ERASED)
#define	ERASED_CW			(CODEWORD_ALL_ERASED | CODEWORD_ERASED)

 
#define READ_LOCATION_OFFSET		0
#define READ_LOCATION_SIZE		16
#define READ_LOCATION_LAST		31

 
#define	NAND_VERSION_MAJOR_MASK		0xf0000000
#define	NAND_VERSION_MAJOR_SHIFT	28
#define	NAND_VERSION_MINOR_MASK		0x0fff0000
#define	NAND_VERSION_MINOR_SHIFT	16

 
#define	OP_PAGE_READ			0x2
#define	OP_PAGE_READ_WITH_ECC		0x3
#define	OP_PAGE_READ_WITH_ECC_SPARE	0x4
#define	OP_PAGE_READ_ONFI_READ		0x5
#define	OP_PROGRAM_PAGE			0x6
#define	OP_PAGE_PROGRAM_WITH_ECC	0x7
#define	OP_PROGRAM_PAGE_SPARE		0x9
#define	OP_BLOCK_ERASE			0xa
#define	OP_CHECK_STATUS			0xc
#define	OP_FETCH_ID			0xb
#define	OP_RESET_DEVICE			0xd

 
#define NAND_DEV_CMD_VLD_VAL		(READ_START_VLD | WRITE_START_VLD | \
					 ERASE_START_VLD | SEQ_READ_START_VLD)

 
#define	BAM_MODE_EN			BIT(0)

 
#define	NANDC_STEP_SIZE			512

 
#define	MAX_NUM_STEPS			(SZ_8K / NANDC_STEP_SIZE)

 
#define	MAX_REG_RD			(3 * MAX_NUM_STEPS)

 
#define	ECC_NONE	BIT(0)
#define	ECC_RS_4BIT	BIT(1)
#define	ECC_BCH_4BIT	BIT(2)
#define	ECC_BCH_8BIT	BIT(3)

#define nandc_set_read_loc_first(chip, reg, cw_offset, read_size, is_last_read_loc)	\
nandc_set_reg(chip, reg,			\
	      ((cw_offset) << READ_LOCATION_OFFSET) |		\
	      ((read_size) << READ_LOCATION_SIZE) |			\
	      ((is_last_read_loc) << READ_LOCATION_LAST))

#define nandc_set_read_loc_last(chip, reg, cw_offset, read_size, is_last_read_loc)	\
nandc_set_reg(chip, reg,			\
	      ((cw_offset) << READ_LOCATION_OFFSET) |		\
	      ((read_size) << READ_LOCATION_SIZE) |			\
	      ((is_last_read_loc) << READ_LOCATION_LAST))
 
#define dev_cmd_reg_addr(nandc, reg) ((nandc)->props->dev_cmd_reg_start + (reg))

 
#define nandc_reg_phys(chip, offset) ((chip)->base_phys + (offset))

 
#define reg_buf_dma_addr(chip, vaddr) \
	((chip)->reg_read_dma + \
	((u8 *)(vaddr) - (u8 *)(chip)->reg_read_buf))

#define QPIC_PER_CW_CMD_ELEMENTS	32
#define QPIC_PER_CW_CMD_SGL		32
#define QPIC_PER_CW_DATA_SGL		8

#define QPIC_NAND_COMPLETION_TIMEOUT	msecs_to_jiffies(2000)

 
 
#define NAND_BAM_NO_EOT			BIT(0)
 
#define NAND_BAM_NWD			BIT(1)
 
#define NAND_BAM_NEXT_SGL		BIT(2)
 
#define NAND_ERASED_CW_SET		BIT(4)

#define MAX_ADDRESS_CYCLE		5

 
struct bam_transaction {
	struct bam_cmd_element *bam_ce;
	struct scatterlist *cmd_sgl;
	struct scatterlist *data_sgl;
	struct dma_async_tx_descriptor *last_data_desc;
	struct dma_async_tx_descriptor *last_cmd_desc;
	struct completion txn_done;
	u32 bam_ce_pos;
	u32 bam_ce_start;
	u32 cmd_sgl_pos;
	u32 cmd_sgl_start;
	u32 tx_sgl_pos;
	u32 tx_sgl_start;
	u32 rx_sgl_pos;
	u32 rx_sgl_start;
	bool wait_second_completion;
};

 
struct desc_info {
	struct dma_async_tx_descriptor *dma_desc;
	struct list_head node;

	union {
		struct scatterlist adm_sgl;
		struct {
			struct scatterlist *bam_sgl;
			int sgl_cnt;
		};
	};
	enum dma_data_direction dir;
};

 
struct nandc_regs {
	__le32 cmd;
	__le32 addr0;
	__le32 addr1;
	__le32 chip_sel;
	__le32 exec;

	__le32 cfg0;
	__le32 cfg1;
	__le32 ecc_bch_cfg;

	__le32 clrflashstatus;
	__le32 clrreadstatus;

	__le32 cmd1;
	__le32 vld;

	__le32 orig_cmd1;
	__le32 orig_vld;

	__le32 ecc_buf_cfg;
	__le32 read_location0;
	__le32 read_location1;
	__le32 read_location2;
	__le32 read_location3;
	__le32 read_location_last0;
	__le32 read_location_last1;
	__le32 read_location_last2;
	__le32 read_location_last3;

	__le32 erased_cw_detect_cfg_clr;
	__le32 erased_cw_detect_cfg_set;
};

 
struct qcom_nand_controller {
	struct device *dev;

	void __iomem *base;

	struct clk *core_clk;
	struct clk *aon_clk;

	struct nandc_regs *regs;
	struct bam_transaction *bam_txn;

	const struct qcom_nandc_props *props;

	struct nand_controller controller;
	struct list_head host_list;

	union {
		 
		struct {
			struct dma_chan *tx_chan;
			struct dma_chan *rx_chan;
			struct dma_chan *cmd_chan;
		};

		 
		struct {
			struct dma_chan *chan;
			unsigned int cmd_crci;
			unsigned int data_crci;
		};
	};

	struct list_head desc_list;

	u8		*data_buffer;
	__le32		*reg_read_buf;

	phys_addr_t base_phys;
	dma_addr_t base_dma;
	dma_addr_t reg_read_dma;

	int		buf_size;
	int		buf_count;
	int		buf_start;
	unsigned int	max_cwperpage;

	int reg_read_pos;

	u32 cmd1, vld;
	bool exec_opwrite;
};

 
struct qcom_nand_boot_partition {
	u32 page_offset;
	u32 page_size;
};

 
struct qcom_op {
	const struct nand_op_instr *data_instr;
	unsigned int data_instr_idx;
	unsigned int rdy_timeout_ms;
	unsigned int rdy_delay_ns;
	u32 addr1_reg;
	u32 addr2_reg;
	u32 cmd_reg;
	u8 flag;
};

 
struct qcom_nand_host {
	struct qcom_nand_boot_partition *boot_partitions;

	struct nand_chip chip;
	struct list_head node;

	int nr_boot_partitions;

	int cs;
	int cw_size;
	int cw_data;
	int ecc_bytes_hw;
	int spare_bytes;
	int bbm_size;

	int last_command;

	u32 cfg0, cfg1;
	u32 cfg0_raw, cfg1_raw;
	u32 ecc_buf_cfg;
	u32 ecc_bch_cfg;
	u32 clrflashstatus;
	u32 clrreadstatus;

	u8 status;
	bool codeword_fixup;
	bool use_ecc;
	bool bch_enabled;
};

 
struct qcom_nandc_props {
	u32 ecc_modes;
	u32 dev_cmd_reg_start;
	bool is_bam;
	bool is_qpic;
	bool qpic_v2;
	bool use_codeword_fixup;
};

 
static void free_bam_transaction(struct qcom_nand_controller *nandc)
{
	struct bam_transaction *bam_txn = nandc->bam_txn;

	devm_kfree(nandc->dev, bam_txn);
}

 
static struct bam_transaction *
alloc_bam_transaction(struct qcom_nand_controller *nandc)
{
	struct bam_transaction *bam_txn;
	size_t bam_txn_size;
	unsigned int num_cw = nandc->max_cwperpage;
	void *bam_txn_buf;

	bam_txn_size =
		sizeof(*bam_txn) + num_cw *
		((sizeof(*bam_txn->bam_ce) * QPIC_PER_CW_CMD_ELEMENTS) +
		(sizeof(*bam_txn->cmd_sgl) * QPIC_PER_CW_CMD_SGL) +
		(sizeof(*bam_txn->data_sgl) * QPIC_PER_CW_DATA_SGL));

	bam_txn_buf = devm_kzalloc(nandc->dev, bam_txn_size, GFP_KERNEL);
	if (!bam_txn_buf)
		return NULL;

	bam_txn = bam_txn_buf;
	bam_txn_buf += sizeof(*bam_txn);

	bam_txn->bam_ce = bam_txn_buf;
	bam_txn_buf +=
		sizeof(*bam_txn->bam_ce) * QPIC_PER_CW_CMD_ELEMENTS * num_cw;

	bam_txn->cmd_sgl = bam_txn_buf;
	bam_txn_buf +=
		sizeof(*bam_txn->cmd_sgl) * QPIC_PER_CW_CMD_SGL * num_cw;

	bam_txn->data_sgl = bam_txn_buf;

	init_completion(&bam_txn->txn_done);

	return bam_txn;
}

 
static void clear_bam_transaction(struct qcom_nand_controller *nandc)
{
	struct bam_transaction *bam_txn = nandc->bam_txn;

	if (!nandc->props->is_bam)
		return;

	bam_txn->bam_ce_pos = 0;
	bam_txn->bam_ce_start = 0;
	bam_txn->cmd_sgl_pos = 0;
	bam_txn->cmd_sgl_start = 0;
	bam_txn->tx_sgl_pos = 0;
	bam_txn->tx_sgl_start = 0;
	bam_txn->rx_sgl_pos = 0;
	bam_txn->rx_sgl_start = 0;
	bam_txn->last_data_desc = NULL;
	bam_txn->wait_second_completion = false;

	sg_init_table(bam_txn->cmd_sgl, nandc->max_cwperpage *
		      QPIC_PER_CW_CMD_SGL);
	sg_init_table(bam_txn->data_sgl, nandc->max_cwperpage *
		      QPIC_PER_CW_DATA_SGL);

	reinit_completion(&bam_txn->txn_done);
}

 
static void qpic_bam_dma_done(void *data)
{
	struct bam_transaction *bam_txn = data;

	 
	if (bam_txn->wait_second_completion)
		bam_txn->wait_second_completion = false;
	else
		complete(&bam_txn->txn_done);
}

static inline struct qcom_nand_host *to_qcom_nand_host(struct nand_chip *chip)
{
	return container_of(chip, struct qcom_nand_host, chip);
}

static inline struct qcom_nand_controller *
get_qcom_nand_controller(struct nand_chip *chip)
{
	return container_of(chip->controller, struct qcom_nand_controller,
			    controller);
}

static inline u32 nandc_read(struct qcom_nand_controller *nandc, int offset)
{
	return ioread32(nandc->base + offset);
}

static inline void nandc_write(struct qcom_nand_controller *nandc, int offset,
			       u32 val)
{
	iowrite32(val, nandc->base + offset);
}

static inline void nandc_read_buffer_sync(struct qcom_nand_controller *nandc,
					  bool is_cpu)
{
	if (!nandc->props->is_bam)
		return;

	if (is_cpu)
		dma_sync_single_for_cpu(nandc->dev, nandc->reg_read_dma,
					MAX_REG_RD *
					sizeof(*nandc->reg_read_buf),
					DMA_FROM_DEVICE);
	else
		dma_sync_single_for_device(nandc->dev, nandc->reg_read_dma,
					   MAX_REG_RD *
					   sizeof(*nandc->reg_read_buf),
					   DMA_FROM_DEVICE);
}

static __le32 *offset_to_nandc_reg(struct nandc_regs *regs, int offset)
{
	switch (offset) {
	case NAND_FLASH_CMD:
		return &regs->cmd;
	case NAND_ADDR0:
		return &regs->addr0;
	case NAND_ADDR1:
		return &regs->addr1;
	case NAND_FLASH_CHIP_SELECT:
		return &regs->chip_sel;
	case NAND_EXEC_CMD:
		return &regs->exec;
	case NAND_FLASH_STATUS:
		return &regs->clrflashstatus;
	case NAND_DEV0_CFG0:
		return &regs->cfg0;
	case NAND_DEV0_CFG1:
		return &regs->cfg1;
	case NAND_DEV0_ECC_CFG:
		return &regs->ecc_bch_cfg;
	case NAND_READ_STATUS:
		return &regs->clrreadstatus;
	case NAND_DEV_CMD1:
		return &regs->cmd1;
	case NAND_DEV_CMD1_RESTORE:
		return &regs->orig_cmd1;
	case NAND_DEV_CMD_VLD:
		return &regs->vld;
	case NAND_DEV_CMD_VLD_RESTORE:
		return &regs->orig_vld;
	case NAND_EBI2_ECC_BUF_CFG:
		return &regs->ecc_buf_cfg;
	case NAND_READ_LOCATION_0:
		return &regs->read_location0;
	case NAND_READ_LOCATION_1:
		return &regs->read_location1;
	case NAND_READ_LOCATION_2:
		return &regs->read_location2;
	case NAND_READ_LOCATION_3:
		return &regs->read_location3;
	case NAND_READ_LOCATION_LAST_CW_0:
		return &regs->read_location_last0;
	case NAND_READ_LOCATION_LAST_CW_1:
		return &regs->read_location_last1;
	case NAND_READ_LOCATION_LAST_CW_2:
		return &regs->read_location_last2;
	case NAND_READ_LOCATION_LAST_CW_3:
		return &regs->read_location_last3;
	default:
		return NULL;
	}
}

static void nandc_set_reg(struct nand_chip *chip, int offset,
			  u32 val)
{
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	struct nandc_regs *regs = nandc->regs;
	__le32 *reg;

	reg = offset_to_nandc_reg(regs, offset);

	if (reg)
		*reg = cpu_to_le32(val);
}

 
static bool qcom_nandc_is_last_cw(struct nand_ecc_ctrl *ecc, int cw)
{
	return cw == (ecc->steps - 1);
}

 
static void nandc_set_read_loc(struct nand_chip *chip, int cw, int reg,
			       int cw_offset, int read_size, int is_last_read_loc)
{
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;
	int reg_base = NAND_READ_LOCATION_0;

	if (nandc->props->qpic_v2 && qcom_nandc_is_last_cw(ecc, cw))
		reg_base = NAND_READ_LOCATION_LAST_CW_0;

	reg_base += reg * 4;

	if (nandc->props->qpic_v2 && qcom_nandc_is_last_cw(ecc, cw))
		return nandc_set_read_loc_last(chip, reg_base, cw_offset,
				read_size, is_last_read_loc);
	else
		return nandc_set_read_loc_first(chip, reg_base, cw_offset,
				read_size, is_last_read_loc);
}

 
static void set_address(struct qcom_nand_host *host, u16 column, int page)
{
	struct nand_chip *chip = &host->chip;

	if (chip->options & NAND_BUSWIDTH_16)
		column >>= 1;

	nandc_set_reg(chip, NAND_ADDR0, page << 16 | column);
	nandc_set_reg(chip, NAND_ADDR1, page >> 16 & 0xff);
}

 
static void update_rw_regs(struct qcom_nand_host *host, int num_cw, bool read, int cw)
{
	struct nand_chip *chip = &host->chip;
	u32 cmd, cfg0, cfg1, ecc_bch_cfg;
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);

	if (read) {
		if (host->use_ecc)
			cmd = OP_PAGE_READ_WITH_ECC | PAGE_ACC | LAST_PAGE;
		else
			cmd = OP_PAGE_READ | PAGE_ACC | LAST_PAGE;
	} else {
		cmd = OP_PROGRAM_PAGE | PAGE_ACC | LAST_PAGE;
	}

	if (host->use_ecc) {
		cfg0 = (host->cfg0 & ~(7U << CW_PER_PAGE)) |
				(num_cw - 1) << CW_PER_PAGE;

		cfg1 = host->cfg1;
		ecc_bch_cfg = host->ecc_bch_cfg;
	} else {
		cfg0 = (host->cfg0_raw & ~(7U << CW_PER_PAGE)) |
				(num_cw - 1) << CW_PER_PAGE;

		cfg1 = host->cfg1_raw;
		ecc_bch_cfg = 1 << ECC_CFG_ECC_DISABLE;
	}

	nandc_set_reg(chip, NAND_FLASH_CMD, cmd);
	nandc_set_reg(chip, NAND_DEV0_CFG0, cfg0);
	nandc_set_reg(chip, NAND_DEV0_CFG1, cfg1);
	nandc_set_reg(chip, NAND_DEV0_ECC_CFG, ecc_bch_cfg);
	if (!nandc->props->qpic_v2)
		nandc_set_reg(chip, NAND_EBI2_ECC_BUF_CFG, host->ecc_buf_cfg);
	nandc_set_reg(chip, NAND_FLASH_STATUS, host->clrflashstatus);
	nandc_set_reg(chip, NAND_READ_STATUS, host->clrreadstatus);
	nandc_set_reg(chip, NAND_EXEC_CMD, 1);

	if (read)
		nandc_set_read_loc(chip, cw, 0, 0, host->use_ecc ?
				   host->cw_data : host->cw_size, 1);
}

 
static int prepare_bam_async_desc(struct qcom_nand_controller *nandc,
				  struct dma_chan *chan,
				  unsigned long flags)
{
	struct desc_info *desc;
	struct scatterlist *sgl;
	unsigned int sgl_cnt;
	int ret;
	struct bam_transaction *bam_txn = nandc->bam_txn;
	enum dma_transfer_direction dir_eng;
	struct dma_async_tx_descriptor *dma_desc;

	desc = kzalloc(sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	if (chan == nandc->cmd_chan) {
		sgl = &bam_txn->cmd_sgl[bam_txn->cmd_sgl_start];
		sgl_cnt = bam_txn->cmd_sgl_pos - bam_txn->cmd_sgl_start;
		bam_txn->cmd_sgl_start = bam_txn->cmd_sgl_pos;
		dir_eng = DMA_MEM_TO_DEV;
		desc->dir = DMA_TO_DEVICE;
	} else if (chan == nandc->tx_chan) {
		sgl = &bam_txn->data_sgl[bam_txn->tx_sgl_start];
		sgl_cnt = bam_txn->tx_sgl_pos - bam_txn->tx_sgl_start;
		bam_txn->tx_sgl_start = bam_txn->tx_sgl_pos;
		dir_eng = DMA_MEM_TO_DEV;
		desc->dir = DMA_TO_DEVICE;
	} else {
		sgl = &bam_txn->data_sgl[bam_txn->rx_sgl_start];
		sgl_cnt = bam_txn->rx_sgl_pos - bam_txn->rx_sgl_start;
		bam_txn->rx_sgl_start = bam_txn->rx_sgl_pos;
		dir_eng = DMA_DEV_TO_MEM;
		desc->dir = DMA_FROM_DEVICE;
	}

	sg_mark_end(sgl + sgl_cnt - 1);
	ret = dma_map_sg(nandc->dev, sgl, sgl_cnt, desc->dir);
	if (ret == 0) {
		dev_err(nandc->dev, "failure in mapping desc\n");
		kfree(desc);
		return -ENOMEM;
	}

	desc->sgl_cnt = sgl_cnt;
	desc->bam_sgl = sgl;

	dma_desc = dmaengine_prep_slave_sg(chan, sgl, sgl_cnt, dir_eng,
					   flags);

	if (!dma_desc) {
		dev_err(nandc->dev, "failure in prep desc\n");
		dma_unmap_sg(nandc->dev, sgl, sgl_cnt, desc->dir);
		kfree(desc);
		return -EINVAL;
	}

	desc->dma_desc = dma_desc;

	 
	if (chan == nandc->cmd_chan)
		bam_txn->last_cmd_desc = dma_desc;
	else
		bam_txn->last_data_desc = dma_desc;

	list_add_tail(&desc->node, &nandc->desc_list);

	return 0;
}

 
static int prep_bam_dma_desc_cmd(struct qcom_nand_controller *nandc, bool read,
				 int reg_off, const void *vaddr,
				 int size, unsigned int flags)
{
	int bam_ce_size;
	int i, ret;
	struct bam_cmd_element *bam_ce_buffer;
	struct bam_transaction *bam_txn = nandc->bam_txn;

	bam_ce_buffer = &bam_txn->bam_ce[bam_txn->bam_ce_pos];

	 
	for (i = 0; i < size; i++) {
		if (read)
			bam_prep_ce(&bam_ce_buffer[i],
				    nandc_reg_phys(nandc, reg_off + 4 * i),
				    BAM_READ_COMMAND,
				    reg_buf_dma_addr(nandc,
						     (__le32 *)vaddr + i));
		else
			bam_prep_ce_le32(&bam_ce_buffer[i],
					 nandc_reg_phys(nandc, reg_off + 4 * i),
					 BAM_WRITE_COMMAND,
					 *((__le32 *)vaddr + i));
	}

	bam_txn->bam_ce_pos += size;

	 
	if (flags & NAND_BAM_NEXT_SGL) {
		bam_ce_buffer = &bam_txn->bam_ce[bam_txn->bam_ce_start];
		bam_ce_size = (bam_txn->bam_ce_pos -
				bam_txn->bam_ce_start) *
				sizeof(struct bam_cmd_element);
		sg_set_buf(&bam_txn->cmd_sgl[bam_txn->cmd_sgl_pos],
			   bam_ce_buffer, bam_ce_size);
		bam_txn->cmd_sgl_pos++;
		bam_txn->bam_ce_start = bam_txn->bam_ce_pos;

		if (flags & NAND_BAM_NWD) {
			ret = prepare_bam_async_desc(nandc, nandc->cmd_chan,
						     DMA_PREP_FENCE |
						     DMA_PREP_CMD);
			if (ret)
				return ret;
		}
	}

	return 0;
}

 
static int prep_bam_dma_desc_data(struct qcom_nand_controller *nandc, bool read,
				  const void *vaddr,
				  int size, unsigned int flags)
{
	int ret;
	struct bam_transaction *bam_txn = nandc->bam_txn;

	if (read) {
		sg_set_buf(&bam_txn->data_sgl[bam_txn->rx_sgl_pos],
			   vaddr, size);
		bam_txn->rx_sgl_pos++;
	} else {
		sg_set_buf(&bam_txn->data_sgl[bam_txn->tx_sgl_pos],
			   vaddr, size);
		bam_txn->tx_sgl_pos++;

		 
		if (!(flags & NAND_BAM_NO_EOT)) {
			ret = prepare_bam_async_desc(nandc, nandc->tx_chan,
						     DMA_PREP_INTERRUPT);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int prep_adm_dma_desc(struct qcom_nand_controller *nandc, bool read,
			     int reg_off, const void *vaddr, int size,
			     bool flow_control)
{
	struct desc_info *desc;
	struct dma_async_tx_descriptor *dma_desc;
	struct scatterlist *sgl;
	struct dma_slave_config slave_conf;
	struct qcom_adm_peripheral_config periph_conf = {};
	enum dma_transfer_direction dir_eng;
	int ret;

	desc = kzalloc(sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	sgl = &desc->adm_sgl;

	sg_init_one(sgl, vaddr, size);

	if (read) {
		dir_eng = DMA_DEV_TO_MEM;
		desc->dir = DMA_FROM_DEVICE;
	} else {
		dir_eng = DMA_MEM_TO_DEV;
		desc->dir = DMA_TO_DEVICE;
	}

	ret = dma_map_sg(nandc->dev, sgl, 1, desc->dir);
	if (ret == 0) {
		ret = -ENOMEM;
		goto err;
	}

	memset(&slave_conf, 0x00, sizeof(slave_conf));

	slave_conf.device_fc = flow_control;
	if (read) {
		slave_conf.src_maxburst = 16;
		slave_conf.src_addr = nandc->base_dma + reg_off;
		if (nandc->data_crci) {
			periph_conf.crci = nandc->data_crci;
			slave_conf.peripheral_config = &periph_conf;
			slave_conf.peripheral_size = sizeof(periph_conf);
		}
	} else {
		slave_conf.dst_maxburst = 16;
		slave_conf.dst_addr = nandc->base_dma + reg_off;
		if (nandc->cmd_crci) {
			periph_conf.crci = nandc->cmd_crci;
			slave_conf.peripheral_config = &periph_conf;
			slave_conf.peripheral_size = sizeof(periph_conf);
		}
	}

	ret = dmaengine_slave_config(nandc->chan, &slave_conf);
	if (ret) {
		dev_err(nandc->dev, "failed to configure dma channel\n");
		goto err;
	}

	dma_desc = dmaengine_prep_slave_sg(nandc->chan, sgl, 1, dir_eng, 0);
	if (!dma_desc) {
		dev_err(nandc->dev, "failed to prepare desc\n");
		ret = -EINVAL;
		goto err;
	}

	desc->dma_desc = dma_desc;

	list_add_tail(&desc->node, &nandc->desc_list);

	return 0;
err:
	kfree(desc);

	return ret;
}

 
static int read_reg_dma(struct qcom_nand_controller *nandc, int first,
			int num_regs, unsigned int flags)
{
	bool flow_control = false;
	void *vaddr;

	vaddr = nandc->reg_read_buf + nandc->reg_read_pos;
	nandc->reg_read_pos += num_regs;

	if (first == NAND_DEV_CMD_VLD || first == NAND_DEV_CMD1)
		first = dev_cmd_reg_addr(nandc, first);

	if (nandc->props->is_bam)
		return prep_bam_dma_desc_cmd(nandc, true, first, vaddr,
					     num_regs, flags);

	if (first == NAND_READ_ID || first == NAND_FLASH_STATUS)
		flow_control = true;

	return prep_adm_dma_desc(nandc, true, first, vaddr,
				 num_regs * sizeof(u32), flow_control);
}

 
static int write_reg_dma(struct qcom_nand_controller *nandc, int first,
			 int num_regs, unsigned int flags)
{
	bool flow_control = false;
	struct nandc_regs *regs = nandc->regs;
	void *vaddr;

	vaddr = offset_to_nandc_reg(regs, first);

	if (first == NAND_ERASED_CW_DETECT_CFG) {
		if (flags & NAND_ERASED_CW_SET)
			vaddr = &regs->erased_cw_detect_cfg_set;
		else
			vaddr = &regs->erased_cw_detect_cfg_clr;
	}

	if (first == NAND_EXEC_CMD)
		flags |= NAND_BAM_NWD;

	if (first == NAND_DEV_CMD1_RESTORE || first == NAND_DEV_CMD1)
		first = dev_cmd_reg_addr(nandc, NAND_DEV_CMD1);

	if (first == NAND_DEV_CMD_VLD_RESTORE || first == NAND_DEV_CMD_VLD)
		first = dev_cmd_reg_addr(nandc, NAND_DEV_CMD_VLD);

	if (nandc->props->is_bam)
		return prep_bam_dma_desc_cmd(nandc, false, first, vaddr,
					     num_regs, flags);

	if (first == NAND_FLASH_CMD)
		flow_control = true;

	return prep_adm_dma_desc(nandc, false, first, vaddr,
				 num_regs * sizeof(u32), flow_control);
}

 
static int read_data_dma(struct qcom_nand_controller *nandc, int reg_off,
			 const u8 *vaddr, int size, unsigned int flags)
{
	if (nandc->props->is_bam)
		return prep_bam_dma_desc_data(nandc, true, vaddr, size, flags);

	return prep_adm_dma_desc(nandc, true, reg_off, vaddr, size, false);
}

 
static int write_data_dma(struct qcom_nand_controller *nandc, int reg_off,
			  const u8 *vaddr, int size, unsigned int flags)
{
	if (nandc->props->is_bam)
		return prep_bam_dma_desc_data(nandc, false, vaddr, size, flags);

	return prep_adm_dma_desc(nandc, false, reg_off, vaddr, size, false);
}

 
static void config_nand_page_read(struct nand_chip *chip)
{
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);

	write_reg_dma(nandc, NAND_ADDR0, 2, 0);
	write_reg_dma(nandc, NAND_DEV0_CFG0, 3, 0);
	if (!nandc->props->qpic_v2)
		write_reg_dma(nandc, NAND_EBI2_ECC_BUF_CFG, 1, 0);
	write_reg_dma(nandc, NAND_ERASED_CW_DETECT_CFG, 1, 0);
	write_reg_dma(nandc, NAND_ERASED_CW_DETECT_CFG, 1,
		      NAND_ERASED_CW_SET | NAND_BAM_NEXT_SGL);
}

 
static void
config_nand_cw_read(struct nand_chip *chip, bool use_ecc, int cw)
{
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;

	int reg = NAND_READ_LOCATION_0;

	if (nandc->props->qpic_v2 && qcom_nandc_is_last_cw(ecc, cw))
		reg = NAND_READ_LOCATION_LAST_CW_0;

	if (nandc->props->is_bam)
		write_reg_dma(nandc, reg, 4, NAND_BAM_NEXT_SGL);

	write_reg_dma(nandc, NAND_FLASH_CMD, 1, NAND_BAM_NEXT_SGL);
	write_reg_dma(nandc, NAND_EXEC_CMD, 1, NAND_BAM_NEXT_SGL);

	if (use_ecc) {
		read_reg_dma(nandc, NAND_FLASH_STATUS, 2, 0);
		read_reg_dma(nandc, NAND_ERASED_CW_DETECT_STATUS, 1,
			     NAND_BAM_NEXT_SGL);
	} else {
		read_reg_dma(nandc, NAND_FLASH_STATUS, 1, NAND_BAM_NEXT_SGL);
	}
}

 
static void
config_nand_single_cw_page_read(struct nand_chip *chip,
				bool use_ecc, int cw)
{
	config_nand_page_read(chip);
	config_nand_cw_read(chip, use_ecc, cw);
}

 
static void config_nand_page_write(struct nand_chip *chip)
{
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);

	write_reg_dma(nandc, NAND_ADDR0, 2, 0);
	write_reg_dma(nandc, NAND_DEV0_CFG0, 3, 0);
	if (!nandc->props->qpic_v2)
		write_reg_dma(nandc, NAND_EBI2_ECC_BUF_CFG, 1,
			      NAND_BAM_NEXT_SGL);
}

 
static void config_nand_cw_write(struct nand_chip *chip)
{
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);

	write_reg_dma(nandc, NAND_FLASH_CMD, 1, NAND_BAM_NEXT_SGL);
	write_reg_dma(nandc, NAND_EXEC_CMD, 1, NAND_BAM_NEXT_SGL);

	read_reg_dma(nandc, NAND_FLASH_STATUS, 1, NAND_BAM_NEXT_SGL);

	write_reg_dma(nandc, NAND_FLASH_STATUS, 1, 0);
	write_reg_dma(nandc, NAND_READ_STATUS, 1, NAND_BAM_NEXT_SGL);
}

 
static int submit_descs(struct qcom_nand_controller *nandc)
{
	struct desc_info *desc, *n;
	dma_cookie_t cookie = 0;
	struct bam_transaction *bam_txn = nandc->bam_txn;
	int ret = 0;

	if (nandc->props->is_bam) {
		if (bam_txn->rx_sgl_pos > bam_txn->rx_sgl_start) {
			ret = prepare_bam_async_desc(nandc, nandc->rx_chan, 0);
			if (ret)
				goto err_unmap_free_desc;
		}

		if (bam_txn->tx_sgl_pos > bam_txn->tx_sgl_start) {
			ret = prepare_bam_async_desc(nandc, nandc->tx_chan,
						   DMA_PREP_INTERRUPT);
			if (ret)
				goto err_unmap_free_desc;
		}

		if (bam_txn->cmd_sgl_pos > bam_txn->cmd_sgl_start) {
			ret = prepare_bam_async_desc(nandc, nandc->cmd_chan,
						   DMA_PREP_CMD);
			if (ret)
				goto err_unmap_free_desc;
		}
	}

	list_for_each_entry(desc, &nandc->desc_list, node)
		cookie = dmaengine_submit(desc->dma_desc);

	if (nandc->props->is_bam) {
		bam_txn->last_cmd_desc->callback = qpic_bam_dma_done;
		bam_txn->last_cmd_desc->callback_param = bam_txn;
		if (bam_txn->last_data_desc) {
			bam_txn->last_data_desc->callback = qpic_bam_dma_done;
			bam_txn->last_data_desc->callback_param = bam_txn;
			bam_txn->wait_second_completion = true;
		}

		dma_async_issue_pending(nandc->tx_chan);
		dma_async_issue_pending(nandc->rx_chan);
		dma_async_issue_pending(nandc->cmd_chan);

		if (!wait_for_completion_timeout(&bam_txn->txn_done,
						 QPIC_NAND_COMPLETION_TIMEOUT))
			ret = -ETIMEDOUT;
	} else {
		if (dma_sync_wait(nandc->chan, cookie) != DMA_COMPLETE)
			ret = -ETIMEDOUT;
	}

err_unmap_free_desc:
	 
	list_for_each_entry_safe(desc, n, &nandc->desc_list, node) {
		list_del(&desc->node);

		if (nandc->props->is_bam)
			dma_unmap_sg(nandc->dev, desc->bam_sgl,
				     desc->sgl_cnt, desc->dir);
		else
			dma_unmap_sg(nandc->dev, &desc->adm_sgl, 1,
				     desc->dir);

		kfree(desc);
	}

	return ret;
}

 
static void clear_read_regs(struct qcom_nand_controller *nandc)
{
	nandc->reg_read_pos = 0;
	nandc_read_buffer_sync(nandc, false);
}

 
static bool erased_chunk_check_and_fixup(u8 *data_buf, int data_len)
{
	u8 empty1, empty2;

	 

	empty1 = data_buf[3];
	empty2 = data_buf[175];

	 
	if ((empty1 == 0x54 && empty2 == 0xff) ||
	    (empty1 == 0xff && empty2 == 0x54)) {
		data_buf[3] = 0xff;
		data_buf[175] = 0xff;
	}

	 
	if (memchr_inv(data_buf, 0xff, data_len)) {
		data_buf[3] = empty1;
		data_buf[175] = empty2;

		return false;
	}

	return true;
}

struct read_stats {
	__le32 flash;
	__le32 buffer;
	__le32 erased_cw;
};

 
static int check_flash_errors(struct qcom_nand_host *host, int cw_cnt)
{
	struct nand_chip *chip = &host->chip;
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	int i;

	nandc_read_buffer_sync(nandc, true);

	for (i = 0; i < cw_cnt; i++) {
		u32 flash = le32_to_cpu(nandc->reg_read_buf[i]);

		if (flash & (FS_OP_ERR | FS_MPU_ERR))
			return -EIO;
	}

	return 0;
}

 
static int
qcom_nandc_read_cw_raw(struct mtd_info *mtd, struct nand_chip *chip,
		       u8 *data_buf, u8 *oob_buf, int page, int cw)
{
	struct qcom_nand_host *host = to_qcom_nand_host(chip);
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;
	int data_size1, data_size2, oob_size1, oob_size2;
	int ret, reg_off = FLASH_BUF_ACC, read_loc = 0;
	int raw_cw = cw;

	nand_read_page_op(chip, page, 0, NULL, 0);
	nandc->buf_count = 0;
	nandc->buf_start = 0;
	clear_read_regs(nandc);
	host->use_ecc = false;

	if (nandc->props->qpic_v2)
		raw_cw = ecc->steps - 1;

	clear_bam_transaction(nandc);
	set_address(host, host->cw_size * cw, page);
	update_rw_regs(host, 1, true, raw_cw);
	config_nand_page_read(chip);

	data_size1 = mtd->writesize - host->cw_size * (ecc->steps - 1);
	oob_size1 = host->bbm_size;

	if (qcom_nandc_is_last_cw(ecc, cw) && !host->codeword_fixup) {
		data_size2 = ecc->size - data_size1 -
			     ((ecc->steps - 1) * 4);
		oob_size2 = (ecc->steps * 4) + host->ecc_bytes_hw +
			    host->spare_bytes;
	} else {
		data_size2 = host->cw_data - data_size1;
		oob_size2 = host->ecc_bytes_hw + host->spare_bytes;
	}

	if (nandc->props->is_bam) {
		nandc_set_read_loc(chip, cw, 0, read_loc, data_size1, 0);
		read_loc += data_size1;

		nandc_set_read_loc(chip, cw, 1, read_loc, oob_size1, 0);
		read_loc += oob_size1;

		nandc_set_read_loc(chip, cw, 2, read_loc, data_size2, 0);
		read_loc += data_size2;

		nandc_set_read_loc(chip, cw, 3, read_loc, oob_size2, 1);
	}

	config_nand_cw_read(chip, false, raw_cw);

	read_data_dma(nandc, reg_off, data_buf, data_size1, 0);
	reg_off += data_size1;

	read_data_dma(nandc, reg_off, oob_buf, oob_size1, 0);
	reg_off += oob_size1;

	read_data_dma(nandc, reg_off, data_buf + data_size1, data_size2, 0);
	reg_off += data_size2;

	read_data_dma(nandc, reg_off, oob_buf + oob_size1, oob_size2, 0);

	ret = submit_descs(nandc);
	if (ret) {
		dev_err(nandc->dev, "failure to read raw cw %d\n", cw);
		return ret;
	}

	return check_flash_errors(host, 1);
}

 
static int
check_for_erased_page(struct qcom_nand_host *host, u8 *data_buf,
		      u8 *oob_buf, unsigned long uncorrectable_cws,
		      int page, unsigned int max_bitflips)
{
	struct nand_chip *chip = &host->chip;
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;
	u8 *cw_data_buf, *cw_oob_buf;
	int cw, data_size, oob_size, ret;

	if (!data_buf)
		data_buf = nand_get_data_buf(chip);

	if (!oob_buf) {
		nand_get_data_buf(chip);
		oob_buf = chip->oob_poi;
	}

	for_each_set_bit(cw, &uncorrectable_cws, ecc->steps) {
		if (qcom_nandc_is_last_cw(ecc, cw) && !host->codeword_fixup) {
			data_size = ecc->size - ((ecc->steps - 1) * 4);
			oob_size = (ecc->steps * 4) + host->ecc_bytes_hw;
		} else {
			data_size = host->cw_data;
			oob_size = host->ecc_bytes_hw;
		}

		 
		cw_data_buf = data_buf + (cw * host->cw_data);
		cw_oob_buf = oob_buf + (cw * ecc->bytes);

		ret = qcom_nandc_read_cw_raw(mtd, chip, cw_data_buf,
					     cw_oob_buf, page, cw);
		if (ret)
			return ret;

		 
		ret = nand_check_erased_ecc_chunk(cw_data_buf, data_size,
						  cw_oob_buf + host->bbm_size,
						  oob_size, NULL,
						  0, ecc->strength);
		if (ret < 0) {
			mtd->ecc_stats.failed++;
		} else {
			mtd->ecc_stats.corrected += ret;
			max_bitflips = max_t(unsigned int, max_bitflips, ret);
		}
	}

	return max_bitflips;
}

 
static int parse_read_errors(struct qcom_nand_host *host, u8 *data_buf,
			     u8 *oob_buf, int page)
{
	struct nand_chip *chip = &host->chip;
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;
	unsigned int max_bitflips = 0, uncorrectable_cws = 0;
	struct read_stats *buf;
	bool flash_op_err = false, erased;
	int i;
	u8 *data_buf_start = data_buf, *oob_buf_start = oob_buf;

	buf = (struct read_stats *)nandc->reg_read_buf;
	nandc_read_buffer_sync(nandc, true);

	for (i = 0; i < ecc->steps; i++, buf++) {
		u32 flash, buffer, erased_cw;
		int data_len, oob_len;

		if (qcom_nandc_is_last_cw(ecc, i)) {
			data_len = ecc->size - ((ecc->steps - 1) << 2);
			oob_len = ecc->steps << 2;
		} else {
			data_len = host->cw_data;
			oob_len = 0;
		}

		flash = le32_to_cpu(buf->flash);
		buffer = le32_to_cpu(buf->buffer);
		erased_cw = le32_to_cpu(buf->erased_cw);

		 
		if ((flash & FS_OP_ERR) && (buffer & BS_UNCORRECTABLE_BIT)) {
			 
			if (host->bch_enabled) {
				erased = (erased_cw & ERASED_CW) == ERASED_CW;
			 
			} else if (data_buf) {
				erased = erased_chunk_check_and_fixup(data_buf,
								      data_len);
			} else {
				erased = false;
			}

			if (!erased)
				uncorrectable_cws |= BIT(i);
		 
		} else if (flash & (FS_OP_ERR | FS_MPU_ERR)) {
			flash_op_err = true;
		 
		} else {
			unsigned int stat;

			stat = buffer & BS_CORRECTABLE_ERR_MSK;
			mtd->ecc_stats.corrected += stat;
			max_bitflips = max(max_bitflips, stat);
		}

		if (data_buf)
			data_buf += data_len;
		if (oob_buf)
			oob_buf += oob_len + ecc->bytes;
	}

	if (flash_op_err)
		return -EIO;

	if (!uncorrectable_cws)
		return max_bitflips;

	return check_for_erased_page(host, data_buf_start, oob_buf_start,
				     uncorrectable_cws, page,
				     max_bitflips);
}

 
static int read_page_ecc(struct qcom_nand_host *host, u8 *data_buf,
			 u8 *oob_buf, int page)
{
	struct nand_chip *chip = &host->chip;
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;
	u8 *data_buf_start = data_buf, *oob_buf_start = oob_buf;
	int i, ret;

	config_nand_page_read(chip);

	 
	for (i = 0; i < ecc->steps; i++) {
		int data_size, oob_size;

		if (qcom_nandc_is_last_cw(ecc, i) && !host->codeword_fixup) {
			data_size = ecc->size - ((ecc->steps - 1) << 2);
			oob_size = (ecc->steps << 2) + host->ecc_bytes_hw +
				   host->spare_bytes;
		} else {
			data_size = host->cw_data;
			oob_size = host->ecc_bytes_hw + host->spare_bytes;
		}

		if (nandc->props->is_bam) {
			if (data_buf && oob_buf) {
				nandc_set_read_loc(chip, i, 0, 0, data_size, 0);
				nandc_set_read_loc(chip, i, 1, data_size,
						   oob_size, 1);
			} else if (data_buf) {
				nandc_set_read_loc(chip, i, 0, 0, data_size, 1);
			} else {
				nandc_set_read_loc(chip, i, 0, data_size,
						   oob_size, 1);
			}
		}

		config_nand_cw_read(chip, true, i);

		if (data_buf)
			read_data_dma(nandc, FLASH_BUF_ACC, data_buf,
				      data_size, 0);

		 
		if (oob_buf) {
			int j;

			for (j = 0; j < host->bbm_size; j++)
				*oob_buf++ = 0xff;

			read_data_dma(nandc, FLASH_BUF_ACC + data_size,
				      oob_buf, oob_size, 0);
		}

		if (data_buf)
			data_buf += data_size;
		if (oob_buf)
			oob_buf += oob_size;
	}

	ret = submit_descs(nandc);
	if (ret) {
		dev_err(nandc->dev, "failure to read page/oob\n");
		return ret;
	}

	return parse_read_errors(host, data_buf_start, oob_buf_start, page);
}

 
static int copy_last_cw(struct qcom_nand_host *host, int page)
{
	struct nand_chip *chip = &host->chip;
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;
	int size;
	int ret;

	clear_read_regs(nandc);

	size = host->use_ecc ? host->cw_data : host->cw_size;

	 
	memset(nandc->data_buffer, 0xff, size);

	set_address(host, host->cw_size * (ecc->steps - 1), page);
	update_rw_regs(host, 1, true, ecc->steps - 1);

	config_nand_single_cw_page_read(chip, host->use_ecc, ecc->steps - 1);

	read_data_dma(nandc, FLASH_BUF_ACC, nandc->data_buffer, size, 0);

	ret = submit_descs(nandc);
	if (ret)
		dev_err(nandc->dev, "failed to copy last codeword\n");

	return ret;
}

static bool qcom_nandc_is_boot_partition(struct qcom_nand_host *host, int page)
{
	struct qcom_nand_boot_partition *boot_partition;
	u32 start, end;
	int i;

	 

	 
	boot_partition = &host->boot_partitions[host->nr_boot_partitions - 1];
	start = boot_partition->page_offset;
	end = start + boot_partition->page_size;

	 
	if (page > end)
		return false;

	 
	if (page < end && page >= start)
		return true;

	 
	for (i = host->nr_boot_partitions - 2; i >= 0; i--) {
		boot_partition = &host->boot_partitions[i];
		start = boot_partition->page_offset;
		end = start + boot_partition->page_size;

		if (page < end && page >= start)
			return true;
	}

	return false;
}

static void qcom_nandc_codeword_fixup(struct qcom_nand_host *host, int page)
{
	bool codeword_fixup = qcom_nandc_is_boot_partition(host, page);

	 
	if (codeword_fixup == host->codeword_fixup)
		return;

	host->codeword_fixup = codeword_fixup;

	host->cw_data = codeword_fixup ? 512 : 516;
	host->spare_bytes = host->cw_size - host->ecc_bytes_hw -
			    host->bbm_size - host->cw_data;

	host->cfg0 &= ~(SPARE_SIZE_BYTES_MASK | UD_SIZE_BYTES_MASK);
	host->cfg0 |= host->spare_bytes << SPARE_SIZE_BYTES |
		      host->cw_data << UD_SIZE_BYTES;

	host->ecc_bch_cfg &= ~ECC_NUM_DATA_BYTES_MASK;
	host->ecc_bch_cfg |= host->cw_data << ECC_NUM_DATA_BYTES;
	host->ecc_buf_cfg = (host->cw_data - 1) << NUM_STEPS;
}

 
static int qcom_nandc_read_page(struct nand_chip *chip, u8 *buf,
				int oob_required, int page)
{
	struct qcom_nand_host *host = to_qcom_nand_host(chip);
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;
	u8 *data_buf, *oob_buf = NULL;

	if (host->nr_boot_partitions)
		qcom_nandc_codeword_fixup(host, page);

	nand_read_page_op(chip, page, 0, NULL, 0);
	nandc->buf_count = 0;
	nandc->buf_start = 0;
	host->use_ecc = true;
	clear_read_regs(nandc);
	set_address(host, 0, page);
	update_rw_regs(host, ecc->steps, true, 0);

	data_buf = buf;
	oob_buf = oob_required ? chip->oob_poi : NULL;

	clear_bam_transaction(nandc);

	return read_page_ecc(host, data_buf, oob_buf, page);
}

 
static int qcom_nandc_read_page_raw(struct nand_chip *chip, u8 *buf,
				    int oob_required, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct qcom_nand_host *host = to_qcom_nand_host(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;
	int cw, ret;
	u8 *data_buf = buf, *oob_buf = chip->oob_poi;

	if (host->nr_boot_partitions)
		qcom_nandc_codeword_fixup(host, page);

	for (cw = 0; cw < ecc->steps; cw++) {
		ret = qcom_nandc_read_cw_raw(mtd, chip, data_buf, oob_buf,
					     page, cw);
		if (ret)
			return ret;

		data_buf += host->cw_data;
		oob_buf += ecc->bytes;
	}

	return 0;
}

 
static int qcom_nandc_read_oob(struct nand_chip *chip, int page)
{
	struct qcom_nand_host *host = to_qcom_nand_host(chip);
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;

	if (host->nr_boot_partitions)
		qcom_nandc_codeword_fixup(host, page);

	clear_read_regs(nandc);
	clear_bam_transaction(nandc);

	host->use_ecc = true;
	set_address(host, 0, page);
	update_rw_regs(host, ecc->steps, true, 0);

	return read_page_ecc(host, NULL, chip->oob_poi, page);
}

 
static int qcom_nandc_write_page(struct nand_chip *chip, const u8 *buf,
				 int oob_required, int page)
{
	struct qcom_nand_host *host = to_qcom_nand_host(chip);
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;
	u8 *data_buf, *oob_buf;
	int i, ret;

	if (host->nr_boot_partitions)
		qcom_nandc_codeword_fixup(host, page);

	nand_prog_page_begin_op(chip, page, 0, NULL, 0);

	set_address(host, 0, page);
	nandc->buf_count = 0;
	nandc->buf_start = 0;
	clear_read_regs(nandc);
	clear_bam_transaction(nandc);

	data_buf = (u8 *)buf;
	oob_buf = chip->oob_poi;

	host->use_ecc = true;
	update_rw_regs(host, ecc->steps, false, 0);
	config_nand_page_write(chip);

	for (i = 0; i < ecc->steps; i++) {
		int data_size, oob_size;

		if (qcom_nandc_is_last_cw(ecc, i) && !host->codeword_fixup) {
			data_size = ecc->size - ((ecc->steps - 1) << 2);
			oob_size = (ecc->steps << 2) + host->ecc_bytes_hw +
				   host->spare_bytes;
		} else {
			data_size = host->cw_data;
			oob_size = ecc->bytes;
		}

		write_data_dma(nandc, FLASH_BUF_ACC, data_buf, data_size,
			       i == (ecc->steps - 1) ? NAND_BAM_NO_EOT : 0);

		 
		if (qcom_nandc_is_last_cw(ecc, i)) {
			oob_buf += host->bbm_size;

			write_data_dma(nandc, FLASH_BUF_ACC + data_size,
				       oob_buf, oob_size, 0);
		}

		config_nand_cw_write(chip);

		data_buf += data_size;
		oob_buf += oob_size;
	}

	ret = submit_descs(nandc);
	if (ret) {
		dev_err(nandc->dev, "failure to write page\n");
		return ret;
	}

	return nand_prog_page_end_op(chip);
}

 
static int qcom_nandc_write_page_raw(struct nand_chip *chip,
				     const u8 *buf, int oob_required,
				     int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct qcom_nand_host *host = to_qcom_nand_host(chip);
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;
	u8 *data_buf, *oob_buf;
	int i, ret;

	if (host->nr_boot_partitions)
		qcom_nandc_codeword_fixup(host, page);

	nand_prog_page_begin_op(chip, page, 0, NULL, 0);
	clear_read_regs(nandc);
	clear_bam_transaction(nandc);

	data_buf = (u8 *)buf;
	oob_buf = chip->oob_poi;

	host->use_ecc = false;
	update_rw_regs(host, ecc->steps, false, 0);
	config_nand_page_write(chip);

	for (i = 0; i < ecc->steps; i++) {
		int data_size1, data_size2, oob_size1, oob_size2;
		int reg_off = FLASH_BUF_ACC;

		data_size1 = mtd->writesize - host->cw_size * (ecc->steps - 1);
		oob_size1 = host->bbm_size;

		if (qcom_nandc_is_last_cw(ecc, i) && !host->codeword_fixup) {
			data_size2 = ecc->size - data_size1 -
				     ((ecc->steps - 1) << 2);
			oob_size2 = (ecc->steps << 2) + host->ecc_bytes_hw +
				    host->spare_bytes;
		} else {
			data_size2 = host->cw_data - data_size1;
			oob_size2 = host->ecc_bytes_hw + host->spare_bytes;
		}

		write_data_dma(nandc, reg_off, data_buf, data_size1,
			       NAND_BAM_NO_EOT);
		reg_off += data_size1;
		data_buf += data_size1;

		write_data_dma(nandc, reg_off, oob_buf, oob_size1,
			       NAND_BAM_NO_EOT);
		reg_off += oob_size1;
		oob_buf += oob_size1;

		write_data_dma(nandc, reg_off, data_buf, data_size2,
			       NAND_BAM_NO_EOT);
		reg_off += data_size2;
		data_buf += data_size2;

		write_data_dma(nandc, reg_off, oob_buf, oob_size2, 0);
		oob_buf += oob_size2;

		config_nand_cw_write(chip);
	}

	ret = submit_descs(nandc);
	if (ret) {
		dev_err(nandc->dev, "failure to write raw page\n");
		return ret;
	}

	return nand_prog_page_end_op(chip);
}

 
static int qcom_nandc_write_oob(struct nand_chip *chip, int page)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct qcom_nand_host *host = to_qcom_nand_host(chip);
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;
	u8 *oob = chip->oob_poi;
	int data_size, oob_size;
	int ret;

	if (host->nr_boot_partitions)
		qcom_nandc_codeword_fixup(host, page);

	host->use_ecc = true;
	clear_bam_transaction(nandc);

	 
	data_size = ecc->size - ((ecc->steps - 1) << 2);
	oob_size = mtd->oobavail;

	memset(nandc->data_buffer, 0xff, host->cw_data);
	 
	mtd_ooblayout_get_databytes(mtd, nandc->data_buffer + data_size, oob,
				    0, mtd->oobavail);

	set_address(host, host->cw_size * (ecc->steps - 1), page);
	update_rw_regs(host, 1, false, 0);

	config_nand_page_write(chip);
	write_data_dma(nandc, FLASH_BUF_ACC,
		       nandc->data_buffer, data_size + oob_size, 0);
	config_nand_cw_write(chip);

	ret = submit_descs(nandc);
	if (ret) {
		dev_err(nandc->dev, "failure to write oob\n");
		return ret;
	}

	return nand_prog_page_end_op(chip);
}

static int qcom_nandc_block_bad(struct nand_chip *chip, loff_t ofs)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct qcom_nand_host *host = to_qcom_nand_host(chip);
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;
	int page, ret, bbpos, bad = 0;

	page = (int)(ofs >> chip->page_shift) & chip->pagemask;

	 
	host->use_ecc = false;

	clear_bam_transaction(nandc);
	ret = copy_last_cw(host, page);
	if (ret)
		goto err;

	if (check_flash_errors(host, 1)) {
		dev_warn(nandc->dev, "error when trying to read BBM\n");
		goto err;
	}

	bbpos = mtd->writesize - host->cw_size * (ecc->steps - 1);

	bad = nandc->data_buffer[bbpos] != 0xff;

	if (chip->options & NAND_BUSWIDTH_16)
		bad = bad || (nandc->data_buffer[bbpos + 1] != 0xff);
err:
	return bad;
}

static int qcom_nandc_block_markbad(struct nand_chip *chip, loff_t ofs)
{
	struct qcom_nand_host *host = to_qcom_nand_host(chip);
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;
	int page, ret;

	clear_read_regs(nandc);
	clear_bam_transaction(nandc);

	 
	memset(nandc->data_buffer, 0x00, host->cw_size);

	page = (int)(ofs >> chip->page_shift) & chip->pagemask;

	 
	host->use_ecc = false;
	set_address(host, host->cw_size * (ecc->steps - 1), page);
	update_rw_regs(host, 1, false, ecc->steps - 1);

	config_nand_page_write(chip);
	write_data_dma(nandc, FLASH_BUF_ACC,
		       nandc->data_buffer, host->cw_size, 0);
	config_nand_cw_write(chip);

	ret = submit_descs(nandc);
	if (ret) {
		dev_err(nandc->dev, "failure to update BBM\n");
		return ret;
	}

	return nand_prog_page_end_op(chip);
}

 
static int qcom_nand_ooblayout_ecc(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *oobregion)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct qcom_nand_host *host = to_qcom_nand_host(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;

	if (section > 1)
		return -ERANGE;

	if (!section) {
		oobregion->length = (ecc->bytes * (ecc->steps - 1)) +
				    host->bbm_size;
		oobregion->offset = 0;
	} else {
		oobregion->length = host->ecc_bytes_hw + host->spare_bytes;
		oobregion->offset = mtd->oobsize - oobregion->length;
	}

	return 0;
}

static int qcom_nand_ooblayout_free(struct mtd_info *mtd, int section,
				    struct mtd_oob_region *oobregion)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct qcom_nand_host *host = to_qcom_nand_host(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;

	if (section)
		return -ERANGE;

	oobregion->length = ecc->steps * 4;
	oobregion->offset = ((ecc->steps - 1) * ecc->bytes) + host->bbm_size;

	return 0;
}

static const struct mtd_ooblayout_ops qcom_nand_ooblayout_ops = {
	.ecc = qcom_nand_ooblayout_ecc,
	.free = qcom_nand_ooblayout_free,
};

static int
qcom_nandc_calc_ecc_bytes(int step_size, int strength)
{
	return strength == 4 ? 12 : 16;
}

NAND_ECC_CAPS_SINGLE(qcom_nandc_ecc_caps, qcom_nandc_calc_ecc_bytes,
		     NANDC_STEP_SIZE, 4, 8);

static int qcom_nand_attach_chip(struct nand_chip *chip)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct qcom_nand_host *host = to_qcom_nand_host(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	int cwperpage, bad_block_byte, ret;
	bool wide_bus;
	int ecc_mode = 1;

	 
	ecc->size = NANDC_STEP_SIZE;
	wide_bus = chip->options & NAND_BUSWIDTH_16 ? true : false;
	cwperpage = mtd->writesize / NANDC_STEP_SIZE;

	 
	ret = nand_ecc_choose_conf(chip, &qcom_nandc_ecc_caps,
				   mtd->oobsize - (cwperpage * 4));
	if (ret) {
		dev_err(nandc->dev, "No valid ECC settings possible\n");
		return ret;
	}

	if (ecc->strength >= 8) {
		 
		host->bch_enabled = true;
		ecc_mode = 1;

		if (wide_bus) {
			host->ecc_bytes_hw = 14;
			host->spare_bytes = 0;
			host->bbm_size = 2;
		} else {
			host->ecc_bytes_hw = 13;
			host->spare_bytes = 2;
			host->bbm_size = 1;
		}
	} else {
		 
		if (nandc->props->ecc_modes & ECC_BCH_4BIT) {
			 
			host->bch_enabled = true;
			ecc_mode = 0;

			if (wide_bus) {
				host->ecc_bytes_hw = 8;
				host->spare_bytes = 2;
				host->bbm_size = 2;
			} else {
				host->ecc_bytes_hw = 7;
				host->spare_bytes = 4;
				host->bbm_size = 1;
			}
		} else {
			 
			host->ecc_bytes_hw = 10;

			if (wide_bus) {
				host->spare_bytes = 0;
				host->bbm_size = 2;
			} else {
				host->spare_bytes = 1;
				host->bbm_size = 1;
			}
		}
	}

	 
	ecc->bytes = host->ecc_bytes_hw + host->spare_bytes + host->bbm_size;

	ecc->read_page		= qcom_nandc_read_page;
	ecc->read_page_raw	= qcom_nandc_read_page_raw;
	ecc->read_oob		= qcom_nandc_read_oob;
	ecc->write_page		= qcom_nandc_write_page;
	ecc->write_page_raw	= qcom_nandc_write_page_raw;
	ecc->write_oob		= qcom_nandc_write_oob;

	ecc->engine_type = NAND_ECC_ENGINE_TYPE_ON_HOST;

	mtd_set_ooblayout(mtd, &qcom_nand_ooblayout_ops);
	 
	if (nandc->props->is_bam)
		free_bam_transaction(nandc);

	nandc->max_cwperpage = max_t(unsigned int, nandc->max_cwperpage,
				     cwperpage);

	 
	if (nandc->props->is_bam) {
		nandc->bam_txn = alloc_bam_transaction(nandc);
		if (!nandc->bam_txn) {
			dev_err(nandc->dev,
				"failed to allocate bam transaction\n");
			return -ENOMEM;
		}
	}

	 
	host->cw_data = 516;

	 
	host->cw_size = host->cw_data + ecc->bytes;
	bad_block_byte = mtd->writesize - host->cw_size * (cwperpage - 1) + 1;

	host->cfg0 = (cwperpage - 1) << CW_PER_PAGE
				| host->cw_data << UD_SIZE_BYTES
				| 0 << DISABLE_STATUS_AFTER_WRITE
				| 5 << NUM_ADDR_CYCLES
				| host->ecc_bytes_hw << ECC_PARITY_SIZE_BYTES_RS
				| 0 << STATUS_BFR_READ
				| 1 << SET_RD_MODE_AFTER_STATUS
				| host->spare_bytes << SPARE_SIZE_BYTES;

	host->cfg1 = 7 << NAND_RECOVERY_CYCLES
				| 0 <<  CS_ACTIVE_BSY
				| bad_block_byte << BAD_BLOCK_BYTE_NUM
				| 0 << BAD_BLOCK_IN_SPARE_AREA
				| 2 << WR_RD_BSY_GAP
				| wide_bus << WIDE_FLASH
				| host->bch_enabled << ENABLE_BCH_ECC;

	host->cfg0_raw = (cwperpage - 1) << CW_PER_PAGE
				| host->cw_size << UD_SIZE_BYTES
				| 5 << NUM_ADDR_CYCLES
				| 0 << SPARE_SIZE_BYTES;

	host->cfg1_raw = 7 << NAND_RECOVERY_CYCLES
				| 0 << CS_ACTIVE_BSY
				| 17 << BAD_BLOCK_BYTE_NUM
				| 1 << BAD_BLOCK_IN_SPARE_AREA
				| 2 << WR_RD_BSY_GAP
				| wide_bus << WIDE_FLASH
				| 1 << DEV0_CFG1_ECC_DISABLE;

	host->ecc_bch_cfg = !host->bch_enabled << ECC_CFG_ECC_DISABLE
				| 0 << ECC_SW_RESET
				| host->cw_data << ECC_NUM_DATA_BYTES
				| 1 << ECC_FORCE_CLK_OPEN
				| ecc_mode << ECC_MODE
				| host->ecc_bytes_hw << ECC_PARITY_SIZE_BYTES_BCH;

	if (!nandc->props->qpic_v2)
		host->ecc_buf_cfg = 0x203 << NUM_STEPS;

	host->clrflashstatus = FS_READY_BSY_N;
	host->clrreadstatus = 0xc0;
	nandc->regs->erased_cw_detect_cfg_clr =
		cpu_to_le32(CLR_ERASED_PAGE_DET);
	nandc->regs->erased_cw_detect_cfg_set =
		cpu_to_le32(SET_ERASED_PAGE_DET);

	dev_dbg(nandc->dev,
		"cfg0 %x cfg1 %x ecc_buf_cfg %x ecc_bch cfg %x cw_size %d cw_data %d strength %d parity_bytes %d steps %d\n",
		host->cfg0, host->cfg1, host->ecc_buf_cfg, host->ecc_bch_cfg,
		host->cw_size, host->cw_data, ecc->strength, ecc->bytes,
		cwperpage);

	return 0;
}

static int qcom_op_cmd_mapping(struct nand_chip *chip, u8 opcode,
			       struct qcom_op *q_op)
{
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	struct qcom_nand_host *host = to_qcom_nand_host(chip);
	int cmd;

	switch (opcode) {
	case NAND_CMD_RESET:
		cmd = OP_RESET_DEVICE;
		break;
	case NAND_CMD_READID:
		cmd = OP_FETCH_ID;
		break;
	case NAND_CMD_PARAM:
		if (nandc->props->qpic_v2)
			cmd = OP_PAGE_READ_ONFI_READ;
		else
			cmd = OP_PAGE_READ;
		break;
	case NAND_CMD_ERASE1:
	case NAND_CMD_ERASE2:
		cmd = OP_BLOCK_ERASE;
		break;
	case NAND_CMD_STATUS:
		cmd = OP_CHECK_STATUS;
		break;
	case NAND_CMD_PAGEPROG:
		cmd = OP_PROGRAM_PAGE;
		q_op->flag = OP_PROGRAM_PAGE;
		nandc->exec_opwrite = true;
		break;
	case NAND_CMD_READ0:
	case NAND_CMD_READSTART:
		if (host->use_ecc)
			cmd = OP_PAGE_READ_WITH_ECC;
		else
			cmd = OP_PAGE_READ;
		break;
	default:
		dev_err(nandc->dev, "Opcode not supported: %u\n", opcode);
		return -EOPNOTSUPP;
	}

	return cmd;
}

 
static int qcom_parse_instructions(struct nand_chip *chip,
				    const struct nand_subop *subop,
				    struct qcom_op *q_op)
{
	const struct nand_op_instr *instr = NULL;
	unsigned int op_id;
	int i, ret;

	for (op_id = 0; op_id < subop->ninstrs; op_id++) {
		unsigned int offset, naddrs;
		const u8 *addrs;

		instr = &subop->instrs[op_id];

		switch (instr->type) {
		case NAND_OP_CMD_INSTR:
			ret = qcom_op_cmd_mapping(chip, instr->ctx.cmd.opcode, q_op);
			if (ret < 0)
				return ret;

			q_op->cmd_reg = ret;
			q_op->rdy_delay_ns = instr->delay_ns;
			break;

		case NAND_OP_ADDR_INSTR:
			offset = nand_subop_get_addr_start_off(subop, op_id);
			naddrs = nand_subop_get_num_addr_cyc(subop, op_id);
			addrs = &instr->ctx.addr.addrs[offset];

			for (i = 0; i < min_t(unsigned int, 4, naddrs); i++)
				q_op->addr1_reg |= addrs[i] << (i * 8);

			if (naddrs > 4)
				q_op->addr2_reg |= addrs[4];

			q_op->rdy_delay_ns = instr->delay_ns;
			break;

		case NAND_OP_DATA_IN_INSTR:
			q_op->data_instr = instr;
			q_op->data_instr_idx = op_id;
			q_op->rdy_delay_ns = instr->delay_ns;
			fallthrough;
		case NAND_OP_DATA_OUT_INSTR:
			q_op->rdy_delay_ns = instr->delay_ns;
			break;

		case NAND_OP_WAITRDY_INSTR:
			q_op->rdy_timeout_ms = instr->ctx.waitrdy.timeout_ms;
			q_op->rdy_delay_ns = instr->delay_ns;
			break;
		}
	}

	return 0;
}

static void qcom_delay_ns(unsigned int ns)
{
	if (!ns)
		return;

	if (ns < 10000)
		ndelay(ns);
	else
		udelay(DIV_ROUND_UP(ns, 1000));
}

static int qcom_wait_rdy_poll(struct nand_chip *chip, unsigned int time_ms)
{
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	unsigned long start = jiffies + msecs_to_jiffies(time_ms);
	u32 flash;

	nandc_read_buffer_sync(nandc, true);

	do {
		flash = le32_to_cpu(nandc->reg_read_buf[0]);
		if (flash & FS_READY_BSY_N)
			return 0;
		cpu_relax();
	} while (time_after(start, jiffies));

	dev_err(nandc->dev, "Timeout waiting for device to be ready:0x%08x\n", flash);

	return -ETIMEDOUT;
}

static int qcom_read_status_exec(struct nand_chip *chip,
				 const struct nand_subop *subop)
{
	struct qcom_nand_host *host = to_qcom_nand_host(chip);
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	struct nand_ecc_ctrl *ecc = &chip->ecc;
	struct qcom_op q_op = {};
	const struct nand_op_instr *instr = NULL;
	unsigned int op_id = 0;
	unsigned int len = 0;
	int ret, num_cw, i;
	u32 flash_status;

	host->status = NAND_STATUS_READY | NAND_STATUS_WP;

	ret = qcom_parse_instructions(chip, subop, &q_op);
	if (ret)
		return ret;

	num_cw = nandc->exec_opwrite ? ecc->steps : 1;
	nandc->exec_opwrite = false;

	nandc->buf_count = 0;
	nandc->buf_start = 0;
	host->use_ecc = false;

	clear_read_regs(nandc);
	clear_bam_transaction(nandc);

	nandc_set_reg(chip, NAND_FLASH_CMD, q_op.cmd_reg);
	nandc_set_reg(chip, NAND_EXEC_CMD, 1);

	write_reg_dma(nandc, NAND_FLASH_CMD, 1, NAND_BAM_NEXT_SGL);
	write_reg_dma(nandc, NAND_EXEC_CMD, 1, NAND_BAM_NEXT_SGL);
	read_reg_dma(nandc, NAND_FLASH_STATUS, 1, NAND_BAM_NEXT_SGL);

	ret = submit_descs(nandc);
	if (ret) {
		dev_err(nandc->dev, "failure in submitting status descriptor\n");
		goto err_out;
	}

	nandc_read_buffer_sync(nandc, true);

	for (i = 0; i < num_cw; i++) {
		flash_status = le32_to_cpu(nandc->reg_read_buf[i]);

		if (flash_status & FS_MPU_ERR)
			host->status &= ~NAND_STATUS_WP;

		if (flash_status & FS_OP_ERR ||
		    (i == (num_cw - 1) && (flash_status & FS_DEVICE_STS_ERR)))
			host->status |= NAND_STATUS_FAIL;
	}

	flash_status = host->status;
	instr = q_op.data_instr;
	op_id = q_op.data_instr_idx;
	len = nand_subop_get_data_len(subop, op_id);
	memcpy(instr->ctx.data.buf.in, &flash_status, len);

err_out:
	return ret;
}

static int qcom_read_id_type_exec(struct nand_chip *chip, const struct nand_subop *subop)
{
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	struct qcom_nand_host *host = to_qcom_nand_host(chip);
	struct qcom_op q_op = {};
	const struct nand_op_instr *instr = NULL;
	unsigned int op_id = 0;
	unsigned int len = 0;
	int ret;

	ret = qcom_parse_instructions(chip, subop, &q_op);
	if (ret)
		return ret;

	nandc->buf_count = 0;
	nandc->buf_start = 0;
	host->use_ecc = false;

	clear_read_regs(nandc);
	clear_bam_transaction(nandc);

	nandc_set_reg(chip, NAND_FLASH_CMD, q_op.cmd_reg);
	nandc_set_reg(chip, NAND_ADDR0, q_op.addr1_reg);
	nandc_set_reg(chip, NAND_ADDR1, q_op.addr2_reg);
	nandc_set_reg(chip, NAND_FLASH_CHIP_SELECT,
		      nandc->props->is_bam ? 0 : DM_EN);

	nandc_set_reg(chip, NAND_EXEC_CMD, 1);

	write_reg_dma(nandc, NAND_FLASH_CMD, 4, NAND_BAM_NEXT_SGL);
	write_reg_dma(nandc, NAND_EXEC_CMD, 1, NAND_BAM_NEXT_SGL);

	read_reg_dma(nandc, NAND_READ_ID, 1, NAND_BAM_NEXT_SGL);

	ret = submit_descs(nandc);
	if (ret) {
		dev_err(nandc->dev, "failure in submitting read id descriptor\n");
		goto err_out;
	}

	instr = q_op.data_instr;
	op_id = q_op.data_instr_idx;
	len = nand_subop_get_data_len(subop, op_id);

	nandc_read_buffer_sync(nandc, true);
	memcpy(instr->ctx.data.buf.in, nandc->reg_read_buf, len);

err_out:
	return ret;
}

static int qcom_misc_cmd_type_exec(struct nand_chip *chip, const struct nand_subop *subop)
{
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	struct qcom_nand_host *host = to_qcom_nand_host(chip);
	struct qcom_op q_op = {};
	int ret;
	int instrs = 1;

	ret = qcom_parse_instructions(chip, subop, &q_op);
	if (ret)
		return ret;

	if (q_op.flag == OP_PROGRAM_PAGE) {
		goto wait_rdy;
	} else if (q_op.cmd_reg == OP_BLOCK_ERASE) {
		q_op.cmd_reg |= PAGE_ACC | LAST_PAGE;
		nandc_set_reg(chip, NAND_ADDR0, q_op.addr1_reg);
		nandc_set_reg(chip, NAND_ADDR1, q_op.addr2_reg);
		nandc_set_reg(chip, NAND_DEV0_CFG0,
			      host->cfg0_raw & ~(7 << CW_PER_PAGE));
		nandc_set_reg(chip, NAND_DEV0_CFG1, host->cfg1_raw);
		instrs = 3;
	} else {
		return 0;
	}

	nandc->buf_count = 0;
	nandc->buf_start = 0;
	host->use_ecc = false;

	clear_read_regs(nandc);
	clear_bam_transaction(nandc);

	nandc_set_reg(chip, NAND_FLASH_CMD, q_op.cmd_reg);
	nandc_set_reg(chip, NAND_EXEC_CMD, 1);

	write_reg_dma(nandc, NAND_FLASH_CMD, instrs, NAND_BAM_NEXT_SGL);
	(q_op.cmd_reg == OP_BLOCK_ERASE) ? write_reg_dma(nandc, NAND_DEV0_CFG0,
	2, NAND_BAM_NEXT_SGL) : read_reg_dma(nandc,
	NAND_FLASH_STATUS, 1, NAND_BAM_NEXT_SGL);

	write_reg_dma(nandc, NAND_EXEC_CMD, 1, NAND_BAM_NEXT_SGL);
	read_reg_dma(nandc, NAND_FLASH_STATUS, 1, NAND_BAM_NEXT_SGL);

	ret = submit_descs(nandc);
	if (ret) {
		dev_err(nandc->dev, "failure in submitting misc descriptor\n");
		goto err_out;
	}

wait_rdy:
	qcom_delay_ns(q_op.rdy_delay_ns);
	ret = qcom_wait_rdy_poll(chip, q_op.rdy_timeout_ms);

err_out:
	return ret;
}

static int qcom_param_page_type_exec(struct nand_chip *chip,  const struct nand_subop *subop)
{
	struct qcom_nand_host *host = to_qcom_nand_host(chip);
	struct qcom_nand_controller *nandc = get_qcom_nand_controller(chip);
	struct qcom_op q_op = {};
	const struct nand_op_instr *instr = NULL;
	unsigned int op_id = 0;
	unsigned int len = 0;
	int ret;

	ret = qcom_parse_instructions(chip, subop, &q_op);
	if (ret)
		return ret;

	q_op.cmd_reg |= PAGE_ACC | LAST_PAGE;

	nandc->buf_count = 0;
	nandc->buf_start = 0;
	host->use_ecc = false;
	clear_read_regs(nandc);
	clear_bam_transaction(nandc);

	nandc_set_reg(chip, NAND_FLASH_CMD, q_op.cmd_reg);

	nandc_set_reg(chip, NAND_ADDR0, 0);
	nandc_set_reg(chip, NAND_ADDR1, 0);
	nandc_set_reg(chip, NAND_DEV0_CFG0, 0 << CW_PER_PAGE
					| 512 << UD_SIZE_BYTES
					| 5 << NUM_ADDR_CYCLES
					| 0 << SPARE_SIZE_BYTES);
	nandc_set_reg(chip, NAND_DEV0_CFG1, 7 << NAND_RECOVERY_CYCLES
					| 0 << CS_ACTIVE_BSY
					| 17 << BAD_BLOCK_BYTE_NUM
					| 1 << BAD_BLOCK_IN_SPARE_AREA
					| 2 << WR_RD_BSY_GAP
					| 0 << WIDE_FLASH
					| 1 << DEV0_CFG1_ECC_DISABLE);
	if (!nandc->props->qpic_v2)
		nandc_set_reg(chip, NAND_EBI2_ECC_BUF_CFG, 1 << ECC_CFG_ECC_DISABLE);

	 
	if (!nandc->props->qpic_v2) {
		nandc_set_reg(chip, NAND_DEV_CMD_VLD,
			      (nandc->vld & ~READ_START_VLD));
		nandc_set_reg(chip, NAND_DEV_CMD1,
			      (nandc->cmd1 & ~(0xFF << READ_ADDR))
			      | NAND_CMD_PARAM << READ_ADDR);
	}

	nandc_set_reg(chip, NAND_EXEC_CMD, 1);

	if (!nandc->props->qpic_v2) {
		nandc_set_reg(chip, NAND_DEV_CMD1_RESTORE, nandc->cmd1);
		nandc_set_reg(chip, NAND_DEV_CMD_VLD_RESTORE, nandc->vld);
	}

	instr = q_op.data_instr;
	op_id = q_op.data_instr_idx;
	len = nand_subop_get_data_len(subop, op_id);

	nandc_set_read_loc(chip, 0, 0, 0, len, 1);

	if (!nandc->props->qpic_v2) {
		write_reg_dma(nandc, NAND_DEV_CMD_VLD, 1, 0);
		write_reg_dma(nandc, NAND_DEV_CMD1, 1, NAND_BAM_NEXT_SGL);
	}

	nandc->buf_count = len;
	memset(nandc->data_buffer, 0xff, nandc->buf_count);

	config_nand_single_cw_page_read(chip, false, 0);

	read_data_dma(nandc, FLASH_BUF_ACC, nandc->data_buffer,
		      nandc->buf_count, 0);

	 
	if (!nandc->props->qpic_v2) {
		write_reg_dma(nandc, NAND_DEV_CMD1_RESTORE, 1, 0);
		write_reg_dma(nandc, NAND_DEV_CMD_VLD_RESTORE, 1, NAND_BAM_NEXT_SGL);
	}

	ret = submit_descs(nandc);
	if (ret) {
		dev_err(nandc->dev, "failure in submitting param page descriptor\n");
		goto err_out;
	}

	ret = qcom_wait_rdy_poll(chip, q_op.rdy_timeout_ms);
	if (ret)
		goto err_out;

	memcpy(instr->ctx.data.buf.in, nandc->data_buffer, len);

err_out:
	return ret;
}

static const struct nand_op_parser qcom_op_parser = NAND_OP_PARSER(
		NAND_OP_PARSER_PATTERN(
			qcom_read_id_type_exec,
			NAND_OP_PARSER_PAT_CMD_ELEM(false),
			NAND_OP_PARSER_PAT_ADDR_ELEM(false, MAX_ADDRESS_CYCLE),
			NAND_OP_PARSER_PAT_DATA_IN_ELEM(false, 8)),
		NAND_OP_PARSER_PATTERN(
			qcom_read_status_exec,
			NAND_OP_PARSER_PAT_CMD_ELEM(false),
			NAND_OP_PARSER_PAT_DATA_IN_ELEM(false, 1)),
		NAND_OP_PARSER_PATTERN(
			qcom_param_page_type_exec,
			NAND_OP_PARSER_PAT_CMD_ELEM(false),
			NAND_OP_PARSER_PAT_ADDR_ELEM(false, MAX_ADDRESS_CYCLE),
			NAND_OP_PARSER_PAT_WAITRDY_ELEM(true),
			NAND_OP_PARSER_PAT_DATA_IN_ELEM(false, 512)),
		NAND_OP_PARSER_PATTERN(
			qcom_misc_cmd_type_exec,
			NAND_OP_PARSER_PAT_CMD_ELEM(false),
			NAND_OP_PARSER_PAT_ADDR_ELEM(true, MAX_ADDRESS_CYCLE),
			NAND_OP_PARSER_PAT_CMD_ELEM(true),
			NAND_OP_PARSER_PAT_WAITRDY_ELEM(false)),
		);

static int qcom_check_op(struct nand_chip *chip,
			 const struct nand_operation *op)
{
	const struct nand_op_instr *instr;
	int op_id;

	for (op_id = 0; op_id < op->ninstrs; op_id++) {
		instr = &op->instrs[op_id];

		switch (instr->type) {
		case NAND_OP_CMD_INSTR:
			if (instr->ctx.cmd.opcode != NAND_CMD_RESET  &&
			    instr->ctx.cmd.opcode != NAND_CMD_READID &&
			    instr->ctx.cmd.opcode != NAND_CMD_PARAM  &&
			    instr->ctx.cmd.opcode != NAND_CMD_ERASE1 &&
			    instr->ctx.cmd.opcode != NAND_CMD_ERASE2 &&
			    instr->ctx.cmd.opcode != NAND_CMD_STATUS &&
			    instr->ctx.cmd.opcode != NAND_CMD_PAGEPROG &&
			    instr->ctx.cmd.opcode != NAND_CMD_READ0 &&
			    instr->ctx.cmd.opcode != NAND_CMD_READSTART)
				return -EOPNOTSUPP;
			break;
		default:
			break;
		}
	}

	return 0;
}

static int qcom_nand_exec_op(struct nand_chip *chip,
			     const struct nand_operation *op, bool check_only)
{
	if (check_only)
		return qcom_check_op(chip, op);

	return nand_op_parser_exec_op(chip, &qcom_op_parser, op, check_only);
}

static const struct nand_controller_ops qcom_nandc_ops = {
	.attach_chip = qcom_nand_attach_chip,
	.exec_op = qcom_nand_exec_op,
};

static void qcom_nandc_unalloc(struct qcom_nand_controller *nandc)
{
	if (nandc->props->is_bam) {
		if (!dma_mapping_error(nandc->dev, nandc->reg_read_dma))
			dma_unmap_single(nandc->dev, nandc->reg_read_dma,
					 MAX_REG_RD *
					 sizeof(*nandc->reg_read_buf),
					 DMA_FROM_DEVICE);

		if (nandc->tx_chan)
			dma_release_channel(nandc->tx_chan);

		if (nandc->rx_chan)
			dma_release_channel(nandc->rx_chan);

		if (nandc->cmd_chan)
			dma_release_channel(nandc->cmd_chan);
	} else {
		if (nandc->chan)
			dma_release_channel(nandc->chan);
	}
}

static int qcom_nandc_alloc(struct qcom_nand_controller *nandc)
{
	int ret;

	ret = dma_set_coherent_mask(nandc->dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(nandc->dev, "failed to set DMA mask\n");
		return ret;
	}

	 
	nandc->buf_size = 532;

	nandc->data_buffer = devm_kzalloc(nandc->dev, nandc->buf_size, GFP_KERNEL);
	if (!nandc->data_buffer)
		return -ENOMEM;

	nandc->regs = devm_kzalloc(nandc->dev, sizeof(*nandc->regs), GFP_KERNEL);
	if (!nandc->regs)
		return -ENOMEM;

	nandc->reg_read_buf = devm_kcalloc(nandc->dev, MAX_REG_RD,
					   sizeof(*nandc->reg_read_buf),
					   GFP_KERNEL);
	if (!nandc->reg_read_buf)
		return -ENOMEM;

	if (nandc->props->is_bam) {
		nandc->reg_read_dma =
			dma_map_single(nandc->dev, nandc->reg_read_buf,
				       MAX_REG_RD *
				       sizeof(*nandc->reg_read_buf),
				       DMA_FROM_DEVICE);
		if (dma_mapping_error(nandc->dev, nandc->reg_read_dma)) {
			dev_err(nandc->dev, "failed to DMA MAP reg buffer\n");
			return -EIO;
		}

		nandc->tx_chan = dma_request_chan(nandc->dev, "tx");
		if (IS_ERR(nandc->tx_chan)) {
			ret = PTR_ERR(nandc->tx_chan);
			nandc->tx_chan = NULL;
			dev_err_probe(nandc->dev, ret,
				      "tx DMA channel request failed\n");
			goto unalloc;
		}

		nandc->rx_chan = dma_request_chan(nandc->dev, "rx");
		if (IS_ERR(nandc->rx_chan)) {
			ret = PTR_ERR(nandc->rx_chan);
			nandc->rx_chan = NULL;
			dev_err_probe(nandc->dev, ret,
				      "rx DMA channel request failed\n");
			goto unalloc;
		}

		nandc->cmd_chan = dma_request_chan(nandc->dev, "cmd");
		if (IS_ERR(nandc->cmd_chan)) {
			ret = PTR_ERR(nandc->cmd_chan);
			nandc->cmd_chan = NULL;
			dev_err_probe(nandc->dev, ret,
				      "cmd DMA channel request failed\n");
			goto unalloc;
		}

		 
		nandc->max_cwperpage = 1;
		nandc->bam_txn = alloc_bam_transaction(nandc);
		if (!nandc->bam_txn) {
			dev_err(nandc->dev,
				"failed to allocate bam transaction\n");
			ret = -ENOMEM;
			goto unalloc;
		}
	} else {
		nandc->chan = dma_request_chan(nandc->dev, "rxtx");
		if (IS_ERR(nandc->chan)) {
			ret = PTR_ERR(nandc->chan);
			nandc->chan = NULL;
			dev_err_probe(nandc->dev, ret,
				      "rxtx DMA channel request failed\n");
			return ret;
		}
	}

	INIT_LIST_HEAD(&nandc->desc_list);
	INIT_LIST_HEAD(&nandc->host_list);

	nand_controller_init(&nandc->controller);
	nandc->controller.ops = &qcom_nandc_ops;

	return 0;
unalloc:
	qcom_nandc_unalloc(nandc);
	return ret;
}

 
static int qcom_nandc_setup(struct qcom_nand_controller *nandc)
{
	u32 nand_ctrl;

	 
	if (!nandc->props->is_qpic)
		nandc_write(nandc, SFLASHC_BURST_CFG, 0);

	if (!nandc->props->qpic_v2)
		nandc_write(nandc, dev_cmd_reg_addr(nandc, NAND_DEV_CMD_VLD),
			    NAND_DEV_CMD_VLD_VAL);

	 
	if (nandc->props->is_bam) {
		nand_ctrl = nandc_read(nandc, NAND_CTRL);

		 
		if (!(nand_ctrl & BAM_MODE_EN))
			nandc_write(nandc, NAND_CTRL, nand_ctrl | BAM_MODE_EN);
	} else {
		nandc_write(nandc, NAND_FLASH_CHIP_SELECT, DM_EN);
	}

	 
	if (!nandc->props->qpic_v2) {
		nandc->cmd1 = nandc_read(nandc, dev_cmd_reg_addr(nandc, NAND_DEV_CMD1));
		nandc->vld = NAND_DEV_CMD_VLD_VAL;
	}

	return 0;
}

static const char * const probes[] = { "cmdlinepart", "ofpart", "qcomsmem", NULL };

static int qcom_nand_host_parse_boot_partitions(struct qcom_nand_controller *nandc,
						struct qcom_nand_host *host,
						struct device_node *dn)
{
	struct nand_chip *chip = &host->chip;
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct qcom_nand_boot_partition *boot_partition;
	struct device *dev = nandc->dev;
	int partitions_count, i, j, ret;

	if (!of_property_present(dn, "qcom,boot-partitions"))
		return 0;

	partitions_count = of_property_count_u32_elems(dn, "qcom,boot-partitions");
	if (partitions_count <= 0) {
		dev_err(dev, "Error parsing boot partition\n");
		return partitions_count ? partitions_count : -EINVAL;
	}

	host->nr_boot_partitions = partitions_count / 2;
	host->boot_partitions = devm_kcalloc(dev, host->nr_boot_partitions,
					     sizeof(*host->boot_partitions), GFP_KERNEL);
	if (!host->boot_partitions) {
		host->nr_boot_partitions = 0;
		return -ENOMEM;
	}

	for (i = 0, j = 0; i < host->nr_boot_partitions; i++, j += 2) {
		boot_partition = &host->boot_partitions[i];

		ret = of_property_read_u32_index(dn, "qcom,boot-partitions", j,
						 &boot_partition->page_offset);
		if (ret) {
			dev_err(dev, "Error parsing boot partition offset at index %d\n", i);
			host->nr_boot_partitions = 0;
			return ret;
		}

		if (boot_partition->page_offset % mtd->writesize) {
			dev_err(dev, "Boot partition offset not multiple of writesize at index %i\n",
				i);
			host->nr_boot_partitions = 0;
			return -EINVAL;
		}
		 
		boot_partition->page_offset /= mtd->writesize;

		ret = of_property_read_u32_index(dn, "qcom,boot-partitions", j + 1,
						 &boot_partition->page_size);
		if (ret) {
			dev_err(dev, "Error parsing boot partition size at index %d\n", i);
			host->nr_boot_partitions = 0;
			return ret;
		}

		if (boot_partition->page_size % mtd->writesize) {
			dev_err(dev, "Boot partition size not multiple of writesize at index %i\n",
				i);
			host->nr_boot_partitions = 0;
			return -EINVAL;
		}
		 
		boot_partition->page_size /= mtd->writesize;
	}

	return 0;
}

static int qcom_nand_host_init_and_register(struct qcom_nand_controller *nandc,
					    struct qcom_nand_host *host,
					    struct device_node *dn)
{
	struct nand_chip *chip = &host->chip;
	struct mtd_info *mtd = nand_to_mtd(chip);
	struct device *dev = nandc->dev;
	int ret;

	ret = of_property_read_u32(dn, "reg", &host->cs);
	if (ret) {
		dev_err(dev, "can't get chip-select\n");
		return -ENXIO;
	}

	nand_set_flash_node(chip, dn);
	mtd->name = devm_kasprintf(dev, GFP_KERNEL, "qcom_nand.%d", host->cs);
	if (!mtd->name)
		return -ENOMEM;

	mtd->owner = THIS_MODULE;
	mtd->dev.parent = dev;

	 
	chip->legacy.block_bad		= qcom_nandc_block_bad;
	chip->legacy.block_markbad	= qcom_nandc_block_markbad;

	chip->controller = &nandc->controller;
	chip->options |= NAND_NO_SUBPAGE_WRITE | NAND_USES_DMA |
			 NAND_SKIP_BBTSCAN;

	 
	host->status = NAND_STATUS_READY | NAND_STATUS_WP;

	ret = nand_scan(chip, 1);
	if (ret)
		return ret;

	ret = mtd_device_parse_register(mtd, probes, NULL, NULL, 0);
	if (ret)
		goto err;

	if (nandc->props->use_codeword_fixup) {
		ret = qcom_nand_host_parse_boot_partitions(nandc, host, dn);
		if (ret)
			goto err;
	}

	return 0;

err:
	nand_cleanup(chip);
	return ret;
}

static int qcom_probe_nand_devices(struct qcom_nand_controller *nandc)
{
	struct device *dev = nandc->dev;
	struct device_node *dn = dev->of_node, *child;
	struct qcom_nand_host *host;
	int ret = -ENODEV;

	for_each_available_child_of_node(dn, child) {
		host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
		if (!host) {
			of_node_put(child);
			return -ENOMEM;
		}

		ret = qcom_nand_host_init_and_register(nandc, host, child);
		if (ret) {
			devm_kfree(dev, host);
			continue;
		}

		list_add_tail(&host->node, &nandc->host_list);
	}

	return ret;
}

 
static int qcom_nandc_parse_dt(struct platform_device *pdev)
{
	struct qcom_nand_controller *nandc = platform_get_drvdata(pdev);
	struct device_node *np = nandc->dev->of_node;
	int ret;

	if (!nandc->props->is_bam) {
		ret = of_property_read_u32(np, "qcom,cmd-crci",
					   &nandc->cmd_crci);
		if (ret) {
			dev_err(nandc->dev, "command CRCI unspecified\n");
			return ret;
		}

		ret = of_property_read_u32(np, "qcom,data-crci",
					   &nandc->data_crci);
		if (ret) {
			dev_err(nandc->dev, "data CRCI unspecified\n");
			return ret;
		}
	}

	return 0;
}

static int qcom_nandc_probe(struct platform_device *pdev)
{
	struct qcom_nand_controller *nandc;
	const void *dev_data;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret;

	nandc = devm_kzalloc(&pdev->dev, sizeof(*nandc), GFP_KERNEL);
	if (!nandc)
		return -ENOMEM;

	platform_set_drvdata(pdev, nandc);
	nandc->dev = dev;

	dev_data = of_device_get_match_data(dev);
	if (!dev_data) {
		dev_err(&pdev->dev, "failed to get device data\n");
		return -ENODEV;
	}

	nandc->props = dev_data;

	nandc->core_clk = devm_clk_get(dev, "core");
	if (IS_ERR(nandc->core_clk))
		return PTR_ERR(nandc->core_clk);

	nandc->aon_clk = devm_clk_get(dev, "aon");
	if (IS_ERR(nandc->aon_clk))
		return PTR_ERR(nandc->aon_clk);

	ret = qcom_nandc_parse_dt(pdev);
	if (ret)
		return ret;

	nandc->base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(nandc->base))
		return PTR_ERR(nandc->base);

	nandc->base_phys = res->start;
	nandc->base_dma = dma_map_resource(dev, res->start,
					   resource_size(res),
					   DMA_BIDIRECTIONAL, 0);
	if (dma_mapping_error(dev, nandc->base_dma))
		return -ENXIO;

	ret = clk_prepare_enable(nandc->core_clk);
	if (ret)
		goto err_core_clk;

	ret = clk_prepare_enable(nandc->aon_clk);
	if (ret)
		goto err_aon_clk;

	ret = qcom_nandc_alloc(nandc);
	if (ret)
		goto err_nandc_alloc;

	ret = qcom_nandc_setup(nandc);
	if (ret)
		goto err_setup;

	ret = qcom_probe_nand_devices(nandc);
	if (ret)
		goto err_setup;

	return 0;

err_setup:
	qcom_nandc_unalloc(nandc);
err_nandc_alloc:
	clk_disable_unprepare(nandc->aon_clk);
err_aon_clk:
	clk_disable_unprepare(nandc->core_clk);
err_core_clk:
	dma_unmap_resource(dev, nandc->base_dma, resource_size(res),
			   DMA_BIDIRECTIONAL, 0);
	return ret;
}

static void qcom_nandc_remove(struct platform_device *pdev)
{
	struct qcom_nand_controller *nandc = platform_get_drvdata(pdev);
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	struct qcom_nand_host *host;
	struct nand_chip *chip;
	int ret;

	list_for_each_entry(host, &nandc->host_list, node) {
		chip = &host->chip;
		ret = mtd_device_unregister(nand_to_mtd(chip));
		WARN_ON(ret);
		nand_cleanup(chip);
	}

	qcom_nandc_unalloc(nandc);

	clk_disable_unprepare(nandc->aon_clk);
	clk_disable_unprepare(nandc->core_clk);

	dma_unmap_resource(&pdev->dev, nandc->base_dma, resource_size(res),
			   DMA_BIDIRECTIONAL, 0);
}

static const struct qcom_nandc_props ipq806x_nandc_props = {
	.ecc_modes = (ECC_RS_4BIT | ECC_BCH_8BIT),
	.is_bam = false,
	.use_codeword_fixup = true,
	.dev_cmd_reg_start = 0x0,
};

static const struct qcom_nandc_props ipq4019_nandc_props = {
	.ecc_modes = (ECC_BCH_4BIT | ECC_BCH_8BIT),
	.is_bam = true,
	.is_qpic = true,
	.dev_cmd_reg_start = 0x0,
};

static const struct qcom_nandc_props ipq8074_nandc_props = {
	.ecc_modes = (ECC_BCH_4BIT | ECC_BCH_8BIT),
	.is_bam = true,
	.is_qpic = true,
	.dev_cmd_reg_start = 0x7000,
};

static const struct qcom_nandc_props sdx55_nandc_props = {
	.ecc_modes = (ECC_BCH_4BIT | ECC_BCH_8BIT),
	.is_bam = true,
	.is_qpic = true,
	.qpic_v2 = true,
	.dev_cmd_reg_start = 0x7000,
};

 
static const struct of_device_id qcom_nandc_of_match[] = {
	{
		.compatible = "qcom,ipq806x-nand",
		.data = &ipq806x_nandc_props,
	},
	{
		.compatible = "qcom,ipq4019-nand",
		.data = &ipq4019_nandc_props,
	},
	{
		.compatible = "qcom,ipq6018-nand",
		.data = &ipq8074_nandc_props,
	},
	{
		.compatible = "qcom,ipq8074-nand",
		.data = &ipq8074_nandc_props,
	},
	{
		.compatible = "qcom,sdx55-nand",
		.data = &sdx55_nandc_props,
	},
	{}
};
MODULE_DEVICE_TABLE(of, qcom_nandc_of_match);

static struct platform_driver qcom_nandc_driver = {
	.driver = {
		.name = "qcom-nandc",
		.of_match_table = qcom_nandc_of_match,
	},
	.probe   = qcom_nandc_probe,
	.remove_new = qcom_nandc_remove,
};
module_platform_driver(qcom_nandc_driver);

MODULE_AUTHOR("Archit Taneja <architt@codeaurora.org>");
MODULE_DESCRIPTION("Qualcomm NAND Controller driver");
MODULE_LICENSE("GPL v2");
