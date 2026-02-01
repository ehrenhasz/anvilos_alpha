 
 

#ifndef __SOF_INTEL_HDA_H
#define __SOF_INTEL_HDA_H

#include <linux/soundwire/sdw.h>
#include <linux/soundwire/sdw_intel.h>
#include <sound/compress_driver.h>
#include <sound/hda_codec.h>
#include <sound/hdaudio_ext.h>
#include "../sof-client-probes.h"
#include "../sof-audio.h"
#include "shim.h"

 
#define PCI_TCSEL			0x44
#define PCI_PGCTL			PCI_TCSEL
#define PCI_CGCTL			0x48

 
#define PCI_PGCTL_ADSPPGD               BIT(2)
#define PCI_PGCTL_LSRMD_MASK		BIT(4)

 
#define PCI_CGCTL_MISCBDCGE_MASK	BIT(6)
#define PCI_CGCTL_ADSPDCGE              BIT(1)

 
#define SOF_HDA_GCAP			0x0
#define SOF_HDA_GCTL			0x8
 
#define SOF_HDA_GCTL_UNSOL		BIT(8)
#define SOF_HDA_LLCH			0x14
#define SOF_HDA_INTCTL			0x20
#define SOF_HDA_INTSTS			0x24
#define SOF_HDA_WAKESTS			0x0E
#define SOF_HDA_WAKESTS_INT_MASK	((1 << 8) - 1)
#define SOF_HDA_RIRBSTS			0x5d

 
#define SOF_HDA_GCTL_RESET		BIT(0)

 
#define SOF_HDA_INT_GLOBAL_EN		BIT(31)
#define SOF_HDA_INT_CTRL_EN		BIT(30)
#define SOF_HDA_INT_ALL_STREAM		0xff

 
#define SOF_HDA_INTSTS_GIS		BIT(31)

#define SOF_HDA_MAX_CAPS		10
#define SOF_HDA_CAP_ID_OFF		16
#define SOF_HDA_CAP_ID_MASK		GENMASK(SOF_HDA_CAP_ID_OFF + 11,\
						SOF_HDA_CAP_ID_OFF)
#define SOF_HDA_CAP_NEXT_MASK		0xFFFF

#define SOF_HDA_GTS_CAP_ID			0x1
#define SOF_HDA_ML_CAP_ID			0x2

#define SOF_HDA_PP_CAP_ID		0x3
#define SOF_HDA_REG_PP_PPCH		0x10
#define SOF_HDA_REG_PP_PPCTL		0x04
#define SOF_HDA_REG_PP_PPSTS		0x08
#define SOF_HDA_PPCTL_PIE		BIT(31)
#define SOF_HDA_PPCTL_GPROCEN		BIT(30)

 
#define SOF_HDA_VS_D0I3C		0x104A

 
#define SOF_HDA_VS_D0I3C_CIP		BIT(0)  
#define SOF_HDA_VS_D0I3C_I3		BIT(2)  

 
#define SOF_HDA_DPIB_ENTRY_SIZE	0x8

#define SOF_HDA_SPIB_CAP_ID		0x4
#define SOF_HDA_DRSM_CAP_ID		0x5

#define SOF_HDA_SPIB_BASE		0x08
#define SOF_HDA_SPIB_INTERVAL		0x08
#define SOF_HDA_SPIB_SPIB		0x00
#define SOF_HDA_SPIB_MAXFIFO		0x04

#define SOF_HDA_PPHC_BASE		0x10
#define SOF_HDA_PPHC_INTERVAL		0x10

#define SOF_HDA_PPLC_BASE		0x10
#define SOF_HDA_PPLC_MULTI		0x10
#define SOF_HDA_PPLC_INTERVAL		0x10

#define SOF_HDA_DRSM_BASE		0x08
#define SOF_HDA_DRSM_INTERVAL		0x08

 
#define SOF_HDA_CL_DMA_SD_INT_DESC_ERR		0x10

 
#define SOF_HDA_CL_DMA_SD_INT_FIFO_ERR		0x08

 
#define SOF_HDA_CL_DMA_SD_INT_COMPLETE		0x04

#define SOF_HDA_CL_DMA_SD_INT_MASK \
	(SOF_HDA_CL_DMA_SD_INT_DESC_ERR | \
	SOF_HDA_CL_DMA_SD_INT_FIFO_ERR | \
	SOF_HDA_CL_DMA_SD_INT_COMPLETE)
#define SOF_HDA_SD_CTL_DMA_START		0x02  

 
#define SOF_HDA_ADSP_LOADER_BASE		0x80
#define SOF_HDA_ADSP_DPLBASE			0x70
#define SOF_HDA_ADSP_DPUBASE			0x74
#define SOF_HDA_ADSP_DPLBASE_ENABLE		0x01

 
#define SOF_HDA_ADSP_REG_SD_CTL			0x00
#define SOF_HDA_ADSP_REG_SD_STS			0x03
#define SOF_HDA_ADSP_REG_SD_LPIB		0x04
#define SOF_HDA_ADSP_REG_SD_CBL			0x08
#define SOF_HDA_ADSP_REG_SD_LVI			0x0C
#define SOF_HDA_ADSP_REG_SD_FIFOW		0x0E
#define SOF_HDA_ADSP_REG_SD_FIFOSIZE		0x10
#define SOF_HDA_ADSP_REG_SD_FORMAT		0x12
#define SOF_HDA_ADSP_REG_SD_FIFOL		0x14
#define SOF_HDA_ADSP_REG_SD_BDLPL		0x18
#define SOF_HDA_ADSP_REG_SD_BDLPU		0x1C
#define SOF_HDA_ADSP_SD_ENTRY_SIZE		0x20

 
#define SOF_DSP_REG_CL_SPBFIFO \
	(SOF_HDA_ADSP_LOADER_BASE + 0x20)
#define SOF_HDA_ADSP_REG_CL_SPBFIFO_SPBFCH	0x0
#define SOF_HDA_ADSP_REG_CL_SPBFIFO_SPBFCCTL	0x4
#define SOF_HDA_ADSP_REG_CL_SPBFIFO_SPIB	0x8
#define SOF_HDA_ADSP_REG_CL_SPBFIFO_MAXFIFOS	0xc

 
#define SOF_HDA_CL_SD_CTL_STREAM_TAG_SHIFT	20
#define SOF_HDA_CL_SD_CTL_STREAM_TAG_MASK \
	GENMASK(SOF_HDA_CL_SD_CTL_STREAM_TAG_SHIFT + 3,\
		SOF_HDA_CL_SD_CTL_STREAM_TAG_SHIFT)

#define HDA_DSP_HDA_BAR				0
#define HDA_DSP_PP_BAR				1
#define HDA_DSP_SPIB_BAR			2
#define HDA_DSP_DRSM_BAR			3
#define HDA_DSP_BAR				4

#define SRAM_WINDOW_OFFSET(x)			(0x80000 + (x) * 0x20000)

#define HDA_DSP_MBOX_OFFSET			SRAM_WINDOW_OFFSET(0)

#define HDA_DSP_PANIC_OFFSET(x) \
	(((x) & 0xFFFFFF) + HDA_DSP_MBOX_OFFSET)

 
#define HDA_DSP_SRAM_REG_ROM_STATUS		(HDA_DSP_MBOX_OFFSET + 0x0)
#define HDA_DSP_SRAM_REG_ROM_ERROR		(HDA_DSP_MBOX_OFFSET + 0x4)
 
#define HDA_DSP_SRAM_REG_FW_STATUS		(HDA_DSP_MBOX_OFFSET + 0x4)
#define HDA_DSP_SRAM_REG_FW_TRACEP		(HDA_DSP_MBOX_OFFSET + 0x8)
#define HDA_DSP_SRAM_REG_FW_END			(HDA_DSP_MBOX_OFFSET + 0xc)

#define HDA_DSP_MBOX_UPLINK_OFFSET		0x81000

#define HDA_DSP_STREAM_RESET_TIMEOUT		300
 
#define HDA_DSP_STREAM_RUN_TIMEOUT		300

#define HDA_DSP_SPIB_ENABLE			1
#define HDA_DSP_SPIB_DISABLE			0

#define SOF_HDA_MAX_BUFFER_SIZE			(32 * PAGE_SIZE)

#define HDA_DSP_STACK_DUMP_SIZE			32

 
#define FSR_STATE_MASK				GENMASK(23, 0)
#define FSR_WAIT_STATE_MASK			GENMASK(27, 24)
#define FSR_MODULE_MASK				GENMASK(30, 28)
#define FSR_HALTED				BIT(31)
#define FSR_TO_STATE_CODE(x)			((x) & FSR_STATE_MASK)
#define FSR_TO_WAIT_STATE_CODE(x)		(((x) & FSR_WAIT_STATE_MASK) >> 24)
#define FSR_TO_MODULE_CODE(x)			(((x) & FSR_MODULE_MASK) >> 28)

 
#define FSR_WAIT_FOR_IPC_BUSY			0x1
#define FSR_WAIT_FOR_IPC_DONE			0x2
#define FSR_WAIT_FOR_CACHE_INVALIDATION		0x3
#define FSR_WAIT_FOR_LP_SRAM_OFF		0x4
#define FSR_WAIT_FOR_DMA_BUFFER_FULL		0x5
#define FSR_WAIT_FOR_CSE_CSR			0x6

 
#define FSR_MOD_ROM				0x0
#define FSR_MOD_ROM_BYP				0x1
#define FSR_MOD_BASE_FW				0x2
#define FSR_MOD_LP_BOOT				0x3
#define FSR_MOD_BRNGUP				0x4
#define FSR_MOD_ROM_EXT				0x5

 
 
#define FSR_STATE_INIT				0x0
#define FSR_STATE_INIT_DONE			0x1
#define FSR_STATE_FW_ENTERED			0x5

 
#define FSR_STATE_ROM_INIT			FSR_STATE_INIT
#define FSR_STATE_ROM_INIT_DONE			FSR_STATE_INIT_DONE
#define FSR_STATE_ROM_CSE_MANIFEST_LOADED	0x2
#define FSR_STATE_ROM_FW_MANIFEST_LOADED	0x3
#define FSR_STATE_ROM_FW_FW_LOADED		0x4
#define FSR_STATE_ROM_FW_ENTERED		FSR_STATE_FW_ENTERED
#define FSR_STATE_ROM_VERIFY_FEATURE_MASK	0x6
#define FSR_STATE_ROM_GET_LOAD_OFFSET		0x7
#define FSR_STATE_ROM_FETCH_ROM_EXT		0x8
#define FSR_STATE_ROM_FETCH_ROM_EXT_DONE	0x9
#define FSR_STATE_ROM_BASEFW_ENTERED		0xf  

 
#define FSR_STATE_ROM_CSE_IMR_REQUEST			0x10
#define FSR_STATE_ROM_CSE_IMR_GRANTED			0x11
#define FSR_STATE_ROM_CSE_VALIDATE_IMAGE_REQUEST	0x12
#define FSR_STATE_ROM_CSE_IMAGE_VALIDATED		0x13

#define FSR_STATE_ROM_CSE_IPC_IFACE_INIT	0x20
#define FSR_STATE_ROM_CSE_IPC_RESET_PHASE_1	0x21
#define FSR_STATE_ROM_CSE_IPC_OPERATIONAL_ENTRY	0x22
#define FSR_STATE_ROM_CSE_IPC_OPERATIONAL	0x23
#define FSR_STATE_ROM_CSE_IPC_DOWN		0x24

 
#define FSR_STATE_BRINGUP_INIT			FSR_STATE_INIT
#define FSR_STATE_BRINGUP_INIT_DONE		FSR_STATE_INIT_DONE
#define FSR_STATE_BRINGUP_HPSRAM_LOAD		0x2
#define FSR_STATE_BRINGUP_UNPACK_START		0X3
#define FSR_STATE_BRINGUP_IMR_RESTORE		0x4
#define FSR_STATE_BRINGUP_FW_ENTERED		FSR_STATE_FW_ENTERED

 
#define HDA_DSP_ROM_CSE_ERROR			40
#define HDA_DSP_ROM_CSE_WRONG_RESPONSE		41
#define HDA_DSP_ROM_IMR_TO_SMALL		42
#define HDA_DSP_ROM_BASE_FW_NOT_FOUND		43
#define HDA_DSP_ROM_CSE_VALIDATION_FAILED	44
#define HDA_DSP_ROM_IPC_FATAL_ERROR		45
#define HDA_DSP_ROM_L2_CACHE_ERROR		46
#define HDA_DSP_ROM_LOAD_OFFSET_TO_SMALL	47
#define HDA_DSP_ROM_API_PTR_INVALID		50
#define HDA_DSP_ROM_BASEFW_INCOMPAT		51
#define HDA_DSP_ROM_UNHANDLED_INTERRUPT		0xBEE00000
#define HDA_DSP_ROM_MEMORY_HOLE_ECC		0xECC00000
#define HDA_DSP_ROM_KERNEL_EXCEPTION		0xCAFE0000
#define HDA_DSP_ROM_USER_EXCEPTION		0xBEEF0000
#define HDA_DSP_ROM_UNEXPECTED_RESET		0xDECAF000
#define HDA_DSP_ROM_NULL_FW_ENTRY		0x4c4c4e55

#define HDA_DSP_ROM_IPC_CONTROL			0x01000000
#define HDA_DSP_ROM_IPC_PURGE_FW		0x00004000

 
#define HDA_DSP_PU_TIMEOUT		50
#define HDA_DSP_PD_TIMEOUT		50
#define HDA_DSP_RESET_TIMEOUT_US	50000
#define HDA_DSP_BASEFW_TIMEOUT_US       3000000
#define HDA_DSP_INIT_TIMEOUT_US	500000
#define HDA_DSP_CTRL_RESET_TIMEOUT		100
#define HDA_DSP_WAIT_TIMEOUT		500	 
#define HDA_DSP_REG_POLL_INTERVAL_US		500	 
#define HDA_DSP_REG_POLL_RETRY_COUNT		50

#define HDA_DSP_ADSPIC_IPC			BIT(0)
#define HDA_DSP_ADSPIS_IPC			BIT(0)

 
#define HDA_DSP_GEN_BASE		0x0
#define HDA_DSP_REG_ADSPCS		(HDA_DSP_GEN_BASE + 0x04)
#define HDA_DSP_REG_ADSPIC		(HDA_DSP_GEN_BASE + 0x08)
#define HDA_DSP_REG_ADSPIS		(HDA_DSP_GEN_BASE + 0x0C)
#define HDA_DSP_REG_ADSPIC2		(HDA_DSP_GEN_BASE + 0x10)
#define HDA_DSP_REG_ADSPIS2		(HDA_DSP_GEN_BASE + 0x14)

#define HDA_DSP_REG_ADSPIC2_SNDW	BIT(5)
#define HDA_DSP_REG_ADSPIS2_SNDW	BIT(5)

 
#define HDA_DSP_IPC_BASE		0x40
#define HDA_DSP_REG_HIPCT		(HDA_DSP_IPC_BASE + 0x00)
#define HDA_DSP_REG_HIPCTE		(HDA_DSP_IPC_BASE + 0x04)
#define HDA_DSP_REG_HIPCI		(HDA_DSP_IPC_BASE + 0x08)
#define HDA_DSP_REG_HIPCIE		(HDA_DSP_IPC_BASE + 0x0C)
#define HDA_DSP_REG_HIPCCTL		(HDA_DSP_IPC_BASE + 0x10)

 
#define HDA_VS_INTEL_EM2		0x1030
#define HDA_VS_INTEL_EM2_L1SEN		BIT(13)
#define HDA_VS_INTEL_LTRP		0x1048
#define HDA_VS_INTEL_LTRP_GB_MASK	0x3F

 
#define HDA_DSP_REG_HIPCI_BUSY		BIT(31)
#define HDA_DSP_REG_HIPCI_MSG_MASK	0x7FFFFFFF

 
#define HDA_DSP_REG_HIPCIE_DONE	BIT(30)
#define HDA_DSP_REG_HIPCIE_MSG_MASK	0x3FFFFFFF

 
#define HDA_DSP_REG_HIPCCTL_DONE	BIT(1)
#define HDA_DSP_REG_HIPCCTL_BUSY	BIT(0)

 
#define HDA_DSP_REG_HIPCT_BUSY		BIT(31)
#define HDA_DSP_REG_HIPCT_MSG_MASK	0x7FFFFFFF

 
#define HDA_DSP_REG_HIPCTE_MSG_MASK	0x3FFFFFFF

#define HDA_DSP_ADSPIC_CL_DMA		BIT(1)
#define HDA_DSP_ADSPIS_CL_DMA		BIT(1)

 
#define BXT_D0I3_DELAY 5000

#define FW_CL_STREAM_NUMBER		0x1
#define HDA_FW_BOOT_ATTEMPTS		3

 

 
#define HDA_DSP_ADSPCS_CRST_SHIFT	0
#define HDA_DSP_ADSPCS_CRST_MASK(cm)	((cm) << HDA_DSP_ADSPCS_CRST_SHIFT)

 
#define HDA_DSP_ADSPCS_CSTALL_SHIFT	8
#define HDA_DSP_ADSPCS_CSTALL_MASK(cm)	((cm) << HDA_DSP_ADSPCS_CSTALL_SHIFT)

 
#define HDA_DSP_ADSPCS_SPA_SHIFT	16
#define HDA_DSP_ADSPCS_SPA_MASK(cm)	((cm) << HDA_DSP_ADSPCS_SPA_SHIFT)

 
#define HDA_DSP_ADSPCS_CPA_SHIFT	24
#define HDA_DSP_ADSPCS_CPA_MASK(cm)	((cm) << HDA_DSP_ADSPCS_CPA_SHIFT)

 
#define SOF_DSP_CORES_MASK(nc)	GENMASK(((nc) - 1), 0)

 
#define CNL_DSP_IPC_BASE		0xc0
#define CNL_DSP_REG_HIPCTDR		(CNL_DSP_IPC_BASE + 0x00)
#define CNL_DSP_REG_HIPCTDA		(CNL_DSP_IPC_BASE + 0x04)
#define CNL_DSP_REG_HIPCTDD		(CNL_DSP_IPC_BASE + 0x08)
#define CNL_DSP_REG_HIPCIDR		(CNL_DSP_IPC_BASE + 0x10)
#define CNL_DSP_REG_HIPCIDA		(CNL_DSP_IPC_BASE + 0x14)
#define CNL_DSP_REG_HIPCIDD		(CNL_DSP_IPC_BASE + 0x18)
#define CNL_DSP_REG_HIPCCTL		(CNL_DSP_IPC_BASE + 0x28)

 
#define CNL_DSP_REG_HIPCIDR_BUSY		BIT(31)
#define CNL_DSP_REG_HIPCIDR_MSG_MASK	0x7FFFFFFF

 
#define CNL_DSP_REG_HIPCIDA_DONE	BIT(31)
#define CNL_DSP_REG_HIPCIDA_MSG_MASK	0x7FFFFFFF

 
#define CNL_DSP_REG_HIPCCTL_DONE	BIT(1)
#define CNL_DSP_REG_HIPCCTL_BUSY	BIT(0)

 
#define CNL_DSP_REG_HIPCTDR_BUSY		BIT(31)
#define CNL_DSP_REG_HIPCTDR_MSG_MASK	0x7FFFFFFF

 
#define CNL_DSP_REG_HIPCTDA_DONE	BIT(31)
#define CNL_DSP_REG_HIPCTDA_MSG_MASK	0x7FFFFFFF

 
#define CNL_DSP_REG_HIPCTDD_MSG_MASK	0x7FFFFFFF

 
#define HDA_DSP_BDL_SIZE			4096
#define HDA_DSP_MAX_BDL_ENTRIES			\
	(HDA_DSP_BDL_SIZE / sizeof(struct sof_intel_dsp_bdl))

 
#define SOF_SKL_NUM_DAIS_NOCODEC	8

#if IS_ENABLED(CONFIG_SND_SOC_SOF_HDA_AUDIO_CODEC)
#define SOF_SKL_NUM_DAIS		15
#else
#define SOF_SKL_NUM_DAIS		SOF_SKL_NUM_DAIS_NOCODEC
#endif

 
#define HDA_DSP_SRAM_REG_ROM_STATUS_SKL	0x8000
#define HDA_ADSP_SRAM0_BASE_SKL		0x8000

 
#define HDA_ADSP_FW_STATUS_SKL		HDA_ADSP_SRAM0_BASE_SKL
#define HDA_ADSP_ERROR_CODE_SKL		(HDA_ADSP_FW_STATUS_SKL + 0x4)

 
#define APL_SSP_BASE_OFFSET	0x2000
#define CNL_SSP_BASE_OFFSET	0x10000

 
#define SSP_DEV_MEM_SIZE	0x1000

 
#define APL_SSP_COUNT		6
#define CNL_SSP_COUNT		3
#define ICL_SSP_COUNT		6
#define TGL_SSP_COUNT		3
#define MTL_SSP_COUNT		3

 
#define SSP_SSC1_OFFSET		0x4
#define SSP_SET_SCLK_CONSUMER	BIT(25)
#define SSP_SET_SFRM_CONSUMER	BIT(24)
#define SSP_SET_CBP_CFP		(SSP_SET_SCLK_CONSUMER | SSP_SET_SFRM_CONSUMER)

#define HDA_IDISP_ADDR		2
#define HDA_IDISP_CODEC(x) ((x) & BIT(HDA_IDISP_ADDR))

struct sof_intel_dsp_bdl {
	__le32 addr_l;
	__le32 addr_h;
	__le32 size;
	__le32 ioc;
} __attribute((packed));

#define SOF_HDA_PLAYBACK_STREAMS	16
#define SOF_HDA_CAPTURE_STREAMS		16
#define SOF_HDA_PLAYBACK		0
#define SOF_HDA_CAPTURE			1

 
#define SOF_HDA_STREAM_DMI_L1_COMPATIBLE	1

 
#define SOF_HDA_D0I3_WORK_DELAY_MS	5000

 
enum sof_hda_D0_substate {
	SOF_HDA_DSP_PM_D0I0,	 
	SOF_HDA_DSP_PM_D0I3,	 
};

 
struct sof_intel_hda_dev {
	bool imrboot_supported;
	bool skip_imr_boot;
	bool booted_from_imr;

	int boot_iteration;

	struct hda_bus hbus;

	 
	const struct sof_intel_dsp_desc *desc;

	 
	struct hdac_ext_stream *dtrace_stream;

	 
	u32 no_ipc_position;

	 
	u32 stream_max;

	 
	bool l1_disabled; 

	 
	struct platform_device *dmic_dev;

	 
	struct delayed_work d0i3_work;

	 
	struct sdw_intel_acpi_info info;

	 
	struct sdw_intel_ctx *sdw;

	 
	bool clk_config_lpro;

	wait_queue_head_t waitq;
	bool code_loading;

	 
	struct nhlt_acpi_table *nhlt;

	 
	struct snd_sof_ipc_msg *delayed_ipc_tx_msg;
};

static inline struct hdac_bus *sof_to_bus(struct snd_sof_dev *s)
{
	struct sof_intel_hda_dev *hda = s->pdata->hw_pdata;

	return &hda->hbus.core;
}

static inline struct hda_bus *sof_to_hbus(struct snd_sof_dev *s)
{
	struct sof_intel_hda_dev *hda = s->pdata->hw_pdata;

	return &hda->hbus;
}

struct sof_intel_hda_stream {
	struct snd_sof_dev *sdev;
	struct hdac_ext_stream hext_stream;
	struct sof_intel_stream sof_intel_stream;
	int host_reserved;  
	u32 flags;
};

#define hstream_to_sof_hda_stream(hstream) \
	container_of(hstream, struct sof_intel_hda_stream, hext_stream)

#define bus_to_sof_hda(bus) \
	container_of(bus, struct sof_intel_hda_dev, hbus.core)

#define SOF_STREAM_SD_OFFSET(s) \
	(SOF_HDA_ADSP_SD_ENTRY_SIZE * ((s)->index) \
	 + SOF_HDA_ADSP_LOADER_BASE)

#define SOF_STREAM_SD_OFFSET_CRST 0x1

 
int hda_dsp_probe(struct snd_sof_dev *sdev);
int hda_dsp_remove(struct snd_sof_dev *sdev);
int hda_dsp_core_power_up(struct snd_sof_dev *sdev, unsigned int core_mask);
int hda_dsp_core_run(struct snd_sof_dev *sdev, unsigned int core_mask);
int hda_dsp_enable_core(struct snd_sof_dev *sdev, unsigned int core_mask);
int hda_dsp_core_reset_power_down(struct snd_sof_dev *sdev,
				  unsigned int core_mask);
int hda_power_down_dsp(struct snd_sof_dev *sdev);
int hda_dsp_core_get(struct snd_sof_dev *sdev, int core);
void hda_dsp_ipc_int_enable(struct snd_sof_dev *sdev);
void hda_dsp_ipc_int_disable(struct snd_sof_dev *sdev);
bool hda_dsp_core_is_enabled(struct snd_sof_dev *sdev, unsigned int core_mask);

int hda_dsp_set_power_state_ipc3(struct snd_sof_dev *sdev,
				 const struct sof_dsp_power_state *target_state);
int hda_dsp_set_power_state_ipc4(struct snd_sof_dev *sdev,
				 const struct sof_dsp_power_state *target_state);

int hda_dsp_suspend(struct snd_sof_dev *sdev, u32 target_state);
int hda_dsp_resume(struct snd_sof_dev *sdev);
int hda_dsp_runtime_suspend(struct snd_sof_dev *sdev);
int hda_dsp_runtime_resume(struct snd_sof_dev *sdev);
int hda_dsp_runtime_idle(struct snd_sof_dev *sdev);
int hda_dsp_shutdown_dma_flush(struct snd_sof_dev *sdev);
int hda_dsp_shutdown(struct snd_sof_dev *sdev);
int hda_dsp_set_hw_params_upon_resume(struct snd_sof_dev *sdev);
void hda_dsp_dump(struct snd_sof_dev *sdev, u32 flags);
void hda_ipc_dump(struct snd_sof_dev *sdev);
void hda_ipc_irq_dump(struct snd_sof_dev *sdev);
void hda_dsp_d0i3_work(struct work_struct *work);
int hda_dsp_disable_interrupts(struct snd_sof_dev *sdev);

 
u32 hda_dsp_get_mult_div(struct snd_sof_dev *sdev, int rate);
u32 hda_dsp_get_bits(struct snd_sof_dev *sdev, int sample_bits);
int hda_dsp_pcm_open(struct snd_sof_dev *sdev,
		     struct snd_pcm_substream *substream);
int hda_dsp_pcm_close(struct snd_sof_dev *sdev,
		      struct snd_pcm_substream *substream);
int hda_dsp_pcm_hw_params(struct snd_sof_dev *sdev,
			  struct snd_pcm_substream *substream,
			  struct snd_pcm_hw_params *params,
			  struct snd_sof_platform_stream_params *platform_params);
int hda_dsp_stream_hw_free(struct snd_sof_dev *sdev,
			   struct snd_pcm_substream *substream);
int hda_dsp_pcm_trigger(struct snd_sof_dev *sdev,
			struct snd_pcm_substream *substream, int cmd);
snd_pcm_uframes_t hda_dsp_pcm_pointer(struct snd_sof_dev *sdev,
				      struct snd_pcm_substream *substream);
int hda_dsp_pcm_ack(struct snd_sof_dev *sdev, struct snd_pcm_substream *substream);

 

int hda_dsp_stream_init(struct snd_sof_dev *sdev);
void hda_dsp_stream_free(struct snd_sof_dev *sdev);
int hda_dsp_stream_hw_params(struct snd_sof_dev *sdev,
			     struct hdac_ext_stream *hext_stream,
			     struct snd_dma_buffer *dmab,
			     struct snd_pcm_hw_params *params);
int hda_dsp_iccmax_stream_hw_params(struct snd_sof_dev *sdev,
				    struct hdac_ext_stream *hext_stream,
				    struct snd_dma_buffer *dmab,
				    struct snd_pcm_hw_params *params);
int hda_dsp_stream_trigger(struct snd_sof_dev *sdev,
			   struct hdac_ext_stream *hext_stream, int cmd);
irqreturn_t hda_dsp_stream_threaded_handler(int irq, void *context);
int hda_dsp_stream_setup_bdl(struct snd_sof_dev *sdev,
			     struct snd_dma_buffer *dmab,
			     struct hdac_stream *hstream);
bool hda_dsp_check_ipc_irq(struct snd_sof_dev *sdev);
bool hda_dsp_check_stream_irq(struct snd_sof_dev *sdev);

snd_pcm_uframes_t hda_dsp_stream_get_position(struct hdac_stream *hstream,
					      int direction, bool can_sleep);

struct hdac_ext_stream *
	hda_dsp_stream_get(struct snd_sof_dev *sdev, int direction, u32 flags);
int hda_dsp_stream_put(struct snd_sof_dev *sdev, int direction, int stream_tag);
int hda_dsp_stream_spib_config(struct snd_sof_dev *sdev,
			       struct hdac_ext_stream *hext_stream,
			       int enable, u32 size);

int hda_ipc_msg_data(struct snd_sof_dev *sdev,
		     struct snd_sof_pcm_stream *sps,
		     void *p, size_t sz);
int hda_set_stream_data_offset(struct snd_sof_dev *sdev,
			       struct snd_sof_pcm_stream *sps,
			       size_t posn_offset);

 
int hda_dsp_ipc_send_msg(struct snd_sof_dev *sdev,
			 struct snd_sof_ipc_msg *msg);
void hda_dsp_ipc_get_reply(struct snd_sof_dev *sdev);
int hda_dsp_ipc_get_mailbox_offset(struct snd_sof_dev *sdev);
int hda_dsp_ipc_get_window_offset(struct snd_sof_dev *sdev, u32 id);

irqreturn_t hda_dsp_ipc_irq_thread(int irq, void *context);
int hda_dsp_ipc_cmd_done(struct snd_sof_dev *sdev, int dir);

 
int hda_dsp_cl_boot_firmware(struct snd_sof_dev *sdev);
int hda_dsp_cl_boot_firmware_iccmax(struct snd_sof_dev *sdev);
int hda_cl_copy_fw(struct snd_sof_dev *sdev, struct hdac_ext_stream *hext_stream);
struct hdac_ext_stream *hda_cl_stream_prepare(struct snd_sof_dev *sdev, unsigned int format,
					      unsigned int size, struct snd_dma_buffer *dmab,
					      int direction);
int hda_cl_cleanup(struct snd_sof_dev *sdev, struct snd_dma_buffer *dmab,
		   struct hdac_ext_stream *hext_stream);
int cl_dsp_init(struct snd_sof_dev *sdev, int stream_tag, bool imr_boot);
#define HDA_CL_STREAM_FORMAT 0x40

 
int hda_dsp_pre_fw_run(struct snd_sof_dev *sdev);
int hda_dsp_post_fw_run(struct snd_sof_dev *sdev);

 
int hda_dsp_ext_man_get_cavs_config_data(struct snd_sof_dev *sdev,
					 const struct sof_ext_man_elem_header *hdr);

 
int hda_dsp_ctrl_get_caps(struct snd_sof_dev *sdev);
void hda_dsp_ctrl_ppcap_enable(struct snd_sof_dev *sdev, bool enable);
void hda_dsp_ctrl_ppcap_int_enable(struct snd_sof_dev *sdev, bool enable);
int hda_dsp_ctrl_link_reset(struct snd_sof_dev *sdev, bool reset);
void hda_dsp_ctrl_misc_clock_gating(struct snd_sof_dev *sdev, bool enable);
int hda_dsp_ctrl_clock_power_gating(struct snd_sof_dev *sdev, bool enable);
int hda_dsp_ctrl_init_chip(struct snd_sof_dev *sdev);
void hda_dsp_ctrl_stop_chip(struct snd_sof_dev *sdev);
 
void sof_hda_bus_init(struct snd_sof_dev *sdev, struct device *dev);
void sof_hda_bus_exit(struct snd_sof_dev *sdev);

#if IS_ENABLED(CONFIG_SND_SOC_SOF_HDA_AUDIO_CODEC)
 
void hda_codec_probe_bus(struct snd_sof_dev *sdev);
void hda_codec_jack_wake_enable(struct snd_sof_dev *sdev, bool enable);
void hda_codec_jack_check(struct snd_sof_dev *sdev);
void hda_codec_check_for_state_change(struct snd_sof_dev *sdev);
void hda_codec_init_cmd_io(struct snd_sof_dev *sdev);
void hda_codec_resume_cmd_io(struct snd_sof_dev *sdev);
void hda_codec_stop_cmd_io(struct snd_sof_dev *sdev);
void hda_codec_suspend_cmd_io(struct snd_sof_dev *sdev);
void hda_codec_detect_mask(struct snd_sof_dev *sdev);
void hda_codec_rirb_status_clear(struct snd_sof_dev *sdev);
bool hda_codec_check_rirb_status(struct snd_sof_dev *sdev);
void hda_codec_set_codec_wakeup(struct snd_sof_dev *sdev, bool status);
void hda_codec_device_remove(struct snd_sof_dev *sdev);

#else

static inline void hda_codec_probe_bus(struct snd_sof_dev *sdev) { }
static inline void hda_codec_jack_wake_enable(struct snd_sof_dev *sdev, bool enable) { }
static inline void hda_codec_jack_check(struct snd_sof_dev *sdev) { }
static inline void hda_codec_check_for_state_change(struct snd_sof_dev *sdev) { }
static inline void hda_codec_init_cmd_io(struct snd_sof_dev *sdev) { }
static inline void hda_codec_resume_cmd_io(struct snd_sof_dev *sdev) { }
static inline void hda_codec_stop_cmd_io(struct snd_sof_dev *sdev) { }
static inline void hda_codec_suspend_cmd_io(struct snd_sof_dev *sdev) { }
static inline void hda_codec_detect_mask(struct snd_sof_dev *sdev) { }
static inline void hda_codec_rirb_status_clear(struct snd_sof_dev *sdev) { }
static inline bool hda_codec_check_rirb_status(struct snd_sof_dev *sdev) { return false; }
static inline void hda_codec_set_codec_wakeup(struct snd_sof_dev *sdev, bool status) { }
static inline void hda_codec_device_remove(struct snd_sof_dev *sdev) { }

#endif  

#if IS_ENABLED(CONFIG_SND_SOC_SOF_HDA_AUDIO_CODEC) && IS_ENABLED(CONFIG_SND_HDA_CODEC_HDMI)

void hda_codec_i915_display_power(struct snd_sof_dev *sdev, bool enable);
int hda_codec_i915_init(struct snd_sof_dev *sdev);
int hda_codec_i915_exit(struct snd_sof_dev *sdev);

#else

static inline void hda_codec_i915_display_power(struct snd_sof_dev *sdev, bool enable) { }
static inline int hda_codec_i915_init(struct snd_sof_dev *sdev) { return 0; }
static inline int hda_codec_i915_exit(struct snd_sof_dev *sdev) { return 0; }

#endif

 
int hda_dsp_trace_init(struct snd_sof_dev *sdev, struct snd_dma_buffer *dmab,
		       struct sof_ipc_dma_trace_params_ext *dtrace_params);
int hda_dsp_trace_release(struct snd_sof_dev *sdev);
int hda_dsp_trace_trigger(struct snd_sof_dev *sdev, int cmd);

 
#if IS_ENABLED(CONFIG_SND_SOC_SOF_INTEL_SOUNDWIRE)

int hda_sdw_check_lcount_common(struct snd_sof_dev *sdev);
int hda_sdw_check_lcount_ext(struct snd_sof_dev *sdev);
int hda_sdw_startup(struct snd_sof_dev *sdev);
void hda_common_enable_sdw_irq(struct snd_sof_dev *sdev, bool enable);
void hda_sdw_int_enable(struct snd_sof_dev *sdev, bool enable);
bool hda_sdw_check_wakeen_irq_common(struct snd_sof_dev *sdev);
void hda_sdw_process_wakeen(struct snd_sof_dev *sdev);
bool hda_common_check_sdw_irq(struct snd_sof_dev *sdev);

#else

static inline int hda_sdw_check_lcount_common(struct snd_sof_dev *sdev)
{
	return 0;
}

static inline int hda_sdw_check_lcount_ext(struct snd_sof_dev *sdev)
{
	return 0;
}

static inline int hda_sdw_startup(struct snd_sof_dev *sdev)
{
	return 0;
}

static inline void hda_common_enable_sdw_irq(struct snd_sof_dev *sdev, bool enable)
{
}

static inline void hda_sdw_int_enable(struct snd_sof_dev *sdev, bool enable)
{
}

static inline bool hda_sdw_check_wakeen_irq_common(struct snd_sof_dev *sdev)
{
	return false;
}

static inline void hda_sdw_process_wakeen(struct snd_sof_dev *sdev)
{
}

static inline bool hda_common_check_sdw_irq(struct snd_sof_dev *sdev)
{
	return false;
}

#endif

int sdw_hda_dai_hw_params(struct snd_pcm_substream *substream,
			  struct snd_pcm_hw_params *params,
			  struct snd_soc_dai *cpu_dai,
			  int link_id);

int sdw_hda_dai_hw_free(struct snd_pcm_substream *substream,
			struct snd_soc_dai *cpu_dai,
			int link_id);

int sdw_hda_dai_trigger(struct snd_pcm_substream *substream, int cmd,
			struct snd_soc_dai *cpu_dai);

 
extern struct snd_soc_dai_driver skl_dai[];
int hda_dsp_dais_suspend(struct snd_sof_dev *sdev);

 
extern struct snd_sof_dsp_ops sof_hda_common_ops;

extern struct snd_sof_dsp_ops sof_skl_ops;
int sof_skl_ops_init(struct snd_sof_dev *sdev);
extern struct snd_sof_dsp_ops sof_apl_ops;
int sof_apl_ops_init(struct snd_sof_dev *sdev);
extern struct snd_sof_dsp_ops sof_cnl_ops;
int sof_cnl_ops_init(struct snd_sof_dev *sdev);
extern struct snd_sof_dsp_ops sof_tgl_ops;
int sof_tgl_ops_init(struct snd_sof_dev *sdev);
extern struct snd_sof_dsp_ops sof_icl_ops;
int sof_icl_ops_init(struct snd_sof_dev *sdev);
extern struct snd_sof_dsp_ops sof_mtl_ops;
int sof_mtl_ops_init(struct snd_sof_dev *sdev);
extern struct snd_sof_dsp_ops sof_lnl_ops;
int sof_lnl_ops_init(struct snd_sof_dev *sdev);

extern const struct sof_intel_dsp_desc skl_chip_info;
extern const struct sof_intel_dsp_desc apl_chip_info;
extern const struct sof_intel_dsp_desc cnl_chip_info;
extern const struct sof_intel_dsp_desc icl_chip_info;
extern const struct sof_intel_dsp_desc tgl_chip_info;
extern const struct sof_intel_dsp_desc tglh_chip_info;
extern const struct sof_intel_dsp_desc ehl_chip_info;
extern const struct sof_intel_dsp_desc jsl_chip_info;
extern const struct sof_intel_dsp_desc adls_chip_info;
extern const struct sof_intel_dsp_desc mtl_chip_info;
extern const struct sof_intel_dsp_desc lnl_chip_info;

 
#if IS_ENABLED(CONFIG_SND_SOC_SOF_HDA_PROBES)
int hda_probes_register(struct snd_sof_dev *sdev);
void hda_probes_unregister(struct snd_sof_dev *sdev);
#else
static inline int hda_probes_register(struct snd_sof_dev *sdev)
{
	return 0;
}

static inline void hda_probes_unregister(struct snd_sof_dev *sdev)
{
}
#endif  

 
int hda_register_clients(struct snd_sof_dev *sdev);
void hda_unregister_clients(struct snd_sof_dev *sdev);

 
struct snd_soc_acpi_mach *hda_machine_select(struct snd_sof_dev *sdev);
void hda_set_mach_params(struct snd_soc_acpi_mach *mach,
			 struct snd_sof_dev *sdev);

 
int hda_pci_intel_probe(struct pci_dev *pci, const struct pci_device_id *pci_id);

struct snd_sof_dai;
struct sof_ipc_dai_config;

#define SOF_HDA_POSITION_QUIRK_USE_SKYLAKE_LEGACY	(0)  
#define SOF_HDA_POSITION_QUIRK_USE_DPIB_REGISTERS	(1)  
#define SOF_HDA_POSITION_QUIRK_USE_DPIB_DDR_UPDATE	(2)  

extern int sof_hda_position_quirk;

void hda_set_dai_drv_ops(struct snd_sof_dev *sdev, struct snd_sof_dsp_ops *ops);
void hda_ops_free(struct snd_sof_dev *sdev);

 
int hda_dsp_cl_boot_firmware_skl(struct snd_sof_dev *sdev);
int hda_dsp_core_stall_reset(struct snd_sof_dev *sdev, unsigned int core_mask);

 
irqreturn_t cnl_ipc4_irq_thread(int irq, void *context);
int cnl_ipc4_send_msg(struct snd_sof_dev *sdev, struct snd_sof_ipc_msg *msg);
irqreturn_t hda_dsp_ipc4_irq_thread(int irq, void *context);
bool hda_ipc4_tx_is_busy(struct snd_sof_dev *sdev);
void hda_dsp_ipc4_schedule_d0i3_work(struct sof_intel_hda_dev *hdev,
				     struct snd_sof_ipc_msg *msg);
int hda_dsp_ipc4_send_msg(struct snd_sof_dev *sdev, struct snd_sof_ipc_msg *msg);
void hda_ipc4_dump(struct snd_sof_dev *sdev);
extern struct sdw_intel_ops sdw_callback;

struct sof_ipc4_fw_library;
int hda_dsp_ipc4_load_library(struct snd_sof_dev *sdev,
			      struct sof_ipc4_fw_library *fw_lib, bool reload);

 
struct hda_dai_widget_dma_ops {
	struct hdac_ext_stream *(*get_hext_stream)(struct snd_sof_dev *sdev,
						   struct snd_soc_dai *cpu_dai,
						   struct snd_pcm_substream *substream);
	struct hdac_ext_stream *(*assign_hext_stream)(struct snd_sof_dev *sdev,
						      struct snd_soc_dai *cpu_dai,
						      struct snd_pcm_substream *substream);
	void (*release_hext_stream)(struct snd_sof_dev *sdev, struct snd_soc_dai *cpu_dai,
				    struct snd_pcm_substream *substream);
	void (*setup_hext_stream)(struct snd_sof_dev *sdev, struct hdac_ext_stream *hext_stream,
				  unsigned int format_val);
	void (*reset_hext_stream)(struct snd_sof_dev *sdev, struct hdac_ext_stream *hext_sream);
	int (*pre_trigger)(struct snd_sof_dev *sdev, struct snd_soc_dai *cpu_dai,
			   struct snd_pcm_substream *substream, int cmd);
	int (*trigger)(struct snd_sof_dev *sdev, struct snd_soc_dai *cpu_dai,
		       struct snd_pcm_substream *substream, int cmd);
	int (*post_trigger)(struct snd_sof_dev *sdev, struct snd_soc_dai *cpu_dai,
			    struct snd_pcm_substream *substream, int cmd);
	void (*codec_dai_set_stream)(struct snd_sof_dev *sdev,
				     struct snd_pcm_substream *substream,
				     struct hdac_stream *hstream);
	unsigned int (*calc_stream_format)(struct snd_sof_dev *sdev,
					   struct snd_pcm_substream *substream,
					   struct snd_pcm_hw_params *params);
	struct hdac_ext_link * (*get_hlink)(struct snd_sof_dev *sdev,
					    struct snd_pcm_substream *substream);
};

const struct hda_dai_widget_dma_ops *
hda_select_dai_widget_ops(struct snd_sof_dev *sdev, struct snd_sof_widget *swidget);
int hda_dai_config(struct snd_soc_dapm_widget *w, unsigned int flags,
		   struct snd_sof_dai_config_data *data);
int hda_link_dma_cleanup(struct snd_pcm_substream *substream, struct hdac_ext_stream *hext_stream,
			 struct snd_soc_dai *cpu_dai);

#endif
