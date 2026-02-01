 
 

#ifndef __SOF_INTEL_HDA_IPC_H
#define __SOF_INTEL_HDA_IPC_H

 

 

 
#define HDA_IPC_RSVD_31		BIT(31)
 
#define HDA_IPC_MSG_COMPACT	BIT(30)
 
#define HDA_IPC_RSP		BIT(29)

#define HDA_IPC_TYPE_SHIFT	24
#define HDA_IPC_TYPE_MASK	GENMASK(28, 24)
#define HDA_IPC_TYPE(x)		((x) << HDA_IPC_TYPE_SHIFT)

#define HDA_IPC_PM_GATE		HDA_IPC_TYPE(0x8U)

 

 
#define HDA_PM_NO_DMA_TRACE	BIT(4)
 
#define HDA_PM_PCG		BIT(3)
 
#define HDA_PM_PPG		BIT(2)
 
#define HDA_PM_PG_STREAMING	BIT(1)
#define HDA_PM_PG_RSVD		BIT(0)

irqreturn_t cnl_ipc_irq_thread(int irq, void *context);
int cnl_ipc_send_msg(struct snd_sof_dev *sdev, struct snd_sof_ipc_msg *msg);
void cnl_ipc_dump(struct snd_sof_dev *sdev);
void cnl_ipc4_dump(struct snd_sof_dev *sdev);

#endif
