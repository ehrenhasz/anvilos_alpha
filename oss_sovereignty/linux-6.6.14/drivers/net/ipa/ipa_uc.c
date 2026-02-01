

 

#include <linux/types.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>

#include "ipa.h"
#include "ipa_uc.h"
#include "ipa_power.h"

 
 

 
#define IPA_SEND_DELAY		100	 

 
struct ipa_uc_mem_area {
	u8 command;		 
	u8 reserved0[3];
	__le32 command_param;
	__le32 command_param_hi;
	u8 response;		 
	u8 reserved1[3];
	__le32 response_param;
	u8 event;		 
	u8 reserved2[3];

	__le32 event_param;
	__le32 first_error_address;
	u8 hw_state;
	u8 warning_counter;
	__le16 reserved3;
	__le16 interface_version;
	__le16 reserved4;
};

 
enum ipa_uc_command {
	IPA_UC_COMMAND_NO_OP		= 0x0,
	IPA_UC_COMMAND_UPDATE_FLAGS	= 0x1,
	IPA_UC_COMMAND_DEBUG_RUN_TEST	= 0x2,
	IPA_UC_COMMAND_DEBUG_GET_INFO	= 0x3,
	IPA_UC_COMMAND_ERR_FATAL	= 0x4,
	IPA_UC_COMMAND_CLK_GATE		= 0x5,
	IPA_UC_COMMAND_CLK_UNGATE	= 0x6,
	IPA_UC_COMMAND_MEMCPY		= 0x7,
	IPA_UC_COMMAND_RESET_PIPE	= 0x8,
	IPA_UC_COMMAND_REG_WRITE	= 0x9,
	IPA_UC_COMMAND_GSI_CH_EMPTY	= 0xa,
};

 
enum ipa_uc_response {
	IPA_UC_RESPONSE_NO_OP		= 0x0,
	IPA_UC_RESPONSE_INIT_COMPLETED	= 0x1,
	IPA_UC_RESPONSE_CMD_COMPLETED	= 0x2,
	IPA_UC_RESPONSE_DEBUG_GET_INFO	= 0x3,
};

 
enum ipa_uc_event {
	IPA_UC_EVENT_NO_OP		= 0x0,
	IPA_UC_EVENT_ERROR		= 0x1,
	IPA_UC_EVENT_LOG_INFO		= 0x2,
};

static struct ipa_uc_mem_area *ipa_uc_shared(struct ipa *ipa)
{
	const struct ipa_mem *mem = ipa_mem_find(ipa, IPA_MEM_UC_SHARED);
	u32 offset = ipa->mem_offset + mem->offset;

	return ipa->mem_virt + offset;
}

 
static void ipa_uc_event_handler(struct ipa *ipa)
{
	struct ipa_uc_mem_area *shared = ipa_uc_shared(ipa);
	struct device *dev = &ipa->pdev->dev;

	if (shared->event == IPA_UC_EVENT_ERROR)
		dev_err(dev, "microcontroller error event\n");
	else if (shared->event != IPA_UC_EVENT_LOG_INFO)
		dev_err(dev, "unsupported microcontroller event %u\n",
			shared->event);
	 
}

 
static void ipa_uc_response_hdlr(struct ipa *ipa)
{
	struct ipa_uc_mem_area *shared = ipa_uc_shared(ipa);
	struct device *dev = &ipa->pdev->dev;

	 
	switch (shared->response) {
	case IPA_UC_RESPONSE_INIT_COMPLETED:
		if (ipa->uc_powered) {
			ipa->uc_loaded = true;
			ipa_power_retention(ipa, true);
			pm_runtime_mark_last_busy(dev);
			(void)pm_runtime_put_autosuspend(dev);
			ipa->uc_powered = false;
		} else {
			dev_warn(dev, "unexpected init_completed response\n");
		}
		break;
	default:
		dev_warn(dev, "unsupported microcontroller response %u\n",
			 shared->response);
		break;
	}
}

void ipa_uc_interrupt_handler(struct ipa *ipa, enum ipa_irq_id irq_id)
{
	 
	if (irq_id == IPA_IRQ_UC_0)
		ipa_uc_event_handler(ipa);
	else if (irq_id == IPA_IRQ_UC_1)
		ipa_uc_response_hdlr(ipa);
}

 
void ipa_uc_config(struct ipa *ipa)
{
	ipa->uc_powered = false;
	ipa->uc_loaded = false;
	ipa_interrupt_enable(ipa, IPA_IRQ_UC_0);
	ipa_interrupt_enable(ipa, IPA_IRQ_UC_1);
}

 
void ipa_uc_deconfig(struct ipa *ipa)
{
	struct device *dev = &ipa->pdev->dev;

	ipa_interrupt_disable(ipa, IPA_IRQ_UC_1);
	ipa_interrupt_disable(ipa, IPA_IRQ_UC_0);
	if (ipa->uc_loaded)
		ipa_power_retention(ipa, false);

	if (!ipa->uc_powered)
		return;

	pm_runtime_mark_last_busy(dev);
	(void)pm_runtime_put_autosuspend(dev);
}

 
void ipa_uc_power(struct ipa *ipa)
{
	static bool already;
	struct device *dev;
	int ret;

	if (already)
		return;
	already = true;		 

	 
	dev = &ipa->pdev->dev;
	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		pm_runtime_put_noidle(dev);
		dev_err(dev, "error %d getting proxy power\n", ret);
	} else {
		ipa->uc_powered = true;
	}
}

 
static void send_uc_command(struct ipa *ipa, u32 command, u32 command_param)
{
	struct ipa_uc_mem_area *shared = ipa_uc_shared(ipa);
	const struct reg *reg;
	u32 val;

	 
	shared->command = command;
	shared->command_param = cpu_to_le32(command_param);
	shared->command_param_hi = 0;
	shared->response = 0;
	shared->response_param = 0;

	 
	reg = ipa_reg(ipa, IPA_IRQ_UC);
	val = reg_bit(reg, UC_INTR);

	iowrite32(val, ipa->reg_virt + reg_offset(reg));
}

 
void ipa_uc_panic_notifier(struct ipa *ipa)
{
	if (!ipa->uc_loaded)
		return;

	send_uc_command(ipa, IPA_UC_COMMAND_ERR_FATAL, 0);

	 
	udelay(IPA_SEND_DELAY);
}
