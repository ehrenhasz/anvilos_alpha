
 

 

#include "bebob.h"

MODULE_DESCRIPTION("BridgeCo BeBoB driver");
MODULE_AUTHOR("Takashi Sakamoto <o-takashi@sakamocchi.jp>");
MODULE_LICENSE("GPL");

static int index[SNDRV_CARDS]	= SNDRV_DEFAULT_IDX;
static char *id[SNDRV_CARDS]	= SNDRV_DEFAULT_STR;
static bool enable[SNDRV_CARDS]	= SNDRV_DEFAULT_ENABLE_PNP;

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "card index");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string");
module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "enable BeBoB sound card");

static DEFINE_MUTEX(devices_mutex);
static DECLARE_BITMAP(devices_used, SNDRV_CARDS);

 
#define INFO_OFFSET_BEBOB_VERSION	0x08
#define INFO_OFFSET_GUID		0x10
#define INFO_OFFSET_HW_MODEL_ID		0x18
#define INFO_OFFSET_HW_MODEL_REVISION	0x1c

#define VEN_EDIROL	0x000040ab
#define VEN_PRESONUS	0x00000a92
#define VEN_BRIDGECO	0x000007f5
#define VEN_MACKIE	0x00000ff2
#define VEN_STANTON	0x00001260
#define VEN_TASCAM	0x0000022e
#define VEN_BEHRINGER	0x00001564
#define VEN_APOGEE	0x000003db
#define VEN_ESI		0x00000f1b
#define VEN_CME		0x0000000a
#define VEN_PHONIC	0x00001496
#define VEN_LYNX	0x000019e5
#define VEN_ICON	0x00001a9e
#define VEN_PRISMSOUND	0x00001198
#define VEN_TERRATEC	0x00000aac
#define VEN_YAMAHA	0x0000a0de
#define VEN_FOCUSRITE	0x0000130e
#define VEN_MAUDIO	0x00000d6c
#define VEN_DIGIDESIGN	0x00a07e
#define OUI_SHOUYO	0x002327

#define MODEL_FOCUSRITE_SAFFIRE_BOTH	0x00000000
#define MODEL_MAUDIO_AUDIOPHILE_BOTH	0x00010060
#define MODEL_MAUDIO_FW1814		0x00010071
#define MODEL_MAUDIO_PROJECTMIX		0x00010091
#define MODEL_MAUDIO_PROFIRELIGHTBRIDGE	0x000100a1

static int
name_device(struct snd_bebob *bebob)
{
	struct fw_device *fw_dev = fw_parent_device(bebob->unit);
	char vendor[24] = {0};
	char model[32] = {0};
	u32 hw_id;
	u32 data[2] = {0};
	u32 revision;
	int err;

	 
	err = fw_csr_string(fw_dev->config_rom + 5, CSR_VENDOR,
			    vendor, sizeof(vendor));
	if (err < 0)
		goto end;

	 
	err = fw_csr_string(bebob->unit->directory, CSR_MODEL,
			    model, sizeof(model));
	if (err < 0)
		goto end;

	 
	err = snd_bebob_read_quad(bebob->unit, INFO_OFFSET_HW_MODEL_ID,
				  &hw_id);
	if (err < 0)
		goto end;

	 
	err = snd_bebob_read_quad(bebob->unit, INFO_OFFSET_HW_MODEL_REVISION,
				  &revision);
	if (err < 0)
		goto end;

	 
	err = snd_bebob_read_block(bebob->unit, INFO_OFFSET_GUID,
				   data, sizeof(data));
	if (err < 0)
		goto end;

	strcpy(bebob->card->driver, "BeBoB");
	strcpy(bebob->card->shortname, model);
	strcpy(bebob->card->mixername, model);
	snprintf(bebob->card->longname, sizeof(bebob->card->longname),
		 "%s %s (id:%d, rev:%d), GUID %08x%08x at %s, S%d",
		 vendor, model, hw_id, revision,
		 data[0], data[1], dev_name(&bebob->unit->device),
		 100 << fw_dev->max_speed);
end:
	return err;
}

static void
bebob_card_free(struct snd_card *card)
{
	struct snd_bebob *bebob = card->private_data;

	mutex_lock(&devices_mutex);
	clear_bit(bebob->card_index, devices_used);
	mutex_unlock(&devices_mutex);

	snd_bebob_stream_destroy_duplex(bebob);

	mutex_destroy(&bebob->mutex);
	fw_unit_put(bebob->unit);
}

static const struct snd_bebob_spec *
get_saffire_spec(struct fw_unit *unit)
{
	char name[24] = {0};

	if (fw_csr_string(unit->directory, CSR_MODEL, name, sizeof(name)) < 0)
		return NULL;

	if (strcmp(name, "SaffireLE") == 0)
		return &saffire_le_spec;
	else
		return &saffire_spec;
}

static bool
check_audiophile_booted(struct fw_unit *unit)
{
	char name[28] = {0};

	if (fw_csr_string(unit->directory, CSR_MODEL, name, sizeof(name)) < 0)
		return false;

	return strncmp(name, "FW Audiophile Bootloader", 24) != 0;
}

static int detect_quirks(struct snd_bebob *bebob, const struct ieee1394_device_id *entry)
{
	if (entry->vendor_id == VEN_MAUDIO) {
		switch (entry->model_id) {
		case MODEL_MAUDIO_PROFIRELIGHTBRIDGE:
			
			
			
			bebob->quirks |= SND_BEBOB_QUIRK_INITIAL_DISCONTINUOUS_DBC;
			break;
		case MODEL_MAUDIO_FW1814:
		case MODEL_MAUDIO_PROJECTMIX:
			
			
			bebob->quirks |= SND_BEBOB_QUIRK_WRONG_DBC;
			break;
		default:
			break;
		}
	}

	return 0;
}

static int bebob_probe(struct fw_unit *unit, const struct ieee1394_device_id *entry)
{
	unsigned int card_index;
	struct snd_card *card;
	struct snd_bebob *bebob;
	const struct snd_bebob_spec *spec;
	int err;

	if (entry->vendor_id == VEN_FOCUSRITE &&
	    entry->model_id == MODEL_FOCUSRITE_SAFFIRE_BOTH)
		spec = get_saffire_spec(unit);
	else if (entry->vendor_id == VEN_MAUDIO &&
		 entry->model_id == MODEL_MAUDIO_AUDIOPHILE_BOTH &&
		 !check_audiophile_booted(unit))
		spec = NULL;
	else
		spec = (const struct snd_bebob_spec *)entry->driver_data;

	if (spec == NULL) {
		
		if (entry->vendor_id == VEN_MAUDIO || entry->vendor_id == VEN_BRIDGECO)
			return snd_bebob_maudio_load_firmware(unit);
		else
			return -ENODEV;
	}

	mutex_lock(&devices_mutex);
	for (card_index = 0; card_index < SNDRV_CARDS; card_index++) {
		if (!test_bit(card_index, devices_used) && enable[card_index])
			break;
	}
	if (card_index >= SNDRV_CARDS) {
		mutex_unlock(&devices_mutex);
		return -ENOENT;
	}

	err = snd_card_new(&unit->device, index[card_index], id[card_index], THIS_MODULE,
			   sizeof(*bebob), &card);
	if (err < 0) {
		mutex_unlock(&devices_mutex);
		return err;
	}
	card->private_free = bebob_card_free;
	set_bit(card_index, devices_used);
	mutex_unlock(&devices_mutex);

	bebob = card->private_data;
	bebob->unit = fw_unit_get(unit);
	dev_set_drvdata(&unit->device, bebob);
	bebob->card = card;
	bebob->card_index = card_index;

	bebob->spec = spec;
	mutex_init(&bebob->mutex);
	spin_lock_init(&bebob->lock);
	init_waitqueue_head(&bebob->hwdep_wait);

	err = name_device(bebob);
	if (err < 0)
		goto error;

	err = detect_quirks(bebob, entry);
	if (err < 0)
		goto error;

	if (bebob->spec == &maudio_special_spec) {
		if (entry->model_id == MODEL_MAUDIO_FW1814)
			err = snd_bebob_maudio_special_discover(bebob, true);
		else
			err = snd_bebob_maudio_special_discover(bebob, false);
	} else {
		err = snd_bebob_stream_discover(bebob);
	}
	if (err < 0)
		goto error;

	err = snd_bebob_stream_init_duplex(bebob);
	if (err < 0)
		goto error;

	snd_bebob_proc_init(bebob);

	if (bebob->midi_input_ports > 0 || bebob->midi_output_ports > 0) {
		err = snd_bebob_create_midi_devices(bebob);
		if (err < 0)
			goto error;
	}

	err = snd_bebob_create_pcm_devices(bebob);
	if (err < 0)
		goto error;

	err = snd_bebob_create_hwdep_device(bebob);
	if (err < 0)
		goto error;

	err = snd_card_register(card);
	if (err < 0)
		goto error;

	if (entry->vendor_id == VEN_MAUDIO &&
	    (entry->model_id == MODEL_MAUDIO_FW1814 || entry->model_id == MODEL_MAUDIO_PROJECTMIX)) {
		
		
		
		
		
		
		
		fw_schedule_bus_reset(fw_parent_device(bebob->unit)->card, false, true);
	}

	return 0;
error:
	snd_card_free(card);
	return err;
}

 
static void
bebob_update(struct fw_unit *unit)
{
	struct snd_bebob *bebob = dev_get_drvdata(&unit->device);

	if (bebob == NULL)
		return;

	fcp_bus_reset(bebob->unit);
}

static void bebob_remove(struct fw_unit *unit)
{
	struct snd_bebob *bebob = dev_get_drvdata(&unit->device);

	if (bebob == NULL)
		return;

	
	snd_card_free(bebob->card);
}

static const struct snd_bebob_rate_spec normal_rate_spec = {
	.get	= &snd_bebob_stream_get_rate,
	.set	= &snd_bebob_stream_set_rate
};
static const struct snd_bebob_spec spec_normal = {
	.clock	= NULL,
	.rate	= &normal_rate_spec,
	.meter	= NULL
};

#define SPECIFIER_1394TA	0x00a02d




#define SND_BEBOB_DEV_ENTRY(vendor, model, data) \
{ \
	.match_flags	= IEEE1394_MATCH_VENDOR_ID | \
			  IEEE1394_MATCH_MODEL_ID | \
			  IEEE1394_MATCH_SPECIFIER_ID, \
	.vendor_id	= vendor, \
	.model_id	= model, \
	.specifier_id	= SPECIFIER_1394TA, \
	.driver_data	= (kernel_ulong_t)data \
}

static const struct ieee1394_device_id bebob_id_table[] = {
	 
	SND_BEBOB_DEV_ENTRY(VEN_EDIROL, 0x00010049, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_EDIROL, 0x00010048, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_PRESONUS, 0x00010000, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_PRESONUS, 0x00010066, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_PRESONUS, 0x00010001, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_BRIDGECO, 0x00010048, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_BRIDGECO, 0x00010049, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_MACKIE, 0x00010065, &spec_normal),
	
	SND_BEBOB_DEV_ENTRY(VEN_MACKIE, 0x00010067, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_STANTON, 0x00000001, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_TASCAM, 0x00010067, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_BEHRINGER, 0x00001204, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_BEHRINGER, 0x00001604, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_BEHRINGER, 0x00000006, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_BEHRINGER, 0x001616, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_BEHRINGER, 0x000610, &spec_normal),
	 
	 
	SND_BEBOB_DEV_ENTRY(VEN_APOGEE, 0x00010048, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_APOGEE, 0x01eeee, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_ESI, 0x00010064, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_CME, 0x00030000, &spec_normal),
	
	SND_BEBOB_DEV_ENTRY(VEN_PHONIC, 0x00050000, &spec_normal),
	
	SND_BEBOB_DEV_ENTRY(VEN_PHONIC, 0x00060000, &spec_normal),
	
	SND_BEBOB_DEV_ENTRY(VEN_PHONIC, 0x00070000, &spec_normal),
	
	SND_BEBOB_DEV_ENTRY(VEN_PHONIC, 0x00080000, &spec_normal),
	
	
	SND_BEBOB_DEV_ENTRY(VEN_PHONIC, 0x00000000, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_LYNX, 0x00000001, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_ICON, 0x00000001, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_PRISMSOUND, 0x00010048, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_PRISMSOUND, 0x0000ada8, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_TERRATEC, 0x00000003, &phase88_rack_spec),
	 
	SND_BEBOB_DEV_ENTRY(VEN_TERRATEC, 0x00000004, &yamaha_terratec_spec),
	 
	SND_BEBOB_DEV_ENTRY(VEN_TERRATEC, 0x00000007, &yamaha_terratec_spec),
	 
	SND_BEBOB_DEV_ENTRY(VEN_TERRATEC, 0x00000005, &spec_normal),
	
	
	SND_BEBOB_DEV_ENTRY(VEN_TERRATEC, 0x00000002, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_YAMAHA, 0x0010000b, &yamaha_terratec_spec),
	 
	SND_BEBOB_DEV_ENTRY(VEN_YAMAHA, 0x0010000c, &yamaha_terratec_spec),
	 
	SND_BEBOB_DEV_ENTRY(VEN_FOCUSRITE, 0x00000003, &saffirepro_26_spec),
	 
	SND_BEBOB_DEV_ENTRY(VEN_FOCUSRITE, 0x000006, &saffirepro_10_spec),
	 
	SND_BEBOB_DEV_ENTRY(VEN_FOCUSRITE, MODEL_FOCUSRITE_SAFFIRE_BOTH,
			    &saffire_spec),
	
	SND_BEBOB_DEV_ENTRY(VEN_BRIDGECO, 0x00010058, NULL),
	SND_BEBOB_DEV_ENTRY(VEN_BRIDGECO, 0x00010046, &maudio_fw410_spec),
	 
	SND_BEBOB_DEV_ENTRY(VEN_MAUDIO, MODEL_MAUDIO_AUDIOPHILE_BOTH,
			    &maudio_audiophile_spec),
	 
	SND_BEBOB_DEV_ENTRY(VEN_MAUDIO, 0x00010062, &maudio_solo_spec),
	 
	SND_BEBOB_DEV_ENTRY(VEN_MAUDIO, 0x0000000a, &maudio_ozonic_spec),
	 
	SND_BEBOB_DEV_ENTRY(VEN_MAUDIO, 0x00010081, &maudio_nrv10_spec),
	 
	SND_BEBOB_DEV_ENTRY(VEN_MAUDIO, MODEL_MAUDIO_PROFIRELIGHTBRIDGE, &spec_normal),
	 
	SND_BEBOB_DEV_ENTRY(VEN_MAUDIO, 0x00010070, NULL),	 
	SND_BEBOB_DEV_ENTRY(VEN_MAUDIO, MODEL_MAUDIO_FW1814,
			    &maudio_special_spec),
	 
	SND_BEBOB_DEV_ENTRY(VEN_MAUDIO, MODEL_MAUDIO_PROJECTMIX,
			    &maudio_special_spec),
	 
	SND_BEBOB_DEV_ENTRY(VEN_DIGIDESIGN, 0x0000a9, &spec_normal),
	
	SND_BEBOB_DEV_ENTRY(OUI_SHOUYO, 0x020002, &spec_normal),
	 
	 
	 
	 
	 
	 
	 
	 
	 
	 
	 
	{}
};
MODULE_DEVICE_TABLE(ieee1394, bebob_id_table);

static struct fw_driver bebob_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= KBUILD_MODNAME,
		.bus	= &fw_bus_type,
	},
	.probe    = bebob_probe,
	.update	  = bebob_update,
	.remove   = bebob_remove,
	.id_table = bebob_id_table,
};

static int __init
snd_bebob_init(void)
{
	return driver_register(&bebob_driver.driver);
}

static void __exit
snd_bebob_exit(void)
{
	driver_unregister(&bebob_driver.driver);
}

module_init(snd_bebob_init);
module_exit(snd_bebob_exit);
