 
#ifndef DDK750_SII164_H__
#define DDK750_SII164_H__

#define USE_DVICHIP

 
enum sii164_hot_plug_mode {
	SII164_HOTPLUG_DISABLE = 0,	 

	SII164_HOTPLUG_USE_MDI,          
	SII164_HOTPLUG_USE_RSEN,         
	SII164_HOTPLUG_USE_HTPLG         
};

 
long sii164_init_chip(unsigned char edgeSelect,
		      unsigned char busSelect,
		      unsigned char dualEdgeClkSelect,
		      unsigned char hsyncEnable,
		      unsigned char vsyncEnable,
		      unsigned char deskewEnable,
		      unsigned char deskewSetting,
		      unsigned char continuousSyncEnable,
		      unsigned char pllFilterEnable,
		      unsigned char pllFilterValue);

unsigned short sii164_get_vendor_id(void);
unsigned short sii164GetDeviceID(void);

#ifdef SII164_FULL_FUNCTIONS
void sii164ResetChip(void);
char *sii164GetChipString(void);
void sii164SetPower(unsigned char powerUp);
void sii164EnableHotPlugDetection(unsigned char enableHotPlug);
unsigned char sii164IsConnected(void);
unsigned char sii164CheckInterrupt(void);
void sii164ClearInterrupt(void);
#endif
 
 
#define SII164_VENDOR_ID_LOW                        0x00
#define SII164_VENDOR_ID_HIGH                       0x01

 
#define SII164_DEVICE_ID_LOW                        0x02
#define SII164_DEVICE_ID_HIGH                       0x03

 
#define SII164_DEVICE_REVISION                      0x04

 
#define SII164_FREQUENCY_LIMIT_LOW                  0x06
#define SII164_FREQUENCY_LIMIT_HIGH                 0x07

 
#define SII164_CONFIGURATION                        0x08

 
#define SII164_CONFIGURATION_POWER_DOWN             0x00
#define SII164_CONFIGURATION_POWER_NORMAL           0x01
#define SII164_CONFIGURATION_POWER_MASK             0x01

 
#define SII164_CONFIGURATION_LATCH_FALLING          0x00
#define SII164_CONFIGURATION_LATCH_RISING           0x02

 
#define SII164_CONFIGURATION_BUS_12BITS             0x00
#define SII164_CONFIGURATION_BUS_24BITS             0x04

 
#define SII164_CONFIGURATION_CLOCK_SINGLE           0x00
#define SII164_CONFIGURATION_CLOCK_DUAL             0x08

 
#define SII164_CONFIGURATION_HSYNC_FORCE_LOW        0x00
#define SII164_CONFIGURATION_HSYNC_AS_IS            0x10

 
#define SII164_CONFIGURATION_VSYNC_FORCE_LOW        0x00
#define SII164_CONFIGURATION_VSYNC_AS_IS            0x20

 
#define SII164_DETECT                               0x09

 
#define SII164_DETECT_MONITOR_STATE_CHANGE          0x00
#define SII164_DETECT_MONITOR_STATE_NO_CHANGE       0x01
#define SII164_DETECT_MONITOR_STATE_CLEAR           0x01
#define SII164_DETECT_MONITOR_STATE_MASK            0x01

 
#define SII164_DETECT_HOT_PLUG_STATUS_OFF           0x00
#define SII164_DETECT_HOT_PLUG_STATUS_ON            0x02
#define SII164_DETECT_HOT_PLUG_STATUS_MASK          0x02

 
#define SII164_DETECT_RECEIVER_SENSE_NOT_DETECTED   0x00
#define SII164_DETECT_RECEIVER_SENSE_DETECTED       0x04

 
#define SII164_DETECT_INTERRUPT_BY_RSEN_PIN         0x00
#define SII164_DETECT_INTERRUPT_BY_HTPLG_PIN        0x08
#define SII164_DETECT_INTERRUPT_MASK                0x08

 
#define SII164_DETECT_MONITOR_SENSE_OUTPUT_HIGH     0x00
#define SII164_DETECT_MONITOR_SENSE_OUTPUT_MDI      0x10
#define SII164_DETECT_MONITOR_SENSE_OUTPUT_RSEN     0x20
#define SII164_DETECT_MONITOR_SENSE_OUTPUT_HTPLG    0x30
#define SII164_DETECT_MONITOR_SENSE_OUTPUT_FLAG     0x30

 
#define SII164_DESKEW                               0x0A

 
#define SII164_DESKEW_GENERAL_PURPOSE_INPUT_MASK    0x0E

 
#define SII164_DESKEW_DISABLE                       0x00
#define SII164_DESKEW_ENABLE                        0x10

 
#define SII164_DESKEW_1_STEP                        0x00
#define SII164_DESKEW_2_STEP                        0x20
#define SII164_DESKEW_3_STEP                        0x40
#define SII164_DESKEW_4_STEP                        0x60
#define SII164_DESKEW_5_STEP                        0x80
#define SII164_DESKEW_6_STEP                        0xA0
#define SII164_DESKEW_7_STEP                        0xC0
#define SII164_DESKEW_8_STEP                        0xE0

 
#define SII164_USER_CONFIGURATION                   0x0B

 
#define SII164_PLL                                  0x0C

 
#define SII164_PLL_FILTER_VALUE_MASK                0x0E

 
#define SII164_PLL_FILTER_DISABLE                   0x00
#define SII164_PLL_FILTER_ENABLE                    0x01

 
#define SII164_PLL_FILTER_SYNC_CONTINUOUS_DISABLE   0x00
#define SII164_PLL_FILTER_SYNC_CONTINUOUS_ENABLE    0x80

#endif
