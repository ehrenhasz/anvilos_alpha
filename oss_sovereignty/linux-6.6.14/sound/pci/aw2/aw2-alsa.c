
 
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/control.h>

#include "saa7146.h"
#include "aw2-saa7146.h"

MODULE_AUTHOR("Cedric Bregardis <cedric.bregardis@free.fr>, "
	      "Jean-Christian Hassler <jhassler@free.fr>");
MODULE_DESCRIPTION("Emagic Audiowerk 2 sound driver");
MODULE_LICENSE("GPL");

 
#define CTL_ROUTE_ANALOG 0
#define CTL_ROUTE_DIGITAL 1

 
   
static const struct snd_pcm_hardware snd_aw2_playback_hw = {
	.info = (SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rates = SNDRV_PCM_RATE_44100,
	.rate_min = 44100,
	.rate_max = 44100,
	.channels_min = 2,
	.channels_max = 4,
	.buffer_bytes_max = 32768,
	.period_bytes_min = 4096,
	.period_bytes_max = 32768,
	.periods_min = 1,
	.periods_max = 1024,
};

static const struct snd_pcm_hardware snd_aw2_capture_hw = {
	.info = (SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rates = SNDRV_PCM_RATE_44100,
	.rate_min = 44100,
	.rate_max = 44100,
	.channels_min = 2,
	.channels_max = 2,
	.buffer_bytes_max = 32768,
	.period_bytes_min = 4096,
	.period_bytes_max = 32768,
	.periods_min = 1,
	.periods_max = 1024,
};

struct aw2_pcm_device {
	struct snd_pcm *pcm;
	unsigned int stream_number;
	struct aw2 *chip;
};

struct aw2 {
	struct snd_aw2_saa7146 saa7146;

	struct pci_dev *pci;
	int irq;
	spinlock_t reg_lock;
	struct mutex mtx;

	unsigned long iobase_phys;
	void __iomem *iobase_virt;

	struct snd_card *card;

	struct aw2_pcm_device device_playback[NB_STREAM_PLAYBACK];
	struct aw2_pcm_device device_capture[NB_STREAM_CAPTURE];
};

 
static int snd_aw2_create(struct snd_card *card, struct pci_dev *pci);
static int snd_aw2_probe(struct pci_dev *pci,
			 const struct pci_device_id *pci_id);
static int snd_aw2_pcm_playback_open(struct snd_pcm_substream *substream);
static int snd_aw2_pcm_playback_close(struct snd_pcm_substream *substream);
static int snd_aw2_pcm_capture_open(struct snd_pcm_substream *substream);
static int snd_aw2_pcm_capture_close(struct snd_pcm_substream *substream);
static int snd_aw2_pcm_prepare_playback(struct snd_pcm_substream *substream);
static int snd_aw2_pcm_prepare_capture(struct snd_pcm_substream *substream);
static int snd_aw2_pcm_trigger_playback(struct snd_pcm_substream *substream,
					int cmd);
static int snd_aw2_pcm_trigger_capture(struct snd_pcm_substream *substream,
				       int cmd);
static snd_pcm_uframes_t snd_aw2_pcm_pointer_playback(struct snd_pcm_substream
						      *substream);
static snd_pcm_uframes_t snd_aw2_pcm_pointer_capture(struct snd_pcm_substream
						     *substream);
static int snd_aw2_new_pcm(struct aw2 *chip);

static int snd_aw2_control_switch_capture_info(struct snd_kcontrol *kcontrol,
					       struct snd_ctl_elem_info *uinfo);
static int snd_aw2_control_switch_capture_get(struct snd_kcontrol *kcontrol,
					      struct snd_ctl_elem_value
					      *ucontrol);
static int snd_aw2_control_switch_capture_put(struct snd_kcontrol *kcontrol,
					      struct snd_ctl_elem_value
					      *ucontrol);

 
static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;
static bool enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for Audiowerk2 soundcard.");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for the Audiowerk2 soundcard.");
module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable Audiowerk2 soundcard.");

static const struct pci_device_id snd_aw2_ids[] = {
	{PCI_VENDOR_ID_PHILIPS, PCI_DEVICE_ID_PHILIPS_SAA7146, 0, 0,
	 0, 0, 0},
	{0}
};

MODULE_DEVICE_TABLE(pci, snd_aw2_ids);

 
static struct pci_driver aw2_driver = {
	.name = KBUILD_MODNAME,
	.id_table = snd_aw2_ids,
	.probe = snd_aw2_probe,
};

module_pci_driver(aw2_driver);

 
static const struct snd_pcm_ops snd_aw2_playback_ops = {
	.open = snd_aw2_pcm_playback_open,
	.close = snd_aw2_pcm_playback_close,
	.prepare = snd_aw2_pcm_prepare_playback,
	.trigger = snd_aw2_pcm_trigger_playback,
	.pointer = snd_aw2_pcm_pointer_playback,
};

 
static const struct snd_pcm_ops snd_aw2_capture_ops = {
	.open = snd_aw2_pcm_capture_open,
	.close = snd_aw2_pcm_capture_close,
	.prepare = snd_aw2_pcm_prepare_capture,
	.trigger = snd_aw2_pcm_trigger_capture,
	.pointer = snd_aw2_pcm_pointer_capture,
};

static const struct snd_kcontrol_new aw2_control = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "PCM Capture Route",
	.index = 0,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.private_value = 0xffff,
	.info = snd_aw2_control_switch_capture_info,
	.get = snd_aw2_control_switch_capture_get,
	.put = snd_aw2_control_switch_capture_put
};

 

 
static void snd_aw2_free(struct snd_card *card)
{
	struct aw2 *chip = card->private_data;

	 
	snd_aw2_saa7146_free(&chip->saa7146);
}

 
static int snd_aw2_create(struct snd_card *card,
			  struct pci_dev *pci)
{
	struct aw2 *chip = card->private_data;
	int err;

	 
	err = pcim_enable_device(pci);
	if (err < 0)
		return err;
	pci_set_master(pci);

	 
	if (dma_set_mask_and_coherent(&pci->dev, DMA_BIT_MASK(32))) {
		dev_err(card->dev, "Impossible to set 32bit mask DMA\n");
		return -ENXIO;
	}

	 
	chip->card = card;
	chip->pci = pci;
	chip->irq = -1;

	 
	err = pcim_iomap_regions(pci, 1 << 0, "Audiowerk2");
	if (err < 0)
		return err;
	chip->iobase_phys = pci_resource_start(pci, 0);
	chip->iobase_virt = pcim_iomap_table(pci)[0];

	 
	snd_aw2_saa7146_setup(&chip->saa7146, chip->iobase_virt);

	if (devm_request_irq(&pci->dev, pci->irq, snd_aw2_saa7146_interrupt,
			     IRQF_SHARED, KBUILD_MODNAME, chip)) {
		dev_err(card->dev, "Cannot grab irq %d\n", pci->irq);
		return -EBUSY;
	}
	chip->irq = pci->irq;
	card->sync_irq = chip->irq;
	card->private_free = snd_aw2_free;

	dev_info(card->dev,
		 "Audiowerk 2 sound card (saa7146 chipset) detected and managed\n");
	return 0;
}

 
static int snd_aw2_probe(struct pci_dev *pci,
			 const struct pci_device_id *pci_id)
{
	static int dev;
	struct snd_card *card;
	struct aw2 *chip;
	int err;

	 
	if (dev >= SNDRV_CARDS)
		return -ENODEV;
	if (!enable[dev]) {
		dev++;
		return -ENOENT;
	}

	 
	err = snd_devm_card_new(&pci->dev, index[dev], id[dev], THIS_MODULE,
				sizeof(*chip), &card);
	if (err < 0)
		return err;
	chip = card->private_data;

	 
	err = snd_aw2_create(card, pci);
	if (err < 0)
		goto error;

	 
	mutex_init(&chip->mtx);
	 
	spin_lock_init(&chip->reg_lock);
	 
	strcpy(card->driver, "aw2");
	strcpy(card->shortname, "Audiowerk2");

	sprintf(card->longname, "%s with SAA7146 irq %i",
		card->shortname, chip->irq);

	 
	snd_aw2_new_pcm(chip);

	 
	err = snd_card_register(card);
	if (err < 0)
		goto error;

	 
	pci_set_drvdata(pci, card);

	dev++;
	return 0;

 error:
	snd_card_free(card);
	return err;
}

 
static int snd_aw2_pcm_playback_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	dev_dbg(substream->pcm->card->dev, "Playback_open\n");
	runtime->hw = snd_aw2_playback_hw;
	return 0;
}

 
static int snd_aw2_pcm_playback_close(struct snd_pcm_substream *substream)
{
	return 0;

}

static int snd_aw2_pcm_capture_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	dev_dbg(substream->pcm->card->dev, "Capture_open\n");
	runtime->hw = snd_aw2_capture_hw;
	return 0;
}

 
static int snd_aw2_pcm_capture_close(struct snd_pcm_substream *substream)
{
	 
	return 0;
}

 
static int snd_aw2_pcm_prepare_playback(struct snd_pcm_substream *substream)
{
	struct aw2_pcm_device *pcm_device = snd_pcm_substream_chip(substream);
	struct aw2 *chip = pcm_device->chip;
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned long period_size, buffer_size;

	mutex_lock(&chip->mtx);

	period_size = snd_pcm_lib_period_bytes(substream);
	buffer_size = snd_pcm_lib_buffer_bytes(substream);

	snd_aw2_saa7146_pcm_init_playback(&chip->saa7146,
					  pcm_device->stream_number,
					  runtime->dma_addr, period_size,
					  buffer_size);

	 
	snd_aw2_saa7146_define_it_playback_callback(pcm_device->stream_number,
						    (snd_aw2_saa7146_it_cb)
						    snd_pcm_period_elapsed,
						    (void *)substream);

	mutex_unlock(&chip->mtx);

	return 0;
}

 
static int snd_aw2_pcm_prepare_capture(struct snd_pcm_substream *substream)
{
	struct aw2_pcm_device *pcm_device = snd_pcm_substream_chip(substream);
	struct aw2 *chip = pcm_device->chip;
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned long period_size, buffer_size;

	mutex_lock(&chip->mtx);

	period_size = snd_pcm_lib_period_bytes(substream);
	buffer_size = snd_pcm_lib_buffer_bytes(substream);

	snd_aw2_saa7146_pcm_init_capture(&chip->saa7146,
					 pcm_device->stream_number,
					 runtime->dma_addr, period_size,
					 buffer_size);

	 
	snd_aw2_saa7146_define_it_capture_callback(pcm_device->stream_number,
						   (snd_aw2_saa7146_it_cb)
						   snd_pcm_period_elapsed,
						   (void *)substream);

	mutex_unlock(&chip->mtx);

	return 0;
}

 
static int snd_aw2_pcm_trigger_playback(struct snd_pcm_substream *substream,
					int cmd)
{
	int status = 0;
	struct aw2_pcm_device *pcm_device = snd_pcm_substream_chip(substream);
	struct aw2 *chip = pcm_device->chip;
	spin_lock(&chip->reg_lock);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		snd_aw2_saa7146_pcm_trigger_start_playback(&chip->saa7146,
							   pcm_device->
							   stream_number);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		snd_aw2_saa7146_pcm_trigger_stop_playback(&chip->saa7146,
							  pcm_device->
							  stream_number);
		break;
	default:
		status = -EINVAL;
	}
	spin_unlock(&chip->reg_lock);
	return status;
}

 
static int snd_aw2_pcm_trigger_capture(struct snd_pcm_substream *substream,
				       int cmd)
{
	int status = 0;
	struct aw2_pcm_device *pcm_device = snd_pcm_substream_chip(substream);
	struct aw2 *chip = pcm_device->chip;
	spin_lock(&chip->reg_lock);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		snd_aw2_saa7146_pcm_trigger_start_capture(&chip->saa7146,
							  pcm_device->
							  stream_number);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		snd_aw2_saa7146_pcm_trigger_stop_capture(&chip->saa7146,
							 pcm_device->
							 stream_number);
		break;
	default:
		status = -EINVAL;
	}
	spin_unlock(&chip->reg_lock);
	return status;
}

 
static snd_pcm_uframes_t snd_aw2_pcm_pointer_playback(struct snd_pcm_substream
						      *substream)
{
	struct aw2_pcm_device *pcm_device = snd_pcm_substream_chip(substream);
	struct aw2 *chip = pcm_device->chip;
	unsigned int current_ptr;

	 
	struct snd_pcm_runtime *runtime = substream->runtime;
	current_ptr =
		snd_aw2_saa7146_get_hw_ptr_playback(&chip->saa7146,
						    pcm_device->stream_number,
						    runtime->dma_area,
						    runtime->buffer_size);

	return bytes_to_frames(substream->runtime, current_ptr);
}

 
static snd_pcm_uframes_t snd_aw2_pcm_pointer_capture(struct snd_pcm_substream
						     *substream)
{
	struct aw2_pcm_device *pcm_device = snd_pcm_substream_chip(substream);
	struct aw2 *chip = pcm_device->chip;
	unsigned int current_ptr;

	 
	struct snd_pcm_runtime *runtime = substream->runtime;
	current_ptr =
		snd_aw2_saa7146_get_hw_ptr_capture(&chip->saa7146,
						   pcm_device->stream_number,
						   runtime->dma_area,
						   runtime->buffer_size);

	return bytes_to_frames(substream->runtime, current_ptr);
}

 
static int snd_aw2_new_pcm(struct aw2 *chip)
{
	struct snd_pcm *pcm_playback_ana;
	struct snd_pcm *pcm_playback_num;
	struct snd_pcm *pcm_capture;
	struct aw2_pcm_device *pcm_device;
	int err = 0;

	 

	err = snd_pcm_new(chip->card, "Audiowerk2 analog playback", 0, 1, 0,
			  &pcm_playback_ana);
	if (err < 0) {
		dev_err(chip->card->dev, "snd_pcm_new error (0x%X)\n", err);
		return err;
	}

	 
	pcm_device = &chip->device_playback[NUM_STREAM_PLAYBACK_ANA];

	 
	strcpy(pcm_playback_ana->name, "Analog playback");
	 
	pcm_playback_ana->private_data = pcm_device;
	 
	snd_pcm_set_ops(pcm_playback_ana, SNDRV_PCM_STREAM_PLAYBACK,
			&snd_aw2_playback_ops);
	 
	pcm_device->pcm = pcm_playback_ana;
	 
	pcm_device->chip = chip;
	 
	pcm_device->stream_number = NUM_STREAM_PLAYBACK_ANA;

	 
	 
	snd_pcm_set_managed_buffer_all(pcm_playback_ana,
				       SNDRV_DMA_TYPE_DEV,
				       &chip->pci->dev,
				       64 * 1024, 64 * 1024);

	err = snd_pcm_new(chip->card, "Audiowerk2 digital playback", 1, 1, 0,
			  &pcm_playback_num);

	if (err < 0) {
		dev_err(chip->card->dev, "snd_pcm_new error (0x%X)\n", err);
		return err;
	}
	 
	pcm_device = &chip->device_playback[NUM_STREAM_PLAYBACK_DIG];

	 
	strcpy(pcm_playback_num->name, "Digital playback");
	 
	pcm_playback_num->private_data = pcm_device;
	 
	snd_pcm_set_ops(pcm_playback_num, SNDRV_PCM_STREAM_PLAYBACK,
			&snd_aw2_playback_ops);
	 
	pcm_device->pcm = pcm_playback_num;
	 
	pcm_device->chip = chip;
	 
	pcm_device->stream_number = NUM_STREAM_PLAYBACK_DIG;

	 
	 
	snd_pcm_set_managed_buffer_all(pcm_playback_num,
				       SNDRV_DMA_TYPE_DEV,
				       &chip->pci->dev,
				       64 * 1024, 64 * 1024);

	err = snd_pcm_new(chip->card, "Audiowerk2 capture", 2, 0, 1,
			  &pcm_capture);

	if (err < 0) {
		dev_err(chip->card->dev, "snd_pcm_new error (0x%X)\n", err);
		return err;
	}

	 
	pcm_device = &chip->device_capture[NUM_STREAM_CAPTURE_ANA];

	 
	strcpy(pcm_capture->name, "Capture");
	 
	pcm_capture->private_data = pcm_device;
	 
	snd_pcm_set_ops(pcm_capture, SNDRV_PCM_STREAM_CAPTURE,
			&snd_aw2_capture_ops);
	 
	pcm_device->pcm = pcm_capture;
	 
	pcm_device->chip = chip;
	 
	pcm_device->stream_number = NUM_STREAM_CAPTURE_ANA;

	 
	 
	snd_pcm_set_managed_buffer_all(pcm_capture,
				       SNDRV_DMA_TYPE_DEV,
				       &chip->pci->dev,
				       64 * 1024, 64 * 1024);

	 
	err = snd_ctl_add(chip->card, snd_ctl_new1(&aw2_control, chip));
	if (err < 0) {
		dev_err(chip->card->dev, "snd_ctl_add error (0x%X)\n", err);
		return err;
	}

	return 0;
}

static int snd_aw2_control_switch_capture_info(struct snd_kcontrol *kcontrol,
					       struct snd_ctl_elem_info *uinfo)
{
	static const char * const texts[2] = {
		"Analog", "Digital"
	};
	return snd_ctl_enum_info(uinfo, 1, 2, texts);
}

static int snd_aw2_control_switch_capture_get(struct snd_kcontrol *kcontrol,
					      struct snd_ctl_elem_value
					      *ucontrol)
{
	struct aw2 *chip = snd_kcontrol_chip(kcontrol);
	if (snd_aw2_saa7146_is_using_digital_input(&chip->saa7146))
		ucontrol->value.enumerated.item[0] = CTL_ROUTE_DIGITAL;
	else
		ucontrol->value.enumerated.item[0] = CTL_ROUTE_ANALOG;
	return 0;
}

static int snd_aw2_control_switch_capture_put(struct snd_kcontrol *kcontrol,
					      struct snd_ctl_elem_value
					      *ucontrol)
{
	struct aw2 *chip = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	int is_disgital =
	    snd_aw2_saa7146_is_using_digital_input(&chip->saa7146);

	if (((ucontrol->value.integer.value[0] == CTL_ROUTE_DIGITAL)
	     && !is_disgital)
	    || ((ucontrol->value.integer.value[0] == CTL_ROUTE_ANALOG)
		&& is_disgital)) {
		snd_aw2_saa7146_use_digital_input(&chip->saa7146, !is_disgital);
		changed = 1;
	}
	return changed;
}
