
 

#include <media/rc-map.h>
#include <linux/module.h>

 
 
static struct rc_map_table trekstor[] = {
	{ 0x0084, KEY_NUMERIC_0 },
	{ 0x0085, KEY_MUTE },             
	{ 0x0086, KEY_HOMEPAGE },         
	{ 0x0087, KEY_UP },               
	{ 0x0088, KEY_OK },               
	{ 0x0089, KEY_RIGHT },            
	{ 0x008a, KEY_FASTFORWARD },      
	{ 0x008b, KEY_VOLUMEUP },         
	{ 0x008c, KEY_DOWN },             
	{ 0x008d, KEY_PLAY },             
	{ 0x008e, KEY_STOP },             
	{ 0x008f, KEY_EPG },              
	{ 0x0090, KEY_NUMERIC_7 },
	{ 0x0091, KEY_NUMERIC_4 },
	{ 0x0092, KEY_NUMERIC_1 },
	{ 0x0093, KEY_CHANNELDOWN },      
	{ 0x0094, KEY_NUMERIC_8 },
	{ 0x0095, KEY_NUMERIC_5 },
	{ 0x0096, KEY_NUMERIC_2 },
	{ 0x0097, KEY_CHANNELUP },        
	{ 0x0098, KEY_NUMERIC_9 },
	{ 0x0099, KEY_NUMERIC_6 },
	{ 0x009a, KEY_NUMERIC_3 },
	{ 0x009b, KEY_VOLUMEDOWN },       
	{ 0x009c, KEY_TV },               
	{ 0x009d, KEY_RECORD },           
	{ 0x009e, KEY_REWIND },           
	{ 0x009f, KEY_LEFT },             
};

static struct rc_map_list trekstor_map = {
	.map = {
		.scan     = trekstor,
		.size     = ARRAY_SIZE(trekstor),
		.rc_proto = RC_PROTO_NEC,
		.name     = RC_MAP_TREKSTOR,
	}
};

static int __init init_rc_map_trekstor(void)
{
	return rc_map_register(&trekstor_map);
}

static void __exit exit_rc_map_trekstor(void)
{
	rc_map_unregister(&trekstor_map);
}

module_init(init_rc_map_trekstor)
module_exit(exit_rc_map_trekstor)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Antti Palosaari <crope@iki.fi>");
