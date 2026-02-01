
 

#include <media/rc-map.h>
#include <linux/module.h>

 
 
static struct rc_map_table avermedia_rm_ks[] = {
	{ 0x0501, KEY_POWER2 },  
	{ 0x0502, KEY_CHANNELUP },  
	{ 0x0503, KEY_CHANNELDOWN },  
	{ 0x0504, KEY_VOLUMEUP },  
	{ 0x0505, KEY_VOLUMEDOWN },  
	{ 0x0506, KEY_MUTE },  
	{ 0x0507, KEY_AGAIN },  
	{ 0x0508, KEY_VIDEO },  
	{ 0x0509, KEY_NUMERIC_1 },  
	{ 0x050a, KEY_NUMERIC_2 },  
	{ 0x050b, KEY_NUMERIC_3 },  
	{ 0x050c, KEY_NUMERIC_4 },  
	{ 0x050d, KEY_NUMERIC_5 },  
	{ 0x050e, KEY_NUMERIC_6 },  
	{ 0x050f, KEY_NUMERIC_7 },  
	{ 0x0510, KEY_NUMERIC_8 },  
	{ 0x0511, KEY_NUMERIC_9 },  
	{ 0x0512, KEY_NUMERIC_0 },  
	{ 0x0513, KEY_AUDIO },  
	{ 0x0515, KEY_EPG },  
	{ 0x0516, KEY_PLAYPAUSE },  
	{ 0x0517, KEY_RECORD },  
	{ 0x0518, KEY_STOP },  
	{ 0x051c, KEY_BACK },  
	{ 0x051d, KEY_FORWARD },  
	{ 0x054d, KEY_INFO },  
	{ 0x0556, KEY_ZOOM },  
};

static struct rc_map_list avermedia_rm_ks_map = {
	.map = {
		.scan     = avermedia_rm_ks,
		.size     = ARRAY_SIZE(avermedia_rm_ks),
		.rc_proto = RC_PROTO_NEC,
		.name     = RC_MAP_AVERMEDIA_RM_KS,
	}
};

static int __init init_rc_map_avermedia_rm_ks(void)
{
	return rc_map_register(&avermedia_rm_ks_map);
}

static void __exit exit_rc_map_avermedia_rm_ks(void)
{
	rc_map_unregister(&avermedia_rm_ks_map);
}

module_init(init_rc_map_avermedia_rm_ks)
module_exit(exit_rc_map_avermedia_rm_ks)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Antti Palosaari <crope@iki.fi>");
