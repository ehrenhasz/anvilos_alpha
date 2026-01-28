#ifndef __RTW89_EFUSE_H__
#define __RTW89_EFUSE_H__
#include "core.h"
int rtw89_parse_efuse_map(struct rtw89_dev *rtwdev);
int rtw89_parse_phycap_map(struct rtw89_dev *rtwdev);
int rtw89_read_efuse_ver(struct rtw89_dev *rtwdev, u8 *efv);
#endif
