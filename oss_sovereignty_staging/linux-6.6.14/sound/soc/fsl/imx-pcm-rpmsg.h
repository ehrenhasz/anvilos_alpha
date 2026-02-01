 
 

#ifndef _IMX_PCM_RPMSG_H
#define _IMX_PCM_RPMSG_H

#include <linux/pm_qos.h>
#include <linux/interrupt.h>
#include <sound/dmaengine_pcm.h>

#define RPMSG_TIMEOUT 1000

 
#define TX_OPEN		0x0
#define	TX_START	0x1
#define	TX_PAUSE	0x2
#define	TX_RESTART	0x3
#define	TX_TERMINATE	0x4
#define	TX_CLOSE	0x5
#define TX_HW_PARAM	0x6
#define	TX_BUFFER	0x7
#define	TX_SUSPEND	0x8
#define	TX_RESUME	0x9

#define	RX_OPEN		0xA
#define	RX_START	0xB
#define	RX_PAUSE	0xC
#define	RX_RESTART	0xD
#define	RX_TERMINATE	0xE
#define	RX_CLOSE	0xF
#define	RX_HW_PARAM	0x10
#define	RX_BUFFER	0x11
#define	RX_SUSPEND	0x12
#define	RX_RESUME	0x13
#define SET_CODEC_VALUE 0x14
#define GET_CODEC_VALUE 0x15
#define	TX_POINTER	0x16
#define	RX_POINTER	0x17
 
#define MSG_TYPE_A_NUM  0x18

 
#define	TX_PERIOD_DONE	0x0
#define	RX_PERIOD_DONE	0x1
 
#define MSG_TYPE_C_NUM  0x2

#define MSG_MAX_NUM     (MSG_TYPE_A_NUM + MSG_TYPE_C_NUM)

#define MSG_TYPE_A	0x0
#define MSG_TYPE_B	0x1
#define MSG_TYPE_C	0x2

#define RESP_NONE		0x0
#define RESP_NOT_ALLOWED	0x1
#define	RESP_SUCCESS		0x2
#define	RESP_FAILED		0x3

#define	RPMSG_S16_LE		0x0
#define	RPMSG_S24_LE		0x1
#define	RPMSG_S32_LE		0x2
#define	RPMSG_DSD_U16_LE	49   
#define	RPMSG_DSD_U24_LE	0x4
#define	RPMSG_DSD_U32_LE	50   

#define	RPMSG_CH_LEFT		0x0
#define	RPMSG_CH_RIGHT		0x1
#define	RPMSG_CH_STEREO		0x2

#define WORK_MAX_NUM    0x30

 
#define IMX_RMPSG_LIFECYCLE     1
#define IMX_RPMSG_PMIC          2
#define IMX_RPMSG_AUDIO         3
#define IMX_RPMSG_KEY           4
#define IMX_RPMSG_GPIO          5
#define IMX_RPMSG_RTC           6
#define IMX_RPMSG_SENSOR        7

 
#define IMX_RMPSG_MAJOR         1
#define IMX_RMPSG_MINOR         0

#define TX SNDRV_PCM_STREAM_PLAYBACK
#define RX SNDRV_PCM_STREAM_CAPTURE

 
struct rpmsg_head {
	u8 cate;
	u8 major;
	u8 minor;
	u8 type;
	u8 cmd;
	u8 reserved[5];
} __packed;

 
struct param_s {
	unsigned char audioindex;
	unsigned char format;
	unsigned char channels;
	unsigned int  rate;
	unsigned int  buffer_addr;
	unsigned int  buffer_size;
	unsigned int  period_size;
	unsigned int  buffer_tail;
} __packed;

 
struct param_r {
	unsigned char audioindex;
	unsigned char resp;
	unsigned char reserved1[1];
	unsigned int  buffer_offset;
	unsigned int  reg_addr;
	unsigned int  reg_data;
	unsigned char reserved2[4];
	unsigned int  buffer_tail;
} __packed;

 
struct rpmsg_s_msg {
	struct rpmsg_head header;
	struct param_s    param;
};

 
struct rpmsg_r_msg {
	struct rpmsg_head header;
	struct param_r    param;
};

 
struct rpmsg_msg {
	struct rpmsg_s_msg  s_msg;
	struct rpmsg_r_msg  r_msg;
};

 
struct work_of_rpmsg {
	struct rpmsg_info   *info;
	 
	struct rpmsg_msg    msg;
	struct work_struct  work;
};

 
struct stream_timer {
	struct timer_list   timer;
	struct rpmsg_info   *info;
	struct snd_pcm_substream *substream;
};

typedef void (*dma_callback)(void *arg);

 
struct rpmsg_info {
	struct rpmsg_device      *rpdev;
	struct device            *dev;
	struct completion        cmd_complete;
	struct pm_qos_request    pm_qos_req;

	 
	struct rpmsg_r_msg       r_msg;
	struct rpmsg_msg         msg[MSG_MAX_NUM];
	 
	struct rpmsg_msg         notify[2];
	bool                     notify_updated[2];

	struct workqueue_struct  *rpmsg_wq;
	struct work_of_rpmsg	 work_list[WORK_MAX_NUM];
	int                      work_write_index;
	int                      work_read_index;
	int                      msg_drop_count[2];
	int                      num_period[2];
	void                     *callback_param[2];
	dma_callback             callback[2];
	int (*send_message)(struct rpmsg_msg *msg, struct rpmsg_info *info);
	spinlock_t               lock[2];  
	spinlock_t               wq_lock;  
	struct mutex             msg_lock;  
	struct stream_timer      stream_timer[2];
};

#define IMX_PCM_DRV_NAME "imx_pcm_rpmsg"

#endif  
