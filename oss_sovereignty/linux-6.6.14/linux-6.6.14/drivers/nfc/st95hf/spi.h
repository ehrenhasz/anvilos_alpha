#ifndef __LINUX_ST95HF_SPI_H
#define __LINUX_ST95HF_SPI_H
#include <linux/spi/spi.h>
#define ST95HF_COMMAND_SEND	0x0
#define ST95HF_COMMAND_RESET	0x1
#define ST95HF_COMMAND_RECEIVE	0x2
#define ST95HF_RESET_CMD_LEN	0x1
struct st95hf_spi_context {
	bool req_issync;
	struct spi_device *spidev;
	struct completion done;
	struct mutex spi_lock;
};
enum req_type {
	SYNC,
	ASYNC,
};
int st95hf_spi_send(struct st95hf_spi_context *spicontext,
		    unsigned char *buffertx,
		    int datalen,
		    enum req_type reqtype);
int st95hf_spi_recv_response(struct st95hf_spi_context *spicontext,
			     unsigned char *receivebuff);
int st95hf_spi_recv_echo_res(struct st95hf_spi_context *spicontext,
			     unsigned char *receivebuff);
#endif
