
 

#include <media/rc-map.h>
#include <linux/module.h>

static struct rc_map_table anysee[] = {
	{ 0x0800, KEY_NUMERIC_0 },
	{ 0x0801, KEY_NUMERIC_1 },
	{ 0x0802, KEY_NUMERIC_2 },
	{ 0x0803, KEY_NUMERIC_3 },
	{ 0x0804, KEY_NUMERIC_4 },
	{ 0x0805, KEY_NUMERIC_5 },
	{ 0x0806, KEY_NUMERIC_6 },
	{ 0x0807, KEY_NUMERIC_7 },
	{ 0x0808, KEY_NUMERIC_8 },
	{ 0x0809, KEY_NUMERIC_9 },
	{ 0x080a, KEY_POWER2 },           
	{ 0x080b, KEY_VIDEO },            
	{ 0x080c, KEY_CHANNEL },          
	{ 0x080d, KEY_NEXT },             
	{ 0x080e, KEY_MENU },             
	{ 0x080f, KEY_EPG },              
	{ 0x0810, KEY_CLEAR },            
	{ 0x0811, KEY_CHANNELUP },
	{ 0x0812, KEY_VOLUMEDOWN },
	{ 0x0813, KEY_VOLUMEUP },
	{ 0x0814, KEY_CHANNELDOWN },
	{ 0x0815, KEY_OK },
	{ 0x0816, KEY_RADIO },            
	{ 0x0817, KEY_INFO },             
	{ 0x0818, KEY_PREVIOUS },         
	{ 0x0819, KEY_FAVORITES },        
	{ 0x081a, KEY_SUBTITLE },         
	{ 0x081b, KEY_CAMERA },           
	{ 0x081c, KEY_YELLOW },
	{ 0x081d, KEY_RED },
	{ 0x081e, KEY_LANGUAGE },         
	{ 0x081f, KEY_GREEN },
	{ 0x0820, KEY_SLEEP },            
	{ 0x0821, KEY_SCREEN },           
	{ 0x0822, KEY_ZOOM },             
	{ 0x0824, KEY_FN },               
	{ 0x0825, KEY_FN },               
	{ 0x0842, KEY_MUTE },             
	{ 0x0844, KEY_BLUE },
	{ 0x0847, KEY_TEXT },             
	{ 0x0848, KEY_STOP },
	{ 0x0849, KEY_RECORD },
	{ 0x0850, KEY_PLAY },
	{ 0x0851, KEY_PAUSE },
};

static struct rc_map_list anysee_map = {
	.map = {
		.scan     = anysee,
		.size     = ARRAY_SIZE(anysee),
		.rc_proto = RC_PROTO_NEC,
		.name     = RC_MAP_ANYSEE,
	}
};

static int __init init_rc_map_anysee(void)
{
	return rc_map_register(&anysee_map);
}

static void __exit exit_rc_map_anysee(void)
{
	rc_map_unregister(&anysee_map);
}

module_init(init_rc_map_anysee)
module_exit(exit_rc_map_anysee)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Antti Palosaari <crope@iki.fi>");
