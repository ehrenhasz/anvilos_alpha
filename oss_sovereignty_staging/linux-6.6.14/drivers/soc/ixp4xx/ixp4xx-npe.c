
 

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/soc/ixp4xx/npe.h>
#include <linux/soc/ixp4xx/cpu.h>

#define DEBUG_MSG			0
#define DEBUG_FW			0

#define NPE_COUNT			3
#define MAX_RETRIES			1000	 
#define NPE_42X_DATA_SIZE		0x800	 
#define NPE_46X_DATA_SIZE		0x1000
#define NPE_A_42X_INSTR_SIZE		0x1000
#define NPE_B_AND_C_42X_INSTR_SIZE	0x800
#define NPE_46X_INSTR_SIZE		0x1000
#define REGS_SIZE			0x1000

#define NPE_PHYS_REG			32

#define FW_MAGIC			0xFEEDF00D
#define FW_BLOCK_TYPE_INSTR		0x0
#define FW_BLOCK_TYPE_DATA		0x1
#define FW_BLOCK_TYPE_EOF		0xF

 
#define CMD_NPE_STEP			0x01
#define CMD_NPE_START			0x02
#define CMD_NPE_STOP			0x03
#define CMD_NPE_CLR_PIPE		0x04
#define CMD_CLR_PROFILE_CNT		0x0C
#define CMD_RD_INS_MEM			0x10  
#define CMD_WR_INS_MEM			0x11
#define CMD_RD_DATA_MEM			0x12  
#define CMD_WR_DATA_MEM			0x13
#define CMD_RD_ECS_REG			0x14  
#define CMD_WR_ECS_REG			0x15

#define STAT_RUN			0x80000000
#define STAT_STOP			0x40000000
#define STAT_CLEAR			0x20000000
#define STAT_ECS_K			0x00800000  

#define NPE_STEVT			0x1B
#define NPE_STARTPC			0x1C
#define NPE_REGMAP			0x1E
#define NPE_CINDEX			0x1F

#define INSTR_WR_REG_SHORT		0x0000C000
#define INSTR_WR_REG_BYTE		0x00004000
#define INSTR_RD_FIFO			0x0F888220
#define INSTR_RESET_MBOX		0x0FAC8210

#define ECS_BG_CTXT_REG_0		0x00  
#define ECS_BG_CTXT_REG_1		0x01  
#define ECS_BG_CTXT_REG_2		0x02
#define ECS_PRI_1_CTXT_REG_0		0x04  
#define ECS_PRI_1_CTXT_REG_1		0x05  
#define ECS_PRI_1_CTXT_REG_2		0x06
#define ECS_PRI_2_CTXT_REG_0		0x08  
#define ECS_PRI_2_CTXT_REG_1		0x09  
#define ECS_PRI_2_CTXT_REG_2		0x0A
#define ECS_DBG_CTXT_REG_0		0x0C  
#define ECS_DBG_CTXT_REG_1		0x0D  
#define ECS_DBG_CTXT_REG_2		0x0E
#define ECS_INSTRUCT_REG		0x11  

#define ECS_REG_0_ACTIVE		0x80000000  
#define ECS_REG_0_NEXTPC_MASK		0x1FFF0000  
#define ECS_REG_0_LDUR_BITS		8
#define ECS_REG_0_LDUR_MASK		0x00000700  
#define ECS_REG_1_CCTXT_BITS		16
#define ECS_REG_1_CCTXT_MASK		0x000F0000  
#define ECS_REG_1_SELCTXT_BITS		0
#define ECS_REG_1_SELCTXT_MASK		0x0000000F  
#define ECS_DBG_REG_2_IF		0x00100000  
#define ECS_DBG_REG_2_IE		0x00080000  

 
#define WFIFO_VALID			0x80000000

 
#define MSGSTAT_OFNE	0x00010000  
#define MSGSTAT_IFNF	0x00020000  
#define MSGSTAT_OFNF	0x00040000  
#define MSGSTAT_IFNE	0x00080000  
#define MSGSTAT_MBINT	0x00100000  
#define MSGSTAT_IFINT	0x00200000  
#define MSGSTAT_OFINT	0x00400000  
#define MSGSTAT_WFINT	0x00800000  

 
#define MSGCTL_OUT_FIFO			0x00010000  
#define MSGCTL_IN_FIFO			0x00020000  
#define MSGCTL_OUT_FIFO_WRITE		0x01000000  
#define MSGCTL_IN_FIFO_WRITE		0x02000000

 
#define RESET_MBOX_STAT			0x0000F0F0

#define NPE_A_FIRMWARE "NPE-A"
#define NPE_B_FIRMWARE "NPE-B"
#define NPE_C_FIRMWARE "NPE-C"

const char *npe_names[] = { NPE_A_FIRMWARE, NPE_B_FIRMWARE, NPE_C_FIRMWARE };

#define print_npe(pri, npe, fmt, ...)					\
	printk(pri "%s: " fmt, npe_name(npe), ## __VA_ARGS__)

#if DEBUG_MSG
#define debug_msg(npe, fmt, ...)					\
	print_npe(KERN_DEBUG, npe, fmt, ## __VA_ARGS__)
#else
#define debug_msg(npe, fmt, ...)
#endif

static struct {
	u32 reg, val;
} ecs_reset[] = {
	{ ECS_BG_CTXT_REG_0,	0xA0000000 },
	{ ECS_BG_CTXT_REG_1,	0x01000000 },
	{ ECS_BG_CTXT_REG_2,	0x00008000 },
	{ ECS_PRI_1_CTXT_REG_0,	0x20000080 },
	{ ECS_PRI_1_CTXT_REG_1,	0x01000000 },
	{ ECS_PRI_1_CTXT_REG_2,	0x00008000 },
	{ ECS_PRI_2_CTXT_REG_0,	0x20000080 },
	{ ECS_PRI_2_CTXT_REG_1,	0x01000000 },
	{ ECS_PRI_2_CTXT_REG_2,	0x00008000 },
	{ ECS_DBG_CTXT_REG_0,	0x20000000 },
	{ ECS_DBG_CTXT_REG_1,	0x00000000 },
	{ ECS_DBG_CTXT_REG_2,	0x001E0000 },
	{ ECS_INSTRUCT_REG,	0x1003C00F },
};

static struct npe npe_tab[NPE_COUNT] = {
	{
		.id	= 0,
	}, {
		.id	= 1,
	}, {
		.id	= 2,
	}
};

int npe_running(struct npe *npe)
{
	return (__raw_readl(&npe->regs->exec_status_cmd) & STAT_RUN) != 0;
}

static void npe_cmd_write(struct npe *npe, u32 addr, int cmd, u32 data)
{
	__raw_writel(data, &npe->regs->exec_data);
	__raw_writel(addr, &npe->regs->exec_addr);
	__raw_writel(cmd, &npe->regs->exec_status_cmd);
}

static u32 npe_cmd_read(struct npe *npe, u32 addr, int cmd)
{
	__raw_writel(addr, &npe->regs->exec_addr);
	__raw_writel(cmd, &npe->regs->exec_status_cmd);
	 
	__raw_readl(&npe->regs->exec_data);
	__raw_readl(&npe->regs->exec_data);
	return __raw_readl(&npe->regs->exec_data);
}

static void npe_clear_active(struct npe *npe, u32 reg)
{
	u32 val = npe_cmd_read(npe, reg, CMD_RD_ECS_REG);
	npe_cmd_write(npe, reg, CMD_WR_ECS_REG, val & ~ECS_REG_0_ACTIVE);
}

static void npe_start(struct npe *npe)
{
	 
	npe_clear_active(npe, ECS_PRI_1_CTXT_REG_0);
	npe_clear_active(npe, ECS_PRI_2_CTXT_REG_0);
	npe_clear_active(npe, ECS_DBG_CTXT_REG_0);

	__raw_writel(CMD_NPE_CLR_PIPE, &npe->regs->exec_status_cmd);
	__raw_writel(CMD_NPE_START, &npe->regs->exec_status_cmd);
}

static void npe_stop(struct npe *npe)
{
	__raw_writel(CMD_NPE_STOP, &npe->regs->exec_status_cmd);
	__raw_writel(CMD_NPE_CLR_PIPE, &npe->regs->exec_status_cmd);  
}

static int __must_check npe_debug_instr(struct npe *npe, u32 instr, u32 ctx,
					u32 ldur)
{
	u32 wc;
	int i;

	 
	npe_cmd_write(npe, ECS_DBG_CTXT_REG_0, CMD_WR_ECS_REG,
		      ECS_REG_0_ACTIVE | (ldur << ECS_REG_0_LDUR_BITS));

	 
	npe_cmd_write(npe, ECS_DBG_CTXT_REG_1, CMD_WR_ECS_REG,
		      (ctx << ECS_REG_1_CCTXT_BITS) |
		      (ctx << ECS_REG_1_SELCTXT_BITS));

	 
	__raw_writel(CMD_NPE_CLR_PIPE, &npe->regs->exec_status_cmd);

	 
	npe_cmd_write(npe, ECS_INSTRUCT_REG, CMD_WR_ECS_REG, instr);

	 
	wc = __raw_readl(&npe->regs->watch_count);

	 
	__raw_writel(CMD_NPE_STEP, &npe->regs->exec_status_cmd);

	 
	for (i = 0; i < MAX_RETRIES; i++) {
		if (wc != __raw_readl(&npe->regs->watch_count))
			return 0;
		udelay(1);
	}

	print_npe(KERN_ERR, npe, "reset: npe_debug_instr(): timeout\n");
	return -ETIMEDOUT;
}

static int __must_check npe_logical_reg_write8(struct npe *npe, u32 addr,
					       u8 val, u32 ctx)
{
	 
	u32 instr = INSTR_WR_REG_BYTE |	 
		addr << 9 |		 
		(val & 0x1F) << 4 |	 
		(val & ~0x1F) << (18 - 5); 
	return npe_debug_instr(npe, instr, ctx, 1);  
}

static int __must_check npe_logical_reg_write16(struct npe *npe, u32 addr,
						u16 val, u32 ctx)
{
	 
	u32 instr = INSTR_WR_REG_SHORT |  
		addr << 9 |		 
		(val & 0x1F) << 4 |	 
		(val & ~0x1F) << (18 - 5); 
	return npe_debug_instr(npe, instr, ctx, 1);  
}

static int __must_check npe_logical_reg_write32(struct npe *npe, u32 addr,
						u32 val, u32 ctx)
{
	 
	if (npe_logical_reg_write16(npe, addr, val >> 16, ctx))
		return -ETIMEDOUT;
	return npe_logical_reg_write16(npe, addr + 2, val & 0xFFFF, ctx);
}

static int npe_reset(struct npe *npe)
{
	u32 reset_bit = (IXP4XX_FEATURE_RESET_NPEA << npe->id);
	u32 val, ctl, exec_count, ctx_reg2;
	int i;

	ctl = (__raw_readl(&npe->regs->messaging_control) | 0x3F000000) &
		0x3F3FFFFF;

	 
	__raw_writel(ctl & 0x3F00FFFF, &npe->regs->messaging_control);

	 
	 
	exec_count = __raw_readl(&npe->regs->exec_count);
	__raw_writel(0, &npe->regs->exec_count);
	 
	ctx_reg2 = npe_cmd_read(npe, ECS_DBG_CTXT_REG_2, CMD_RD_ECS_REG);
	npe_cmd_write(npe, ECS_DBG_CTXT_REG_2, CMD_WR_ECS_REG, ctx_reg2 |
		      ECS_DBG_REG_2_IF | ECS_DBG_REG_2_IE);

	 
	while (__raw_readl(&npe->regs->watchpoint_fifo) & WFIFO_VALID)
		;
	while (__raw_readl(&npe->regs->messaging_status) & MSGSTAT_OFNE)
		 
		print_npe(KERN_DEBUG, npe, "npe_reset: read FIFO = 0x%X\n",
			  __raw_readl(&npe->regs->in_out_fifo));

	while (__raw_readl(&npe->regs->messaging_status) & MSGSTAT_IFNE)
		 
		if (npe_debug_instr(npe, INSTR_RD_FIFO, 0, 0))
			return -ETIMEDOUT;

	 
	__raw_writel(RESET_MBOX_STAT, &npe->regs->mailbox_status);
	 
	if (npe_debug_instr(npe, INSTR_RESET_MBOX, 0, 0))
		return -ETIMEDOUT;

	 
	for (val = 0; val < NPE_PHYS_REG; val++) {
		if (npe_logical_reg_write16(npe, NPE_REGMAP, val >> 1, 0))
			return -ETIMEDOUT;
		 
		if (npe_logical_reg_write32(npe, (val & 1) * 4, 0, 0))
			return -ETIMEDOUT;
	}

	 

	 
	val = npe_cmd_read(npe, ECS_BG_CTXT_REG_0, CMD_RD_ECS_REG);
	val &= ~ECS_REG_0_NEXTPC_MASK;
	val |= (0   << 16) & ECS_REG_0_NEXTPC_MASK;
	npe_cmd_write(npe, ECS_BG_CTXT_REG_0, CMD_WR_ECS_REG, val);

	for (i = 0; i < 16; i++) {
		if (i) {	 
			 
			if (npe_logical_reg_write8(npe, NPE_STEVT, 0x80, i))
				return -ETIMEDOUT;
			if (npe_logical_reg_write16(npe, NPE_STARTPC, 0, i))
				return -ETIMEDOUT;
		}
		 
		if (npe_logical_reg_write16(npe, NPE_REGMAP, 0x820, i))
			return -ETIMEDOUT;
		if (npe_logical_reg_write8(npe, NPE_CINDEX, 0, i))
			return -ETIMEDOUT;
	}

	 
	 
	npe_cmd_write(npe, ECS_DBG_CTXT_REG_0, CMD_WR_ECS_REG, 0);
	 
	__raw_writel(CMD_NPE_CLR_PIPE, &npe->regs->exec_status_cmd);
	 
	__raw_writel(exec_count, &npe->regs->exec_count);
	npe_cmd_write(npe, ECS_DBG_CTXT_REG_2, CMD_WR_ECS_REG, ctx_reg2);

	 
	for (val = 0; val < ARRAY_SIZE(ecs_reset); val++)
		npe_cmd_write(npe, ecs_reset[val].reg, CMD_WR_ECS_REG,
			      ecs_reset[val].val);

	 
	__raw_writel(CMD_CLR_PROFILE_CNT, &npe->regs->exec_status_cmd);

	__raw_writel(0, &npe->regs->exec_count);
	__raw_writel(0, &npe->regs->action_points[0]);
	__raw_writel(0, &npe->regs->action_points[1]);
	__raw_writel(0, &npe->regs->action_points[2]);
	__raw_writel(0, &npe->regs->action_points[3]);
	__raw_writel(0, &npe->regs->watch_count);

	 
	val = cpu_ixp4xx_features(npe->rmap);
	 
	regmap_write(npe->rmap, IXP4XX_EXP_CNFG2, val & ~reset_bit);
	 
	regmap_write(npe->rmap, IXP4XX_EXP_CNFG2, val | reset_bit);

	for (i = 0; i < MAX_RETRIES; i++) {
		val = cpu_ixp4xx_features(npe->rmap);
		if (val & reset_bit)
			break;	 
		udelay(1);
	}
	if (i == MAX_RETRIES)
		return -ETIMEDOUT;

	npe_stop(npe);

	 
	__raw_writel(ctl, &npe->regs->messaging_control);
	return 0;
}


int npe_send_message(struct npe *npe, const void *msg, const char *what)
{
	const u32 *send = msg;
	int cycles = 0;

	debug_msg(npe, "Trying to send message %s [%08X:%08X]\n",
		  what, send[0], send[1]);

	if (__raw_readl(&npe->regs->messaging_status) & MSGSTAT_IFNE) {
		debug_msg(npe, "NPE input FIFO not empty\n");
		return -EIO;
	}

	__raw_writel(send[0], &npe->regs->in_out_fifo);

	if (!(__raw_readl(&npe->regs->messaging_status) & MSGSTAT_IFNF)) {
		debug_msg(npe, "NPE input FIFO full\n");
		return -EIO;
	}

	__raw_writel(send[1], &npe->regs->in_out_fifo);

	while ((cycles < MAX_RETRIES) &&
	       (__raw_readl(&npe->regs->messaging_status) & MSGSTAT_IFNE)) {
		udelay(1);
		cycles++;
	}

	if (cycles == MAX_RETRIES) {
		debug_msg(npe, "Timeout sending message\n");
		return -ETIMEDOUT;
	}

#if DEBUG_MSG > 1
	debug_msg(npe, "Sending a message took %i cycles\n", cycles);
#endif
	return 0;
}

int npe_recv_message(struct npe *npe, void *msg, const char *what)
{
	u32 *recv = msg;
	int cycles = 0, cnt = 0;

	debug_msg(npe, "Trying to receive message %s\n", what);

	while (cycles < MAX_RETRIES) {
		if (__raw_readl(&npe->regs->messaging_status) & MSGSTAT_OFNE) {
			recv[cnt++] = __raw_readl(&npe->regs->in_out_fifo);
			if (cnt == 2)
				break;
		} else {
			udelay(1);
			cycles++;
		}
	}

	switch(cnt) {
	case 1:
		debug_msg(npe, "Received [%08X]\n", recv[0]);
		break;
	case 2:
		debug_msg(npe, "Received [%08X:%08X]\n", recv[0], recv[1]);
		break;
	}

	if (cycles == MAX_RETRIES) {
		debug_msg(npe, "Timeout waiting for message\n");
		return -ETIMEDOUT;
	}

#if DEBUG_MSG > 1
	debug_msg(npe, "Receiving a message took %i cycles\n", cycles);
#endif
	return 0;
}

int npe_send_recv_message(struct npe *npe, void *msg, const char *what)
{
	int result;
	u32 *send = msg, recv[2];

	if ((result = npe_send_message(npe, msg, what)) != 0)
		return result;
	if ((result = npe_recv_message(npe, recv, what)) != 0)
		return result;

	if ((recv[0] != send[0]) || (recv[1] != send[1])) {
		debug_msg(npe, "Message %s: unexpected message received\n",
			  what);
		return -EIO;
	}
	return 0;
}


int npe_load_firmware(struct npe *npe, const char *name, struct device *dev)
{
	const struct firmware *fw_entry;

	struct dl_block {
		u32 type;
		u32 offset;
	} *blk;

	struct dl_image {
		u32 magic;
		u32 id;
		u32 size;
		union {
			DECLARE_FLEX_ARRAY(u32, data);
			DECLARE_FLEX_ARRAY(struct dl_block, blocks);
		};
	} *image;

	struct dl_codeblock {
		u32 npe_addr;
		u32 size;
		u32 data[];
	} *cb;

	int i, j, err, data_size, instr_size, blocks, table_end;
	u32 cmd;

	if ((err = request_firmware(&fw_entry, name, dev)) != 0)
		return err;

	err = -EINVAL;
	if (fw_entry->size < sizeof(struct dl_image)) {
		print_npe(KERN_ERR, npe, "incomplete firmware file\n");
		goto err;
	}
	image = (struct dl_image*)fw_entry->data;

#if DEBUG_FW
	print_npe(KERN_DEBUG, npe, "firmware: %08X %08X %08X (0x%X bytes)\n",
		  image->magic, image->id, image->size, image->size * 4);
#endif

	if (image->magic == swab32(FW_MAGIC)) {  
		image->id = swab32(image->id);
		image->size = swab32(image->size);
	} else if (image->magic != FW_MAGIC) {
		print_npe(KERN_ERR, npe, "bad firmware file magic: 0x%X\n",
			  image->magic);
		goto err;
	}
	if ((image->size * 4 + sizeof(struct dl_image)) != fw_entry->size) {
		print_npe(KERN_ERR, npe,
			  "inconsistent size of firmware file\n");
		goto err;
	}
	if (((image->id >> 24) & 0xF  ) != npe->id) {
		print_npe(KERN_ERR, npe, "firmware file NPE ID mismatch\n");
		goto err;
	}
	if (image->magic == swab32(FW_MAGIC))
		for (i = 0; i < image->size; i++)
			image->data[i] = swab32(image->data[i]);

	if (cpu_is_ixp42x() && ((image->id >> 28) & 0xF  )) {
		print_npe(KERN_INFO, npe, "IXP43x/IXP46x firmware ignored on "
			  "IXP42x\n");
		goto err;
	}

	if (npe_running(npe)) {
		print_npe(KERN_INFO, npe, "unable to load firmware, NPE is "
			  "already running\n");
		err = -EBUSY;
		goto err;
	}
#if 0
	npe_stop(npe);
	npe_reset(npe);
#endif

	print_npe(KERN_INFO, npe, "firmware functionality 0x%X, "
		  "revision 0x%X:%X\n", (image->id >> 16) & 0xFF,
		  (image->id >> 8) & 0xFF, image->id & 0xFF);

	if (cpu_is_ixp42x()) {
		if (!npe->id)
			instr_size = NPE_A_42X_INSTR_SIZE;
		else
			instr_size = NPE_B_AND_C_42X_INSTR_SIZE;
		data_size = NPE_42X_DATA_SIZE;
	} else {
		instr_size = NPE_46X_INSTR_SIZE;
		data_size = NPE_46X_DATA_SIZE;
	}

	for (blocks = 0; blocks * sizeof(struct dl_block) / 4 < image->size;
	     blocks++)
		if (image->blocks[blocks].type == FW_BLOCK_TYPE_EOF)
			break;
	if (blocks * sizeof(struct dl_block) / 4 >= image->size) {
		print_npe(KERN_INFO, npe, "firmware EOF block marker not "
			  "found\n");
		goto err;
	}

#if DEBUG_FW
	print_npe(KERN_DEBUG, npe, "%i firmware blocks found\n", blocks);
#endif

	table_end = blocks * sizeof(struct dl_block) / 4 + 1  ;
	for (i = 0, blk = image->blocks; i < blocks; i++, blk++) {
		if (blk->offset > image->size - sizeof(struct dl_codeblock) / 4
		    || blk->offset < table_end) {
			print_npe(KERN_INFO, npe, "invalid offset 0x%X of "
				  "firmware block #%i\n", blk->offset, i);
			goto err;
		}

		cb = (struct dl_codeblock*)&image->data[blk->offset];
		if (blk->type == FW_BLOCK_TYPE_INSTR) {
			if (cb->npe_addr + cb->size > instr_size)
				goto too_big;
			cmd = CMD_WR_INS_MEM;
		} else if (blk->type == FW_BLOCK_TYPE_DATA) {
			if (cb->npe_addr + cb->size > data_size)
				goto too_big;
			cmd = CMD_WR_DATA_MEM;
		} else {
			print_npe(KERN_INFO, npe, "invalid firmware block #%i "
				  "type 0x%X\n", i, blk->type);
			goto err;
		}
		if (blk->offset + sizeof(*cb) / 4 + cb->size > image->size) {
			print_npe(KERN_INFO, npe, "firmware block #%i doesn't "
				  "fit in firmware image: type %c, start 0x%X,"
				  " length 0x%X\n", i,
				  blk->type == FW_BLOCK_TYPE_INSTR ? 'I' : 'D',
				  cb->npe_addr, cb->size);
			goto err;
		}

		for (j = 0; j < cb->size; j++)
			npe_cmd_write(npe, cb->npe_addr + j, cmd, cb->data[j]);
	}

	npe_start(npe);
	if (!npe_running(npe))
		print_npe(KERN_ERR, npe, "unable to start\n");
	release_firmware(fw_entry);
	return 0;

too_big:
	print_npe(KERN_INFO, npe, "firmware block #%i doesn't fit in NPE "
		  "memory: type %c, start 0x%X, length 0x%X\n", i,
		  blk->type == FW_BLOCK_TYPE_INSTR ? 'I' : 'D',
		  cb->npe_addr, cb->size);
err:
	release_firmware(fw_entry);
	return err;
}


struct npe *npe_request(unsigned id)
{
	if (id < NPE_COUNT)
		if (npe_tab[id].valid)
			if (try_module_get(THIS_MODULE))
				return &npe_tab[id];
	return NULL;
}

void npe_release(struct npe *npe)
{
	module_put(THIS_MODULE);
}

static int ixp4xx_npe_probe(struct platform_device *pdev)
{
	int i, found = 0;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct resource *res;
	struct regmap *rmap;
	u32 val;

	 
	rmap = syscon_regmap_lookup_by_compatible("syscon");
	if (IS_ERR(rmap))
		return dev_err_probe(dev, PTR_ERR(rmap),
				     "failed to look up syscon\n");

	for (i = 0; i < NPE_COUNT; i++) {
		struct npe *npe = &npe_tab[i];

		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res)
			return -ENODEV;

		val = cpu_ixp4xx_features(rmap);

		if (!(val & (IXP4XX_FEATURE_RESET_NPEA << i))) {
			dev_info(dev, "NPE%d at %pR not available\n",
				 i, res);
			continue;  
		}
		npe->regs = devm_ioremap_resource(dev, res);
		if (IS_ERR(npe->regs))
			return PTR_ERR(npe->regs);
		npe->rmap = rmap;

		if (npe_reset(npe)) {
			dev_info(dev, "NPE%d at %pR does not reset\n",
				 i, res);
			continue;
		}
		npe->valid = 1;
		dev_info(dev, "NPE%d at %pR registered\n", i, res);
		found++;
	}

	if (!found)
		return -ENODEV;

	 
	if (IS_ENABLED(CONFIG_OF) && np)
		devm_of_platform_populate(dev);

	return 0;
}

static int ixp4xx_npe_remove(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < NPE_COUNT; i++)
		if (npe_tab[i].regs) {
			npe_reset(&npe_tab[i]);
		}

	return 0;
}

static const struct of_device_id ixp4xx_npe_of_match[] = {
	{
		.compatible = "intel,ixp4xx-network-processing-engine",
        },
	{},
};

static struct platform_driver ixp4xx_npe_driver = {
	.driver = {
		.name           = "ixp4xx-npe",
		.of_match_table = ixp4xx_npe_of_match,
	},
	.probe = ixp4xx_npe_probe,
	.remove = ixp4xx_npe_remove,
};
module_platform_driver(ixp4xx_npe_driver);

MODULE_AUTHOR("Krzysztof Halasa");
MODULE_LICENSE("GPL v2");
MODULE_FIRMWARE(NPE_A_FIRMWARE);
MODULE_FIRMWARE(NPE_B_FIRMWARE);
MODULE_FIRMWARE(NPE_C_FIRMWARE);

EXPORT_SYMBOL(npe_names);
EXPORT_SYMBOL(npe_running);
EXPORT_SYMBOL(npe_request);
EXPORT_SYMBOL(npe_release);
EXPORT_SYMBOL(npe_load_firmware);
EXPORT_SYMBOL(npe_send_message);
EXPORT_SYMBOL(npe_recv_message);
EXPORT_SYMBOL(npe_send_recv_message);
