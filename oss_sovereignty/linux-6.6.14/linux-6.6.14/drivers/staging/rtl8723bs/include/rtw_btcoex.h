#ifndef __RTW_BTCOEX_H__
#define __RTW_BTCOEX_H__
#include <drv_types.h>
#define	PACKET_NORMAL			0
#define	PACKET_DHCP				1
#define	PACKET_ARP				2
#define	PACKET_EAPOL			3
void rtw_btcoex_MediaStatusNotify(struct adapter *, u8 mediaStatus);
void rtw_btcoex_HaltNotify(struct adapter *);
void rtw_btcoex_RejectApAggregatedPacket(struct adapter *, u8 enable);
void rtw_btcoex_LPS_Enter(struct adapter *);
void rtw_btcoex_LPS_Leave(struct adapter *);
#endif  
