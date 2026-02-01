
 

#include <linux/firmware.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-topology.h>
#include <kunit/test.h>

 

 
static struct device *test_dev;

static struct device_driver test_drv = {
	.name = "sound-soc-topology-test-driver",
};

static int snd_soc_tplg_test_init(struct kunit *test)
{
	test_dev = root_device_register("sound-soc-topology-test");
	test_dev = get_device(test_dev);
	if (!test_dev)
		return -ENODEV;

	test_dev->driver = &test_drv;

	return 0;
}

static void snd_soc_tplg_test_exit(struct kunit *test)
{
	put_device(test_dev);
	root_device_unregister(test_dev);
}

 
struct kunit_soc_component {
	struct kunit *kunit;
	int expect;  
	struct snd_soc_component comp;
	struct snd_soc_card card;
	struct firmware fw;
};

static int d_probe(struct snd_soc_component *component)
{
	struct kunit_soc_component *kunit_comp =
			container_of(component, struct kunit_soc_component, comp);
	int ret;

	ret = snd_soc_tplg_component_load(component, NULL, &kunit_comp->fw);
	KUNIT_EXPECT_EQ_MSG(kunit_comp->kunit, kunit_comp->expect, ret,
			    "Failed topology load");

	return 0;
}

static void d_remove(struct snd_soc_component *component)
{
	struct kunit_soc_component *kunit_comp =
			container_of(component, struct kunit_soc_component, comp);
	int ret;

	ret = snd_soc_tplg_component_remove(component);
	KUNIT_EXPECT_EQ(kunit_comp->kunit, 0, ret);
}

 
SND_SOC_DAILINK_DEF(dummy, DAILINK_COMP_ARRAY(COMP_DUMMY()));

SND_SOC_DAILINK_DEF(platform, DAILINK_COMP_ARRAY(COMP_PLATFORM("sound-soc-topology-test")));

static struct snd_soc_dai_link kunit_dai_links[] = {
	{
		.name = "KUNIT Audio Port",
		.id = 0,
		.stream_name = "Audio Playback/Capture",
		.nonatomic = 1,
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST, SND_SOC_DPCM_TRIGGER_POST},
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(dummy, dummy, platform),
	},
};

static const struct snd_soc_component_driver test_component = {
	.name = "sound-soc-topology-test",
	.probe = d_probe,
	.remove = d_remove,
};

 

 
 
 
 

struct tplg_tmpl_001 {
	struct snd_soc_tplg_hdr header;
	struct snd_soc_tplg_manifest manifest;
} __packed;

static struct tplg_tmpl_001 tplg_tmpl_empty = {
	.header = {
		.magic = cpu_to_le32(SND_SOC_TPLG_MAGIC),
		.abi = cpu_to_le32(5),
		.version = 0,
		.type = cpu_to_le32(SND_SOC_TPLG_TYPE_MANIFEST),
		.size = cpu_to_le32(sizeof(struct snd_soc_tplg_hdr)),
		.vendor_type = 0,
		.payload_size = cpu_to_le32(sizeof(struct snd_soc_tplg_manifest)),
		.index = 0,
		.count = cpu_to_le32(1),
	},

	.manifest = {
		.size = cpu_to_le32(sizeof(struct snd_soc_tplg_manifest)),
		 
	},
};

 

struct tplg_tmpl_002 {
	struct snd_soc_tplg_hdr header;
	struct snd_soc_tplg_manifest manifest;
	struct snd_soc_tplg_hdr pcm_header;
	struct snd_soc_tplg_pcm pcm;
} __packed;

static struct tplg_tmpl_002 tplg_tmpl_with_pcm = {
	.header = {
		.magic = cpu_to_le32(SND_SOC_TPLG_MAGIC),
		.abi = cpu_to_le32(5),
		.version = 0,
		.type = cpu_to_le32(SND_SOC_TPLG_TYPE_MANIFEST),
		.size = cpu_to_le32(sizeof(struct snd_soc_tplg_hdr)),
		.vendor_type = 0,
		.payload_size = cpu_to_le32(sizeof(struct snd_soc_tplg_manifest)),
		.index = 0,
		.count = cpu_to_le32(1),
	},
	.manifest = {
		.size = cpu_to_le32(sizeof(struct snd_soc_tplg_manifest)),
		.pcm_elems = cpu_to_le32(1),
		 
	},
	.pcm_header = {
		.magic = cpu_to_le32(SND_SOC_TPLG_MAGIC),
		.abi = cpu_to_le32(5),
		.version = 0,
		.type = cpu_to_le32(SND_SOC_TPLG_TYPE_PCM),
		.size = cpu_to_le32(sizeof(struct snd_soc_tplg_hdr)),
		.vendor_type = 0,
		.payload_size = cpu_to_le32(sizeof(struct snd_soc_tplg_pcm)),
		.index = 0,
		.count = cpu_to_le32(1),
	},
	.pcm = {
		.size = cpu_to_le32(sizeof(struct snd_soc_tplg_pcm)),
		.pcm_name = "KUNIT Audio",
		.dai_name = "kunit-audio-dai",
		.pcm_id = 0,
		.dai_id = 0,
		.playback = cpu_to_le32(1),
		.capture = cpu_to_le32(1),
		.compress = 0,
		.stream = {
			[0] = {
				.channels = cpu_to_le32(2),
			},
			[1] = {
				.channels = cpu_to_le32(2),
			},
		},
		.num_streams = 0,
		.caps = {
			[0] = {
				.name = "kunit-audio-playback",
				.channels_min = cpu_to_le32(2),
				.channels_max = cpu_to_le32(2),
			},
			[1] = {
				.name = "kunit-audio-capture",
				.channels_min = cpu_to_le32(2),
				.channels_max = cpu_to_le32(2),
			},
		},
		.flag_mask = 0,
		.flags = 0,
		.priv = { 0 },
	},
};

 

 
 

 
static int d_probe_null_comp(struct snd_soc_component *component)
{
	struct kunit_soc_component *kunit_comp =
			container_of(component, struct kunit_soc_component, comp);
	int ret;

	 
	ret = snd_soc_tplg_component_load(NULL, NULL, &kunit_comp->fw);
	KUNIT_EXPECT_EQ_MSG(kunit_comp->kunit, kunit_comp->expect, ret,
			    "Failed topology load");

	return 0;
}

static const struct snd_soc_component_driver test_component_null_comp = {
	.name = "sound-soc-topology-test",
	.probe = d_probe_null_comp,
};

static void snd_soc_tplg_test_load_with_null_comp(struct kunit *test)
{
	struct kunit_soc_component *kunit_comp;
	int ret;

	 
	kunit_comp = kunit_kzalloc(test, sizeof(*kunit_comp), GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_comp);
	kunit_comp->kunit = test;
	kunit_comp->expect = -EINVAL;  

	kunit_comp->card.dev = test_dev,
	kunit_comp->card.name = "kunit-card",
	kunit_comp->card.owner = THIS_MODULE,
	kunit_comp->card.dai_link = kunit_dai_links,
	kunit_comp->card.num_links = ARRAY_SIZE(kunit_dai_links),
	kunit_comp->card.fully_routed = true,

	 
	ret = snd_soc_register_card(&kunit_comp->card);
	if (ret != 0 && ret != -EPROBE_DEFER)
		KUNIT_FAIL(test, "Failed to register card");

	ret = snd_soc_component_initialize(&kunit_comp->comp, &test_component_null_comp, test_dev);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = snd_soc_add_component(&kunit_comp->comp, NULL, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	 
	snd_soc_unregister_card(&kunit_comp->card);
	snd_soc_unregister_component(test_dev);
}




 
static void snd_soc_tplg_test_load_with_null_ops(struct kunit *test)
{
	struct kunit_soc_component *kunit_comp;
	int ret;

	 
	kunit_comp = kunit_kzalloc(test, sizeof(*kunit_comp), GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_comp);
	kunit_comp->kunit = test;
	kunit_comp->expect = 0;  

	kunit_comp->card.dev = test_dev,
	kunit_comp->card.name = "kunit-card",
	kunit_comp->card.owner = THIS_MODULE,
	kunit_comp->card.dai_link = kunit_dai_links,
	kunit_comp->card.num_links = ARRAY_SIZE(kunit_dai_links),
	kunit_comp->card.fully_routed = true,

	 
	ret = snd_soc_register_card(&kunit_comp->card);
	if (ret != 0 && ret != -EPROBE_DEFER)
		KUNIT_FAIL(test, "Failed to register card");

	ret = snd_soc_component_initialize(&kunit_comp->comp, &test_component, test_dev);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = snd_soc_add_component(&kunit_comp->comp, NULL, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	 
	snd_soc_unregister_card(&kunit_comp->card);

	snd_soc_unregister_component(test_dev);
}

 
 

 
static int d_probe_null_fw(struct snd_soc_component *component)
{
	struct kunit_soc_component *kunit_comp =
			container_of(component, struct kunit_soc_component, comp);
	int ret;

	 
	ret = snd_soc_tplg_component_load(component, NULL, NULL);
	KUNIT_EXPECT_EQ_MSG(kunit_comp->kunit, kunit_comp->expect, ret,
			    "Failed topology load");

	return 0;
}

static const struct snd_soc_component_driver test_component_null_fw = {
	.name = "sound-soc-topology-test",
	.probe = d_probe_null_fw,
};

static void snd_soc_tplg_test_load_with_null_fw(struct kunit *test)
{
	struct kunit_soc_component *kunit_comp;
	int ret;

	 
	kunit_comp = kunit_kzalloc(test, sizeof(*kunit_comp), GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_comp);
	kunit_comp->kunit = test;
	kunit_comp->expect = -EINVAL;  

	kunit_comp->card.dev = test_dev,
	kunit_comp->card.name = "kunit-card",
	kunit_comp->card.owner = THIS_MODULE,
	kunit_comp->card.dai_link = kunit_dai_links,
	kunit_comp->card.num_links = ARRAY_SIZE(kunit_dai_links),
	kunit_comp->card.fully_routed = true,

	 
	ret = snd_soc_register_card(&kunit_comp->card);
	if (ret != 0 && ret != -EPROBE_DEFER)
		KUNIT_FAIL(test, "Failed to register card");

	ret = snd_soc_component_initialize(&kunit_comp->comp, &test_component_null_fw, test_dev);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = snd_soc_add_component(&kunit_comp->comp, NULL, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	 
	snd_soc_unregister_card(&kunit_comp->card);

	snd_soc_unregister_component(test_dev);
}



static void snd_soc_tplg_test_load_empty_tplg(struct kunit *test)
{
	struct kunit_soc_component *kunit_comp;
	struct tplg_tmpl_001 *data;
	int size;
	int ret;

	 
	kunit_comp = kunit_kzalloc(test, sizeof(*kunit_comp), GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_comp);
	kunit_comp->kunit = test;
	kunit_comp->expect = 0;  

	size = sizeof(tplg_tmpl_empty);
	data = kunit_kzalloc(kunit_comp->kunit, size, GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(kunit_comp->kunit, data);

	memcpy(data, &tplg_tmpl_empty, sizeof(tplg_tmpl_empty));

	kunit_comp->fw.data = (u8 *)data;
	kunit_comp->fw.size = size;

	kunit_comp->card.dev = test_dev,
	kunit_comp->card.name = "kunit-card",
	kunit_comp->card.owner = THIS_MODULE,
	kunit_comp->card.dai_link = kunit_dai_links,
	kunit_comp->card.num_links = ARRAY_SIZE(kunit_dai_links),
	kunit_comp->card.fully_routed = true,

	 
	ret = snd_soc_register_card(&kunit_comp->card);
	if (ret != 0 && ret != -EPROBE_DEFER)
		KUNIT_FAIL(test, "Failed to register card");

	ret = snd_soc_component_initialize(&kunit_comp->comp, &test_component, test_dev);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = snd_soc_add_component(&kunit_comp->comp, NULL, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	 
	snd_soc_unregister_card(&kunit_comp->card);

	snd_soc_unregister_component(test_dev);
}





static void snd_soc_tplg_test_load_empty_tplg_bad_magic(struct kunit *test)
{
	struct kunit_soc_component *kunit_comp;
	struct tplg_tmpl_001 *data;
	int size;
	int ret;

	 
	kunit_comp = kunit_kzalloc(test, sizeof(*kunit_comp), GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_comp);
	kunit_comp->kunit = test;
	kunit_comp->expect = -EINVAL;  

	size = sizeof(tplg_tmpl_empty);
	data = kunit_kzalloc(kunit_comp->kunit, size, GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(kunit_comp->kunit, data);

	memcpy(data, &tplg_tmpl_empty, sizeof(tplg_tmpl_empty));
	 
	data->header.magic = cpu_to_le32(SND_SOC_TPLG_MAGIC + 1);

	kunit_comp->fw.data = (u8 *)data;
	kunit_comp->fw.size = size;

	kunit_comp->card.dev = test_dev,
	kunit_comp->card.name = "kunit-card",
	kunit_comp->card.owner = THIS_MODULE,
	kunit_comp->card.dai_link = kunit_dai_links,
	kunit_comp->card.num_links = ARRAY_SIZE(kunit_dai_links),
	kunit_comp->card.fully_routed = true,

	 
	ret = snd_soc_register_card(&kunit_comp->card);
	if (ret != 0 && ret != -EPROBE_DEFER)
		KUNIT_FAIL(test, "Failed to register card");

	ret = snd_soc_component_initialize(&kunit_comp->comp, &test_component, test_dev);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = snd_soc_add_component(&kunit_comp->comp, NULL, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	 
	snd_soc_unregister_card(&kunit_comp->card);

	snd_soc_unregister_component(test_dev);
}





static void snd_soc_tplg_test_load_empty_tplg_bad_abi(struct kunit *test)
{
	struct kunit_soc_component *kunit_comp;
	struct tplg_tmpl_001 *data;
	int size;
	int ret;

	 
	kunit_comp = kunit_kzalloc(test, sizeof(*kunit_comp), GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_comp);
	kunit_comp->kunit = test;
	kunit_comp->expect = -EINVAL;  

	size = sizeof(tplg_tmpl_empty);
	data = kunit_kzalloc(kunit_comp->kunit, size, GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(kunit_comp->kunit, data);

	memcpy(data, &tplg_tmpl_empty, sizeof(tplg_tmpl_empty));
	 
	data->header.abi = cpu_to_le32(SND_SOC_TPLG_ABI_VERSION + 1);

	kunit_comp->fw.data = (u8 *)data;
	kunit_comp->fw.size = size;

	kunit_comp->card.dev = test_dev,
	kunit_comp->card.name = "kunit-card",
	kunit_comp->card.owner = THIS_MODULE,
	kunit_comp->card.dai_link = kunit_dai_links,
	kunit_comp->card.num_links = ARRAY_SIZE(kunit_dai_links),
	kunit_comp->card.fully_routed = true,

	 
	ret = snd_soc_register_card(&kunit_comp->card);
	if (ret != 0 && ret != -EPROBE_DEFER)
		KUNIT_FAIL(test, "Failed to register card");

	ret = snd_soc_component_initialize(&kunit_comp->comp, &test_component, test_dev);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = snd_soc_add_component(&kunit_comp->comp, NULL, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	 
	snd_soc_unregister_card(&kunit_comp->card);

	snd_soc_unregister_component(test_dev);
}





static void snd_soc_tplg_test_load_empty_tplg_bad_size(struct kunit *test)
{
	struct kunit_soc_component *kunit_comp;
	struct tplg_tmpl_001 *data;
	int size;
	int ret;

	 
	kunit_comp = kunit_kzalloc(test, sizeof(*kunit_comp), GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_comp);
	kunit_comp->kunit = test;
	kunit_comp->expect = -EINVAL;  

	size = sizeof(tplg_tmpl_empty);
	data = kunit_kzalloc(kunit_comp->kunit, size, GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(kunit_comp->kunit, data);

	memcpy(data, &tplg_tmpl_empty, sizeof(tplg_tmpl_empty));
	 
	data->header.size = cpu_to_le32(sizeof(struct snd_soc_tplg_hdr) + 1);

	kunit_comp->fw.data = (u8 *)data;
	kunit_comp->fw.size = size;

	kunit_comp->card.dev = test_dev,
	kunit_comp->card.name = "kunit-card",
	kunit_comp->card.owner = THIS_MODULE,
	kunit_comp->card.dai_link = kunit_dai_links,
	kunit_comp->card.num_links = ARRAY_SIZE(kunit_dai_links),
	kunit_comp->card.fully_routed = true,

	 
	ret = snd_soc_register_card(&kunit_comp->card);
	if (ret != 0 && ret != -EPROBE_DEFER)
		KUNIT_FAIL(test, "Failed to register card");

	ret = snd_soc_component_initialize(&kunit_comp->comp, &test_component, test_dev);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = snd_soc_add_component(&kunit_comp->comp, NULL, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	 
	snd_soc_unregister_card(&kunit_comp->card);

	snd_soc_unregister_component(test_dev);
}





static void snd_soc_tplg_test_load_empty_tplg_bad_payload_size(struct kunit *test)
{
	struct kunit_soc_component *kunit_comp;
	struct tplg_tmpl_001 *data;
	int size;
	int ret;

	 
	kunit_comp = kunit_kzalloc(test, sizeof(*kunit_comp), GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_comp);
	kunit_comp->kunit = test;
	kunit_comp->expect = -EINVAL;  

	size = sizeof(tplg_tmpl_empty);
	data = kunit_kzalloc(kunit_comp->kunit, size, GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(kunit_comp->kunit, data);

	memcpy(data, &tplg_tmpl_empty, sizeof(tplg_tmpl_empty));
	 
	data->header.payload_size = 0;

	kunit_comp->fw.data = (u8 *)data;
	kunit_comp->fw.size = size;

	kunit_comp->card.dev = test_dev,
	kunit_comp->card.name = "kunit-card",
	kunit_comp->card.owner = THIS_MODULE,
	kunit_comp->card.dai_link = kunit_dai_links,
	kunit_comp->card.num_links = ARRAY_SIZE(kunit_dai_links),
	kunit_comp->card.fully_routed = true,

	 
	ret = snd_soc_register_card(&kunit_comp->card);
	if (ret != 0 && ret != -EPROBE_DEFER)
		KUNIT_FAIL(test, "Failed to register card");

	ret = snd_soc_component_initialize(&kunit_comp->comp, &test_component, test_dev);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = snd_soc_add_component(&kunit_comp->comp, NULL, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	 
	snd_soc_unregister_component(test_dev);

	snd_soc_unregister_card(&kunit_comp->card);
}



static void snd_soc_tplg_test_load_pcm_tplg(struct kunit *test)
{
	struct kunit_soc_component *kunit_comp;
	u8 *data;
	int size;
	int ret;

	 
	kunit_comp = kunit_kzalloc(test, sizeof(*kunit_comp), GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_comp);
	kunit_comp->kunit = test;
	kunit_comp->expect = 0;  

	size = sizeof(tplg_tmpl_with_pcm);
	data = kunit_kzalloc(kunit_comp->kunit, size, GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(kunit_comp->kunit, data);

	memcpy(data, &tplg_tmpl_with_pcm, sizeof(tplg_tmpl_with_pcm));

	kunit_comp->fw.data = data;
	kunit_comp->fw.size = size;

	kunit_comp->card.dev = test_dev,
	kunit_comp->card.name = "kunit-card",
	kunit_comp->card.owner = THIS_MODULE,
	kunit_comp->card.dai_link = kunit_dai_links,
	kunit_comp->card.num_links = ARRAY_SIZE(kunit_dai_links),
	kunit_comp->card.fully_routed = true,

	 
	ret = snd_soc_register_card(&kunit_comp->card);
	if (ret != 0 && ret != -EPROBE_DEFER)
		KUNIT_FAIL(test, "Failed to register card");

	ret = snd_soc_component_initialize(&kunit_comp->comp, &test_component, test_dev);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = snd_soc_add_component(&kunit_comp->comp, NULL, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	snd_soc_unregister_component(test_dev);

	 
	snd_soc_unregister_card(&kunit_comp->card);
}




static void snd_soc_tplg_test_load_pcm_tplg_reload_comp(struct kunit *test)
{
	struct kunit_soc_component *kunit_comp;
	u8 *data;
	int size;
	int ret;
	int i;

	 
	kunit_comp = kunit_kzalloc(test, sizeof(*kunit_comp), GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_comp);
	kunit_comp->kunit = test;
	kunit_comp->expect = 0;  

	size = sizeof(tplg_tmpl_with_pcm);
	data = kunit_kzalloc(kunit_comp->kunit, size, GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(kunit_comp->kunit, data);

	memcpy(data, &tplg_tmpl_with_pcm, sizeof(tplg_tmpl_with_pcm));

	kunit_comp->fw.data = data;
	kunit_comp->fw.size = size;

	kunit_comp->card.dev = test_dev,
	kunit_comp->card.name = "kunit-card",
	kunit_comp->card.owner = THIS_MODULE,
	kunit_comp->card.dai_link = kunit_dai_links,
	kunit_comp->card.num_links = ARRAY_SIZE(kunit_dai_links),
	kunit_comp->card.fully_routed = true,

	 
	ret = snd_soc_register_card(&kunit_comp->card);
	if (ret != 0 && ret != -EPROBE_DEFER)
		KUNIT_FAIL(test, "Failed to register card");

	for (i = 0; i < 100; i++) {
		ret = snd_soc_component_initialize(&kunit_comp->comp, &test_component, test_dev);
		KUNIT_EXPECT_EQ(test, 0, ret);

		ret = snd_soc_add_component(&kunit_comp->comp, NULL, 0);
		KUNIT_EXPECT_EQ(test, 0, ret);

		snd_soc_unregister_component(test_dev);
	}

	 
	snd_soc_unregister_card(&kunit_comp->card);
}




static void snd_soc_tplg_test_load_pcm_tplg_reload_card(struct kunit *test)
{
	struct kunit_soc_component *kunit_comp;
	u8 *data;
	int size;
	int ret;
	int i;

	 
	kunit_comp = kunit_kzalloc(test, sizeof(*kunit_comp), GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_comp);
	kunit_comp->kunit = test;
	kunit_comp->expect = 0;  

	size = sizeof(tplg_tmpl_with_pcm);
	data = kunit_kzalloc(kunit_comp->kunit, size, GFP_KERNEL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(kunit_comp->kunit, data);

	memcpy(data, &tplg_tmpl_with_pcm, sizeof(tplg_tmpl_with_pcm));

	kunit_comp->fw.data = data;
	kunit_comp->fw.size = size;

	kunit_comp->card.dev = test_dev,
	kunit_comp->card.name = "kunit-card",
	kunit_comp->card.owner = THIS_MODULE,
	kunit_comp->card.dai_link = kunit_dai_links,
	kunit_comp->card.num_links = ARRAY_SIZE(kunit_dai_links),
	kunit_comp->card.fully_routed = true,

	 
	ret = snd_soc_component_initialize(&kunit_comp->comp, &test_component, test_dev);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = snd_soc_add_component(&kunit_comp->comp, NULL, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	for (i = 0; i < 100; i++) {
		ret = snd_soc_register_card(&kunit_comp->card);
		if (ret != 0 && ret != -EPROBE_DEFER)
			KUNIT_FAIL(test, "Failed to register card");

		snd_soc_unregister_card(&kunit_comp->card);
	}

	 
	snd_soc_unregister_component(test_dev);
}

 

static struct kunit_case snd_soc_tplg_test_cases[] = {
	KUNIT_CASE(snd_soc_tplg_test_load_with_null_comp),
	KUNIT_CASE(snd_soc_tplg_test_load_with_null_ops),
	KUNIT_CASE(snd_soc_tplg_test_load_with_null_fw),
	KUNIT_CASE(snd_soc_tplg_test_load_empty_tplg),
	KUNIT_CASE(snd_soc_tplg_test_load_empty_tplg_bad_magic),
	KUNIT_CASE(snd_soc_tplg_test_load_empty_tplg_bad_abi),
	KUNIT_CASE(snd_soc_tplg_test_load_empty_tplg_bad_size),
	KUNIT_CASE(snd_soc_tplg_test_load_empty_tplg_bad_payload_size),
	KUNIT_CASE(snd_soc_tplg_test_load_pcm_tplg),
	KUNIT_CASE(snd_soc_tplg_test_load_pcm_tplg_reload_comp),
	KUNIT_CASE(snd_soc_tplg_test_load_pcm_tplg_reload_card),
	{}
};

static struct kunit_suite snd_soc_tplg_test_suite = {
	.name = "snd_soc_tplg_test",
	.init = snd_soc_tplg_test_init,
	.exit = snd_soc_tplg_test_exit,
	.test_cases = snd_soc_tplg_test_cases,
};

kunit_test_suites(&snd_soc_tplg_test_suite);

MODULE_LICENSE("GPL");
