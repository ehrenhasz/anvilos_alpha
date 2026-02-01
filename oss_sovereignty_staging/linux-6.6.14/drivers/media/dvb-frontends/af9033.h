 
 

#ifndef AF9033_H
#define AF9033_H

 
struct af9033_config {
	 
	u32 clock;

	 
#define AF9033_ADC_MULTIPLIER_1X   0
#define AF9033_ADC_MULTIPLIER_2X   1
	u8 adc_multiplier;

	 
#define AF9033_TUNER_TUA9001     0x27  
#define AF9033_TUNER_FC0011      0x28  
#define AF9033_TUNER_FC0012      0x2e  
#define AF9033_TUNER_MXL5007T    0xa0  
#define AF9033_TUNER_TDA18218    0xa1  
#define AF9033_TUNER_FC2580      0x32  
 
#define AF9033_TUNER_IT9135_38   0x38  
#define AF9033_TUNER_IT9135_51   0x51  
#define AF9033_TUNER_IT9135_52   0x52  
 
#define AF9033_TUNER_IT9135_60   0x60  
#define AF9033_TUNER_IT9135_61   0x61  
#define AF9033_TUNER_IT9135_62   0x62  
	u8 tuner;

	 
#define AF9033_TS_MODE_USB       0
#define AF9033_TS_MODE_PARALLEL  1
#define AF9033_TS_MODE_SERIAL    2
	u8 ts_mode:2;

	 
	bool spec_inv;

	 
	bool dyn0_clk;

	 
	struct af9033_ops *ops;

	 
	struct dvb_frontend **fe;

	 
	struct regmap *regmap;
};

struct af9033_ops {
	int (*pid_filter_ctrl)(struct dvb_frontend *fe, int onoff);
	int (*pid_filter)(struct dvb_frontend *fe, int index, u16 pid,
			  int onoff);
};

#endif  
