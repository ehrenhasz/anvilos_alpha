
 

#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/iommu.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>


#include <sound/core.h>
#include <sound/emu10k1.h>
#include <linux/firmware.h>
#include "p16v.h"
#include "tina2.h"
#include "p17v.h"


#define HANA_FILENAME "emu/hana.fw"
#define DOCK_FILENAME "emu/audio_dock.fw"
#define EMU1010B_FILENAME "emu/emu1010b.fw"
#define MICRO_DOCK_FILENAME "emu/micro_dock.fw"
#define EMU0404_FILENAME "emu/emu0404.fw"
#define EMU1010_NOTEBOOK_FILENAME "emu/emu1010_notebook.fw"

MODULE_FIRMWARE(HANA_FILENAME);
MODULE_FIRMWARE(DOCK_FILENAME);
MODULE_FIRMWARE(EMU1010B_FILENAME);
MODULE_FIRMWARE(MICRO_DOCK_FILENAME);
MODULE_FIRMWARE(EMU0404_FILENAME);
MODULE_FIRMWARE(EMU1010_NOTEBOOK_FILENAME);


 

void snd_emu10k1_voice_init(struct snd_emu10k1 *emu, int ch)
{
	snd_emu10k1_ptr_write_multiple(emu, ch,
		DCYSUSV, 0,
		VTFT, VTFT_FILTERTARGET_MASK,
		CVCF, CVCF_CURRENTFILTER_MASK,
		PTRX, 0,
		CPF, 0,
		CCR, 0,

		PSST, 0,
		DSL, 0x10,
		CCCA, 0,
		Z1, 0,
		Z2, 0,
		FXRT, 0x32100000,

		
		DCYSUSM, 0,
		ATKHLDV, 0,
		ATKHLDM, 0,
		IP, 0,
		IFATN, IFATN_FILTERCUTOFF_MASK | IFATN_ATTENUATION_MASK,
		PEFE, 0,
		FMMOD, 0,
		TREMFRQ, 24,	 
		FM2FRQ2, 24,	 
		LFOVAL2, 0,
		LFOVAL1, 0,
		ENVVOL, 0,
		ENVVAL, 0,

		REGLIST_END);

	 
	if (emu->audigy) {
		snd_emu10k1_ptr_write_multiple(emu, ch,
			A_CSBA, 0,
			A_CSDC, 0,
			A_CSFE, 0,
			A_CSHG, 0,
			A_FXRT1, 0x03020100,
			A_FXRT2, 0x07060504,
			A_SENDAMOUNTS, 0,
			REGLIST_END);
	}
}

static const unsigned int spi_dac_init[] = {
		0x00ff,
		0x02ff,
		0x0400,
		0x0520,
		0x0600,
		0x08ff,
		0x0aff,
		0x0cff,
		0x0eff,
		0x10ff,
		0x1200,
		0x1400,
		0x1480,
		0x1800,
		0x1aff,
		0x1cff,
		0x1e00,
		0x0530,
		0x0602,
		0x0622,
		0x1400,
};

static const unsigned int i2c_adc_init[][2] = {
	{ 0x17, 0x00 },  
	{ 0x07, 0x00 },  
	{ 0x0b, 0x22 },   
	{ 0x0c, 0x22 },   
	{ 0x0d, 0x08 },   
	{ 0x0e, 0xcf },   
	{ 0x0f, 0xcf },   
	{ 0x10, 0x7b },   
	{ 0x11, 0x00 },   
	{ 0x12, 0x32 },   
	{ 0x13, 0x00 },   
	{ 0x14, 0xa6 },   
	{ 0x15, ADC_MUX_2 },   
};

static int snd_emu10k1_init(struct snd_emu10k1 *emu, int enable_ir)
{
	unsigned int silent_page;
	int ch;
	u32 tmp;

	 
	outl(HCFG_LOCKSOUNDCACHE | HCFG_LOCKTANKCACHE_MASK |
		HCFG_MUTEBUTTONENABLE, emu->port + HCFG);

	outl(0, emu->port + INTE);

	snd_emu10k1_ptr_write_multiple(emu, 0,
		 
		MICBS, ADCBS_BUFSIZE_NONE,
		MICBA, 0,
		FXBS, ADCBS_BUFSIZE_NONE,
		FXBA, 0,
		ADCBS, ADCBS_BUFSIZE_NONE,
		ADCBA, 0,

		 
		CLIEL, 0,
		CLIEH, 0,

		 
		SOLEL, 0,
		SOLEH, 0,

		REGLIST_END);

	if (emu->audigy) {
		 
		snd_emu10k1_ptr_write(emu, SPBYPASS, 0, SPBYPASS_FORMAT);
		 
		snd_emu10k1_ptr_write(emu, AC97SLOT, 0, AC97SLOT_REAR_RIGHT |
				      AC97SLOT_REAR_LEFT);
	}

	 
	for (ch = 0; ch < NUM_G; ch++)
		snd_emu10k1_voice_init(emu, ch);

	snd_emu10k1_ptr_write_multiple(emu, 0,
		SPCS0, emu->spdif_bits[0],
		SPCS1, emu->spdif_bits[1],
		SPCS2, emu->spdif_bits[2],
		REGLIST_END);

	if (emu->card_capabilities->emu_model) {
	} else if (emu->card_capabilities->ca0151_chip) {  
		 
		 
		snd_emu10k1_ptr_write(emu, A_I2S_CAPTURE_RATE, 0, A_I2S_CAPTURE_96000);

		 
		snd_emu10k1_ptr20_write(emu, SRCSel, 0, 0x14);
		 
		 
		snd_emu10k1_ptr20_write(emu, SRCMULTI_ENABLE, 0, 0xFFFFFFFF);

		 
		outl(0x0201, emu->port + HCFG2);
		 
		snd_emu10k1_ptr20_write(emu, CAPTURE_P16V_SOURCE, 0, 0x78e4);
	} else if (emu->card_capabilities->ca0108_chip) {  
		 
		dev_info(emu->card->dev, "Audigy2 value: Special config.\n");
		 
		snd_emu10k1_ptr_write(emu, A_I2S_CAPTURE_RATE, 0, A_I2S_CAPTURE_96000);

		 
		snd_emu10k1_ptr20_write(emu, P17V_SRCSel, 0, 0x14);

		 
		snd_emu10k1_ptr20_write(emu, P17V_MIXER_I2S_ENABLE, 0, 0xFF000000);

		 
		 
		snd_emu10k1_ptr20_write(emu, P17V_MIXER_SPDIF_ENABLE, 0, 0xFF000000);

		tmp = inw(emu->port + A_IOCFG) & ~0x8;  
		outw(tmp, emu->port + A_IOCFG);
	}
	if (emu->card_capabilities->spi_dac) {  
		int size, n;

		size = ARRAY_SIZE(spi_dac_init);
		for (n = 0; n < size; n++)
			snd_emu10k1_spi_write(emu, spi_dac_init[n]);

		snd_emu10k1_ptr20_write(emu, 0x60, 0, 0x10);
		 
		outw(0x76, emu->port + A_IOCFG);  
	}
	if (emu->card_capabilities->i2c_adc) {  
		int size, n;

		snd_emu10k1_ptr20_write(emu, P17V_I2S_SRC_SEL, 0, 0x2020205f);
		tmp = inw(emu->port + A_IOCFG);
		outw(tmp | 0x4, emu->port + A_IOCFG);   
		tmp = inw(emu->port + A_IOCFG);
		size = ARRAY_SIZE(i2c_adc_init);
		for (n = 0; n < size; n++)
			snd_emu10k1_i2c_write(emu, i2c_adc_init[n][0], i2c_adc_init[n][1]);
		for (n = 0; n < 4; n++) {
			emu->i2c_capture_volume[n][0] = 0xcf;
			emu->i2c_capture_volume[n][1] = 0xcf;
		}
	}


	snd_emu10k1_ptr_write(emu, PTB, 0, emu->ptb_pages.addr);
	snd_emu10k1_ptr_write(emu, TCB, 0, 0);	 
	snd_emu10k1_ptr_write(emu, TCBS, 0, TCBS_BUFFSIZE_256K);	 

	silent_page = (emu->silent_page.addr << emu->address_mode) | (emu->address_mode ? MAP_PTI_MASK1 : MAP_PTI_MASK0);
	for (ch = 0; ch < NUM_G; ch++) {
		snd_emu10k1_ptr_write(emu, MAPA, ch, silent_page);
		snd_emu10k1_ptr_write(emu, MAPB, ch, silent_page);
	}

	if (emu->card_capabilities->emu_model) {
		outl(HCFG_AUTOMUTE_ASYNC |
			HCFG_EMU32_SLAVE |
			HCFG_AUDIOENABLE, emu->port + HCFG);
	 
	} else if (emu->audigy) {
		if (emu->revision == 4)  
			outl(HCFG_AUDIOENABLE |
			     HCFG_AC3ENABLE_CDSPDIF |
			     HCFG_AC3ENABLE_GPSPDIF |
			     HCFG_AUTOMUTE | HCFG_JOYENABLE, emu->port + HCFG);
		else
			outl(HCFG_AUTOMUTE | HCFG_JOYENABLE, emu->port + HCFG);
	 
	} else if (emu->model == 0x20 ||
	    emu->model == 0xc400 ||
	    (emu->model == 0x21 && emu->revision < 6))
		outl(HCFG_LOCKTANKCACHE_MASK | HCFG_AUTOMUTE, emu->port + HCFG);
	else
		 
		outl(HCFG_LOCKTANKCACHE_MASK | HCFG_AUTOMUTE | HCFG_JOYENABLE, emu->port + HCFG);

	if (enable_ir) {	 
		if (emu->card_capabilities->emu_model) {
			;   
		} else if (emu->card_capabilities->i2c_adc) {
			;   
		} else if (emu->audigy) {
			u16 reg = inw(emu->port + A_IOCFG);
			outw(reg | A_IOCFG_GPOUT2, emu->port + A_IOCFG);
			udelay(500);
			outw(reg | A_IOCFG_GPOUT1 | A_IOCFG_GPOUT2, emu->port + A_IOCFG);
			udelay(100);
			outw(reg, emu->port + A_IOCFG);
		} else {
			unsigned int reg = inl(emu->port + HCFG);
			outl(reg | HCFG_GPOUT2, emu->port + HCFG);
			udelay(500);
			outl(reg | HCFG_GPOUT1 | HCFG_GPOUT2, emu->port + HCFG);
			udelay(100);
			outl(reg, emu->port + HCFG);
		}
	}

	if (emu->card_capabilities->emu_model) {
		;   
	} else if (emu->card_capabilities->i2c_adc) {
		;   
	} else if (emu->audigy) {	 
		u16 reg = inw(emu->port + A_IOCFG);
		outw(reg | A_IOCFG_GPOUT0, emu->port + A_IOCFG);
	}

	if (emu->address_mode == 0) {
		 
		outl(inl(emu->port + HCFG) | HCFG_EXPANDED_MEM, emu->port + HCFG);
	}

	return 0;
}

static void snd_emu10k1_audio_enable(struct snd_emu10k1 *emu)
{
	 
	outl(inl(emu->port + HCFG) | HCFG_AUDIOENABLE, emu->port + HCFG);

	 
	if (emu->card_capabilities->emu_model) {
		;   
	} else if (emu->card_capabilities->i2c_adc) {
		;   
	} else if (emu->audigy) {
		outw(inw(emu->port + A_IOCFG) & ~0x44, emu->port + A_IOCFG);

		if (emu->card_capabilities->ca0151_chip) {  
			 
			outw(inw(emu->port + A_IOCFG) | 0x0040, emu->port + A_IOCFG);
		} else if (emu->card_capabilities->ca0108_chip) {  
			 
			outw(inw(emu->port + A_IOCFG) | 0x0060, emu->port + A_IOCFG);
		} else {
			 
			outw(inw(emu->port + A_IOCFG) | 0x0080, emu->port + A_IOCFG);
		}
	}

#if 0
	{
	unsigned int tmp;
	 
	 
	emu->tos_link = 0;
	tmp = inl(emu->port + HCFG);
	if (tmp & (HCFG_GPINPUT0 | HCFG_GPINPUT1)) {
		outl(tmp|0x800, emu->port + HCFG);
		udelay(50);
		if (tmp != (inl(emu->port + HCFG) & ~0x800)) {
			emu->tos_link = 1;
			outl(tmp, emu->port + HCFG);
		}
	}
	}
#endif

	if (emu->card_capabilities->emu_model)
		snd_emu10k1_intr_enable(emu, INTE_PCIERRORENABLE | INTE_A_GPIOENABLE);
	else
		snd_emu10k1_intr_enable(emu, INTE_PCIERRORENABLE);
}

int snd_emu10k1_done(struct snd_emu10k1 *emu)
{
	int ch;

	outl(0, emu->port + INTE);

	 
	for (ch = 0; ch < NUM_G; ch++) {
		snd_emu10k1_ptr_write_multiple(emu, ch,
			DCYSUSV, 0,
			VTFT, 0,
			CVCF, 0,
			PTRX, 0,
			CPF, 0,
			REGLIST_END);
	}

	
	if (emu->audigy)
		snd_emu10k1_ptr_write(emu, A_DBG, 0, A_DBG_SINGLE_STEP);
	else
		snd_emu10k1_ptr_write(emu, DBG, 0, EMU10K1_DBG_SINGLE_STEP);

	snd_emu10k1_ptr_write_multiple(emu, 0,
		 
		MICBS, 0,
		MICBA, 0,
		FXBS, 0,
		FXBA, 0,
		FXWC, 0,
		ADCBS, ADCBS_BUFSIZE_NONE,
		ADCBA, 0,
		TCBS, TCBS_BUFFSIZE_16K,
		TCB, 0,

		 
		CLIEL, 0,
		CLIEH, 0,
		SOLEL, 0,
		SOLEH, 0,

		PTB, 0,

		REGLIST_END);

	 
	outl(HCFG_LOCKSOUNDCACHE | HCFG_LOCKTANKCACHE_MASK | HCFG_MUTEBUTTONENABLE, emu->port + HCFG);

	return 0;
}

 

 
#define HOOKN_BIT		(1L << 12)
#define HANDN_BIT		(1L << 11)
#define PULSEN_BIT		(1L << 10)

#define EC_GDI1			(1 << 13)
#define EC_GDI0			(1 << 14)

#define EC_NUM_CONTROL_BITS	20

#define EC_AC3_DATA_SELN	0x0001L
#define EC_EE_DATA_SEL		0x0002L
#define EC_EE_CNTRL_SELN	0x0004L
#define EC_EECLK		0x0008L
#define EC_EECS			0x0010L
#define EC_EESDO		0x0020L
#define EC_TRIM_CSN		0x0040L
#define EC_TRIM_SCLK		0x0080L
#define EC_TRIM_SDATA		0x0100L
#define EC_TRIM_MUTEN		0x0200L
#define EC_ADCCAL		0x0400L
#define EC_ADCRSTN		0x0800L
#define EC_DACCAL		0x1000L
#define EC_DACMUTEN		0x2000L
#define EC_LEDN			0x4000L

#define EC_SPDIF0_SEL_SHIFT	15
#define EC_SPDIF1_SEL_SHIFT	17
#define EC_SPDIF0_SEL_MASK	(0x3L << EC_SPDIF0_SEL_SHIFT)
#define EC_SPDIF1_SEL_MASK	(0x7L << EC_SPDIF1_SEL_SHIFT)
#define EC_SPDIF0_SELECT(_x)	(((_x) << EC_SPDIF0_SEL_SHIFT) & EC_SPDIF0_SEL_MASK)
#define EC_SPDIF1_SELECT(_x)	(((_x) << EC_SPDIF1_SEL_SHIFT) & EC_SPDIF1_SEL_MASK)
#define EC_CURRENT_PROM_VERSION 0x01	 

#define EC_EEPROM_SIZE		0x40	 

 
#define EC_PROM_VERSION_ADDR	0x20	 
#define EC_BOARDREV0_ADDR	0x21	 
#define EC_BOARDREV1_ADDR	0x22	 

#define EC_LAST_PROMFILE_ADDR	0x2f

#define EC_SERIALNUM_ADDR	0x30	 
#define EC_CHECKSUM_ADDR	0x3f	 


 
#define EC_RAW_RUN_MODE		(EC_DACMUTEN | EC_ADCRSTN | EC_TRIM_MUTEN | \
				 EC_TRIM_CSN)


#define EC_DEFAULT_ADC_GAIN	0xC4C4
#define EC_DEFAULT_SPDIF0_SEL	0x0
#define EC_DEFAULT_SPDIF1_SEL	0x4

 

static void snd_emu10k1_ecard_write(struct snd_emu10k1 *emu, unsigned int value)
{
	unsigned short count;
	unsigned int data;
	unsigned long hc_port;
	unsigned int hc_value;

	hc_port = emu->port + HCFG;
	hc_value = inl(hc_port) & ~(HOOKN_BIT | HANDN_BIT | PULSEN_BIT);
	outl(hc_value, hc_port);

	for (count = 0; count < EC_NUM_CONTROL_BITS; count++) {

		 
		data = ((value & 0x1) ? PULSEN_BIT : 0);
		value >>= 1;

		outl(hc_value | data, hc_port);

		 
		outl(hc_value | data | HANDN_BIT, hc_port);
		outl(hc_value | data, hc_port);
	}

	 
	outl(hc_value | HOOKN_BIT, hc_port);
	outl(hc_value, hc_port);
}

 

static void snd_emu10k1_ecard_setadcgain(struct snd_emu10k1 *emu,
					 unsigned short gain)
{
	unsigned int bit;

	 
	snd_emu10k1_ecard_write(emu, emu->ecard_ctrl & ~EC_TRIM_CSN);

	 
	snd_emu10k1_ecard_write(emu, emu->ecard_ctrl & ~EC_TRIM_CSN);

	for (bit = (1 << 15); bit; bit >>= 1) {
		unsigned int value;

		value = emu->ecard_ctrl & ~(EC_TRIM_CSN | EC_TRIM_SDATA);

		if (gain & bit)
			value |= EC_TRIM_SDATA;

		 
		snd_emu10k1_ecard_write(emu, value);
		snd_emu10k1_ecard_write(emu, value | EC_TRIM_SCLK);
		snd_emu10k1_ecard_write(emu, value);
	}

	snd_emu10k1_ecard_write(emu, emu->ecard_ctrl);
}

static int snd_emu10k1_ecard_init(struct snd_emu10k1 *emu)
{
	unsigned int hc_value;

	 
	emu->ecard_ctrl = EC_RAW_RUN_MODE |
			  EC_SPDIF0_SELECT(EC_DEFAULT_SPDIF0_SEL) |
			  EC_SPDIF1_SELECT(EC_DEFAULT_SPDIF1_SEL);

	 
	hc_value = inl(emu->port + HCFG);
	outl(hc_value | HCFG_AUDIOENABLE | HCFG_CODECFORMAT_I2S, emu->port + HCFG);
	inl(emu->port + HCFG);

	 
	snd_emu10k1_ecard_write(emu, EC_ADCCAL | EC_LEDN | EC_TRIM_CSN);

	 
	snd_emu10k1_ecard_write(emu, EC_DACCAL | EC_LEDN | EC_TRIM_CSN);

	 
	snd_emu10k1_wait(emu, 48000);

	 
	snd_emu10k1_ecard_write(emu, EC_ADCCAL | EC_LEDN | EC_TRIM_CSN);

	 
	snd_emu10k1_ecard_write(emu, emu->ecard_ctrl);

	 
	snd_emu10k1_ecard_setadcgain(emu, EC_DEFAULT_ADC_GAIN);

	return 0;
}

static int snd_emu10k1_cardbus_init(struct snd_emu10k1 *emu)
{
	unsigned long special_port;
	__always_unused unsigned int value;

	 
	special_port = emu->port + 0x38;
	value = inl(special_port);
	outl(0x00d00000, special_port);
	value = inl(special_port);
	outl(0x00d00001, special_port);
	value = inl(special_port);
	outl(0x00d0005f, special_port);
	value = inl(special_port);
	outl(0x00d0007f, special_port);
	value = inl(special_port);
	outl(0x0090007f, special_port);
	value = inl(special_port);

	snd_emu10k1_ptr20_write(emu, TINA2_VOLUME, 0, 0xfefefefe);  
	 
	msleep(200);
	return 0;
}

static int snd_emu1010_load_firmware_entry(struct snd_emu10k1 *emu,
				     const struct firmware *fw_entry)
{
	int n, i;
	u16 reg;
	u8 value;
	__always_unused u16 write_post;

	if (!fw_entry)
		return -EIO;

	 
	 
	 
	spin_lock_irq(&emu->emu_lock);
	outw(0x00, emu->port + A_GPIO);  
	write_post = inw(emu->port + A_GPIO);
	udelay(100);
	outw(0x80, emu->port + A_GPIO);  
	write_post = inw(emu->port + A_GPIO);
	udelay(100);  
	for (n = 0; n < fw_entry->size; n++) {
		value = fw_entry->data[n];
		for (i = 0; i < 8; i++) {
			reg = 0x80;
			if (value & 0x1)
				reg = reg | 0x20;
			value = value >> 1;
			outw(reg, emu->port + A_GPIO);
			write_post = inw(emu->port + A_GPIO);
			outw(reg | 0x40, emu->port + A_GPIO);
			write_post = inw(emu->port + A_GPIO);
		}
	}
	 
	outw(0x10, emu->port + A_GPIO);
	write_post = inw(emu->port + A_GPIO);
	spin_unlock_irq(&emu->emu_lock);

	return 0;
}

 
static const char * const firmware_names[5][2] = {
	[EMU_MODEL_EMU1010] = {
		HANA_FILENAME, DOCK_FILENAME
	},
	[EMU_MODEL_EMU1010B] = {
		EMU1010B_FILENAME, MICRO_DOCK_FILENAME
	},
	[EMU_MODEL_EMU1616] = {
		EMU1010_NOTEBOOK_FILENAME, MICRO_DOCK_FILENAME
	},
	[EMU_MODEL_EMU0404] = {
		EMU0404_FILENAME, NULL
	},
};

static int snd_emu1010_load_firmware(struct snd_emu10k1 *emu, int dock,
				     const struct firmware **fw)
{
	const char *filename;
	int err;

	if (!*fw) {
		filename = firmware_names[emu->card_capabilities->emu_model][dock];
		if (!filename)
			return 0;
		err = request_firmware(fw, filename, &emu->pci->dev);
		if (err)
			return err;
	}

	return snd_emu1010_load_firmware_entry(emu, *fw);
}

static void emu1010_firmware_work(struct work_struct *work)
{
	struct snd_emu10k1 *emu;
	u32 tmp, tmp2, reg;
	int err;

	emu = container_of(work, struct snd_emu10k1,
			   emu1010.firmware_work);
	if (emu->card->shutdown)
		return;
#ifdef CONFIG_PM_SLEEP
	if (emu->suspend)
		return;
#endif
	snd_emu1010_fpga_read(emu, EMU_HANA_OPTION_CARDS, &reg);  
	if (reg & EMU_HANA_OPTION_DOCK_OFFLINE) {
		 
		 
		dev_info(emu->card->dev,
			 "emu1010: Loading Audio Dock Firmware\n");
		snd_emu1010_fpga_write(emu, EMU_HANA_FPGA_CONFIG,
				       EMU_HANA_FPGA_CONFIG_AUDIODOCK);
		err = snd_emu1010_load_firmware(emu, 1, &emu->dock_fw);
		if (err < 0)
			return;
		snd_emu1010_fpga_write(emu, EMU_HANA_FPGA_CONFIG, 0);
		snd_emu1010_fpga_read(emu, EMU_HANA_ID, &tmp);
		dev_info(emu->card->dev,
			 "emu1010: EMU_HANA+DOCK_ID = 0x%x\n", tmp);
		if ((tmp & 0x1f) != 0x15) {
			 
			dev_info(emu->card->dev,
				 "emu1010: Loading Audio Dock Firmware file failed, reg = 0x%x\n",
				 tmp);
			return;
		}
		dev_info(emu->card->dev,
			 "emu1010: Audio Dock Firmware loaded\n");
		snd_emu1010_fpga_read(emu, EMU_DOCK_MAJOR_REV, &tmp);
		snd_emu1010_fpga_read(emu, EMU_DOCK_MINOR_REV, &tmp2);
		dev_info(emu->card->dev, "Audio Dock ver: %u.%u\n", tmp, tmp2);
		 
		 
		msleep(10);
		 
		snd_emu1010_fpga_write(emu, EMU_HANA_UNMUTE, EMU_UNMUTE);
	}
}

static void emu1010_clock_work(struct work_struct *work)
{
	struct snd_emu10k1 *emu;
	struct snd_ctl_elem_id id;

	emu = container_of(work, struct snd_emu10k1,
			   emu1010.clock_work);
	if (emu->card->shutdown)
		return;
#ifdef CONFIG_PM_SLEEP
	if (emu->suspend)
		return;
#endif

	spin_lock_irq(&emu->reg_lock);
	
	emu->emu1010.clock_source = emu->emu1010.clock_fallback;
	emu->emu1010.wclock = 1 - emu->emu1010.clock_source;
	snd_emu1010_update_clock(emu);
	spin_unlock_irq(&emu->reg_lock);
	snd_ctl_build_ioff(&id, emu->ctl_clock_source, 0);
	snd_ctl_notify(emu->card, SNDRV_CTL_EVENT_MASK_VALUE, &id);
}

static void emu1010_interrupt(struct snd_emu10k1 *emu)
{
	u32 sts;

	snd_emu1010_fpga_read(emu, EMU_HANA_IRQ_STATUS, &sts);
	if (sts & EMU_HANA_IRQ_DOCK_LOST) {
		 
		dev_info(emu->card->dev, "emu1010: Audio Dock detached\n");
		 
		snd_emu1010_fpga_write(emu, EMU_HANA_UNMUTE, EMU_UNMUTE);
	} else if (sts & EMU_HANA_IRQ_DOCK) {
		schedule_work(&emu->emu1010.firmware_work);
	}
	if (sts & EMU_HANA_IRQ_WCLK_CHANGED)
		schedule_work(&emu->emu1010.clock_work);
}

 
static int snd_emu10k1_emu1010_init(struct snd_emu10k1 *emu)
{
	u32 tmp, tmp2, reg;
	int err;

	dev_info(emu->card->dev, "emu1010: Special config.\n");

	 
	outl(HCFG_LOCKSOUNDCACHE | HCFG_LOCKTANKCACHE_MASK, emu->port + HCFG);

	 
	snd_emu1010_fpga_write(emu, EMU_HANA_DOCK_PWR, 0);

	 
	snd_emu1010_fpga_read(emu, EMU_HANA_ID, &reg);
	dev_dbg(emu->card->dev, "reg1 = 0x%x\n", reg);
	if ((reg & 0x3f) == 0x15) {
		 
		 

		snd_emu1010_fpga_write(emu, EMU_HANA_FPGA_CONFIG, EMU_HANA_FPGA_CONFIG_HANA);
	}
	snd_emu1010_fpga_read(emu, EMU_HANA_ID, &reg);
	dev_dbg(emu->card->dev, "reg2 = 0x%x\n", reg);
	if ((reg & 0x3f) == 0x15) {
		 
		dev_info(emu->card->dev,
			 "emu1010: FPGA failed to return to programming mode\n");
		return -ENODEV;
	}
	dev_info(emu->card->dev, "emu1010: EMU_HANA_ID = 0x%x\n", reg);

	err = snd_emu1010_load_firmware(emu, 0, &emu->firmware);
	if (err < 0) {
		dev_info(emu->card->dev, "emu1010: Loading Firmware failed\n");
		return err;
	}

	 
	snd_emu1010_fpga_read(emu, EMU_HANA_ID, &reg);
	if ((reg & 0x3f) != 0x15) {
		 
		dev_info(emu->card->dev,
			 "emu1010: Loading Hana Firmware file failed, reg = 0x%x\n",
			 reg);
		return -ENODEV;
	}

	dev_info(emu->card->dev, "emu1010: Hana Firmware loaded\n");
	snd_emu1010_fpga_read(emu, EMU_HANA_MAJOR_REV, &tmp);
	snd_emu1010_fpga_read(emu, EMU_HANA_MINOR_REV, &tmp2);
	dev_info(emu->card->dev, "emu1010: Hana version: %u.%u\n", tmp, tmp2);
	 
	snd_emu1010_fpga_write(emu, EMU_HANA_DOCK_PWR, EMU_HANA_DOCK_PWR_ON);

	snd_emu1010_fpga_read(emu, EMU_HANA_OPTION_CARDS, &reg);
	dev_info(emu->card->dev, "emu1010: Card options = 0x%x\n", reg);
	if (reg & EMU_HANA_OPTION_DOCK_OFFLINE)
		schedule_work(&emu->emu1010.firmware_work);
	if (emu->card_capabilities->no_adat) {
		emu->emu1010.optical_in = 0;  
		emu->emu1010.optical_out = 0;  
	} else {
		 
		emu->emu1010.optical_in = 1;  
		emu->emu1010.optical_out = 1;  
	}
	tmp = (emu->emu1010.optical_in ? EMU_HANA_OPTICAL_IN_ADAT : EMU_HANA_OPTICAL_IN_SPDIF) |
		(emu->emu1010.optical_out ? EMU_HANA_OPTICAL_OUT_ADAT : EMU_HANA_OPTICAL_OUT_SPDIF);
	snd_emu1010_fpga_write(emu, EMU_HANA_OPTICAL_TYPE, tmp);
	 
	emu->emu1010.adc_pads = 0x00;
	snd_emu1010_fpga_write(emu, EMU_HANA_ADC_PADS, emu->emu1010.adc_pads);
	 
	snd_emu1010_fpga_write(emu, EMU_HANA_DOCK_MISC, EMU_HANA_DOCK_PHONES_192_DAC4);
	 
	emu->emu1010.dac_pads = EMU_HANA_DOCK_DAC_PAD1 | EMU_HANA_DOCK_DAC_PAD2 |
				EMU_HANA_DOCK_DAC_PAD3 | EMU_HANA_DOCK_DAC_PAD4;
	snd_emu1010_fpga_write(emu, EMU_HANA_DAC_PADS, emu->emu1010.dac_pads);
	 
	snd_emu1010_fpga_write(emu, EMU_HANA_SPDIF_MODE, EMU_HANA_SPDIF_MODE_RX_INVALID);
	 
	snd_emu1010_fpga_write(emu, EMU_HANA_MIDI_IN, EMU_HANA_MIDI_INA_FROM_HAMOA | EMU_HANA_MIDI_INB_FROM_DOCK2);
	snd_emu1010_fpga_write(emu, EMU_HANA_MIDI_OUT, EMU_HANA_MIDI_OUT_DOCK2 | EMU_HANA_MIDI_OUT_SYNC2);

	emu->gpio_interrupt = emu1010_interrupt;
	
	snd_emu1010_fpga_write(emu, EMU_HANA_IRQ_ENABLE,
			       EMU_HANA_IRQ_DOCK | EMU_HANA_IRQ_DOCK_LOST | EMU_HANA_IRQ_WCLK_CHANGED);
	snd_emu1010_fpga_read(emu, EMU_HANA_IRQ_STATUS, &reg);  

	emu->emu1010.clock_source = 1;   
	emu->emu1010.clock_fallback = 1;   
	 
	snd_emu1010_fpga_write(emu, EMU_HANA_DEFCLOCK, EMU_HANA_DEFCLOCK_48K);
	 
	emu->emu1010.wclock = EMU_HANA_WCLOCK_INT_48K;
	snd_emu1010_fpga_write(emu, EMU_HANA_WCLOCK, EMU_HANA_WCLOCK_INT_48K);
	 
	snd_emu1010_update_clock(emu);

	
	
	snd_emu1010_fpga_write(emu, EMU_HANA_UNMUTE, EMU_UNMUTE);

	return 0;
}
 

#ifdef CONFIG_PM_SLEEP
static int alloc_pm_buffer(struct snd_emu10k1 *emu);
static void free_pm_buffer(struct snd_emu10k1 *emu);
#endif

static void snd_emu10k1_free(struct snd_card *card)
{
	struct snd_emu10k1 *emu = card->private_data;

	if (emu->port) {	 
		snd_emu10k1_fx8010_tram_setup(emu, 0);
		snd_emu10k1_done(emu);
		snd_emu10k1_free_efx(emu);
	}
	if (emu->card_capabilities->emu_model == EMU_MODEL_EMU1010) {
		 
		snd_emu1010_fpga_write(emu, EMU_HANA_DOCK_PWR, 0);
	}
	cancel_work_sync(&emu->emu1010.firmware_work);
	cancel_work_sync(&emu->emu1010.clock_work);
	release_firmware(emu->firmware);
	release_firmware(emu->dock_fw);
	snd_util_memhdr_free(emu->memhdr);
	if (emu->silent_page.area)
		snd_dma_free_pages(&emu->silent_page);
	if (emu->ptb_pages.area)
		snd_dma_free_pages(&emu->ptb_pages);
	vfree(emu->page_ptr_table);
	vfree(emu->page_addr_table);
#ifdef CONFIG_PM_SLEEP
	free_pm_buffer(emu);
#endif
}

static const struct snd_emu_chip_details emu_chip_details[] = {
	 
	 
	 
	{.vendor = 0x1102, .device = 0x0008, .subsystem = 0x10241102,
	 .driver = "Audigy2", .name = "SB Audigy 5/Rx [SB1550]",
	 .id = "Audigy2",
	 .emu10k2_chip = 1,
	 .ca0108_chip = 1,
	 .spk71 = 1,
	 .adc_1361t = 1,   
	 .ac97_chip = 1},
	 
	 
	 
	 
	 
	{.vendor = 0x1102, .device = 0x0008, .subsystem = 0x10211102,
	 .driver = "Audigy2", .name = "SB Audigy 4 [SB0610]",
	 .id = "Audigy2",
	 .emu10k2_chip = 1,
	 .ca0108_chip = 1,
	 .spk71 = 1,
	 .adc_1361t = 1,   
	 .ac97_chip = 1} ,
	 
	 
	 
	 
	{.vendor = 0x1102, .device = 0x0008, .subsystem = 0x10011102,
	 .driver = "Audigy2", .name = "SB Audigy 2 Value [SB0400]",
	 .id = "Audigy2",
	 .emu10k2_chip = 1,
	 .ca0108_chip = 1,
	 .spk71 = 1,
	 .ac97_chip = 1} ,
	 
	 
	 
	 
	 
	 
	{.vendor = 0x1102, .device = 0x0008, .subsystem = 0x20011102,
	 .driver = "Audigy2", .name = "Audigy 2 ZS Notebook [SB0530]",
	 .id = "Audigy2",
	 .emu10k2_chip = 1,
	 .ca0108_chip = 1,
	 .ca_cardbus_chip = 1,
	 .spi_dac = 1,
	 .i2c_adc = 1,
	 .spk71 = 1} ,
	 
	 
	 
	 
	{.vendor = 0x1102, .device = 0x0008, .subsystem = 0x42011102,
	 .driver = "Audigy2", .name = "E-MU 02 CardBus [MAEM8950]",
	 .id = "EMU1010",
	 .emu10k2_chip = 1,
	 .ca0108_chip = 1,
	 .ca_cardbus_chip = 1,
	 .spk71 = 1 ,
	 .emu_model = EMU_MODEL_EMU1616},
	 
	 
	 
	 
	{.vendor = 0x1102, .device = 0x0008, .subsystem = 0x40041102,
	 .driver = "Audigy2", .name = "E-MU 1010b PCI [MAEM8960]",
	 .id = "EMU1010",
	 .emu10k2_chip = 1,
	 .ca0108_chip = 1,
	 .spk71 = 1,
	 .emu_model = EMU_MODEL_EMU1010B},  
	 
	 
	 
	 
	{.vendor = 0x1102, .device = 0x0008, .subsystem = 0x40071102,
	 .driver = "Audigy2", .name = "E-MU 1010 PCIe [MAEM8986]",
	 .id = "EMU1010",
	 .emu10k2_chip = 1,
	 .ca0108_chip = 1,
	 .spk71 = 1,
	 .emu_model = EMU_MODEL_EMU1010B},  
	 
	 
	 
	 
	{.vendor = 0x1102, .device = 0x0004, .subsystem = 0x40011102,
	 .driver = "Audigy2", .name = "E-MU 1010 [MAEM8810]",
	 .id = "EMU1010",
	 .emu10k2_chip = 1,
	 .ca0102_chip = 1,
	 .spk71 = 1,
	 .emu_model = EMU_MODEL_EMU1010},  
	 
	 
	 
	{.vendor = 0x1102, .device = 0x0008, .subsystem = 0x40021102,
	 .driver = "Audigy2", .name = "E-MU 0404b PCI [MAEM8852]",
	 .id = "EMU0404",
	 .emu10k2_chip = 1,
	 .ca0108_chip = 1,
	 .spk20 = 1,
	 .no_adat = 1,
	 .emu_model = EMU_MODEL_EMU0404},  
	 
	 
	 
	{.vendor = 0x1102, .device = 0x0004, .subsystem = 0x40021102,
	 .driver = "Audigy2", .name = "E-MU 0404 [MAEM8850]",
	 .id = "EMU0404",
	 .emu10k2_chip = 1,
	 .ca0102_chip = 1,
	 .spk20 = 1,
	 .no_adat = 1,
	 .emu_model = EMU_MODEL_EMU0404},  
	 
	 
	{.vendor = 0x1102, .device = 0x0008, .subsystem = 0x40051102,
	 .driver = "Audigy2", .name = "E-MU 0404 PCIe [MAEM8984]",
	 .id = "EMU0404",
	 .emu10k2_chip = 1,
	 .ca0108_chip = 1,
	 .spk20 = 1,
	 .no_adat = 1,
	 .emu_model = EMU_MODEL_EMU0404},  
	{.vendor = 0x1102, .device = 0x0008,
	 .driver = "Audigy2", .name = "SB Audigy 2 Value [Unknown]",
	 .id = "Audigy2",
	 .emu10k2_chip = 1,
	 .ca0108_chip = 1,
	 .ac97_chip = 1} ,
	 
	{.vendor = 0x1102, .device = 0x0004, .subsystem = 0x20071102,
	 .driver = "Audigy2", .name = "SB Audigy 4 PRO [SB0380]",
	 .id = "Audigy2",
	 .emu10k2_chip = 1,
	 .ca0102_chip = 1,
	 .ca0151_chip = 1,
	 .spk71 = 1,
	 .spdif_bug = 1,
	 .ac97_chip = 1} ,
	 
	 
	{.vendor = 0x1102, .device = 0x0004, .subsystem = 0x20061102,
	 .driver = "Audigy2", .name = "SB Audigy 2 [SB0350b]",
	 .id = "Audigy2",
	 .emu10k2_chip = 1,
	 .ca0102_chip = 1,
	 .ca0151_chip = 1,
	 .spk71 = 1,
	 .spdif_bug = 1,
	 .invert_shared_spdif = 1,	 
	 .ac97_chip = 1} ,
	 
	{.vendor = 0x1102, .device = 0x0004, .subsystem = 0x20051102,
	 .driver = "Audigy2", .name = "SB Audigy 2 ZS [SB0350a]",
	 .id = "Audigy2",
	 .emu10k2_chip = 1,
	 .ca0102_chip = 1,
	 .ca0151_chip = 1,
	 .spk71 = 1,
	 .spdif_bug = 1,
	 .invert_shared_spdif = 1,	 
	 .ac97_chip = 1} ,
	{.vendor = 0x1102, .device = 0x0004, .subsystem = 0x20021102,
	 .driver = "Audigy2", .name = "SB Audigy 2 ZS [SB0350]",
	 .id = "Audigy2",
	 .emu10k2_chip = 1,
	 .ca0102_chip = 1,
	 .ca0151_chip = 1,
	 .spk71 = 1,
	 .spdif_bug = 1,
	 .invert_shared_spdif = 1,	 
	 .ac97_chip = 1} ,
	{.vendor = 0x1102, .device = 0x0004, .subsystem = 0x20011102,
	 .driver = "Audigy2", .name = "SB Audigy 2 ZS [SB0360]",
	 .id = "Audigy2",
	 .emu10k2_chip = 1,
	 .ca0102_chip = 1,
	 .ca0151_chip = 1,
	 .spk71 = 1,
	 .spdif_bug = 1,
	 .invert_shared_spdif = 1,	 
	 .ac97_chip = 1} ,
	 
	 
	 
	{.vendor = 0x1102, .device = 0x0004, .subsystem = 0x10071102,
	 .driver = "Audigy2", .name = "SB Audigy 2 [SB0240]",
	 .id = "Audigy2",
	 .emu10k2_chip = 1,
	 .ca0102_chip = 1,
	 .ca0151_chip = 1,
	 .spk71 = 1,
	 .spdif_bug = 1,
	 .adc_1361t = 1,   
	 .ac97_chip = 1} ,
	 
	 
	{.vendor = 0x1102, .device = 0x0004, .subsystem = 0x10051102,
	 .driver = "Audigy2", .name = "Audigy 2 Platinum EX [SB0280]",
	 .id = "Audigy2",
	 .emu10k2_chip = 1,
	 .ca0102_chip = 1,
	 .ca0151_chip = 1,
	 .spk71 = 1,
	 .spdif_bug = 1} ,
	 
	 
	{.vendor = 0x1102, .device = 0x0004, .subsystem = 0x10031102,
	 .driver = "Audigy2", .name = "SB Audigy 2 ZS [SB0353]",
	 .id = "Audigy2",
	 .emu10k2_chip = 1,
	 .ca0102_chip = 1,
	 .ca0151_chip = 1,
	 .spk71 = 1,
	 .spdif_bug = 1,
	 .invert_shared_spdif = 1,	 
	 .ac97_chip = 1} ,
	 
	 
	{.vendor = 0x1102, .device = 0x0004, .subsystem = 0x10021102,
	 .driver = "Audigy2", .name = "SB Audigy 2 Platinum [SB0240P]",
	 .id = "Audigy2",
	 .emu10k2_chip = 1,
	 .ca0102_chip = 1,
	 .ca0151_chip = 1,
	 .spk71 = 1,
	 .spdif_bug = 1,
	 .invert_shared_spdif = 1,	 
	 .adc_1361t = 1,   
	 .ac97_chip = 1} ,
	{.vendor = 0x1102, .device = 0x0004, .revision = 0x04,
	 .driver = "Audigy2", .name = "SB Audigy 2 [Unknown]",
	 .id = "Audigy2",
	 .emu10k2_chip = 1,
	 .ca0102_chip = 1,
	 .ca0151_chip = 1,
	 .spdif_bug = 1,
	 .ac97_chip = 1} ,
	{.vendor = 0x1102, .device = 0x0004, .subsystem = 0x00531102,
	 .driver = "Audigy", .name = "SB Audigy 1 [SB0092]",
	 .id = "Audigy",
	 .emu10k2_chip = 1,
	 .ca0102_chip = 1,
	 .ac97_chip = 1} ,
	{.vendor = 0x1102, .device = 0x0004, .subsystem = 0x00521102,
	 .driver = "Audigy", .name = "SB Audigy 1 ES [SB0160]",
	 .id = "Audigy",
	 .emu10k2_chip = 1,
	 .ca0102_chip = 1,
	 .spdif_bug = 1,
	 .ac97_chip = 1} ,
	{.vendor = 0x1102, .device = 0x0004, .subsystem = 0x00511102,
	 .driver = "Audigy", .name = "SB Audigy 1 [SB0090]",
	 .id = "Audigy",
	 .emu10k2_chip = 1,
	 .ca0102_chip = 1,
	 .ac97_chip = 1} ,
	{.vendor = 0x1102, .device = 0x0004,
	 .driver = "Audigy", .name = "Audigy 1 [Unknown]",
	 .id = "Audigy",
	 .emu10k2_chip = 1,
	 .ca0102_chip = 1,
	 .ac97_chip = 1} ,
	{.vendor = 0x1102, .device = 0x0002, .subsystem = 0x100a1102,
	 .driver = "EMU10K1", .name = "SB Live! 5.1 [SB0220]",
	 .id = "Live",
	 .emu10k1_chip = 1,
	 .ac97_chip = 1,
	 .sblive51 = 1} ,
	{.vendor = 0x1102, .device = 0x0002, .subsystem = 0x806b1102,
	 .driver = "EMU10K1", .name = "SB Live! [SB0105]",
	 .id = "Live",
	 .emu10k1_chip = 1,
	 .ac97_chip = 1,
	 .sblive51 = 1} ,
	{.vendor = 0x1102, .device = 0x0002, .subsystem = 0x806a1102,
	 .driver = "EMU10K1", .name = "SB Live! Value [SB0103]",
	 .id = "Live",
	 .emu10k1_chip = 1,
	 .ac97_chip = 1,
	 .sblive51 = 1} ,
	{.vendor = 0x1102, .device = 0x0002, .subsystem = 0x80691102,
	 .driver = "EMU10K1", .name = "SB Live! Value [SB0101]",
	 .id = "Live",
	 .emu10k1_chip = 1,
	 .ac97_chip = 1,
	 .sblive51 = 1} ,
	 
	 
	 
	{.vendor = 0x1102, .device = 0x0002, .subsystem = 0x80661102,
	 .driver = "EMU10K1", .name = "SB Live! 5.1 Dell OEM [SB0228]",
	 .id = "Live",
	 .emu10k1_chip = 1,
	 .ac97_chip = 1,
	 .sblive51 = 1} ,
	 
	{.vendor = 0x1102, .device = 0x0002, .subsystem = 0x80651102,
	 .driver = "EMU10K1", .name = "SB Live! 5.1 [SB0220]",
	 .id = "Live",
	 .emu10k1_chip = 1,
	 .ac97_chip = 1,
	 .sblive51 = 1} ,
	{.vendor = 0x1102, .device = 0x0002, .subsystem = 0x80641102,
	 .driver = "EMU10K1", .name = "SB Live! 5.1",
	 .id = "Live",
	 .emu10k1_chip = 1,
	 .ac97_chip = 1,
	 .sblive51 = 1} ,
	 
	{.vendor = 0x1102, .device = 0x0002, .subsystem = 0x80611102,
	 .driver = "EMU10K1", .name = "SB Live! 5.1 [SB0060]",
	 .id = "Live",
	 .emu10k1_chip = 1,
	 .ac97_chip = 2,  
	 .sblive51 = 1} ,
	{.vendor = 0x1102, .device = 0x0002, .subsystem = 0x80511102,
	 .driver = "EMU10K1", .name = "SB Live! Value [CT4850]",
	 .id = "Live",
	 .emu10k1_chip = 1,
	 .ac97_chip = 1,
	 .sblive51 = 1} ,
	 
	 
	 
	{.vendor = 0x1102, .device = 0x0002, .subsystem = 0x80401102,
	 .driver = "EMU10K1", .name = "SB Live! Platinum [CT4760P]",
	 .id = "Live",
	 .emu10k1_chip = 1,
	 .ac97_chip = 1} ,
	{.vendor = 0x1102, .device = 0x0002, .subsystem = 0x80321102,
	 .driver = "EMU10K1", .name = "SB Live! Value [CT4871]",
	 .id = "Live",
	 .emu10k1_chip = 1,
	 .ac97_chip = 1,
	 .sblive51 = 1} ,
	{.vendor = 0x1102, .device = 0x0002, .subsystem = 0x80311102,
	 .driver = "EMU10K1", .name = "SB Live! Value [CT4831]",
	 .id = "Live",
	 .emu10k1_chip = 1,
	 .ac97_chip = 1,
	 .sblive51 = 1} ,
	{.vendor = 0x1102, .device = 0x0002, .subsystem = 0x80281102,
	 .driver = "EMU10K1", .name = "SB Live! Value [CT4870]",
	 .id = "Live",
	 .emu10k1_chip = 1,
	 .ac97_chip = 1,
	 .sblive51 = 1} ,
	 
	{.vendor = 0x1102, .device = 0x0002, .subsystem = 0x80271102,
	 .driver = "EMU10K1", .name = "SB Live! Value [CT4832]",
	 .id = "Live",
	 .emu10k1_chip = 1,
	 .ac97_chip = 1,
	 .sblive51 = 1} ,
	{.vendor = 0x1102, .device = 0x0002, .subsystem = 0x80261102,
	 .driver = "EMU10K1", .name = "SB Live! Value [CT4830]",
	 .id = "Live",
	 .emu10k1_chip = 1,
	 .ac97_chip = 1,
	 .sblive51 = 1} ,
	{.vendor = 0x1102, .device = 0x0002, .subsystem = 0x80231102,
	 .driver = "EMU10K1", .name = "SB PCI512 [CT4790]",
	 .id = "Live",
	 .emu10k1_chip = 1,
	 .ac97_chip = 1,
	 .sblive51 = 1} ,
	{.vendor = 0x1102, .device = 0x0002, .subsystem = 0x80221102,
	 .driver = "EMU10K1", .name = "SB Live! Value [CT4780]",
	 .id = "Live",
	 .emu10k1_chip = 1,
	 .ac97_chip = 1,
	 .sblive51 = 1} ,
	{.vendor = 0x1102, .device = 0x0002, .subsystem = 0x40011102,
	 .driver = "EMU10K1", .name = "E-MU APS [PC545]",
	 .id = "APS",
	 .emu10k1_chip = 1,
	 .ecard = 1} ,
	{.vendor = 0x1102, .device = 0x0002, .subsystem = 0x00211102,
	 .driver = "EMU10K1", .name = "SB Live! [CT4620]",
	 .id = "Live",
	 .emu10k1_chip = 1,
	 .ac97_chip = 1,
	 .sblive51 = 1} ,
	{.vendor = 0x1102, .device = 0x0002, .subsystem = 0x00201102,
	 .driver = "EMU10K1", .name = "SB Live! Value [CT4670]",
	 .id = "Live",
	 .emu10k1_chip = 1,
	 .ac97_chip = 1,
	 .sblive51 = 1} ,
	{.vendor = 0x1102, .device = 0x0002,
	 .driver = "EMU10K1", .name = "SB Live! [Unknown]",
	 .id = "Live",
	 .emu10k1_chip = 1,
	 .ac97_chip = 1,
	 .sblive51 = 1} ,
	{ }  
};

 
static void snd_emu10k1_detect_iommu(struct snd_emu10k1 *emu)
{
	struct iommu_domain *domain;

	emu->iommu_workaround = false;

	domain = iommu_get_domain_for_dev(emu->card->dev);
	if (!domain || domain->type == IOMMU_DOMAIN_IDENTITY)
		return;

	dev_notice(emu->card->dev,
		   "non-passthrough IOMMU detected, widening DMA allocations");
	emu->iommu_workaround = true;
}

int snd_emu10k1_create(struct snd_card *card,
		       struct pci_dev *pci,
		       unsigned short extin_mask,
		       unsigned short extout_mask,
		       long max_cache_bytes,
		       int enable_ir,
		       uint subsystem)
{
	struct snd_emu10k1 *emu = card->private_data;
	int idx, err;
	int is_audigy;
	size_t page_table_size;
	__le32 *pgtbl;
	unsigned int silent_page;
	const struct snd_emu_chip_details *c;

	 
	err = pcim_enable_device(pci);
	if (err < 0)
		return err;

	card->private_free = snd_emu10k1_free;
	emu->card = card;
	spin_lock_init(&emu->reg_lock);
	spin_lock_init(&emu->emu_lock);
	spin_lock_init(&emu->spi_lock);
	spin_lock_init(&emu->i2c_lock);
	spin_lock_init(&emu->voice_lock);
	spin_lock_init(&emu->synth_lock);
	spin_lock_init(&emu->memblk_lock);
	mutex_init(&emu->fx8010.lock);
	INIT_LIST_HEAD(&emu->mapped_link_head);
	INIT_LIST_HEAD(&emu->mapped_order_link_head);
	emu->pci = pci;
	emu->irq = -1;
	emu->synth = NULL;
	emu->get_synth_voice = NULL;
	INIT_WORK(&emu->emu1010.firmware_work, emu1010_firmware_work);
	INIT_WORK(&emu->emu1010.clock_work, emu1010_clock_work);
	 
	emu->revision = pci->revision;
	pci_read_config_dword(pci, PCI_SUBSYSTEM_VENDOR_ID, &emu->serial);
	pci_read_config_word(pci, PCI_SUBSYSTEM_ID, &emu->model);
	dev_dbg(card->dev,
		"vendor = 0x%x, device = 0x%x, subsystem_vendor_id = 0x%x, subsystem_id = 0x%x\n",
		pci->vendor, pci->device, emu->serial, emu->model);

	for (c = emu_chip_details; c->vendor; c++) {
		if (c->vendor == pci->vendor && c->device == pci->device) {
			if (subsystem) {
				if (c->subsystem && (c->subsystem == subsystem))
					break;
				else
					continue;
			} else {
				if (c->subsystem && (c->subsystem != emu->serial))
					continue;
				if (c->revision && c->revision != emu->revision)
					continue;
			}
			break;
		}
	}
	if (c->vendor == 0) {
		dev_err(card->dev, "emu10k1: Card not recognised\n");
		return -ENOENT;
	}
	emu->card_capabilities = c;
	if (c->subsystem && !subsystem)
		dev_dbg(card->dev, "Sound card name = %s\n", c->name);
	else if (subsystem)
		dev_dbg(card->dev, "Sound card name = %s, "
			"vendor = 0x%x, device = 0x%x, subsystem = 0x%x. "
			"Forced to subsystem = 0x%x\n",	c->name,
			pci->vendor, pci->device, emu->serial, c->subsystem);
	else
		dev_dbg(card->dev, "Sound card name = %s, "
			"vendor = 0x%x, device = 0x%x, subsystem = 0x%x.\n",
			c->name, pci->vendor, pci->device,
			emu->serial);

	if (!*card->id && c->id)
		strscpy(card->id, c->id, sizeof(card->id));

	is_audigy = emu->audigy = c->emu10k2_chip;

	snd_emu10k1_detect_iommu(emu);

	 
	emu->address_mode = is_audigy ? 0 : 1;
	 
	emu->dma_mask = emu->address_mode ? EMU10K1_DMA_MASK : AUDIGY_DMA_MASK;
	if (dma_set_mask_and_coherent(&pci->dev, emu->dma_mask) < 0) {
		dev_err(card->dev,
			"architecture does not support PCI busmaster DMA with mask 0x%lx\n",
			emu->dma_mask);
		return -ENXIO;
	}
	if (is_audigy)
		emu->gpr_base = A_FXGPREGBASE;
	else
		emu->gpr_base = FXGPREGBASE;

	err = pci_request_regions(pci, "EMU10K1");
	if (err < 0)
		return err;
	emu->port = pci_resource_start(pci, 0);

	emu->max_cache_pages = max_cache_bytes >> PAGE_SHIFT;

	page_table_size = sizeof(u32) * (emu->address_mode ? MAXPAGES1 :
					 MAXPAGES0);
	if (snd_emu10k1_alloc_pages_maybe_wider(emu, page_table_size,
						&emu->ptb_pages) < 0)
		return -ENOMEM;
	dev_dbg(card->dev, "page table address range is %.8lx:%.8lx\n",
		(unsigned long)emu->ptb_pages.addr,
		(unsigned long)(emu->ptb_pages.addr + emu->ptb_pages.bytes));

	emu->page_ptr_table = vmalloc(array_size(sizeof(void *),
						 emu->max_cache_pages));
	emu->page_addr_table = vmalloc(array_size(sizeof(unsigned long),
						  emu->max_cache_pages));
	if (!emu->page_ptr_table || !emu->page_addr_table)
		return -ENOMEM;

	if (snd_emu10k1_alloc_pages_maybe_wider(emu, EMUPAGESIZE,
						&emu->silent_page) < 0)
		return -ENOMEM;
	dev_dbg(card->dev, "silent page range is %.8lx:%.8lx\n",
		(unsigned long)emu->silent_page.addr,
		(unsigned long)(emu->silent_page.addr +
				emu->silent_page.bytes));

	emu->memhdr = snd_util_memhdr_new(emu->max_cache_pages * PAGE_SIZE);
	if (!emu->memhdr)
		return -ENOMEM;
	emu->memhdr->block_extra_size = sizeof(struct snd_emu10k1_memblk) -
		sizeof(struct snd_util_memblk);

	pci_set_master(pci);

	 
	 
	if (extin_mask == 0)
		extin_mask = 0x3fcf;   
	if (extout_mask == 0)
		extout_mask = 0x7fff;   
	emu->fx8010.extin_mask = extin_mask;
	emu->fx8010.extout_mask = extout_mask;
	emu->enable_ir = enable_ir;

	if (emu->card_capabilities->ca_cardbus_chip) {
		err = snd_emu10k1_cardbus_init(emu);
		if (err < 0)
			return err;
	}
	if (emu->card_capabilities->ecard) {
		err = snd_emu10k1_ecard_init(emu);
		if (err < 0)
			return err;
	} else if (emu->card_capabilities->emu_model) {
		err = snd_emu10k1_emu1010_init(emu);
		if (err < 0)
			return err;
	} else {
		 
		snd_emu10k1_ptr_write(emu, AC97SLOT, 0,
					AC97SLOT_CNTR|AC97SLOT_LFE);
	}

	 
	emu->fx8010.itram_size = (16 * 1024)/2;
	emu->fx8010.etram_pages.area = NULL;
	emu->fx8010.etram_pages.bytes = 0;

	 
	if (devm_request_irq(&pci->dev, pci->irq, snd_emu10k1_interrupt,
			     IRQF_SHARED, KBUILD_MODNAME, emu))
		return -EBUSY;
	emu->irq = pci->irq;
	card->sync_irq = emu->irq;

	 
	emu->spdif_bits[0] = emu->spdif_bits[1] =
		emu->spdif_bits[2] = SPCS_CLKACCY_1000PPM | SPCS_SAMPLERATE_48 |
		SPCS_CHANNELNUM_LEFT | SPCS_SOURCENUM_UNSPEC |
		SPCS_GENERATIONSTATUS | 0x00001200 |
		0x00000000 | SPCS_EMPHASIS_NONE | SPCS_COPYRIGHT;

	 
	memset(emu->silent_page.area, 0, emu->silent_page.bytes);
	silent_page = emu->silent_page.addr << emu->address_mode;
	pgtbl = (__le32 *)emu->ptb_pages.area;
	for (idx = 0; idx < (emu->address_mode ? MAXPAGES1 : MAXPAGES0); idx++)
		pgtbl[idx] = cpu_to_le32(silent_page | idx);

	 
	for (idx = 0; idx < NUM_G; idx++)
		emu->voices[idx].number = idx;

	err = snd_emu10k1_init(emu, enable_ir);
	if (err < 0)
		return err;
#ifdef CONFIG_PM_SLEEP
	err = alloc_pm_buffer(emu);
	if (err < 0)
		return err;
#endif

	 
	err = snd_emu10k1_init_efx(emu);
	if (err < 0)
		return err;
	snd_emu10k1_audio_enable(emu);

#ifdef CONFIG_SND_PROC_FS
	snd_emu10k1_proc_init(emu);
#endif
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static const unsigned char saved_regs[] = {
	CPF, PTRX, CVCF, VTFT, Z1, Z2, PSST, DSL, CCCA, CCR, CLP,
	FXRT, MAPA, MAPB, ENVVOL, ATKHLDV, DCYSUSV, LFOVAL1, ENVVAL,
	ATKHLDM, DCYSUSM, LFOVAL2, IP, IFATN, PEFE, FMMOD, TREMFRQ, FM2FRQ2,
	TEMPENV, ADCCR, FXWC, MICBA, ADCBA, FXBA,
	MICBS, ADCBS, FXBS, CDCS, GPSCS, SPCS0, SPCS1, SPCS2,
	SPBYPASS, AC97SLOT, CDSRCS, GPSRCS, ZVSRCS, MICIDX, ADCIDX, FXIDX,
	0xff  
};
static const unsigned char saved_regs_audigy[] = {
	A_ADCIDX, A_MICIDX, A_FXWC1, A_FXWC2, A_EHC,
	A_FXRT2, A_SENDAMOUNTS, A_FXRT1,
	0xff  
};

static int alloc_pm_buffer(struct snd_emu10k1 *emu)
{
	int size;

	size = ARRAY_SIZE(saved_regs);
	if (emu->audigy)
		size += ARRAY_SIZE(saved_regs_audigy);
	emu->saved_ptr = vmalloc(array3_size(4, NUM_G, size));
	if (!emu->saved_ptr)
		return -ENOMEM;
	if (snd_emu10k1_efx_alloc_pm_buffer(emu) < 0)
		return -ENOMEM;
	if (emu->card_capabilities->ca0151_chip &&
	    snd_p16v_alloc_pm_buffer(emu) < 0)
		return -ENOMEM;
	return 0;
}

static void free_pm_buffer(struct snd_emu10k1 *emu)
{
	vfree(emu->saved_ptr);
	snd_emu10k1_efx_free_pm_buffer(emu);
	if (emu->card_capabilities->ca0151_chip)
		snd_p16v_free_pm_buffer(emu);
}

void snd_emu10k1_suspend_regs(struct snd_emu10k1 *emu)
{
	int i;
	const unsigned char *reg;
	unsigned int *val;

	val = emu->saved_ptr;
	for (reg = saved_regs; *reg != 0xff; reg++)
		for (i = 0; i < NUM_G; i++, val++)
			*val = snd_emu10k1_ptr_read(emu, *reg, i);
	if (emu->audigy) {
		for (reg = saved_regs_audigy; *reg != 0xff; reg++)
			for (i = 0; i < NUM_G; i++, val++)
				*val = snd_emu10k1_ptr_read(emu, *reg, i);
	}
	if (emu->audigy)
		emu->saved_a_iocfg = inw(emu->port + A_IOCFG);
	emu->saved_hcfg = inl(emu->port + HCFG);
}

void snd_emu10k1_resume_init(struct snd_emu10k1 *emu)
{
	if (emu->card_capabilities->ca_cardbus_chip)
		snd_emu10k1_cardbus_init(emu);
	if (emu->card_capabilities->ecard)
		snd_emu10k1_ecard_init(emu);
	else if (emu->card_capabilities->emu_model)
		snd_emu10k1_emu1010_init(emu);
	else
		snd_emu10k1_ptr_write(emu, AC97SLOT, 0, AC97SLOT_CNTR|AC97SLOT_LFE);
	snd_emu10k1_init(emu, emu->enable_ir);
}

void snd_emu10k1_resume_regs(struct snd_emu10k1 *emu)
{
	int i;
	const unsigned char *reg;
	unsigned int *val;

	snd_emu10k1_audio_enable(emu);

	 
	if (emu->audigy)
		outw(emu->saved_a_iocfg, emu->port + A_IOCFG);
	outl(emu->saved_hcfg, emu->port + HCFG);

	val = emu->saved_ptr;
	for (reg = saved_regs; *reg != 0xff; reg++)
		for (i = 0; i < NUM_G; i++, val++)
			snd_emu10k1_ptr_write(emu, *reg, i, *val);
	if (emu->audigy) {
		for (reg = saved_regs_audigy; *reg != 0xff; reg++)
			for (i = 0; i < NUM_G; i++, val++)
				snd_emu10k1_ptr_write(emu, *reg, i, *val);
	}
}
#endif
