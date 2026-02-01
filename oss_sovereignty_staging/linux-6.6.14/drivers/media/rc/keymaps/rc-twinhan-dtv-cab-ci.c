
 

#include <media/rc-map.h>
#include <linux/module.h>

static struct rc_map_table twinhan_dtv_cab_ci[] = {
	{ 0x29, KEY_POWER},
	{ 0x28, KEY_FAVORITES},
	{ 0x30, KEY_TEXT},
	{ 0x17, KEY_INFO},               
	{ 0x23, KEY_EPG},
	{ 0x3b, KEY_F22},                

	{ 0x3c, KEY_NUMERIC_1},
	{ 0x3e, KEY_NUMERIC_2},
	{ 0x39, KEY_NUMERIC_3},
	{ 0x36, KEY_NUMERIC_4},
	{ 0x22, KEY_NUMERIC_5},
	{ 0x20, KEY_NUMERIC_6},
	{ 0x32, KEY_NUMERIC_7},
	{ 0x26, KEY_NUMERIC_8},
	{ 0x24, KEY_NUMERIC_9},
	{ 0x2a, KEY_NUMERIC_0},

	{ 0x33, KEY_CANCEL},
	{ 0x2c, KEY_BACK},
	{ 0x15, KEY_CLEAR},
	{ 0x3f, KEY_TAB},
	{ 0x10, KEY_ENTER},
	{ 0x14, KEY_UP},
	{ 0x0d, KEY_RIGHT},
	{ 0x0e, KEY_DOWN},
	{ 0x11, KEY_LEFT},

	{ 0x21, KEY_VOLUMEUP},
	{ 0x35, KEY_VOLUMEDOWN},
	{ 0x3d, KEY_CHANNELDOWN},
	{ 0x3a, KEY_CHANNELUP},
	{ 0x2e, KEY_RECORD},
	{ 0x2b, KEY_PLAY},
	{ 0x13, KEY_PAUSE},
	{ 0x25, KEY_STOP},

	{ 0x1f, KEY_REWIND},
	{ 0x2d, KEY_FASTFORWARD},
	{ 0x1e, KEY_PREVIOUS},           
	{ 0x1d, KEY_NEXT},               

	{ 0x0b, KEY_CAMERA},             
	{ 0x0f, KEY_LANGUAGE},           
	{ 0x18, KEY_MODE},               
	{ 0x12, KEY_ZOOM},               
	{ 0x1c, KEY_SUBTITLE},
	{ 0x2f, KEY_MUTE},
	{ 0x16, KEY_F20},                
	{ 0x38, KEY_F21},                

	{ 0x37, KEY_SWITCHVIDEOMODE},    
	{ 0x31, KEY_AGAIN},              
	{ 0x1a, KEY_KPPLUS},             
	{ 0x19, KEY_KPMINUS},            
	{ 0x27, KEY_RED},
	{ 0x0C, KEY_GREEN},
	{ 0x01, KEY_YELLOW},
	{ 0x00, KEY_BLUE},
};

static struct rc_map_list twinhan_dtv_cab_ci_map = {
	.map = {
		.scan     = twinhan_dtv_cab_ci,
		.size     = ARRAY_SIZE(twinhan_dtv_cab_ci),
		.rc_proto = RC_PROTO_UNKNOWN,	 
		.name     = RC_MAP_TWINHAN_DTV_CAB_CI,
	}
};

static int __init init_rc_map_twinhan_dtv_cab_ci(void)
{
	return rc_map_register(&twinhan_dtv_cab_ci_map);
}

static void __exit exit_rc_map_twinhan_dtv_cab_ci(void)
{
	rc_map_unregister(&twinhan_dtv_cab_ci_map);
}

module_init(init_rc_map_twinhan_dtv_cab_ci);
module_exit(exit_rc_map_twinhan_dtv_cab_ci);

MODULE_LICENSE("GPL");
