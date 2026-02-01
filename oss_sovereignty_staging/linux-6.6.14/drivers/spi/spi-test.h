 
 

#include <linux/spi/spi.h>

#define SPI_TEST_MAX_TRANSFERS 4
#define SPI_TEST_MAX_SIZE (32 * PAGE_SIZE)
#define SPI_TEST_MAX_ITERATE 32

 
#define RX_START	BIT(30)
#define TX_START	BIT(31)
#define RX(off)		((void *)(RX_START + off))
#define TX(off)		((void *)(TX_START + off))

 
#define SPI_TEST_MAX_SIZE_HALF	BIT(29)

 
#define SPI_TEST_PATTERN_UNWRITTEN 0xAA
#define SPI_TEST_PATTERN_DO_NOT_WRITE 0x55
#define SPI_TEST_CHECK_DO_NOT_WRITE 64

 

struct spi_test {
	char description[64];
	struct spi_message msg;
	struct spi_transfer transfers[SPI_TEST_MAX_TRANSFERS];
	unsigned int transfer_count;
	int (*run_test)(struct spi_device *spi, struct spi_test *test,
			void *tx, void *rx);
	int (*execute_msg)(struct spi_device *spi, struct spi_test *test,
			   void *tx, void *rx);
	int expected_return;
	 
	int iterate_len[SPI_TEST_MAX_ITERATE];
	int iterate_tx_align;
	int iterate_rx_align;
	u32 iterate_transfer_mask;
	 
	u32 fill_option;
#define FILL_MEMSET_8	0	 
#define FILL_MEMSET_16	1	 
#define FILL_MEMSET_24	2	 
#define FILL_MEMSET_32	3	 
#define FILL_COUNT_8	4	 
#define FILL_COUNT_16	5	 
#define FILL_COUNT_24	6	 
#define FILL_COUNT_32	7	 
#define FILL_TRANSFER_BYTE_8  8	 
#define FILL_TRANSFER_BYTE_16 9	 
#define FILL_TRANSFER_BYTE_24 10  
#define FILL_TRANSFER_BYTE_32 11  
#define FILL_TRANSFER_NUM     16  
	u32 fill_pattern;
	unsigned long long elapsed_time;
};

 
int spi_test_run_test(struct spi_device *spi,
		      const struct spi_test *test,
		      void *tx, void *rx);

 
int spi_test_execute_msg(struct spi_device *spi,
			 struct spi_test *test,
			 void *tx, void *rx);

 
int spi_test_run_tests(struct spi_device *spi,
		       struct spi_test *tests);

#define ITERATE_LEN_LIST 0, 1, 2, 3, 7, 11, 16, 31, 32, 64, 97, 128, 251, 256, \
		1021, 1024, 1031, 4093, PAGE_SIZE, 4099, 65536, 65537
 
#define ITERATE_LEN ITERATE_LEN_LIST, -1
#define ITERATE_MAX_LEN ITERATE_LEN_LIST, (SPI_TEST_MAX_SIZE - 1), \
		SPI_TEST_MAX_SIZE, -1

 
#define ITERATE_ALIGN sizeof(int)
