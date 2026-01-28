#ifndef NCR5380_H
#define NCR5380_H
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_transport_spi.h>
#define NDEBUG_ARBITRATION	0x1
#define NDEBUG_AUTOSENSE	0x2
#define NDEBUG_DMA		0x4
#define NDEBUG_HANDSHAKE	0x8
#define NDEBUG_INFORMATION	0x10
#define NDEBUG_INIT		0x20
#define NDEBUG_INTR		0x40
#define NDEBUG_LINKED		0x80
#define NDEBUG_MAIN		0x100
#define NDEBUG_NO_DATAOUT	0x200
#define NDEBUG_NO_WRITE		0x400
#define NDEBUG_PIO		0x800
#define NDEBUG_PSEUDO_DMA	0x1000
#define NDEBUG_QUEUES		0x2000
#define NDEBUG_RESELECTION	0x4000
#define NDEBUG_SELECTION	0x8000
#define NDEBUG_USLEEP		0x10000
#define NDEBUG_LAST_BYTE_SENT	0x20000
#define NDEBUG_RESTART_SELECT	0x40000
#define NDEBUG_EXTENDED		0x80000
#define NDEBUG_C400_PREAD	0x100000
#define NDEBUG_C400_PWRITE	0x200000
#define NDEBUG_LISTS		0x400000
#define NDEBUG_ABORT		0x800000
#define NDEBUG_TAGS		0x1000000
#define NDEBUG_MERGING		0x2000000
#define NDEBUG_ANY		0xFFFFFFFFUL
#define OUTPUT_DATA_REG         0	 
#define CURRENT_SCSI_DATA_REG   0	 
#define INITIATOR_COMMAND_REG	1	 
#define ICR_ASSERT_RST		0x80	 
#define ICR_ARBITRATION_PROGRESS 0x40	 
#define ICR_TRI_STATE		0x40	 
#define ICR_ARBITRATION_LOST	0x20	 
#define ICR_DIFF_ENABLE		0x20	 
#define ICR_ASSERT_ACK		0x10	 
#define ICR_ASSERT_BSY		0x08	 
#define ICR_ASSERT_SEL 		0x04	 
#define ICR_ASSERT_ATN		0x02	 
#define ICR_ASSERT_DATA		0x01	 
#define ICR_BASE		0
#define MODE_REG		2
#define MR_BLOCK_DMA_MODE	0x80	 
#define MR_TARGET		0x40	 
#define MR_ENABLE_PAR_CHECK	0x20	 
#define MR_ENABLE_PAR_INTR	0x10	 
#define MR_ENABLE_EOP_INTR	0x08	 
#define MR_MONITOR_BSY		0x04	 
#define MR_DMA_MODE		0x02	 
#define MR_ARBITRATE		0x01	 
#define MR_BASE			0
#define TARGET_COMMAND_REG	3
#define TCR_LAST_BYTE_SENT	0x80	 
#define TCR_ASSERT_REQ		0x08	 
#define TCR_ASSERT_MSG		0x04	 
#define TCR_ASSERT_CD		0x02	 
#define TCR_ASSERT_IO		0x01	 
#define STATUS_REG		4	 
#define SR_RST			0x80
#define SR_BSY			0x40
#define SR_REQ			0x20
#define SR_MSG			0x10
#define SR_CD			0x08
#define SR_IO			0x04
#define SR_SEL			0x02
#define SR_DBP			0x01
#define SELECT_ENABLE_REG	4	 
#define BUS_AND_STATUS_REG	5	 
#define BASR_END_DMA_TRANSFER	0x80	 
#define BASR_DRQ		0x40	 
#define BASR_PARITY_ERROR	0x20	 
#define BASR_IRQ		0x10	 
#define BASR_PHASE_MATCH	0x08	 
#define BASR_BUSY_ERROR		0x04	 
#define BASR_ATN 		0x02	 
#define BASR_ACK		0x01	 
#define START_DMA_SEND_REG	5	 
#define INPUT_DATA_REG			6	 
#define START_DMA_TARGET_RECEIVE_REG	6	 
#define RESET_PARITY_INTERRUPT_REG	7	 
#define START_DMA_INITIATOR_RECEIVE_REG 7	 
#define CSR_RESET              0x80	 
#define CSR_53C80_REG          0x80	 
#define CSR_TRANS_DIR          0x40	 
#define CSR_SCSI_BUFF_INTR     0x20	 
#define CSR_53C80_INTR         0x10	 
#define CSR_SHARED_INTR        0x08	 
#define CSR_HOST_BUF_NOT_RDY   0x04	 
#define CSR_SCSI_BUF_RDY       0x02	 
#define CSR_GATED_53C80_IRQ    0x01	 
#define CSR_BASE CSR_53C80_INTR
#define PHASE_MASK 	(SR_MSG | SR_CD | SR_IO)
#define PHASE_DATAOUT		0
#define PHASE_DATAIN		SR_IO
#define PHASE_CMDOUT		SR_CD
#define PHASE_STATIN		(SR_CD | SR_IO)
#define PHASE_MSGOUT		(SR_MSG | SR_CD)
#define PHASE_MSGIN		(SR_MSG | SR_CD | SR_IO)
#define PHASE_UNKNOWN		0xff
#define PHASE_SR_TO_TCR(phase) ((phase) >> 2)
#ifndef NO_IRQ
#define NO_IRQ		0
#endif
#define FLAG_DMA_FIXUP			1	 
#define FLAG_NO_PSEUDO_DMA		8	 
#define FLAG_LATE_DMA_SETUP		32	 
#define FLAG_TOSHIBA_DELAY		128	 
struct NCR5380_hostdata {
	NCR5380_implementation_fields;		 
	u8 __iomem *io;				 
	u8 __iomem *pdma_io;			 
	unsigned long poll_loops;		 
	spinlock_t lock;			 
	struct scsi_cmnd *connected;		 
	struct list_head disconnected;		 
	struct Scsi_Host *host;			 
	struct workqueue_struct *work_q;	 
	struct work_struct main_task;		 
	int flags;				 
	int dma_len;				 
	int read_overruns;	 
	unsigned long io_port;			 
	unsigned long base;			 
	struct list_head unissued;		 
	struct scsi_cmnd *selecting;		 
	struct list_head autosense;		 
	struct scsi_cmnd *sensing;		 
	struct scsi_eh_save ses;		 
	unsigned char busy[8];			 
	unsigned char id_mask;			 
	unsigned char id_higher_mask;		 
	unsigned char last_message;		 
	unsigned long region_size;		 
	char info[168];				 
};
struct NCR5380_cmd {
	char *ptr;
	int this_residual;
	struct scatterlist *buffer;
	int status;
	int message;
	int phase;
	struct list_head list;
};
#define NCR5380_PIO_CHUNK_SIZE		256
#define NCR5380_REG_POLL_TIME		10
static inline struct scsi_cmnd *NCR5380_to_scmd(struct NCR5380_cmd *ncmd_ptr)
{
	return ((struct scsi_cmnd *)ncmd_ptr) - 1;
}
static inline struct NCR5380_cmd *NCR5380_to_ncmd(struct scsi_cmnd *cmd)
{
	return scsi_cmd_priv(cmd);
}
#ifndef NDEBUG
#define NDEBUG (0)
#endif
#define dprintk(flg, fmt, ...) \
	do { if ((NDEBUG) & (flg)) \
		printk(KERN_DEBUG fmt, ## __VA_ARGS__); } while (0)
#define dsprintk(flg, host, fmt, ...) \
	do { if ((NDEBUG) & (flg)) \
		shost_printk(KERN_DEBUG, host, fmt, ## __VA_ARGS__); \
	} while (0)
#if NDEBUG
#define NCR5380_dprint(flg, arg) \
	do { if ((NDEBUG) & (flg)) NCR5380_print(arg); } while (0)
#define NCR5380_dprint_phase(flg, arg) \
	do { if ((NDEBUG) & (flg)) NCR5380_print_phase(arg); } while (0)
static void NCR5380_print_phase(struct Scsi_Host *instance);
static void NCR5380_print(struct Scsi_Host *instance);
#else
#define NCR5380_dprint(flg, arg)       do {} while (0)
#define NCR5380_dprint_phase(flg, arg) do {} while (0)
#endif
static int NCR5380_init(struct Scsi_Host *instance, int flags);
static int NCR5380_maybe_reset_bus(struct Scsi_Host *);
static void NCR5380_exit(struct Scsi_Host *instance);
static void NCR5380_information_transfer(struct Scsi_Host *instance);
static irqreturn_t NCR5380_intr(int irq, void *dev_id);
static void NCR5380_main(struct work_struct *work);
static const char *NCR5380_info(struct Scsi_Host *instance);
static void NCR5380_reselect(struct Scsi_Host *instance);
static bool NCR5380_select(struct Scsi_Host *, struct scsi_cmnd *);
static int NCR5380_transfer_dma(struct Scsi_Host *instance, unsigned char *phase, int *count, unsigned char **data);
static int NCR5380_transfer_pio(struct Scsi_Host *instance, unsigned char *phase, int *count, unsigned char **data,
				unsigned int can_sleep);
static int NCR5380_poll_politely2(struct NCR5380_hostdata *,
                                  unsigned int, u8, u8,
                                  unsigned int, u8, u8, unsigned long);
static inline int NCR5380_poll_politely(struct NCR5380_hostdata *hostdata,
                                        unsigned int reg, u8 bit, u8 val,
                                        unsigned long wait)
{
	if ((NCR5380_read(reg) & bit) == val)
		return 0;
	return NCR5380_poll_politely2(hostdata, reg, bit, val,
						reg, bit, val, wait);
}
static int NCR5380_dma_xfer_len(struct NCR5380_hostdata *,
                                struct scsi_cmnd *);
static int NCR5380_dma_send_setup(struct NCR5380_hostdata *,
                                  unsigned char *, int);
static int NCR5380_dma_recv_setup(struct NCR5380_hostdata *,
                                  unsigned char *, int);
static int NCR5380_dma_residual(struct NCR5380_hostdata *);
static inline int NCR5380_dma_xfer_none(struct NCR5380_hostdata *hostdata,
                                        struct scsi_cmnd *cmd)
{
	return 0;
}
static inline int NCR5380_dma_setup_none(struct NCR5380_hostdata *hostdata,
                                         unsigned char *data, int count)
{
	return 0;
}
static inline int NCR5380_dma_residual_none(struct NCR5380_hostdata *hostdata)
{
	return 0;
}
#endif				 
