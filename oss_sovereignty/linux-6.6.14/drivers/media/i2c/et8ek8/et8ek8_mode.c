
 

#include "et8ek8_reg.h"

 

 
static struct et8ek8_reglist mode1_poweron_mode2_16vga_2592x1968_12_07fps = {
 
	.type = ET8EK8_REGLIST_POWERON,
	.mode = {
		.sensor_width = 2592,
		.sensor_height = 1968,
		.sensor_window_origin_x = 0,
		.sensor_window_origin_y = 0,
		.sensor_window_width = 2592,
		.sensor_window_height = 1968,
		.width = 3288,
		.height = 2016,
		.window_origin_x = 0,
		.window_origin_y = 0,
		.window_width = 2592,
		.window_height = 1968,
		.pixel_clock = 80000000,
		.ext_clock = 9600000,
		.timeperframe = {
			.numerator = 100,
			.denominator = 1207
		},
		.max_exp = 2012,
		 
		.bus_format = MEDIA_BUS_FMT_SGRBG10_1X10,
		.sensitivity = 65536
	},
	.regs = {
		 
		{ ET8EK8_REG_8BIT, 0x126C, 0xCC },
		 
		{ ET8EK8_REG_8BIT, 0x1269, 0x00 },
		 
		{ ET8EK8_REG_8BIT, 0x1220, 0x89 },
		 
		{ ET8EK8_REG_8BIT, 0x123A, 0x07 },
		{ ET8EK8_REG_8BIT, 0x1241, 0x94 },
		{ ET8EK8_REG_8BIT, 0x1242, 0x02 },
		{ ET8EK8_REG_8BIT, 0x124B, 0x00 },
		{ ET8EK8_REG_8BIT, 0x1255, 0xFF },
		{ ET8EK8_REG_8BIT, 0x1256, 0x9F },
		{ ET8EK8_REG_8BIT, 0x1258, 0x00 },
		 
		{ ET8EK8_REG_8BIT, 0x125D, 0x88 },
		 
		{ ET8EK8_REG_8BIT, 0x125E, 0xC0 },
		 
		{ ET8EK8_REG_8BIT, 0x1263, 0x98 },
		{ ET8EK8_REG_8BIT, 0x1268, 0xC6 },
		{ ET8EK8_REG_8BIT, 0x1434, 0x00 },
		{ ET8EK8_REG_8BIT, 0x1163, 0x44 },
		{ ET8EK8_REG_8BIT, 0x1166, 0x29 },
		{ ET8EK8_REG_8BIT, 0x1140, 0x02 },
		{ ET8EK8_REG_8BIT, 0x1011, 0x24 },
		{ ET8EK8_REG_8BIT, 0x1151, 0x80 },
		{ ET8EK8_REG_8BIT, 0x1152, 0x23 },
		 
		{ ET8EK8_REG_8BIT, 0x1014, 0x05 },
		{ ET8EK8_REG_8BIT, 0x1033, 0x06 },
		{ ET8EK8_REG_8BIT, 0x1034, 0x79 },
		{ ET8EK8_REG_8BIT, 0x1423, 0x3F },
		{ ET8EK8_REG_8BIT, 0x1424, 0x3F },
		{ ET8EK8_REG_8BIT, 0x1426, 0x00 },
		 
		{ ET8EK8_REG_8BIT, 0x1439, 0x00 },
		 
		{ ET8EK8_REG_8BIT, 0x161F, 0x60 },
		 
		{ ET8EK8_REG_8BIT, 0x1634, 0x00 },
		{ ET8EK8_REG_8BIT, 0x1646, 0x00 },
		{ ET8EK8_REG_8BIT, 0x1648, 0x00 },
		{ ET8EK8_REG_8BIT, 0x113E, 0x01 },
		{ ET8EK8_REG_8BIT, 0x113F, 0x22 },
		{ ET8EK8_REG_8BIT, 0x1239, 0x64 },
		{ ET8EK8_REG_8BIT, 0x1238, 0x02 },
		{ ET8EK8_REG_8BIT, 0x123B, 0x70 },
		{ ET8EK8_REG_8BIT, 0x123A, 0x07 },
		{ ET8EK8_REG_8BIT, 0x121B, 0x64 },
		{ ET8EK8_REG_8BIT, 0x121D, 0x64 },
		{ ET8EK8_REG_8BIT, 0x1221, 0x00 },
		{ ET8EK8_REG_8BIT, 0x1220, 0x89 },
		{ ET8EK8_REG_8BIT, 0x1223, 0x00 },
		{ ET8EK8_REG_8BIT, 0x1222, 0x54 },
		{ ET8EK8_REG_8BIT, 0x125D, 0x88 },  
		{ ET8EK8_REG_TERM, 0, 0}
	}
};

 
static struct et8ek8_reglist mode1_16vga_2592x1968_13_12fps_dpcm10_8 = {
 
	.type = ET8EK8_REGLIST_MODE,
	.mode = {
		.sensor_width = 2592,
		.sensor_height = 1968,
		.sensor_window_origin_x = 0,
		.sensor_window_origin_y = 0,
		.sensor_window_width = 2592,
		.sensor_window_height = 1968,
		.width = 3072,
		.height = 2016,
		.window_origin_x = 0,
		.window_origin_y = 0,
		.window_width = 2592,
		.window_height = 1968,
		.pixel_clock = 80000000,
		.ext_clock = 9600000,
		.timeperframe = {
			.numerator = 100,
			.denominator = 1292
		},
		.max_exp = 2012,
		 
		.bus_format = MEDIA_BUS_FMT_SGRBG10_DPCM8_1X8,
		.sensitivity = 65536
	},
	.regs = {
		{ ET8EK8_REG_8BIT, 0x1239, 0x57 },
		{ ET8EK8_REG_8BIT, 0x1238, 0x82 },
		{ ET8EK8_REG_8BIT, 0x123B, 0x70 },
		{ ET8EK8_REG_8BIT, 0x123A, 0x06 },
		{ ET8EK8_REG_8BIT, 0x121B, 0x64 },
		{ ET8EK8_REG_8BIT, 0x121D, 0x64 },
		{ ET8EK8_REG_8BIT, 0x1221, 0x00 },
		{ ET8EK8_REG_8BIT, 0x1220, 0x80 },  
		{ ET8EK8_REG_8BIT, 0x1223, 0x00 },
		{ ET8EK8_REG_8BIT, 0x1222, 0x54 },
		{ ET8EK8_REG_8BIT, 0x125D, 0x83 },  
		{ ET8EK8_REG_TERM, 0, 0}
	}
};

 
static struct et8ek8_reglist mode3_4vga_1296x984_29_99fps_dpcm10_8 = {
 
	.type = ET8EK8_REGLIST_MODE,
	.mode = {
		.sensor_width = 2592,
		.sensor_height = 1968,
		.sensor_window_origin_x = 0,
		.sensor_window_origin_y = 0,
		.sensor_window_width = 2592,
		.sensor_window_height = 1968,
		.width = 3192,
		.height = 1008,
		.window_origin_x = 0,
		.window_origin_y = 0,
		.window_width = 1296,
		.window_height = 984,
		.pixel_clock = 96533333,
		.ext_clock = 9600000,
		.timeperframe = {
			.numerator = 100,
			.denominator = 3000
		},
		.max_exp = 1004,
		 
		.bus_format = MEDIA_BUS_FMT_SGRBG10_DPCM8_1X8,
		.sensitivity = 65536
	},
	.regs = {
		{ ET8EK8_REG_8BIT, 0x1239, 0x5A },
		{ ET8EK8_REG_8BIT, 0x1238, 0x82 },
		{ ET8EK8_REG_8BIT, 0x123B, 0x70 },
		{ ET8EK8_REG_8BIT, 0x123A, 0x05 },
		{ ET8EK8_REG_8BIT, 0x121B, 0x63 },
		{ ET8EK8_REG_8BIT, 0x1220, 0x85 },
		{ ET8EK8_REG_8BIT, 0x1221, 0x00 },
		{ ET8EK8_REG_8BIT, 0x1222, 0x54 },
		{ ET8EK8_REG_8BIT, 0x1223, 0x00 },
		{ ET8EK8_REG_8BIT, 0x121D, 0x63 },
		{ ET8EK8_REG_8BIT, 0x125D, 0x83 },  
		{ ET8EK8_REG_TERM, 0, 0}
	}
};

 
static struct et8ek8_reglist mode4_svga_864x656_29_88fps = {
 
	.type = ET8EK8_REGLIST_MODE,
	.mode = {
		.sensor_width = 2592,
		.sensor_height = 1968,
		.sensor_window_origin_x = 0,
		.sensor_window_origin_y = 0,
		.sensor_window_width = 2592,
		.sensor_window_height = 1968,
		.width = 3984,
		.height = 672,
		.window_origin_x = 0,
		.window_origin_y = 0,
		.window_width = 864,
		.window_height = 656,
		.pixel_clock = 80000000,
		.ext_clock = 9600000,
		.timeperframe = {
			.numerator = 100,
			.denominator = 2988
		},
		.max_exp = 668,
		 
		.bus_format = MEDIA_BUS_FMT_SGRBG10_1X10,
		.sensitivity = 65536
	},
	.regs = {
		{ ET8EK8_REG_8BIT, 0x1239, 0x64 },
		{ ET8EK8_REG_8BIT, 0x1238, 0x02 },
		{ ET8EK8_REG_8BIT, 0x123B, 0x71 },
		{ ET8EK8_REG_8BIT, 0x123A, 0x07 },
		{ ET8EK8_REG_8BIT, 0x121B, 0x62 },
		{ ET8EK8_REG_8BIT, 0x121D, 0x62 },
		{ ET8EK8_REG_8BIT, 0x1221, 0x00 },
		{ ET8EK8_REG_8BIT, 0x1220, 0xA6 },
		{ ET8EK8_REG_8BIT, 0x1223, 0x00 },
		{ ET8EK8_REG_8BIT, 0x1222, 0x54 },
		{ ET8EK8_REG_8BIT, 0x125D, 0x88 },  
		{ ET8EK8_REG_TERM, 0, 0}
	}
};

 
static struct et8ek8_reglist mode5_vga_648x492_29_93fps = {
 
	.type = ET8EK8_REGLIST_MODE,
	.mode = {
		.sensor_width = 2592,
		.sensor_height = 1968,
		.sensor_window_origin_x = 0,
		.sensor_window_origin_y = 0,
		.sensor_window_width = 2592,
		.sensor_window_height = 1968,
		.width = 5304,
		.height = 504,
		.window_origin_x = 0,
		.window_origin_y = 0,
		.window_width = 648,
		.window_height = 492,
		.pixel_clock = 80000000,
		.ext_clock = 9600000,
		.timeperframe = {
			.numerator = 100,
			.denominator = 2993
		},
		.max_exp = 500,
		 
		.bus_format = MEDIA_BUS_FMT_SGRBG10_1X10,
		.sensitivity = 65536
	},
	.regs = {
		{ ET8EK8_REG_8BIT, 0x1239, 0x64 },
		{ ET8EK8_REG_8BIT, 0x1238, 0x02 },
		{ ET8EK8_REG_8BIT, 0x123B, 0x71 },
		{ ET8EK8_REG_8BIT, 0x123A, 0x07 },
		{ ET8EK8_REG_8BIT, 0x121B, 0x61 },
		{ ET8EK8_REG_8BIT, 0x121D, 0x61 },
		{ ET8EK8_REG_8BIT, 0x1221, 0x00 },
		{ ET8EK8_REG_8BIT, 0x1220, 0xDD },
		{ ET8EK8_REG_8BIT, 0x1223, 0x00 },
		{ ET8EK8_REG_8BIT, 0x1222, 0x54 },
		{ ET8EK8_REG_8BIT, 0x125D, 0x88 },  
		{ ET8EK8_REG_TERM, 0, 0}
	}
};

 
static struct et8ek8_reglist mode2_16vga_2592x1968_3_99fps = {
 
	.type = ET8EK8_REGLIST_MODE,
	.mode = {
		.sensor_width = 2592,
		.sensor_height = 1968,
		.sensor_window_origin_x = 0,
		.sensor_window_origin_y = 0,
		.sensor_window_width = 2592,
		.sensor_window_height = 1968,
		.width = 3288,
		.height = 6096,
		.window_origin_x = 0,
		.window_origin_y = 0,
		.window_width = 2592,
		.window_height = 1968,
		.pixel_clock = 80000000,
		.ext_clock = 9600000,
		.timeperframe = {
			.numerator = 100,
			.denominator = 399
		},
		.max_exp = 6092,
		 
		.bus_format = MEDIA_BUS_FMT_SGRBG10_1X10,
		.sensitivity = 65536
	},
	.regs = {
		{ ET8EK8_REG_8BIT, 0x1239, 0x64 },
		{ ET8EK8_REG_8BIT, 0x1238, 0x02 },
		{ ET8EK8_REG_8BIT, 0x123B, 0x70 },
		{ ET8EK8_REG_8BIT, 0x123A, 0x07 },
		{ ET8EK8_REG_8BIT, 0x121B, 0x64 },
		{ ET8EK8_REG_8BIT, 0x121D, 0x64 },
		{ ET8EK8_REG_8BIT, 0x1221, 0x00 },
		{ ET8EK8_REG_8BIT, 0x1220, 0x89 },
		{ ET8EK8_REG_8BIT, 0x1223, 0x00 },
		{ ET8EK8_REG_8BIT, 0x1222, 0xFE },
		{ ET8EK8_REG_TERM, 0, 0}
	}
};

 
static struct et8ek8_reglist mode_648x492_5fps = {
 
	.type = ET8EK8_REGLIST_MODE,
	.mode = {
		.sensor_width = 2592,
		.sensor_height = 1968,
		.sensor_window_origin_x = 0,
		.sensor_window_origin_y = 0,
		.sensor_window_width = 2592,
		.sensor_window_height = 1968,
		.width = 5304,
		.height = 504,
		.window_origin_x = 0,
		.window_origin_y = 0,
		.window_width = 648,
		.window_height = 492,
		.pixel_clock = 13333333,
		.ext_clock = 9600000,
		.timeperframe = {
			.numerator = 100,
			.denominator = 499
		},
		.max_exp = 500,
		 
		.bus_format = MEDIA_BUS_FMT_SGRBG10_1X10,
		.sensitivity = 65536
	},
	.regs = {
		{ ET8EK8_REG_8BIT, 0x1239, 0x64 },
		{ ET8EK8_REG_8BIT, 0x1238, 0x02 },
		{ ET8EK8_REG_8BIT, 0x123B, 0x71 },
		{ ET8EK8_REG_8BIT, 0x123A, 0x57 },
		{ ET8EK8_REG_8BIT, 0x121B, 0x61 },
		{ ET8EK8_REG_8BIT, 0x121D, 0x61 },
		{ ET8EK8_REG_8BIT, 0x1221, 0x00 },
		{ ET8EK8_REG_8BIT, 0x1220, 0xDD },
		{ ET8EK8_REG_8BIT, 0x1223, 0x00 },
		{ ET8EK8_REG_8BIT, 0x1222, 0x54 },
		{ ET8EK8_REG_8BIT, 0x125D, 0x88 },  
		{ ET8EK8_REG_TERM, 0, 0}
	}
};

 
static struct et8ek8_reglist mode3_4vga_1296x984_5fps = {
 
	.type = ET8EK8_REGLIST_MODE,
	.mode = {
		.sensor_width = 2592,
		.sensor_height = 1968,
		.sensor_window_origin_x = 0,
		.sensor_window_origin_y = 0,
		.sensor_window_width = 2592,
		.sensor_window_height = 1968,
		.width = 3288,
		.height = 3000,
		.window_origin_x = 0,
		.window_origin_y = 0,
		.window_width = 1296,
		.window_height = 984,
		.pixel_clock = 49400000,
		.ext_clock = 9600000,
		.timeperframe = {
			.numerator = 100,
			.denominator = 501
		},
		.max_exp = 2996,
		 
		.bus_format = MEDIA_BUS_FMT_SGRBG10_1X10,
		.sensitivity = 65536
	},
	.regs = {
		{ ET8EK8_REG_8BIT, 0x1239, 0x7B },
		{ ET8EK8_REG_8BIT, 0x1238, 0x82 },
		{ ET8EK8_REG_8BIT, 0x123B, 0x70 },
		{ ET8EK8_REG_8BIT, 0x123A, 0x17 },
		{ ET8EK8_REG_8BIT, 0x121B, 0x63 },
		{ ET8EK8_REG_8BIT, 0x121D, 0x63 },
		{ ET8EK8_REG_8BIT, 0x1221, 0x00 },
		{ ET8EK8_REG_8BIT, 0x1220, 0x89 },
		{ ET8EK8_REG_8BIT, 0x1223, 0x00 },
		{ ET8EK8_REG_8BIT, 0x1222, 0xFA },
		{ ET8EK8_REG_8BIT, 0x125D, 0x88 },  
		{ ET8EK8_REG_TERM, 0, 0}
	}
};

 
static struct et8ek8_reglist mode_4vga_1296x984_25fps_dpcm10_8 = {
 
	.type = ET8EK8_REGLIST_MODE,
	.mode = {
		.sensor_width = 2592,
		.sensor_height = 1968,
		.sensor_window_origin_x = 0,
		.sensor_window_origin_y = 0,
		.sensor_window_width = 2592,
		.sensor_window_height = 1968,
		.width = 3192,
		.height = 1056,
		.window_origin_x = 0,
		.window_origin_y = 0,
		.window_width = 1296,
		.window_height = 984,
		.pixel_clock = 84266667,
		.ext_clock = 9600000,
		.timeperframe = {
			.numerator = 100,
			.denominator = 2500
		},
		.max_exp = 1052,
		 
		.bus_format = MEDIA_BUS_FMT_SGRBG10_DPCM8_1X8,
		.sensitivity = 65536
	},
	.regs = {
		{ ET8EK8_REG_8BIT, 0x1239, 0x4F },
		{ ET8EK8_REG_8BIT, 0x1238, 0x02 },
		{ ET8EK8_REG_8BIT, 0x123B, 0x70 },
		{ ET8EK8_REG_8BIT, 0x123A, 0x05 },
		{ ET8EK8_REG_8BIT, 0x121B, 0x63 },
		{ ET8EK8_REG_8BIT, 0x1220, 0x85 },
		{ ET8EK8_REG_8BIT, 0x1221, 0x00 },
		{ ET8EK8_REG_8BIT, 0x1222, 0x58 },
		{ ET8EK8_REG_8BIT, 0x1223, 0x00 },
		{ ET8EK8_REG_8BIT, 0x121D, 0x63 },
		{ ET8EK8_REG_8BIT, 0x125D, 0x83 },
		{ ET8EK8_REG_TERM, 0, 0}
	}
};

struct et8ek8_meta_reglist meta_reglist = {
	.version = "V14 03-June-2008",
	.reglist = {
		{ .ptr = &mode1_poweron_mode2_16vga_2592x1968_12_07fps },
		{ .ptr = &mode1_16vga_2592x1968_13_12fps_dpcm10_8 },
		{ .ptr = &mode3_4vga_1296x984_29_99fps_dpcm10_8 },
		{ .ptr = &mode4_svga_864x656_29_88fps },
		{ .ptr = &mode5_vga_648x492_29_93fps },
		{ .ptr = &mode2_16vga_2592x1968_3_99fps },
		{ .ptr = &mode_648x492_5fps },
		{ .ptr = &mode3_4vga_1296x984_5fps },
		{ .ptr = &mode_4vga_1296x984_25fps_dpcm10_8 },
		{ .ptr = NULL }
	}
};
