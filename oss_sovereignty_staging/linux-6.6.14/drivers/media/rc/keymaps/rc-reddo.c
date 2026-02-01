
 

#include <media/rc-map.h>
#include <linux/module.h>

 

static struct rc_map_table reddo[] = {
	{ 0x61d601, KEY_EPG },              
	{ 0x61d602, KEY_NUMERIC_3 },
	{ 0x61d604, KEY_NUMERIC_1 },
	{ 0x61d605, KEY_NUMERIC_5 },
	{ 0x61d606, KEY_NUMERIC_6 },
	{ 0x61d607, KEY_CHANNELDOWN },      
	{ 0x61d608, KEY_NUMERIC_2 },
	{ 0x61d609, KEY_CHANNELUP },        
	{ 0x61d60a, KEY_NUMERIC_9 },
	{ 0x61d60b, KEY_ZOOM },             
	{ 0x61d60c, KEY_NUMERIC_7 },
	{ 0x61d60d, KEY_NUMERIC_8 },
	{ 0x61d60e, KEY_VOLUMEUP },         
	{ 0x61d60f, KEY_NUMERIC_4 },
	{ 0x61d610, KEY_ESC },              
	{ 0x61d611, KEY_NUMERIC_0 },
	{ 0x61d612, KEY_OK },               
	{ 0x61d613, KEY_VOLUMEDOWN },       
	{ 0x61d614, KEY_RECORD },           
	{ 0x61d615, KEY_STOP },             
	{ 0x61d616, KEY_PLAY },             
	{ 0x61d617, KEY_MUTE },             
	{ 0x61d643, KEY_POWER2 },           
};

static struct rc_map_list reddo_map = {
	.map = {
		.scan     = reddo,
		.size     = ARRAY_SIZE(reddo),
		.rc_proto = RC_PROTO_NECX,
		.name     = RC_MAP_REDDO,
	}
};

static int __init init_rc_map_reddo(void)
{
	return rc_map_register(&reddo_map);
}

static void __exit exit_rc_map_reddo(void)
{
	rc_map_unregister(&reddo_map);
}

module_init(init_rc_map_reddo)
module_exit(exit_rc_map_reddo)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Antti Palosaari <crope@iki.fi>");
