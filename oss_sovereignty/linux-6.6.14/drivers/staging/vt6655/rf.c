
 

#include "mac.h"
#include "srom.h"
#include "rf.h"
#include "baseband.h"

#define BY_AL2230_REG_LEN     23  
#define CB_AL2230_INIT_SEQ    15
#define SWITCH_CHANNEL_DELAY_AL2230 200  
#define AL2230_PWR_IDX_LEN    64

#define BY_AL7230_REG_LEN     23  
#define CB_AL7230_INIT_SEQ    16
#define SWITCH_CHANNEL_DELAY_AL7230 200  
#define AL7230_PWR_IDX_LEN    64

static const unsigned long al2230_init_table[CB_AL2230_INIT_SEQ] = {
	0x03F79000 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x03333100 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x01A00200 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x00FFF300 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0005A400 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0F4DC500 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0805B600 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0146C700 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x00068800 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0403B900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x00DBBA00 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x00099B00 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0BDFFC00 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x00000D00 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x00580F00 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW
};

static const unsigned long al2230_channel_table0[CB_MAX_CHANNEL] = {
	0x03F79000 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x03F79000 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x03E79000 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x03E79000 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x03F7A000 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x03F7A000 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x03E7A000 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x03E7A000 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x03F7B000 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x03F7B000 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x03E7B000 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x03E7B000 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x03F7C000 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x03E7C000 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW   
};

static const unsigned long al2230_channel_table1[CB_MAX_CHANNEL] = {
	0x03333100 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x0B333100 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x03333100 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x0B333100 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x03333100 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x0B333100 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x03333100 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x0B333100 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x03333100 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x0B333100 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x03333100 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x0B333100 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x03333100 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,  
	0x06666100 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW   
};

static unsigned long al2230_power_table[AL2230_PWR_IDX_LEN] = {
	0x04040900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04041900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04042900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04043900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04044900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04045900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04046900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04047900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04048900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04049900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0404A900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0404B900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0404C900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0404D900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0404E900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0404F900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04050900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04051900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04052900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04053900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04054900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04055900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04056900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04057900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04058900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04059900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0405A900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0405B900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0405C900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0405D900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0405E900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0405F900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04060900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04061900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04062900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04063900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04064900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04065900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04066900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04067900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04068900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04069900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0406A900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0406B900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0406C900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0406D900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0406E900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0406F900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04070900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04071900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04072900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04073900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04074900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04075900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04076900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04077900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04078900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x04079900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0407A900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0407B900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0407C900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0407D900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0407E900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW,
	0x0407F900 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW
};

 
bool IFRFbWriteEmbedded(struct vnt_private *priv, unsigned long dwData)
{
	void __iomem *iobase = priv->port_offset;
	unsigned short ww;
	unsigned long dwValue;

	iowrite32((u32)dwData, iobase + MAC_REG_IFREGCTL);

	 
	for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
		dwValue = ioread32(iobase + MAC_REG_IFREGCTL);
		if (dwValue & IFREGCTL_DONE)
			break;
	}

	if (ww == W_MAX_TIMEOUT)
		return false;

	return true;
}

 
static bool RFbAL2230Init(struct vnt_private *priv)
{
	void __iomem *iobase = priv->port_offset;
	int     ii;
	bool ret;

	ret = true;

	 
	iowrite8(0, iobase + MAC_REG_SOFTPWRCTL);

	vt6655_mac_word_reg_bits_on(iobase, MAC_REG_SOFTPWRCTL,
				    (SOFTPWRCTL_SWPECTI | SOFTPWRCTL_TXPEINV));
	 
	vt6655_mac_word_reg_bits_off(iobase, MAC_REG_SOFTPWRCTL, SOFTPWRCTL_SWPE3);

	 
	IFRFbWriteEmbedded(priv, (0x07168700 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW));

	for (ii = 0; ii < CB_AL2230_INIT_SEQ; ii++)
		ret &= IFRFbWriteEmbedded(priv, al2230_init_table[ii]);
	MACvTimer0MicroSDelay(priv, 30);  

	 
	vt6655_mac_word_reg_bits_on(iobase, MAC_REG_SOFTPWRCTL, SOFTPWRCTL_SWPE3);

	MACvTimer0MicroSDelay(priv, 150); 
	ret &= IFRFbWriteEmbedded(priv, (0x00d80f00 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW));
	MACvTimer0MicroSDelay(priv, 30); 
	ret &= IFRFbWriteEmbedded(priv, (0x00780f00 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW));
	MACvTimer0MicroSDelay(priv, 30); 
	ret &= IFRFbWriteEmbedded(priv,
				  al2230_init_table[CB_AL2230_INIT_SEQ - 1]);

	vt6655_mac_word_reg_bits_on(iobase, MAC_REG_SOFTPWRCTL, (SOFTPWRCTL_SWPE3    |
								 SOFTPWRCTL_SWPE2    |
								 SOFTPWRCTL_SWPECTI  |
								 SOFTPWRCTL_TXPEINV));

	 
	iowrite8(PSSIG_WPE3 | PSSIG_WPE2, iobase + MAC_REG_PSPWRSIG);

	return ret;
}

static bool RFbAL2230SelectChannel(struct vnt_private *priv, unsigned char byChannel)
{
	void __iomem *iobase = priv->port_offset;
	bool ret;

	ret = true;

	ret &= IFRFbWriteEmbedded(priv, al2230_channel_table0[byChannel - 1]);
	ret &= IFRFbWriteEmbedded(priv, al2230_channel_table1[byChannel - 1]);

	 
	iowrite8(byChannel & 0x7F, iobase + MAC_REG_CHANNEL);
	MACvTimer0MicroSDelay(priv, SWITCH_CHANNEL_DELAY_AL2230);
	 
	iowrite8(byChannel | 0x80, iobase + MAC_REG_CHANNEL);

	return ret;
}

 
bool RFbInit(struct vnt_private *priv)
{
	bool ret = true;

	switch (priv->byRFType) {
	case RF_AIROHA:
	case RF_AL2230S:
		priv->max_pwr_level = AL2230_PWR_IDX_LEN;
		ret = RFbAL2230Init(priv);
		break;
	case RF_NOTHING:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}
	return ret;
}

 
bool RFbSelectChannel(struct vnt_private *priv, unsigned char byRFType,
		      u16 byChannel)
{
	bool ret = true;

	switch (byRFType) {
	case RF_AIROHA:
	case RF_AL2230S:
		ret = RFbAL2230SelectChannel(priv, byChannel);
		break;
		 
	case RF_NOTHING:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}
	return ret;
}

 
bool rf_write_wake_prog_syn(struct vnt_private *priv, unsigned char rf_type,
			    u16 channel)
{
	void __iomem *iobase = priv->port_offset;
	int i;
	unsigned char init_count = 0;
	unsigned char sleep_count = 0;
	unsigned short idx = MISCFIFO_SYNDATA_IDX;

	iowrite16(0, iobase + MAC_REG_MISCFFNDEX);
	switch (rf_type) {
	case RF_AIROHA:
	case RF_AL2230S:

		if (channel > CB_MAX_CHANNEL_24G)
			return false;

		  
		init_count = CB_AL2230_INIT_SEQ + 2;
		sleep_count = 0;

		for (i = 0; i < CB_AL2230_INIT_SEQ; i++)
			MACvSetMISCFifo(priv, idx++, al2230_init_table[i]);

		MACvSetMISCFifo(priv, idx++, al2230_channel_table0[channel - 1]);
		MACvSetMISCFifo(priv, idx++, al2230_channel_table1[channel - 1]);
		break;

		 

	case RF_NOTHING:
		return true;

	default:
		return false;
	}

	MACvSetMISCFifo(priv, MISCFIFO_SYNINFO_IDX, (unsigned long)MAKEWORD(sleep_count, init_count));

	return true;
}

 
bool RFbSetPower(struct vnt_private *priv, unsigned int rate, u16 uCH)
{
	bool ret;
	unsigned char byPwr = 0;
	unsigned char byDec = 0;

	if (priv->dwDiagRefCount != 0)
		return true;

	if ((uCH < 1) || (uCH > CB_MAX_CHANNEL))
		return false;

	switch (rate) {
	case RATE_1M:
	case RATE_2M:
	case RATE_5M:
	case RATE_11M:
		if (uCH > CB_MAX_CHANNEL_24G)
			return false;

		byPwr = priv->abyCCKPwrTbl[uCH];
		break;
	case RATE_6M:
	case RATE_9M:
	case RATE_12M:
	case RATE_18M:
		byPwr = priv->abyOFDMPwrTbl[uCH];
		byDec = byPwr + 10;

		if (byDec >= priv->max_pwr_level)
			byDec = priv->max_pwr_level - 1;

		byPwr = byDec;
		break;
	case RATE_24M:
	case RATE_36M:
	case RATE_48M:
	case RATE_54M:
		byPwr = priv->abyOFDMPwrTbl[uCH];
		break;
	}

	if (priv->byCurPwr == byPwr)
		return true;

	ret = RFbRawSetPower(priv, byPwr, rate);
	if (ret)
		priv->byCurPwr = byPwr;

	return ret;
}

 

bool RFbRawSetPower(struct vnt_private *priv, unsigned char byPwr,
		    unsigned int rate)
{
	bool ret = true;

	if (byPwr >= priv->max_pwr_level)
		return false;

	switch (priv->byRFType) {
	case RF_AIROHA:
		ret &= IFRFbWriteEmbedded(priv, al2230_power_table[byPwr]);
		if (rate <= RATE_11M)
			ret &= IFRFbWriteEmbedded(priv, 0x0001B400 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW);
		else
			ret &= IFRFbWriteEmbedded(priv, 0x0005A400 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW);

		break;

	case RF_AL2230S:
		ret &= IFRFbWriteEmbedded(priv, al2230_power_table[byPwr]);
		if (rate <= RATE_11M) {
			ret &= IFRFbWriteEmbedded(priv, 0x040C1400 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW);
			ret &= IFRFbWriteEmbedded(priv, 0x00299B00 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW);
		} else {
			ret &= IFRFbWriteEmbedded(priv, 0x0005A400 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW);
			ret &= IFRFbWriteEmbedded(priv, 0x00099B00 + (BY_AL2230_REG_LEN << 3) + IFREGCTL_REGW);
		}

		break;

	default:
		break;
	}
	return ret;
}

 
void
RFvRSSITodBm(struct vnt_private *priv, unsigned char byCurrRSSI, long *pldBm)
{
	unsigned char byIdx = (((byCurrRSSI & 0xC0) >> 6) & 0x03);
	long b = (byCurrRSSI & 0x3F);
	long a = 0;
	unsigned char abyAIROHARF[4] = {0, 18, 0, 40};

	switch (priv->byRFType) {
	case RF_AIROHA:
	case RF_AL2230S:
		a = abyAIROHARF[byIdx];
		break;
	default:
		break;
	}

	*pldBm = -1 * (a + b * 2);
}

