
 

struct usbmix_dB_map {
	int min;
	int max;
	bool min_mute;
};

struct usbmix_name_map {
	int id;
	const char *name;
	int control;
	const struct usbmix_dB_map *dB;
};

struct usbmix_selector_map {
	int id;
	int count;
	const char **names;
};

struct usbmix_ctl_map {
	u32 id;
	const struct usbmix_name_map *map;
	const struct usbmix_selector_map *selector_map;
	const struct usbmix_connector_map *connector_map;
};

 

 

static const struct usbmix_name_map extigy_map[] = {
	 
	{ 2, "PCM Playback" },  
	 
	 
	{ 5, NULL },  
	{ 6, "Digital In" },  
	 
	{ 8, "Line Playback" },  
	 
	{ 10, "Mic Playback" },  
	{ 11, "Capture Source" },  
	{ 12, "Capture" },  
	 
	 
	 
	 
	{ 17, NULL, 1 },  
	{ 17, "Channel Routing", 2 },	 
	{ 18, "Tone Control - Bass", UAC_FU_BASS },  
	{ 18, "Tone Control - Treble", UAC_FU_TREBLE },  
	{ 18, "Master Playback" },  
	 
	 
	{ 21, NULL },  
	{ 22, "Digital Out Playback" },  
	{ 23, "Digital Out1 Playback" },     
	 
	{ 25, "IEC958 Optical Playback" },  
	{ 26, "IEC958 Optical Playback" },  
	{ 27, NULL },  
	 
	{ 29, NULL },  
	{ 0 }  
};

 
static const struct usbmix_dB_map mp3plus_dB_1 = {.min = -4781, .max = 0};
						 
static const struct usbmix_dB_map mp3plus_dB_2 = {.min = -1781, .max = 618};
						 

static const struct usbmix_name_map mp3plus_map[] = {
	 
	 
	 
	 
	 
	 
	 
	{ 8, "Capture Source" },  
		 
	{ 9, "Master Playback" },  
	   
	{ 10,   NULL, 2, .dB = &mp3plus_dB_2 },
		 
	{ 10, "Mic Boost", 7 },  
	{ 11, "Line Capture", .dB = &mp3plus_dB_2 },
		 
	{ 12, "Digital In Playback" },  
	{ 13,   .dB = &mp3plus_dB_1 },
		 
	{ 14, "Line Playback", .dB = &mp3plus_dB_1 },  
	 
	{ 0 }  
};

 
static const struct usbmix_name_map audigy2nx_map[] = {
	 
	 
	{ 6, "Digital In Playback" },  
	 
	{ 8, "Line Playback" },  
	{ 11, "What-U-Hear Capture" },  
	{ 12, "Line Capture" },  
	{ 13, "Digital In Capture" },  
	{ 14, "Capture Source" },  
	 
	 
	{ 17, NULL },  
	{ 18, "Master Playback" },  
	 
	 
	{ 21, NULL },  
	{ 22, "Digital Out Playback" },  
	{ 23, NULL },  
	 
	{ 27, NULL },  
	{ 28, "Speaker Playback" },  
	{ 29, "Digital Out Source" },  
	{ 30, "Headphone Playback" },  
	{ 31, "Headphone Source" },  
	{ 0 }  
};

static const struct usbmix_name_map mbox1_map[] = {
	{ 1, "Clock" },
	{ 0 }  
};

static const struct usbmix_selector_map c400_selectors[] = {
	{
		.id = 0x80,
		.count = 2,
		.names = (const char*[]) {"Internal", "SPDIF"}
	},
	{ 0 }  
};

static const struct usbmix_selector_map audigy2nx_selectors[] = {
	{
		.id = 14,  
		.count = 3,
		.names = (const char*[]) {"Line", "Digital In", "What-U-Hear"}
	},
	{
		.id = 29,  
		.count = 3,
		.names = (const char*[]) {"Front", "PCM", "Digital In"}
	},
	{
		.id = 31,  
		.count = 2,
		.names = (const char*[]) {"Front", "Side"}
	},
	{ 0 }  
};

 
static const struct usbmix_name_map live24ext_map[] = {
	 
	{ 5, "Mic Capture" },  
	{ 0 }  
};

 
static const struct usbmix_name_map linex_map[] = {
	 
	  
	{ 3, "Master" },  
	{ 0 }  
};

static const struct usbmix_name_map maya44_map[] = {
	 
	{ 2, "Line Playback" },  
	 
	{ 4, "Line Playback" },  
	 
	 
	{ 7, "Master Playback" },  
	 
	 
	{ 10, "Line Capture" },  
	 
	 
	{ }
};

 

static const struct usbmix_name_map justlink_map[] = {
	 
	 
	{ 3, NULL},  
	 
	 
	 
	{ 7, "Master Playback" },  
	{ 8, NULL },  
	{ 9, NULL },  
	 
	 
	{ 0xc, NULL },  
	{ 0 }  
};

 
static const struct usbmix_name_map aureon_51_2_map[] = {
	 
	 
	 
	 
	 
	 
	 
	{ 8, "Capture Source" },  
	{ 9, "Master Playback" },  
	{ 10, "Mic Capture" },  
	{ 11, "Line Capture" },  
	{ 12, "IEC958 In Capture" },  
	{ 13, "Mic Playback" },  
	{ 14, "Line Playback" },  
	 
	{}  
};

static const struct usbmix_name_map scratch_live_map[] = {
	 
	 
	 
	{ 4, "Line 1 In" },  
	 
	 
	 
	 
	{ 9, "Line 2 In" },  
	 
	 
	 
	{ 0 }  
};

static const struct usbmix_name_map ebox44_map[] = {
	{ 4, NULL },  
	{ 6, NULL },  
	{ 7, NULL },  
	{ 10, NULL },  
	{ 11, NULL },  
	{ 0 }
};

 
static const struct usbmix_name_map hercules_usb51_map[] = {
	{ 8, "Capture Source" },	 
	{ 9, "Master Playback" },	 
	{ 10, "Mic Boost", 7 },		 
	{ 11, "Line Capture" },		 
	{ 13, "Mic Bypass Playback" },	 
	{ 14, "Line Bypass Playback" },	 
	{ 0 }				 
};

 
static const struct usbmix_name_map gamecom780_map[] = {
	{ 9, NULL },  
	{}
};

 
static const struct usbmix_name_map scms_usb3318_map[] = {
	{ 10, NULL },
	{ 0 }
};

 
static const struct usbmix_dB_map bose_companion5_dB = {-5006, -6};
static const struct usbmix_name_map bose_companion5_map[] = {
	{ 3, NULL, .dB = &bose_companion5_dB },
	{ 0 }	 
};

 
static const struct usbmix_dB_map bose_soundlink_dB = {-8283, -0, true};
static const struct usbmix_name_map bose_soundlink_map[] = {
	{ 2, NULL, .dB = &bose_soundlink_dB },
	{ 0 }	 
};

 
static const struct usbmix_dB_map sennheiser_pc8_dB = {-9500, 0};
static const struct usbmix_name_map sennheiser_pc8_map[] = {
	{ 9, NULL, .dB = &sennheiser_pc8_dB },
	{ 0 }    
};

 
static const struct usbmix_name_map dell_alc4020_map[] = {
	{ 4, NULL },	 
	{ 16, NULL },
	{ 19, NULL },
	{ 0 }
};

 
static const struct usbmix_name_map corsair_virtuoso_map[] = {
	{ 3, "Mic Capture" },
	{ 6, "Sidetone Playback" },
	{ 0 }
};

 
 
static const struct usbmix_dB_map ms_usb_link_dB = { -3225, 0, true };
static const struct usbmix_name_map ms_usb_link_map[] = {
	{ 9, NULL, .dB = &ms_usb_link_dB },
	{ 10, NULL },  
	{ 0 }    
};

 
static const struct usbmix_name_map asus_zenith_ii_map[] = {
	{ 19, NULL, 12 },  
	{ 16, "Speaker" },		 
	{ 22, "Speaker Playback" },	 
	{ 7, "Line" },			 
	{ 19, "Line Capture" },		 
	{ 8, "Mic" },			 
	{ 20, "Mic Capture" },		 
	{ 9, "Front Mic" },		 
	{ 21, "Front Mic Capture" },	 
	{ 17, "IEC958" },		 
	{ 23, "IEC958 Playback" },	 
	{}
};

static const struct usbmix_connector_map asus_zenith_ii_connector_map[] = {
	{ 10, 16 },	 
	{ 11, 17 },	 
	{ 13, 7 },	 
	{ 14, 8 },	 
	{ 15, 9 },	 
	{}
};

static const struct usbmix_name_map lenovo_p620_rear_map[] = {
	{ 19, NULL, 12 },  
	{}
};

 
static const struct usbmix_name_map trx40_mobo_map[] = {
	{ 18, NULL },  
	{ 19, NULL, 12 },  
	{ 16, "Speaker" },		 
	{ 22, "Speaker Playback" },	 
	{ 7, "Line" },			 
	{ 19, "Line Capture" },		 
	{ 17, "Front Headphone" },	 
	{ 23, "Front Headphone Playback" },	 
	{ 8, "Mic" },			 
	{ 20, "Mic Capture" },		 
	{ 9, "Front Mic" },		 
	{ 21, "Front Mic Capture" },	 
	{ 24, "IEC958 Playback" },	 
	{}
};

static const struct usbmix_connector_map trx40_mobo_connector_map[] = {
	{ 10, 16 },	 
	{ 11, 17 },	 
	{ 13, 7 },	 
	{ 14, 8 },	 
	{ 15, 9 },	 
	{}
};

 
static const struct usbmix_name_map aorus_master_alc1220vb_map[] = {
	{ 17, NULL },			 
	{ 19, NULL, 12 },  
	{ 16, "Line Out" },		 
	{ 22, "Line Out Playback" },	 
	{ 7, "Line" },			 
	{ 19, "Line Capture" },		 
	{ 8, "Mic" },			 
	{ 20, "Mic Capture" },		 
	{ 9, "Front Mic" },		 
	{ 21, "Front Mic Capture" },	 
	{}
};

 
static const struct usbmix_name_map msi_mpg_x570s_carbon_max_wifi_alc4080_map[] = {
	{ 29, "Speaker Playback" },
	{ 30, "Front Headphone Playback" },
	{ 32, "IEC958 Playback" },
	{}
};

 
static const struct usbmix_name_map gigabyte_b450_map[] = {
	{ 24, NULL },			 
	{ 21, "Speaker" },		 
	{ 29, "Speaker Playback" },	 
	{ 22, "Headphone" },		 
	{ 30, "Headphone Playback" },	 
	{ 11, "Line" },			 
	{ 27, "Line Capture" },		 
	{ 12, "Mic" },			 
	{ 28, "Mic Capture" },		 
	{ 9, "Front Mic" },		 
	{ 25, "Front Mic Capture" },	 
	{}
};

static const struct usbmix_connector_map gigabyte_b450_connector_map[] = {
	{ 13, 21 },	 
	{ 14, 22 },	 
	{ 19, 11 },	 
	{ 20, 12 },	 
	{ 17, 9 },	 
	{}
};

 

static const struct usbmix_ctl_map usbmix_ctl_maps[] = {
	{
		.id = USB_ID(0x041e, 0x3000),
		.map = extigy_map,
	},
	{
		.id = USB_ID(0x041e, 0x3010),
		.map = mp3plus_map,
	},
	{
		.id = USB_ID(0x041e, 0x3020),
		.map = audigy2nx_map,
		.selector_map = audigy2nx_selectors,
	},
 	{
		.id = USB_ID(0x041e, 0x3040),
		.map = live24ext_map,
	},
	{
		.id = USB_ID(0x041e, 0x3048),
		.map = audigy2nx_map,
		.selector_map = audigy2nx_selectors,
	},
	{	 
		.id = USB_ID(0x047f, 0xc010),
		.map = gamecom780_map,
	},
	{
		 
		.id = USB_ID(0x06f8, 0xc000),
		.map = hercules_usb51_map,
	},
	{
		.id = USB_ID(0x0763, 0x2030),
		.selector_map = c400_selectors,
	},
	{
		.id = USB_ID(0x0763, 0x2031),
		.selector_map = c400_selectors,
	},
	{
		.id = USB_ID(0x08bb, 0x2702),
		.map = linex_map,
	},
	{
		.id = USB_ID(0x0a92, 0x0091),
		.map = maya44_map,
	},
	{
		.id = USB_ID(0x0c45, 0x1158),
		.map = justlink_map,
	},
	{
		.id = USB_ID(0x0ccd, 0x0028),
		.map = aureon_51_2_map,
	},
	{
		.id = USB_ID(0x0bda, 0x4014),
		.map = dell_alc4020_map,
	},
	{
		.id = USB_ID(0x0dba, 0x1000),
		.map = mbox1_map,
	},
	{
		.id = USB_ID(0x13e5, 0x0001),
		.map = scratch_live_map,
	},
	{
		.id = USB_ID(0x200c, 0x1018),
		.map = ebox44_map,
	},
	{
		 
		.id = USB_ID(0x2573, 0x0008),
		.map = maya44_map,
	},
	{
		 
		.id = USB_ID(0x27ac, 0x1000),
		.map = scms_usb3318_map,
	},
	{
		 
		.id = USB_ID(0x25c4, 0x0003),
		.map = scms_usb3318_map,
	},
	{
		 
		.id = USB_ID(0x05a7, 0x1020),
		.map = bose_companion5_map,
	},
	{
		 
		.id = USB_ID(0x05a7, 0x40fa),
		.map = bose_soundlink_map,
	},
	{
		 
		.id = USB_ID(0x1b1c, 0x0a3f),
		.map = corsair_virtuoso_map,
	},
	{
		 
		.id = USB_ID(0x1b1c, 0x0a40),
		.map = corsair_virtuoso_map,
	},
	{
		 
		.id = USB_ID(0x1b1c, 0x0a3d),
		.map = corsair_virtuoso_map,
	},
	{
		 
		.id = USB_ID(0x1b1c, 0x0a3e),
		.map = corsair_virtuoso_map,
	},
	{
		 
		.id = USB_ID(0x1b1c, 0x0a41),
		.map = corsair_virtuoso_map,
	},
	{
		 
		.id = USB_ID(0x1b1c, 0x0a42),
		.map = corsair_virtuoso_map,
	},
	{	 
		.id = USB_ID(0x0414, 0xa001),
		.map = aorus_master_alc1220vb_map,
	},
	{	 
		.id = USB_ID(0x0414, 0xa002),
		.map = trx40_mobo_map,
		.connector_map = trx40_mobo_connector_map,
	},
	{	 
		.id = USB_ID(0x0414, 0xa00d),
		.map = gigabyte_b450_map,
		.connector_map = gigabyte_b450_connector_map,
	},
	{	 
		.id = USB_ID(0x0b05, 0x1916),
		.map = asus_zenith_ii_map,
		.connector_map = asus_zenith_ii_connector_map,
	},
	{	 
		.id = USB_ID(0x0b05, 0x1917),
		.map = trx40_mobo_map,
		.connector_map = trx40_mobo_connector_map,
	},
	{	 
		.id = USB_ID(0x0db0, 0x0d64),
		.map = trx40_mobo_map,
		.connector_map = trx40_mobo_connector_map,
	},
	{	 
		.id = USB_ID(0x0db0, 0x419c),
		.map = msi_mpg_x570s_carbon_max_wifi_alc4080_map,
	},
	{	 
		.id = USB_ID(0x0db0, 0xa073),
		.map = msi_mpg_x570s_carbon_max_wifi_alc4080_map,
	},
	{	 
		.id = USB_ID(0x0db0, 0x543d),
		.map = trx40_mobo_map,
		.connector_map = trx40_mobo_connector_map,
	},
	{	 
		.id = USB_ID(0x26ce, 0x0a01),
		.map = trx40_mobo_map,
		.connector_map = trx40_mobo_connector_map,
	},
	{	 
		.id = USB_ID(0x17aa, 0x1046),
		.map = lenovo_p620_rear_map,
	},
	{
		 
		.id = USB_ID(0x1395, 0x0025),
		.map = sennheiser_pc8_map,
	},
	{
		 
		.id = USB_ID(0x045e, 0x083c),
		.map = ms_usb_link_map,
	},
	{ 0 }  
};

 

static const struct usbmix_name_map uac3_badd_generic_io_map[] = {
	{ UAC3_BADD_FU_ID2, "Generic Out Playback" },
	{ UAC3_BADD_FU_ID5, "Generic In Capture" },
	{ 0 }					 
};
static const struct usbmix_name_map uac3_badd_headphone_map[] = {
	{ UAC3_BADD_FU_ID2, "Headphone Playback" },
	{ 0 }					 
};
static const struct usbmix_name_map uac3_badd_speaker_map[] = {
	{ UAC3_BADD_FU_ID2, "Speaker Playback" },
	{ 0 }					 
};
static const struct usbmix_name_map uac3_badd_microphone_map[] = {
	{ UAC3_BADD_FU_ID5, "Mic Capture" },
	{ 0 }					 
};
 
static const struct usbmix_name_map uac3_badd_headset_map[] = {
	{ UAC3_BADD_FU_ID2, "Headset Playback" },
	{ UAC3_BADD_FU_ID5, "Headset Capture" },
	{ UAC3_BADD_FU_ID7, "Sidetone Mixing" },
	{ 0 }					 
};
static const struct usbmix_name_map uac3_badd_speakerphone_map[] = {
	{ UAC3_BADD_FU_ID2, "Speaker Playback" },
	{ UAC3_BADD_FU_ID5, "Mic Capture" },
	{ 0 }					 
};

static const struct usbmix_ctl_map uac3_badd_usbmix_ctl_maps[] = {
	{
		.id = UAC3_FUNCTION_SUBCLASS_GENERIC_IO,
		.map = uac3_badd_generic_io_map,
	},
	{
		.id = UAC3_FUNCTION_SUBCLASS_HEADPHONE,
		.map = uac3_badd_headphone_map,
	},
	{
		.id = UAC3_FUNCTION_SUBCLASS_SPEAKER,
		.map = uac3_badd_speaker_map,
	},
	{
		.id = UAC3_FUNCTION_SUBCLASS_MICROPHONE,
		.map = uac3_badd_microphone_map,
	},
	{
		.id = UAC3_FUNCTION_SUBCLASS_HEADSET,
		.map = uac3_badd_headset_map,
	},
	{
		.id = UAC3_FUNCTION_SUBCLASS_HEADSET_ADAPTER,
		.map = uac3_badd_headset_map,
	},
	{
		.id = UAC3_FUNCTION_SUBCLASS_SPEAKERPHONE,
		.map = uac3_badd_speakerphone_map,
	},
	{ 0 }  
};
