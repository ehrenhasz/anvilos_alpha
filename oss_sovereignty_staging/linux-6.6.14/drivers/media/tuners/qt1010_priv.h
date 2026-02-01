 
 

#ifndef QT1010_PRIV_H
#define QT1010_PRIV_H

 

#define QT1010_STEP         (125 * kHz)  
#define QT1010_MIN_FREQ   (48 * MHz)
#define QT1010_MAX_FREQ  (860 * MHz)
#define QT1010_OFFSET   (1246 * MHz)

#define QT1010_WR 0
#define QT1010_RD 1
#define QT1010_M1 3

typedef struct {
	u8 oper, reg, val;
} qt1010_i2c_oper_t;

struct qt1010_priv {
	struct qt1010_config *cfg;
	struct i2c_adapter   *i2c;

	u8 reg1f_init_val;
	u8 reg20_init_val;
	u8 reg25_init_val;

	u32 frequency;
};

#endif
