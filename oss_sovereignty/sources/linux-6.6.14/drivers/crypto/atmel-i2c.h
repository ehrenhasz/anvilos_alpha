


#ifndef __ATMEL_I2C_H__
#define __ATMEL_I2C_H__

#include <linux/hw_random.h>
#include <linux/types.h>

#define ATMEL_ECC_PRIORITY		300

#define COMMAND				0x03 
#define SLEEP_TOKEN			0x01
#define WAKE_TOKEN_MAX_SIZE		8


#define WORD_ADDR_SIZE			1
#define COUNT_SIZE			1
#define CRC_SIZE			2
#define CMD_OVERHEAD_SIZE		(COUNT_SIZE + CRC_SIZE)


#define ATMEL_ECC_NIST_P256_N_SIZE	32
#define ATMEL_ECC_PUBKEY_SIZE		(2 * ATMEL_ECC_NIST_P256_N_SIZE)

#define STATUS_RSP_SIZE			4
#define ECDH_RSP_SIZE			(32 + CMD_OVERHEAD_SIZE)
#define GENKEY_RSP_SIZE			(ATMEL_ECC_PUBKEY_SIZE + \
					 CMD_OVERHEAD_SIZE)
#define READ_RSP_SIZE			(4 + CMD_OVERHEAD_SIZE)
#define RANDOM_RSP_SIZE			(32 + CMD_OVERHEAD_SIZE)
#define MAX_RSP_SIZE			GENKEY_RSP_SIZE


struct atmel_i2c_cmd {
	u8 word_addr;
	u8 count;
	u8 opcode;
	u8 param1;
	__le16 param2;
	u8 data[MAX_RSP_SIZE];
	u8 msecs;
	u16 rxsize;
} __packed;


#define STATUS_SIZE			0x04
#define STATUS_NOERR			0x00
#define STATUS_WAKE_SUCCESSFUL		0x11


#define CONFIGURATION_ZONE		0


#define RSP_DATA_IDX			1 
#define DATA_SLOT_2			2 


#define DEVICE_LOCK_ADDR		0x15
#define LOCK_VALUE_IDX			(RSP_DATA_IDX + 2)
#define LOCK_CONFIG_IDX			(RSP_DATA_IDX + 3)


#define TWHI_MIN			1500
#define TWHI_MAX			1550


#define TWLO_USEC			60


#define MAX_EXEC_TIME_ECDH		58
#define MAX_EXEC_TIME_GENKEY		115
#define MAX_EXEC_TIME_READ		1
#define MAX_EXEC_TIME_RANDOM		50


#define OPCODE_ECDH			0x43
#define OPCODE_GENKEY			0x40
#define OPCODE_READ			0x02
#define OPCODE_RANDOM			0x1b


#define READ_COUNT			7


#define RANDOM_COUNT			7


#define GENKEY_COUNT			7
#define GENKEY_MODE_PRIVATE		0x04


#define ECDH_COUNT			71
#define ECDH_PREFIX_MODE		0x00


struct atmel_ecc_driver_data {
	struct list_head i2c_client_list;
	spinlock_t i2c_list_lock;
} ____cacheline_aligned;


struct atmel_i2c_client_priv {
	struct i2c_client *client;
	struct list_head i2c_client_list_node;
	struct mutex lock;
	u8 wake_token[WAKE_TOKEN_MAX_SIZE];
	size_t wake_token_sz;
	atomic_t tfm_count ____cacheline_aligned;
	struct hwrng hwrng;
};


struct atmel_i2c_work_data {
	void *ctx;
	struct i2c_client *client;
	void (*cbk)(struct atmel_i2c_work_data *work_data, void *areq,
		    int status);
	void *areq;
	struct work_struct work;
	struct atmel_i2c_cmd cmd;
};

int atmel_i2c_probe(struct i2c_client *client);

void atmel_i2c_enqueue(struct atmel_i2c_work_data *work_data,
		       void (*cbk)(struct atmel_i2c_work_data *work_data,
				   void *areq, int status),
		       void *areq);
void atmel_i2c_flush_queue(void);

int atmel_i2c_send_receive(struct i2c_client *client, struct atmel_i2c_cmd *cmd);

void atmel_i2c_init_read_cmd(struct atmel_i2c_cmd *cmd);
void atmel_i2c_init_random_cmd(struct atmel_i2c_cmd *cmd);
void atmel_i2c_init_genkey_cmd(struct atmel_i2c_cmd *cmd, u16 keyid);
int atmel_i2c_init_ecdh_cmd(struct atmel_i2c_cmd *cmd,
			    struct scatterlist *pubkey);

#endif 
