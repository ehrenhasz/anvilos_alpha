
 

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/list_sort.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/vmalloc.h>
#include <linux/spi/spi.h>

#include "spi-test.h"

 
static int simulate_only;
module_param(simulate_only, int, 0);
MODULE_PARM_DESC(simulate_only, "if not 0 do not execute the spi message");

 
static int dump_messages;
module_param(dump_messages, int, 0);
MODULE_PARM_DESC(dump_messages,
		 "=1 dump the basic spi_message_structure, " \
		 "=2 dump the spi_message_structure including data, " \
		 "=3 dump the spi_message structure before and after execution");
 
static int loopback;
module_param(loopback, int, 0);
MODULE_PARM_DESC(loopback,
		 "if set enable loopback mode, where the rx_buf "	\
		 "is checked to match tx_buf after the spi_message "	\
		 "is executed");

static int loop_req;
module_param(loop_req, int, 0);
MODULE_PARM_DESC(loop_req,
		 "if set controller will be asked to enable test loop mode. " \
		 "If controller supported it, MISO and MOSI will be connected");

static int no_cs;
module_param(no_cs, int, 0);
MODULE_PARM_DESC(no_cs,
		 "if set Chip Select (CS) will not be used");

 
static int run_only_iter_len = -1;
module_param(run_only_iter_len, int, 0);
MODULE_PARM_DESC(run_only_iter_len,
		 "only run tests for a length of this number in iterate_len list");

 
static int run_only_test = -1;
module_param(run_only_test, int, 0);
MODULE_PARM_DESC(run_only_test,
		 "only run the test with this number (0-based !)");

 
static int use_vmalloc;
module_param(use_vmalloc, int, 0644);
MODULE_PARM_DESC(use_vmalloc,
		 "use vmalloc'ed buffers instead of kmalloc'ed");

 
static int check_ranges = 1;
module_param(check_ranges, int, 0644);
MODULE_PARM_DESC(check_ranges,
		 "checks rx_buffer pattern are valid");

static unsigned int delay_ms = 100;
module_param(delay_ms, uint, 0644);
MODULE_PARM_DESC(delay_ms,
		 "delay between tests, in milliseconds (default: 100)");

 
static struct spi_test spi_tests[] = {
	{
		.description	= "tx/rx-transfer - start of page",
		.fill_option	= FILL_COUNT_8,
		.iterate_len    = { ITERATE_MAX_LEN },
		.iterate_tx_align = ITERATE_ALIGN,
		.iterate_rx_align = ITERATE_ALIGN,
		.transfer_count = 1,
		.transfers		= {
			{
				.tx_buf = TX(0),
				.rx_buf = RX(0),
			},
		},
	},
	{
		.description	= "tx/rx-transfer - crossing PAGE_SIZE",
		.fill_option	= FILL_COUNT_8,
		.iterate_len    = { ITERATE_LEN },
		.iterate_tx_align = ITERATE_ALIGN,
		.iterate_rx_align = ITERATE_ALIGN,
		.transfer_count = 1,
		.transfers		= {
			{
				.tx_buf = TX(PAGE_SIZE - 4),
				.rx_buf = RX(PAGE_SIZE - 4),
			},
		},
	},
	{
		.description	= "tx-transfer - only",
		.fill_option	= FILL_COUNT_8,
		.iterate_len    = { ITERATE_MAX_LEN },
		.iterate_tx_align = ITERATE_ALIGN,
		.transfer_count = 1,
		.transfers		= {
			{
				.tx_buf = TX(0),
			},
		},
	},
	{
		.description	= "rx-transfer - only",
		.fill_option	= FILL_COUNT_8,
		.iterate_len    = { ITERATE_MAX_LEN },
		.iterate_rx_align = ITERATE_ALIGN,
		.transfer_count = 1,
		.transfers		= {
			{
				.rx_buf = RX(0),
			},
		},
	},
	{
		.description	= "two tx-transfers - alter both",
		.fill_option	= FILL_COUNT_8,
		.iterate_len    = { ITERATE_LEN },
		.iterate_tx_align = ITERATE_ALIGN,
		.iterate_transfer_mask = BIT(0) | BIT(1),
		.transfer_count = 2,
		.transfers		= {
			{
				.tx_buf = TX(0),
			},
			{
				 
				.tx_buf = TX(SPI_TEST_MAX_SIZE_HALF),
			},
		},
	},
	{
		.description	= "two tx-transfers - alter first",
		.fill_option	= FILL_COUNT_8,
		.iterate_len    = { ITERATE_MAX_LEN },
		.iterate_tx_align = ITERATE_ALIGN,
		.iterate_transfer_mask = BIT(0),
		.transfer_count = 2,
		.transfers		= {
			{
				.tx_buf = TX(64),
			},
			{
				.len = 1,
				.tx_buf = TX(0),
			},
		},
	},
	{
		.description	= "two tx-transfers - alter second",
		.fill_option	= FILL_COUNT_8,
		.iterate_len    = { ITERATE_MAX_LEN },
		.iterate_tx_align = ITERATE_ALIGN,
		.iterate_transfer_mask = BIT(1),
		.transfer_count = 2,
		.transfers		= {
			{
				.len = 16,
				.tx_buf = TX(0),
			},
			{
				.tx_buf = TX(64),
			},
		},
	},
	{
		.description	= "two transfers tx then rx - alter both",
		.fill_option	= FILL_COUNT_8,
		.iterate_len    = { ITERATE_MAX_LEN },
		.iterate_tx_align = ITERATE_ALIGN,
		.iterate_transfer_mask = BIT(0) | BIT(1),
		.transfer_count = 2,
		.transfers		= {
			{
				.tx_buf = TX(0),
			},
			{
				.rx_buf = RX(0),
			},
		},
	},
	{
		.description	= "two transfers tx then rx - alter tx",
		.fill_option	= FILL_COUNT_8,
		.iterate_len    = { ITERATE_MAX_LEN },
		.iterate_tx_align = ITERATE_ALIGN,
		.iterate_transfer_mask = BIT(0),
		.transfer_count = 2,
		.transfers		= {
			{
				.tx_buf = TX(0),
			},
			{
				.len = 1,
				.rx_buf = RX(0),
			},
		},
	},
	{
		.description	= "two transfers tx then rx - alter rx",
		.fill_option	= FILL_COUNT_8,
		.iterate_len    = { ITERATE_MAX_LEN },
		.iterate_tx_align = ITERATE_ALIGN,
		.iterate_transfer_mask = BIT(1),
		.transfer_count = 2,
		.transfers		= {
			{
				.len = 1,
				.tx_buf = TX(0),
			},
			{
				.rx_buf = RX(0),
			},
		},
	},
	{
		.description	= "two tx+rx transfers - alter both",
		.fill_option	= FILL_COUNT_8,
		.iterate_len    = { ITERATE_LEN },
		.iterate_tx_align = ITERATE_ALIGN,
		.iterate_transfer_mask = BIT(0) | BIT(1),
		.transfer_count = 2,
		.transfers		= {
			{
				.tx_buf = TX(0),
				.rx_buf = RX(0),
			},
			{
				 
				.tx_buf = TX(SPI_TEST_MAX_SIZE_HALF),
				.rx_buf = RX(SPI_TEST_MAX_SIZE_HALF),
			},
		},
	},
	{
		.description	= "two tx+rx transfers - alter first",
		.fill_option	= FILL_COUNT_8,
		.iterate_len    = { ITERATE_MAX_LEN },
		.iterate_tx_align = ITERATE_ALIGN,
		.iterate_transfer_mask = BIT(0),
		.transfer_count = 2,
		.transfers		= {
			{
				 
				.tx_buf = TX(1024),
				.rx_buf = RX(1024),
			},
			{
				.len = 1,
				 
				.tx_buf = TX(0),
				.rx_buf = RX(0),
			},
		},
	},
	{
		.description	= "two tx+rx transfers - alter second",
		.fill_option	= FILL_COUNT_8,
		.iterate_len    = { ITERATE_MAX_LEN },
		.iterate_tx_align = ITERATE_ALIGN,
		.iterate_transfer_mask = BIT(1),
		.transfer_count = 2,
		.transfers		= {
			{
				.len = 1,
				.tx_buf = TX(0),
				.rx_buf = RX(0),
			},
			{
				 
				.tx_buf = TX(1024),
				.rx_buf = RX(1024),
			},
		},
	},
	{
		.description	= "two tx+rx transfers - delay after transfer",
		.fill_option	= FILL_COUNT_8,
		.iterate_len    = { ITERATE_MAX_LEN },
		.iterate_transfer_mask = BIT(0) | BIT(1),
		.transfer_count = 2,
		.transfers		= {
			{
				.tx_buf = TX(0),
				.rx_buf = RX(0),
				.delay = {
					.value = 1000,
					.unit = SPI_DELAY_UNIT_USECS,
				},
			},
			{
				.tx_buf = TX(0),
				.rx_buf = RX(0),
				.delay = {
					.value = 1000,
					.unit = SPI_DELAY_UNIT_USECS,
				},
			},
		},
	},
	{
		.description	= "three tx+rx transfers with overlapping cache lines",
		.fill_option	= FILL_COUNT_8,
		 
		.iterate_len    = { 512, -1 },
		.iterate_transfer_mask = BIT(1),
		.transfer_count = 3,
		.transfers		= {
			{
				.len = 1,
				.tx_buf = TX(0),
				.rx_buf = RX(0),
			},
			{
				.tx_buf = TX(1),
				.rx_buf = RX(1),
			},
			{
				.len = 1,
				.tx_buf = TX(513),
				.rx_buf = RX(513),
			},
		},
	},

	{   }
};

static int spi_loopback_test_probe(struct spi_device *spi)
{
	int ret;

	if (loop_req || no_cs) {
		spi->mode |= loop_req ? SPI_LOOP : 0;
		spi->mode |= no_cs ? SPI_NO_CS : 0;
		ret = spi_setup(spi);
		if (ret) {
			dev_err(&spi->dev, "SPI setup with SPI_LOOP or SPI_NO_CS failed (%d)\n",
				ret);
			return ret;
		}
	}

	dev_info(&spi->dev, "Executing spi-loopback-tests\n");

	ret = spi_test_run_tests(spi, spi_tests);

	dev_info(&spi->dev, "Finished spi-loopback-tests with return: %i\n",
		 ret);

	return ret;
}

 
static struct of_device_id spi_loopback_test_of_match[] = {
	{ .compatible	= "linux,spi-loopback-test", },
	{ }
};

 
module_param_string(compatible, spi_loopback_test_of_match[0].compatible,
		    sizeof(spi_loopback_test_of_match[0].compatible),
		    0000);

MODULE_DEVICE_TABLE(of, spi_loopback_test_of_match);

static struct spi_driver spi_loopback_test_driver = {
	.driver = {
		.name = "spi-loopback-test",
		.owner = THIS_MODULE,
		.of_match_table = spi_loopback_test_of_match,
	},
	.probe = spi_loopback_test_probe,
};

module_spi_driver(spi_loopback_test_driver);

MODULE_AUTHOR("Martin Sperl <kernel@martin.sperl.org>");
MODULE_DESCRIPTION("test spi_driver to check core functionality");
MODULE_LICENSE("GPL");

 

 

#define RANGE_CHECK(ptr, plen, start, slen) \
	((ptr >= start) && (ptr + plen <= start + slen))

 
#define SPI_TEST_MAX_SIZE_PLUS (SPI_TEST_MAX_SIZE + PAGE_SIZE)

static void spi_test_print_hex_dump(char *pre, const void *ptr, size_t len)
{
	 
	if (len < 1024) {
		print_hex_dump(KERN_INFO, pre,
			       DUMP_PREFIX_OFFSET, 16, 1,
			       ptr, len, 0);
		return;
	}
	 
	print_hex_dump(KERN_INFO, pre,
		       DUMP_PREFIX_OFFSET, 16, 1,
		       ptr, 512, 0);
	 
	pr_info("%s truncated - continuing at offset %04zx\n",
		pre, len - 512);
	print_hex_dump(KERN_INFO, pre,
		       DUMP_PREFIX_OFFSET, 16, 1,
		       ptr + (len - 512), 512, 0);
}

static void spi_test_dump_message(struct spi_device *spi,
				  struct spi_message *msg,
				  bool dump_data)
{
	struct spi_transfer *xfer;
	int i;
	u8 b;

	dev_info(&spi->dev, "  spi_msg@%pK\n", msg);
	if (msg->status)
		dev_info(&spi->dev, "    status:        %i\n",
			 msg->status);
	dev_info(&spi->dev, "    frame_length:  %i\n",
		 msg->frame_length);
	dev_info(&spi->dev, "    actual_length: %i\n",
		 msg->actual_length);

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		dev_info(&spi->dev, "    spi_transfer@%pK\n", xfer);
		dev_info(&spi->dev, "      len:    %i\n", xfer->len);
		dev_info(&spi->dev, "      tx_buf: %pK\n", xfer->tx_buf);
		if (dump_data && xfer->tx_buf)
			spi_test_print_hex_dump("          TX: ",
						xfer->tx_buf,
						xfer->len);

		dev_info(&spi->dev, "      rx_buf: %pK\n", xfer->rx_buf);
		if (dump_data && xfer->rx_buf)
			spi_test_print_hex_dump("          RX: ",
						xfer->rx_buf,
						xfer->len);
		 
		if (xfer->rx_buf) {
			for (i = 0 ; i < xfer->len ; i++) {
				b = ((u8 *)xfer->rx_buf)[xfer->len - 1 - i];
				if (b != SPI_TEST_PATTERN_UNWRITTEN)
					break;
			}
			if (i)
				dev_info(&spi->dev,
					 "      rx_buf filled with %02x starts at offset: %i\n",
					 SPI_TEST_PATTERN_UNWRITTEN,
					 xfer->len - i);
		}
	}
}

struct rx_ranges {
	struct list_head list;
	u8 *start;
	u8 *end;
};

static int rx_ranges_cmp(void *priv, const struct list_head *a,
			 const struct list_head *b)
{
	struct rx_ranges *rx_a = list_entry(a, struct rx_ranges, list);
	struct rx_ranges *rx_b = list_entry(b, struct rx_ranges, list);

	if (rx_a->start > rx_b->start)
		return 1;
	if (rx_a->start < rx_b->start)
		return -1;
	return 0;
}

static int spi_check_rx_ranges(struct spi_device *spi,
			       struct spi_message *msg,
			       void *rx)
{
	struct spi_transfer *xfer;
	struct rx_ranges ranges[SPI_TEST_MAX_TRANSFERS], *r;
	int i = 0;
	LIST_HEAD(ranges_list);
	u8 *addr;
	int ret = 0;

	 
	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		 
		if (!xfer->rx_buf)
			continue;
		 
		if (RANGE_CHECK(xfer->rx_buf, xfer->len,
				rx, SPI_TEST_MAX_SIZE_PLUS)) {
			ranges[i].start = xfer->rx_buf;
			ranges[i].end = xfer->rx_buf + xfer->len;
			list_add(&ranges[i].list, &ranges_list);
			i++;
		}
	}

	 
	if (!i)
		return 0;

	 
	list_sort(NULL, &ranges_list, rx_ranges_cmp);

	 
	for (addr = rx; addr < (u8 *)rx + SPI_TEST_MAX_SIZE_PLUS; addr++) {
		 
		if (*addr == SPI_TEST_PATTERN_DO_NOT_WRITE)
			continue;

		 
		list_for_each_entry(r, &ranges_list, list) {
			 
			if ((addr >= r->start) && (addr < r->end))
				addr = r->end;
		}
		 
		if (*addr == SPI_TEST_PATTERN_DO_NOT_WRITE)
			continue;

		 
		 
		dev_err(&spi->dev,
			"loopback strangeness - rx changed outside of allowed range at: %pK\n",
			addr);
		 
		ret = -ERANGE;
	}

	return ret;
}

static int spi_test_check_elapsed_time(struct spi_device *spi,
				       struct spi_test *test)
{
	int i;
	unsigned long long estimated_time = 0;
	unsigned long long delay_usecs = 0;

	for (i = 0; i < test->transfer_count; i++) {
		struct spi_transfer *xfer = test->transfers + i;
		unsigned long long nbits = (unsigned long long)BITS_PER_BYTE *
					   xfer->len;

		delay_usecs += xfer->delay.value;
		if (!xfer->speed_hz)
			continue;
		estimated_time += div_u64(nbits * NSEC_PER_SEC, xfer->speed_hz);
	}

	estimated_time += delay_usecs * NSEC_PER_USEC;
	if (test->elapsed_time < estimated_time) {
		dev_err(&spi->dev,
			"elapsed time %lld ns is shorter than minimum estimated time %lld ns\n",
			test->elapsed_time, estimated_time);

		return -EINVAL;
	}

	return 0;
}

static int spi_test_check_loopback_result(struct spi_device *spi,
					  struct spi_message *msg,
					  void *tx, void *rx)
{
	struct spi_transfer *xfer;
	u8 rxb, txb;
	size_t i;
	int ret;

	 
	if (check_ranges) {
		ret = spi_check_rx_ranges(spi, msg, rx);
		if (ret)
			return ret;
	}

	 
	if (!loopback)
		return 0;

	 
	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		 
		if (!xfer->len || !xfer->rx_buf)
			continue;
		 
		if (xfer->tx_buf) {
			for (i = 0; i < xfer->len; i++) {
				txb = ((u8 *)xfer->tx_buf)[i];
				rxb = ((u8 *)xfer->rx_buf)[i];
				if (txb != rxb)
					goto mismatch_error;
			}
		} else {
			 
			txb = ((u8 *)xfer->rx_buf)[0];
			 
			if (!((txb == 0) || (txb == 0xff))) {
				dev_err(&spi->dev,
					"loopback strangeness - we expect 0x00 or 0xff, but not 0x%02x\n",
					txb);
				return -EINVAL;
			}
			 
			for (i = 1; i < xfer->len; i++) {
				rxb = ((u8 *)xfer->rx_buf)[i];
				if (rxb != txb)
					goto mismatch_error;
			}
		}
	}

	return 0;

mismatch_error:
	dev_err(&spi->dev,
		"loopback strangeness - transfer mismatch on byte %04zx - expected 0x%02x, but got 0x%02x\n",
		i, txb, rxb);

	return -EINVAL;
}

static int spi_test_translate(struct spi_device *spi,
			      void **ptr, size_t len,
			      void *tx, void *rx)
{
	size_t off;

	 
	if (!*ptr)
		return 0;

	 
	if (((size_t)*ptr) & SPI_TEST_MAX_SIZE_HALF)
		 
		*ptr += (SPI_TEST_MAX_SIZE_PLUS / 2) -
			SPI_TEST_MAX_SIZE_HALF;

	 
	if (RANGE_CHECK(*ptr, len,  RX(0), SPI_TEST_MAX_SIZE_PLUS)) {
		off = *ptr - RX(0);
		*ptr = rx + off;

		return 0;
	}

	 
	if (RANGE_CHECK(*ptr, len,  TX(0), SPI_TEST_MAX_SIZE_PLUS)) {
		off = *ptr - TX(0);
		*ptr = tx + off;

		return 0;
	}

	dev_err(&spi->dev,
		"PointerRange [%pK:%pK[ not in range [%pK:%pK[ or [%pK:%pK[\n",
		*ptr, *ptr + len,
		RX(0), RX(SPI_TEST_MAX_SIZE),
		TX(0), TX(SPI_TEST_MAX_SIZE));

	return -EINVAL;
}

static int spi_test_fill_pattern(struct spi_device *spi,
				 struct spi_test *test)
{
	struct spi_transfer *xfers = test->transfers;
	u8 *tx_buf;
	size_t count = 0;
	int i, j;

#ifdef __BIG_ENDIAN
#define GET_VALUE_BYTE(value, index, bytes) \
	(value >> (8 * (bytes - 1 - count % bytes)))
#else
#define GET_VALUE_BYTE(value, index, bytes) \
	(value >> (8 * (count % bytes)))
#endif

	 
	for (i = 0; i < test->transfer_count; i++) {
		 
		if (xfers[i].rx_buf)
			memset(xfers[i].rx_buf, SPI_TEST_PATTERN_UNWRITTEN,
			       xfers[i].len);
		 
		tx_buf = (u8 *)xfers[i].tx_buf;
		if (!tx_buf)
			continue;
		 
		for (j = 0; j < xfers[i].len; j++, tx_buf++, count++) {
			 
			switch (test->fill_option) {
			case FILL_MEMSET_8:
				*tx_buf = test->fill_pattern;
				break;
			case FILL_MEMSET_16:
				*tx_buf = GET_VALUE_BYTE(test->fill_pattern,
							 count, 2);
				break;
			case FILL_MEMSET_24:
				*tx_buf = GET_VALUE_BYTE(test->fill_pattern,
							 count, 3);
				break;
			case FILL_MEMSET_32:
				*tx_buf = GET_VALUE_BYTE(test->fill_pattern,
							 count, 4);
				break;
			case FILL_COUNT_8:
				*tx_buf = count;
				break;
			case FILL_COUNT_16:
				*tx_buf = GET_VALUE_BYTE(count, count, 2);
				break;
			case FILL_COUNT_24:
				*tx_buf = GET_VALUE_BYTE(count, count, 3);
				break;
			case FILL_COUNT_32:
				*tx_buf = GET_VALUE_BYTE(count, count, 4);
				break;
			case FILL_TRANSFER_BYTE_8:
				*tx_buf = j;
				break;
			case FILL_TRANSFER_BYTE_16:
				*tx_buf = GET_VALUE_BYTE(j, j, 2);
				break;
			case FILL_TRANSFER_BYTE_24:
				*tx_buf = GET_VALUE_BYTE(j, j, 3);
				break;
			case FILL_TRANSFER_BYTE_32:
				*tx_buf = GET_VALUE_BYTE(j, j, 4);
				break;
			case FILL_TRANSFER_NUM:
				*tx_buf = i;
				break;
			default:
				dev_err(&spi->dev,
					"unsupported fill_option: %i\n",
					test->fill_option);
				return -EINVAL;
			}
		}
	}

	return 0;
}

static int _spi_test_run_iter(struct spi_device *spi,
			      struct spi_test *test,
			      void *tx, void *rx)
{
	struct spi_message *msg = &test->msg;
	struct spi_transfer *x;
	int i, ret;

	 
	spi_message_init_no_memset(msg);

	 
	memset(rx, SPI_TEST_PATTERN_DO_NOT_WRITE, SPI_TEST_MAX_SIZE_PLUS);

	 
	for (i = 0; i < test->transfer_count; i++) {
		x = &test->transfers[i];

		 
		ret = spi_test_translate(spi, (void **)&x->tx_buf, x->len,
					 (void *)tx, rx);
		if (ret)
			return ret;

		 
		ret = spi_test_translate(spi, &x->rx_buf, x->len,
					 (void *)tx, rx);
		if (ret)
			return ret;

		 
		spi_message_add_tail(x, msg);
	}

	 
	ret = spi_test_fill_pattern(spi, test);
	if (ret)
		return ret;

	 
	if (test->execute_msg)
		ret = test->execute_msg(spi, test, tx, rx);
	else
		ret = spi_test_execute_msg(spi, test, tx, rx);

	 
	if (ret == test->expected_return)
		return 0;

	dev_err(&spi->dev,
		"test failed - test returned %i, but we expect %i\n",
		ret, test->expected_return);

	if (ret)
		return ret;

	 
	return -EFAULT;
}

static int spi_test_run_iter(struct spi_device *spi,
			     const struct spi_test *testtemplate,
			     void *tx, void *rx,
			     size_t len,
			     size_t tx_off,
			     size_t rx_off
	)
{
	struct spi_test test;
	int i, tx_count, rx_count;

	 
	memcpy(&test, testtemplate, sizeof(test));

	 
	if (!(test.iterate_transfer_mask & (BIT(test.transfer_count) - 1)))
		test.iterate_transfer_mask = 1;

	 
	rx_count = tx_count = 0;
	for (i = 0; i < test.transfer_count; i++) {
		if (test.transfers[i].tx_buf)
			tx_count++;
		if (test.transfers[i].rx_buf)
			rx_count++;
	}

	 
	if (tx_off && (!tx_count)) {
		dev_warn_once(&spi->dev,
			      "%s: iterate_tx_off configured with tx_buf==NULL - ignoring\n",
			      test.description);
		return 0;
	}
	if (rx_off && (!rx_count)) {
		dev_warn_once(&spi->dev,
			      "%s: iterate_rx_off configured with rx_buf==NULL - ignoring\n",
			      test.description);
		return 0;
	}

	 
	if (!(len || tx_off || rx_off)) {
		dev_info(&spi->dev, "Running test %s\n", test.description);
	} else {
		dev_info(&spi->dev,
			 "  with iteration values: len = %zu, tx_off = %zu, rx_off = %zu\n",
			 len, tx_off, rx_off);
	}

	 
	for (i = 0; i < test.transfer_count; i++) {
		 
		if (!(test.iterate_transfer_mask & BIT(i)))
			continue;
		test.transfers[i].len = len;
		if (test.transfers[i].tx_buf)
			test.transfers[i].tx_buf += tx_off;
		if (test.transfers[i].rx_buf)
			test.transfers[i].rx_buf += rx_off;
	}

	 
	return _spi_test_run_iter(spi, &test, tx, rx);
}

 
int spi_test_execute_msg(struct spi_device *spi, struct spi_test *test,
			 void *tx, void *rx)
{
	struct spi_message *msg = &test->msg;
	int ret = 0;
	int i;

	 
	if (!simulate_only) {
		ktime_t start;

		 
		if (dump_messages == 3)
			spi_test_dump_message(spi, msg, true);

		start = ktime_get();
		 
		ret = spi_sync(spi, msg);
		test->elapsed_time = ktime_to_ns(ktime_sub(ktime_get(), start));
		if (ret == -ETIMEDOUT) {
			dev_info(&spi->dev,
				 "spi-message timed out - rerunning...\n");
			 
			for (i = 0; i < 16; i++)
				schedule();
			ret = spi_sync(spi, msg);
		}
		if (ret) {
			dev_err(&spi->dev,
				"Failed to execute spi_message: %i\n",
				ret);
			goto exit;
		}

		 
		if (msg->frame_length != msg->actual_length) {
			dev_err(&spi->dev,
				"actual length differs from expected\n");
			ret = -EIO;
			goto exit;
		}

		 
		ret = spi_test_check_loopback_result(spi, msg, tx, rx);
		if (ret)
			goto exit;

		ret = spi_test_check_elapsed_time(spi, test);
	}

	 
exit:
	if (dump_messages || ret)
		spi_test_dump_message(spi, msg,
				      (dump_messages >= 2) || (ret));

	return ret;
}
EXPORT_SYMBOL_GPL(spi_test_execute_msg);

 

int spi_test_run_test(struct spi_device *spi, const struct spi_test *test,
		      void *tx, void *rx)
{
	int idx_len;
	size_t len;
	size_t tx_align, rx_align;
	int ret;

	 
	if (test->transfer_count >= SPI_TEST_MAX_TRANSFERS) {
		dev_err(&spi->dev,
			"%s: Exceeded max number of transfers with %i\n",
			test->description, test->transfer_count);
		return -E2BIG;
	}

	 

	 
#define FOR_EACH_ALIGNMENT(var)						\
	for (var = 0;							\
	    var < (test->iterate_##var ?				\
			(spi->master->dma_alignment ?			\
			 spi->master->dma_alignment :			\
			 test->iterate_##var) :				\
			1);						\
	    var++)

	for (idx_len = 0; idx_len < SPI_TEST_MAX_ITERATE &&
	     (len = test->iterate_len[idx_len]) != -1; idx_len++) {
		if ((run_only_iter_len > -1) && len != run_only_iter_len)
			continue;
		FOR_EACH_ALIGNMENT(tx_align) {
			FOR_EACH_ALIGNMENT(rx_align) {
				 
				ret = spi_test_run_iter(spi, test,
							tx, rx,
							len,
							tx_align,
							rx_align);
				if (ret)
					return ret;
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(spi_test_run_test);

 

int spi_test_run_tests(struct spi_device *spi,
		       struct spi_test *tests)
{
	char *rx = NULL, *tx = NULL;
	int ret = 0, count = 0;
	struct spi_test *test;

	 
	if (use_vmalloc)
		rx = vmalloc(SPI_TEST_MAX_SIZE_PLUS);
	else
		rx = kzalloc(SPI_TEST_MAX_SIZE_PLUS, GFP_KERNEL);
	if (!rx)
		return -ENOMEM;


	if (use_vmalloc)
		tx = vmalloc(SPI_TEST_MAX_SIZE_PLUS);
	else
		tx = kzalloc(SPI_TEST_MAX_SIZE_PLUS, GFP_KERNEL);
	if (!tx) {
		ret = -ENOMEM;
		goto err_tx;
	}

	 
	for (test = tests, count = 0; test->description[0];
	     test++, count++) {
		 
		if ((run_only_test > -1) && (count != run_only_test))
			continue;
		 
		if (test->run_test)
			ret = test->run_test(spi, test, tx, rx);
		else
			ret = spi_test_run_test(spi, test, tx, rx);
		if (ret)
			goto out;
		 
		if (delay_ms)
			mdelay(delay_ms);
		schedule();
	}

out:
	kvfree(tx);
err_tx:
	kvfree(rx);
	return ret;
}
EXPORT_SYMBOL_GPL(spi_test_run_tests);
