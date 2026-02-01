
 
 
#include <drv_types.h>
#include <rtw_debug.h>
#include <HalPwrSeqCmd.h>


 
 
 
 
 
 
 
 
 
u8 HalPwrSeqCmdParsing(
	struct adapter *padapter,
	u8 CutVersion,
	u8 FabVersion,
	u8 InterfaceType,
	struct wlan_pwr_cfg PwrSeqCmd[]
)
{
	struct wlan_pwr_cfg PwrCfgCmd;
	u8 bPollingBit = false;
	u32 AryIdx = 0;
	u8 value = 0;
	u32 offset = 0;
	u32 pollingCount = 0;  
	u32 maxPollingCnt = 5000;

	do {
		PwrCfgCmd = PwrSeqCmd[AryIdx];

		 
		if (
			(GET_PWR_CFG_FAB_MASK(PwrCfgCmd) & FabVersion) &&
			(GET_PWR_CFG_CUT_MASK(PwrCfgCmd) & CutVersion) &&
			(GET_PWR_CFG_INTF_MASK(PwrCfgCmd) & InterfaceType)
		) {
			switch (GET_PWR_CFG_CMD(PwrCfgCmd)) {
			case PWR_CMD_READ:
				break;

			case PWR_CMD_WRITE:
				offset = GET_PWR_CFG_OFFSET(PwrCfgCmd);

				 
				 
				 
				 
				if (GET_PWR_CFG_BASE(PwrCfgCmd) == PWR_BASEADDR_SDIO) {
					 
					value = SdioLocalCmd52Read1Byte(padapter, offset);

					value &= ~(GET_PWR_CFG_MASK(PwrCfgCmd));
					value |= (
						GET_PWR_CFG_VALUE(PwrCfgCmd) &
						GET_PWR_CFG_MASK(PwrCfgCmd)
					);

					 
					SdioLocalCmd52Write1Byte(padapter, offset, value);
				} else {
					 
					value = rtw_read8(padapter, offset);

					value &= (~(GET_PWR_CFG_MASK(PwrCfgCmd)));
					value |= (
						GET_PWR_CFG_VALUE(PwrCfgCmd)
						&GET_PWR_CFG_MASK(PwrCfgCmd)
					);

					 
					rtw_write8(padapter, offset, value);
				}
				break;

			case PWR_CMD_POLLING:

				bPollingBit = false;
				offset = GET_PWR_CFG_OFFSET(PwrCfgCmd);
				do {
					if (GET_PWR_CFG_BASE(PwrCfgCmd) == PWR_BASEADDR_SDIO)
						value = SdioLocalCmd52Read1Byte(padapter, offset);
					else
						value = rtw_read8(padapter, offset);

					value = value&GET_PWR_CFG_MASK(PwrCfgCmd);
					if (
						value == (GET_PWR_CFG_VALUE(PwrCfgCmd) &
						GET_PWR_CFG_MASK(PwrCfgCmd))
					)
						bPollingBit = true;
					else
						udelay(10);

					if (pollingCount++ > maxPollingCnt)
						return false;

				} while (!bPollingBit);

				break;

			case PWR_CMD_DELAY:
				if (GET_PWR_CFG_VALUE(PwrCfgCmd) == PWRSEQ_DELAY_US)
					udelay(GET_PWR_CFG_OFFSET(PwrCfgCmd));
				else
					udelay(GET_PWR_CFG_OFFSET(PwrCfgCmd)*1000);
				break;

			case PWR_CMD_END:
				 
				return true;

			default:
				break;
			}
		}

		AryIdx++; 
	} while (1);

	return true;
}
