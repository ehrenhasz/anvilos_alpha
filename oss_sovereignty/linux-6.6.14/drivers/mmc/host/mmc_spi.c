
 
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/bio.h>
#include <linux/dma-mapping.h>
#include <linux/crc7.h>
#include <linux/crc-itu-t.h>
#include <linux/scatterlist.h>

#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>		 
#include <linux/mmc/slot-gpio.h>

#include <linux/spi/spi.h>
#include <linux/spi/mmc_spi.h>

#include <asm/unaligned.h>


 


 

 
#define SPI_MMC_RESPONSE_CODE(x)	((x) & 0x1f)
#define SPI_RESPONSE_ACCEPTED		((2 << 1)|1)
#define SPI_RESPONSE_CRC_ERR		((5 << 1)|1)
#define SPI_RESPONSE_WRITE_ERR		((6 << 1)|1)

 
#define SPI_TOKEN_SINGLE	0xfe	 
#define SPI_TOKEN_MULTI_WRITE	0xfc	 
#define SPI_TOKEN_STOP_TRAN	0xfd	 

#define MMC_SPI_BLOCKSIZE	512

#define MMC_SPI_R1B_TIMEOUT_MS	3000
#define MMC_SPI_INIT_TIMEOUT_MS	3000

 
#define MMC_SPI_BLOCKSATONCE	128

 

 

 
struct scratch {
	u8			status[29];
	u8			data_token;
	__be16			crc_val;
};

struct mmc_spi_host {
	struct mmc_host		*mmc;
	struct spi_device	*spi;

	unsigned char		power_mode;
	u16			powerup_msecs;

	struct mmc_spi_platform_data	*pdata;

	 
	struct spi_transfer	token, t, crc, early_status;
	struct spi_message	m;

	 
	struct spi_transfer	status;
	struct spi_message	readback;

	 
	struct device		*dma_dev;

	 
	struct scratch		*data;
	dma_addr_t		data_dma;

	 
	void			*ones;
	dma_addr_t		ones_dma;
};


 

 

static inline int mmc_cs_off(struct mmc_spi_host *host)
{
	 
	return spi_setup(host->spi);
}

static int
mmc_spi_readbytes(struct mmc_spi_host *host, unsigned len)
{
	int status;

	if (len > sizeof(*host->data)) {
		WARN_ON(1);
		return -EIO;
	}

	host->status.len = len;

	if (host->dma_dev)
		dma_sync_single_for_device(host->dma_dev,
				host->data_dma, sizeof(*host->data),
				DMA_FROM_DEVICE);

	status = spi_sync_locked(host->spi, &host->readback);

	if (host->dma_dev)
		dma_sync_single_for_cpu(host->dma_dev,
				host->data_dma, sizeof(*host->data),
				DMA_FROM_DEVICE);

	return status;
}

static int mmc_spi_skip(struct mmc_spi_host *host, unsigned long timeout,
			unsigned n, u8 byte)
{
	u8 *cp = host->data->status;
	unsigned long start = jiffies;

	do {
		int		status;
		unsigned	i;

		status = mmc_spi_readbytes(host, n);
		if (status < 0)
			return status;

		for (i = 0; i < n; i++) {
			if (cp[i] != byte)
				return cp[i];
		}

		 
		cond_resched();
	} while (time_is_after_jiffies(start + timeout));
	return -ETIMEDOUT;
}

static inline int
mmc_spi_wait_unbusy(struct mmc_spi_host *host, unsigned long timeout)
{
	return mmc_spi_skip(host, timeout, sizeof(host->data->status), 0);
}

static int mmc_spi_readtoken(struct mmc_spi_host *host, unsigned long timeout)
{
	return mmc_spi_skip(host, timeout, 1, 0xff);
}


 

static char *maptype(struct mmc_command *cmd)
{
	switch (mmc_spi_resp_type(cmd)) {
	case MMC_RSP_SPI_R1:	return "R1";
	case MMC_RSP_SPI_R1B:	return "R1B";
	case MMC_RSP_SPI_R2:	return "R2/R5";
	case MMC_RSP_SPI_R3:	return "R3/R4/R7";
	default:		return "?";
	}
}

 
static int mmc_spi_response_get(struct mmc_spi_host *host,
		struct mmc_command *cmd, int cs_on)
{
	unsigned long timeout_ms;
	u8	*cp = host->data->status;
	u8	*end = cp + host->t.len;
	int	value = 0;
	int	bitshift;
	u8 	leftover = 0;
	unsigned short rotator;
	int 	i;
	char	tag[32];

	snprintf(tag, sizeof(tag), "  ... CMD%d response SPI_%s",
		cmd->opcode, maptype(cmd));

	 
	cp += 8;
	while (cp < end && *cp == 0xff)
		cp++;

	 
	if (cp == end) {
		cp = host->data->status;
		end = cp+1;

		 
		for (i = 2; i < 16; i++) {
			value = mmc_spi_readbytes(host, 1);
			if (value < 0)
				goto done;
			if (*cp != 0xff)
				goto checkstatus;
		}
		value = -ETIMEDOUT;
		goto done;
	}

checkstatus:
	bitshift = 0;
	if (*cp & 0x80)	{
		 
		rotator = *cp++ << 8;
		 
		if (cp == end) {
			value = mmc_spi_readbytes(host, 1);
			if (value < 0)
				goto done;
			cp = host->data->status;
			end = cp+1;
		}
		rotator |= *cp++;
		while (rotator & 0x8000) {
			bitshift++;
			rotator <<= 1;
		}
		cmd->resp[0] = rotator >> 8;
		leftover = rotator;
	} else {
		cmd->resp[0] = *cp++;
	}
	cmd->error = 0;

	 
	if (cmd->resp[0] != 0) {
		if ((R1_SPI_PARAMETER | R1_SPI_ADDRESS)
				& cmd->resp[0])
			value = -EFAULT;  
		else if (R1_SPI_ILLEGAL_COMMAND & cmd->resp[0])
			value = -ENOSYS;  
		else if (R1_SPI_COM_CRC & cmd->resp[0])
			value = -EILSEQ;  
		else if ((R1_SPI_ERASE_SEQ | R1_SPI_ERASE_RESET)
				& cmd->resp[0])
			value = -EIO;     
		 
	}

	switch (mmc_spi_resp_type(cmd)) {

	 
	case MMC_RSP_SPI_R1B:
		 
		while (cp < end && *cp == 0)
			cp++;
		if (cp == end) {
			timeout_ms = cmd->busy_timeout ? cmd->busy_timeout :
				MMC_SPI_R1B_TIMEOUT_MS;
			mmc_spi_wait_unbusy(host, msecs_to_jiffies(timeout_ms));
		}
		break;

	 
	case MMC_RSP_SPI_R2:
		 
		if (cp == end) {
			value = mmc_spi_readbytes(host, 1);
			if (value < 0)
				goto done;
			cp = host->data->status;
			end = cp+1;
		}
		if (bitshift) {
			rotator = leftover << 8;
			rotator |= *cp << bitshift;
			cmd->resp[0] |= (rotator & 0xFF00);
		} else {
			cmd->resp[0] |= *cp << 8;
		}
		break;

	 
	case MMC_RSP_SPI_R3:
		rotator = leftover << 8;
		cmd->resp[1] = 0;
		for (i = 0; i < 4; i++) {
			cmd->resp[1] <<= 8;
			 
			if (cp == end) {
				value = mmc_spi_readbytes(host, 1);
				if (value < 0)
					goto done;
				cp = host->data->status;
				end = cp+1;
			}
			if (bitshift) {
				rotator |= *cp++ << bitshift;
				cmd->resp[1] |= (rotator >> 8);
				rotator <<= 8;
			} else {
				cmd->resp[1] |= *cp++;
			}
		}
		break;

	 
	case MMC_RSP_SPI_R1:
		break;

	default:
		dev_dbg(&host->spi->dev, "bad response type %04x\n",
			mmc_spi_resp_type(cmd));
		if (value >= 0)
			value = -EINVAL;
		goto done;
	}

	if (value < 0)
		dev_dbg(&host->spi->dev, "%s: resp %04x %08x\n",
			tag, cmd->resp[0], cmd->resp[1]);

	 
	if (value >= 0 && cs_on)
		return value;
done:
	if (value < 0)
		cmd->error = value;
	mmc_cs_off(host);
	return value;
}

 
static int
mmc_spi_command_send(struct mmc_spi_host *host,
		struct mmc_request *mrq,
		struct mmc_command *cmd, int cs_on)
{
	struct scratch		*data = host->data;
	u8			*cp = data->status;
	int			status;
	struct spi_transfer	*t;

	 
	memset(cp, 0xff, sizeof(data->status));

	cp[1] = 0x40 | cmd->opcode;
	put_unaligned_be32(cmd->arg, cp + 2);
	cp[6] = crc7_be(0, cp + 1, 5) | 0x01;
	cp += 7;

	 
	if (cs_on && (mrq->data->flags & MMC_DATA_READ)) {
		cp += 2;	 
		 
	} else {
		cp += 10;	 
		if (cmd->flags & MMC_RSP_SPI_S2)	 
			cp++;
		else if (cmd->flags & MMC_RSP_SPI_B4)	 
			cp += 4;
		else if (cmd->flags & MMC_RSP_BUSY)	 
			cp = data->status + sizeof(data->status);
		 
	}

	dev_dbg(&host->spi->dev, "  CMD%d, resp %s\n",
		cmd->opcode, maptype(cmd));

	 
	spi_message_init(&host->m);

	t = &host->t;
	memset(t, 0, sizeof(*t));
	t->tx_buf = t->rx_buf = data->status;
	t->tx_dma = t->rx_dma = host->data_dma;
	t->len = cp - data->status;
	t->cs_change = 1;
	spi_message_add_tail(t, &host->m);

	if (host->dma_dev) {
		host->m.is_dma_mapped = 1;
		dma_sync_single_for_device(host->dma_dev,
				host->data_dma, sizeof(*host->data),
				DMA_BIDIRECTIONAL);
	}
	status = spi_sync_locked(host->spi, &host->m);

	if (host->dma_dev)
		dma_sync_single_for_cpu(host->dma_dev,
				host->data_dma, sizeof(*host->data),
				DMA_BIDIRECTIONAL);
	if (status < 0) {
		dev_dbg(&host->spi->dev, "  ... write returned %d\n", status);
		cmd->error = status;
		return status;
	}

	 
	return mmc_spi_response_get(host, cmd, cs_on);
}

 
static void
mmc_spi_setup_data_message(
	struct mmc_spi_host	*host,
	bool			multiple,
	enum dma_data_direction	direction)
{
	struct spi_transfer	*t;
	struct scratch		*scratch = host->data;
	dma_addr_t		dma = host->data_dma;

	spi_message_init(&host->m);
	if (dma)
		host->m.is_dma_mapped = 1;

	 
	if (direction == DMA_TO_DEVICE) {
		t = &host->token;
		memset(t, 0, sizeof(*t));
		t->len = 1;
		if (multiple)
			scratch->data_token = SPI_TOKEN_MULTI_WRITE;
		else
			scratch->data_token = SPI_TOKEN_SINGLE;
		t->tx_buf = &scratch->data_token;
		if (dma)
			t->tx_dma = dma + offsetof(struct scratch, data_token);
		spi_message_add_tail(t, &host->m);
	}

	 
	t = &host->t;
	memset(t, 0, sizeof(*t));
	t->tx_buf = host->ones;
	t->tx_dma = host->ones_dma;
	 
	spi_message_add_tail(t, &host->m);

	t = &host->crc;
	memset(t, 0, sizeof(*t));
	t->len = 2;
	if (direction == DMA_TO_DEVICE) {
		 
		t->tx_buf = &scratch->crc_val;
		if (dma)
			t->tx_dma = dma + offsetof(struct scratch, crc_val);
	} else {
		t->tx_buf = host->ones;
		t->tx_dma = host->ones_dma;
		t->rx_buf = &scratch->crc_val;
		if (dma)
			t->rx_dma = dma + offsetof(struct scratch, crc_val);
	}
	spi_message_add_tail(t, &host->m);

	 
	if (multiple || direction == DMA_TO_DEVICE) {
		t = &host->early_status;
		memset(t, 0, sizeof(*t));
		t->len = (direction == DMA_TO_DEVICE) ? sizeof(scratch->status) : 1;
		t->tx_buf = host->ones;
		t->tx_dma = host->ones_dma;
		t->rx_buf = scratch->status;
		if (dma)
			t->rx_dma = dma + offsetof(struct scratch, status);
		t->cs_change = 1;
		spi_message_add_tail(t, &host->m);
	}
}

 
static int
mmc_spi_writeblock(struct mmc_spi_host *host, struct spi_transfer *t,
	unsigned long timeout)
{
	struct spi_device	*spi = host->spi;
	int			status, i;
	struct scratch		*scratch = host->data;
	u32			pattern;

	if (host->mmc->use_spi_crc)
		scratch->crc_val = cpu_to_be16(crc_itu_t(0, t->tx_buf, t->len));
	if (host->dma_dev)
		dma_sync_single_for_device(host->dma_dev,
				host->data_dma, sizeof(*scratch),
				DMA_BIDIRECTIONAL);

	status = spi_sync_locked(spi, &host->m);

	if (status != 0) {
		dev_dbg(&spi->dev, "write error (%d)\n", status);
		return status;
	}

	if (host->dma_dev)
		dma_sync_single_for_cpu(host->dma_dev,
				host->data_dma, sizeof(*scratch),
				DMA_BIDIRECTIONAL);

	 
	pattern = get_unaligned_be32(scratch->status);

	 
	pattern |= 0xE0000000;

	 
	while (pattern & 0x80000000)
		pattern <<= 1;
	 
	pattern >>= 27;

	switch (pattern) {
	case SPI_RESPONSE_ACCEPTED:
		status = 0;
		break;
	case SPI_RESPONSE_CRC_ERR:
		 
		status = -EILSEQ;
		break;
	case SPI_RESPONSE_WRITE_ERR:
		 
		status = -EIO;
		break;
	default:
		status = -EPROTO;
		break;
	}
	if (status != 0) {
		dev_dbg(&spi->dev, "write error %02x (%d)\n",
			scratch->status[0], status);
		return status;
	}

	t->tx_buf += t->len;
	if (host->dma_dev)
		t->tx_dma += t->len;

	 
	for (i = 4; i < sizeof(scratch->status); i++) {
		 
		if (scratch->status[i] & 0x01)
			return 0;
	}
	return mmc_spi_wait_unbusy(host, timeout);
}

 
static int
mmc_spi_readblock(struct mmc_spi_host *host, struct spi_transfer *t,
	unsigned long timeout)
{
	struct spi_device	*spi = host->spi;
	int			status;
	struct scratch		*scratch = host->data;
	unsigned int 		bitshift;
	u8			leftover;

	 
	status = mmc_spi_readbytes(host, 1);
	if (status < 0)
		return status;
	status = scratch->status[0];
	if (status == 0xff || status == 0)
		status = mmc_spi_readtoken(host, timeout);

	if (status < 0) {
		dev_dbg(&spi->dev, "read error %02x (%d)\n", status, status);
		return status;
	}

	 
	bitshift = 7;
	while (status & 0x80) {
		status <<= 1;
		bitshift--;
	}
	leftover = status << 1;

	if (host->dma_dev) {
		dma_sync_single_for_device(host->dma_dev,
				host->data_dma, sizeof(*scratch),
				DMA_BIDIRECTIONAL);
		dma_sync_single_for_device(host->dma_dev,
				t->rx_dma, t->len,
				DMA_FROM_DEVICE);
	}

	status = spi_sync_locked(spi, &host->m);
	if (status < 0) {
		dev_dbg(&spi->dev, "read error %d\n", status);
		return status;
	}

	if (host->dma_dev) {
		dma_sync_single_for_cpu(host->dma_dev,
				host->data_dma, sizeof(*scratch),
				DMA_BIDIRECTIONAL);
		dma_sync_single_for_cpu(host->dma_dev,
				t->rx_dma, t->len,
				DMA_FROM_DEVICE);
	}

	if (bitshift) {
		 
		u8 *cp = t->rx_buf;
		unsigned int len;
		unsigned int bitright = 8 - bitshift;
		u8 temp;
		for (len = t->len; len; len--) {
			temp = *cp;
			*cp++ = leftover | (temp >> bitshift);
			leftover = temp << bitright;
		}
		cp = (u8 *) &scratch->crc_val;
		temp = *cp;
		*cp++ = leftover | (temp >> bitshift);
		leftover = temp << bitright;
		temp = *cp;
		*cp = leftover | (temp >> bitshift);
	}

	if (host->mmc->use_spi_crc) {
		u16 crc = crc_itu_t(0, t->rx_buf, t->len);

		be16_to_cpus(&scratch->crc_val);
		if (scratch->crc_val != crc) {
			dev_dbg(&spi->dev,
				"read - crc error: crc_val=0x%04x, computed=0x%04x len=%d\n",
				scratch->crc_val, crc, t->len);
			return -EILSEQ;
		}
	}

	t->rx_buf += t->len;
	if (host->dma_dev)
		t->rx_dma += t->len;

	return 0;
}

 
static void
mmc_spi_data_do(struct mmc_spi_host *host, struct mmc_command *cmd,
		struct mmc_data *data, u32 blk_size)
{
	struct spi_device	*spi = host->spi;
	struct device		*dma_dev = host->dma_dev;
	struct spi_transfer	*t;
	enum dma_data_direction	direction = mmc_get_dma_dir(data);
	struct scatterlist	*sg;
	unsigned		n_sg;
	bool			multiple = (data->blocks > 1);
	const char		*write_or_read = (direction == DMA_TO_DEVICE) ? "write" : "read";
	u32			clock_rate;
	unsigned long		timeout;

	mmc_spi_setup_data_message(host, multiple, direction);
	t = &host->t;

	if (t->speed_hz)
		clock_rate = t->speed_hz;
	else
		clock_rate = spi->max_speed_hz;

	timeout = data->timeout_ns / 1000 +
		  data->timeout_clks * 1000000 / clock_rate;
	timeout = usecs_to_jiffies((unsigned int)timeout) + 1;

	 
	for_each_sg(data->sg, sg, data->sg_len, n_sg) {
		int			status = 0;
		dma_addr_t		dma_addr = 0;
		void			*kmap_addr;
		unsigned		length = sg->length;
		enum dma_data_direction	dir = direction;

		 
		if (dma_dev) {
			 
			if ((sg->offset != 0 || length != PAGE_SIZE)
					&& dir == DMA_FROM_DEVICE)
				dir = DMA_BIDIRECTIONAL;

			dma_addr = dma_map_page(dma_dev, sg_page(sg), 0,
						PAGE_SIZE, dir);
			if (dma_mapping_error(dma_dev, dma_addr)) {
				data->error = -EFAULT;
				break;
			}
			if (direction == DMA_TO_DEVICE)
				t->tx_dma = dma_addr + sg->offset;
			else
				t->rx_dma = dma_addr + sg->offset;
		}

		 
		kmap_addr = kmap(sg_page(sg));
		if (direction == DMA_TO_DEVICE)
			t->tx_buf = kmap_addr + sg->offset;
		else
			t->rx_buf = kmap_addr + sg->offset;

		 
		while (length) {
			t->len = min(length, blk_size);

			dev_dbg(&spi->dev, "    %s block, %d bytes\n", write_or_read, t->len);

			if (direction == DMA_TO_DEVICE)
				status = mmc_spi_writeblock(host, t, timeout);
			else
				status = mmc_spi_readblock(host, t, timeout);
			if (status < 0)
				break;

			data->bytes_xfered += t->len;
			length -= t->len;

			if (!multiple)
				break;
		}

		 
		if (direction == DMA_FROM_DEVICE)
			flush_dcache_page(sg_page(sg));
		kunmap(sg_page(sg));
		if (dma_dev)
			dma_unmap_page(dma_dev, dma_addr, PAGE_SIZE, dir);

		if (status < 0) {
			data->error = status;
			dev_dbg(&spi->dev, "%s status %d\n", write_or_read, status);
			break;
		}
	}

	 
	if (direction == DMA_TO_DEVICE && multiple) {
		struct scratch	*scratch = host->data;
		int		tmp;
		const unsigned	statlen = sizeof(scratch->status);

		dev_dbg(&spi->dev, "    STOP_TRAN\n");

		 
		INIT_LIST_HEAD(&host->m.transfers);
		list_add(&host->early_status.transfer_list,
				&host->m.transfers);

		memset(scratch->status, 0xff, statlen);
		scratch->status[0] = SPI_TOKEN_STOP_TRAN;

		host->early_status.tx_buf = host->early_status.rx_buf;
		host->early_status.tx_dma = host->early_status.rx_dma;
		host->early_status.len = statlen;

		if (host->dma_dev)
			dma_sync_single_for_device(host->dma_dev,
					host->data_dma, sizeof(*scratch),
					DMA_BIDIRECTIONAL);

		tmp = spi_sync_locked(spi, &host->m);

		if (host->dma_dev)
			dma_sync_single_for_cpu(host->dma_dev,
					host->data_dma, sizeof(*scratch),
					DMA_BIDIRECTIONAL);

		if (tmp < 0) {
			if (!data->error)
				data->error = tmp;
			return;
		}

		 
		for (tmp = 2; tmp < statlen; tmp++) {
			if (scratch->status[tmp] != 0)
				return;
		}
		tmp = mmc_spi_wait_unbusy(host, timeout);
		if (tmp < 0 && !data->error)
			data->error = tmp;
	}
}

 

 

static void mmc_spi_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct mmc_spi_host	*host = mmc_priv(mmc);
	int			status = -EINVAL;
	int			crc_retry = 5;
	struct mmc_command	stop;

#ifdef DEBUG
	 
	{
		struct mmc_command	*cmd;
		int			invalid = 0;

		cmd = mrq->cmd;
		if (!mmc_spi_resp_type(cmd)) {
			dev_dbg(&host->spi->dev, "bogus command\n");
			cmd->error = -EINVAL;
			invalid = 1;
		}

		cmd = mrq->stop;
		if (cmd && !mmc_spi_resp_type(cmd)) {
			dev_dbg(&host->spi->dev, "bogus STOP command\n");
			cmd->error = -EINVAL;
			invalid = 1;
		}

		if (invalid) {
			dump_stack();
			mmc_request_done(host->mmc, mrq);
			return;
		}
	}
#endif

	 
	spi_bus_lock(host->spi->master);

crc_recover:
	 
	status = mmc_spi_command_send(host, mrq, mrq->cmd, mrq->data != NULL);
	if (status == 0 && mrq->data) {
		mmc_spi_data_do(host, mrq->cmd, mrq->data, mrq->data->blksz);

		 
		if (mrq->data->error == -EILSEQ && crc_retry) {
			stop.opcode = MMC_STOP_TRANSMISSION;
			stop.arg = 0;
			stop.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;
			status = mmc_spi_command_send(host, mrq, &stop, 0);
			crc_retry--;
			mrq->data->error = 0;
			goto crc_recover;
		}

		if (mrq->stop)
			status = mmc_spi_command_send(host, mrq, mrq->stop, 0);
		else
			mmc_cs_off(host);
	}

	 
	spi_bus_unlock(host->spi->master);

	mmc_request_done(host->mmc, mrq);
}

 
static void mmc_spi_initsequence(struct mmc_spi_host *host)
{
	 
	mmc_spi_wait_unbusy(host, msecs_to_jiffies(MMC_SPI_INIT_TIMEOUT_MS));
	mmc_spi_readbytes(host, 10);

	 
	host->spi->mode ^= SPI_CS_HIGH;
	if (spi_setup(host->spi) != 0) {
		 
		dev_warn(&host->spi->dev,
				"can't change chip-select polarity\n");
		host->spi->mode ^= SPI_CS_HIGH;
	} else {
		mmc_spi_readbytes(host, 18);

		host->spi->mode ^= SPI_CS_HIGH;
		if (spi_setup(host->spi) != 0) {
			 
			dev_err(&host->spi->dev,
					"can't restore chip-select polarity\n");
		}
	}
}

static char *mmc_powerstring(u8 power_mode)
{
	switch (power_mode) {
	case MMC_POWER_OFF: return "off";
	case MMC_POWER_UP:  return "up";
	case MMC_POWER_ON:  return "on";
	}
	return "?";
}

static void mmc_spi_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct mmc_spi_host *host = mmc_priv(mmc);

	if (host->power_mode != ios->power_mode) {
		int		canpower;

		canpower = host->pdata && host->pdata->setpower;

		dev_dbg(&host->spi->dev, "power %s (%d)%s\n",
				mmc_powerstring(ios->power_mode),
				ios->vdd,
				canpower ? ", can switch" : "");

		 
		if (canpower) {
			switch (ios->power_mode) {
			case MMC_POWER_OFF:
			case MMC_POWER_UP:
				host->pdata->setpower(&host->spi->dev,
						ios->vdd);
				if (ios->power_mode == MMC_POWER_UP)
					msleep(host->powerup_msecs);
			}
		}

		 
		if (ios->power_mode == MMC_POWER_ON)
			mmc_spi_initsequence(host);

		 
		if (canpower && ios->power_mode == MMC_POWER_OFF) {
			int mres;
			u8 nullbyte = 0;

			host->spi->mode &= ~(SPI_CPOL|SPI_CPHA);
			mres = spi_setup(host->spi);
			if (mres < 0)
				dev_dbg(&host->spi->dev,
					"switch to SPI mode 0 failed\n");

			if (spi_write(host->spi, &nullbyte, 1) < 0)
				dev_dbg(&host->spi->dev,
					"put spi signals to low failed\n");

			 
			msleep(10);
			if (mres == 0) {
				host->spi->mode |= (SPI_CPOL|SPI_CPHA);
				mres = spi_setup(host->spi);
				if (mres < 0)
					dev_dbg(&host->spi->dev,
						"switch back to SPI mode 3 failed\n");
			}
		}

		host->power_mode = ios->power_mode;
	}

	if (host->spi->max_speed_hz != ios->clock && ios->clock != 0) {
		int		status;

		host->spi->max_speed_hz = ios->clock;
		status = spi_setup(host->spi);
		dev_dbg(&host->spi->dev, "  clock to %d Hz, %d\n",
			host->spi->max_speed_hz, status);
	}
}

static const struct mmc_host_ops mmc_spi_ops = {
	.request	= mmc_spi_request,
	.set_ios	= mmc_spi_set_ios,
	.get_ro		= mmc_gpio_get_ro,
	.get_cd		= mmc_gpio_get_cd,
};


 

 

static irqreturn_t
mmc_spi_detect_irq(int irq, void *mmc)
{
	struct mmc_spi_host *host = mmc_priv(mmc);
	u16 delay_msec = max(host->pdata->detect_delay, (u16)100);

	mmc_detect_change(mmc, msecs_to_jiffies(delay_msec));
	return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_DMA
static int mmc_spi_dma_alloc(struct mmc_spi_host *host)
{
	struct spi_device *spi = host->spi;
	struct device *dev;

	if (!spi->master->dev.parent->dma_mask)
		return 0;

	dev = spi->master->dev.parent;

	host->ones_dma = dma_map_single(dev, host->ones, MMC_SPI_BLOCKSIZE,
					DMA_TO_DEVICE);
	if (dma_mapping_error(dev, host->ones_dma))
		return -ENOMEM;

	host->data_dma = dma_map_single(dev, host->data, sizeof(*host->data),
					DMA_BIDIRECTIONAL);
	if (dma_mapping_error(dev, host->data_dma)) {
		dma_unmap_single(dev, host->ones_dma, MMC_SPI_BLOCKSIZE,
				 DMA_TO_DEVICE);
		return -ENOMEM;
	}

	dma_sync_single_for_cpu(dev, host->data_dma, sizeof(*host->data),
				DMA_BIDIRECTIONAL);

	host->dma_dev = dev;
	return 0;
}

static void mmc_spi_dma_free(struct mmc_spi_host *host)
{
	if (!host->dma_dev)
		return;

	dma_unmap_single(host->dma_dev, host->ones_dma, MMC_SPI_BLOCKSIZE,
			 DMA_TO_DEVICE);
	dma_unmap_single(host->dma_dev, host->data_dma,	sizeof(*host->data),
			 DMA_BIDIRECTIONAL);
}
#else
static inline int mmc_spi_dma_alloc(struct mmc_spi_host *host) { return 0; }
static inline void mmc_spi_dma_free(struct mmc_spi_host *host) {}
#endif

static int mmc_spi_probe(struct spi_device *spi)
{
	void			*ones;
	struct mmc_host		*mmc;
	struct mmc_spi_host	*host;
	int			status;
	bool			has_ro = false;

	 
	if (spi->master->flags & SPI_MASTER_HALF_DUPLEX)
		return -EINVAL;

	 
	if (spi->mode != SPI_MODE_3)
		spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;

	status = spi_setup(spi);
	if (status < 0) {
		dev_dbg(&spi->dev, "needs SPI mode %02x, %d KHz; %d\n",
				spi->mode, spi->max_speed_hz / 1000,
				status);
		return status;
	}

	 
	status = -ENOMEM;
	ones = kmalloc(MMC_SPI_BLOCKSIZE, GFP_KERNEL);
	if (!ones)
		goto nomem;
	memset(ones, 0xff, MMC_SPI_BLOCKSIZE);

	mmc = mmc_alloc_host(sizeof(*host), &spi->dev);
	if (!mmc)
		goto nomem;

	mmc->ops = &mmc_spi_ops;
	mmc->max_blk_size = MMC_SPI_BLOCKSIZE;
	mmc->max_segs = MMC_SPI_BLOCKSATONCE;
	mmc->max_req_size = MMC_SPI_BLOCKSATONCE * MMC_SPI_BLOCKSIZE;
	mmc->max_blk_count = MMC_SPI_BLOCKSATONCE;

	mmc->caps = MMC_CAP_SPI;

	 
	mmc->f_min = 400000;
	mmc->f_max = spi->max_speed_hz;

	host = mmc_priv(mmc);
	host->mmc = mmc;
	host->spi = spi;

	host->ones = ones;

	dev_set_drvdata(&spi->dev, mmc);

	 
	host->pdata = mmc_spi_get_pdata(spi);
	if (host->pdata)
		mmc->ocr_avail = host->pdata->ocr_mask;
	if (!mmc->ocr_avail) {
		dev_warn(&spi->dev, "ASSUMING 3.2-3.4 V slot power\n");
		mmc->ocr_avail = MMC_VDD_32_33|MMC_VDD_33_34;
	}
	if (host->pdata && host->pdata->setpower) {
		host->powerup_msecs = host->pdata->powerup_msecs;
		if (!host->powerup_msecs || host->powerup_msecs > 250)
			host->powerup_msecs = 250;
	}

	 
	host->data = kmalloc(sizeof(*host->data), GFP_KERNEL);
	if (!host->data)
		goto fail_nobuf1;

	status = mmc_spi_dma_alloc(host);
	if (status)
		goto fail_dma;

	 
	spi_message_init(&host->readback);
	host->readback.is_dma_mapped = (host->dma_dev != NULL);

	spi_message_add_tail(&host->status, &host->readback);
	host->status.tx_buf = host->ones;
	host->status.tx_dma = host->ones_dma;
	host->status.rx_buf = &host->data->status;
	host->status.rx_dma = host->data_dma + offsetof(struct scratch, status);
	host->status.cs_change = 1;

	 
	if (host->pdata && host->pdata->init) {
		status = host->pdata->init(&spi->dev, mmc_spi_detect_irq, mmc);
		if (status != 0)
			goto fail_glue_init;
	}

	 
	if (host->pdata) {
		mmc->caps |= host->pdata->caps;
		mmc->caps2 |= host->pdata->caps2;
	}

	status = mmc_add_host(mmc);
	if (status != 0)
		goto fail_glue_init;

	 
	status = mmc_gpiod_request_cd(mmc, NULL, 0, false, 1000);
	if (status == -EPROBE_DEFER)
		goto fail_gpiod_request;
	if (!status) {
		 
		mmc->caps &= ~MMC_CAP_NEEDS_POLL;
		mmc_gpiod_request_cd_irq(mmc);
	}
	mmc_detect_change(mmc, 0);

	 
	status = mmc_gpiod_request_ro(mmc, NULL, 1, 0);
	if (status == -EPROBE_DEFER)
		goto fail_gpiod_request;
	if (!status)
		has_ro = true;

	dev_info(&spi->dev, "SD/MMC host %s%s%s%s%s\n",
			dev_name(&mmc->class_dev),
			host->dma_dev ? "" : ", no DMA",
			has_ro ? "" : ", no WP",
			(host->pdata && host->pdata->setpower)
				? "" : ", no poweroff",
			(mmc->caps & MMC_CAP_NEEDS_POLL)
				? ", cd polling" : "");
	return 0;

fail_gpiod_request:
	mmc_remove_host(mmc);
fail_glue_init:
	mmc_spi_dma_free(host);
fail_dma:
	kfree(host->data);
fail_nobuf1:
	mmc_spi_put_pdata(spi);
	mmc_free_host(mmc);
nomem:
	kfree(ones);
	return status;
}


static void mmc_spi_remove(struct spi_device *spi)
{
	struct mmc_host		*mmc = dev_get_drvdata(&spi->dev);
	struct mmc_spi_host	*host = mmc_priv(mmc);

	 
	if (host->pdata && host->pdata->exit)
		host->pdata->exit(&spi->dev, mmc);

	mmc_remove_host(mmc);

	mmc_spi_dma_free(host);
	kfree(host->data);
	kfree(host->ones);

	spi->max_speed_hz = mmc->f_max;
	mmc_spi_put_pdata(spi);
	mmc_free_host(mmc);
}

static const struct spi_device_id mmc_spi_dev_ids[] = {
	{ "mmc-spi-slot"},
	{ },
};
MODULE_DEVICE_TABLE(spi, mmc_spi_dev_ids);

static const struct of_device_id mmc_spi_of_match_table[] = {
	{ .compatible = "mmc-spi-slot", },
	{},
};
MODULE_DEVICE_TABLE(of, mmc_spi_of_match_table);

static struct spi_driver mmc_spi_driver = {
	.driver = {
		.name =		"mmc_spi",
		.of_match_table = mmc_spi_of_match_table,
	},
	.id_table =	mmc_spi_dev_ids,
	.probe =	mmc_spi_probe,
	.remove =	mmc_spi_remove,
};

module_spi_driver(mmc_spi_driver);

MODULE_AUTHOR("Mike Lavender, David Brownell, Hans-Peter Nilsson, Jan Nikitenko");
MODULE_DESCRIPTION("SPI SD/MMC host driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:mmc_spi");
