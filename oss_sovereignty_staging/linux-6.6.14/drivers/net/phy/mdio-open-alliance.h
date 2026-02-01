 
 

#ifndef __MDIO_OPEN_ALLIANCE__
#define __MDIO_OPEN_ALLIANCE__

#include <linux/mdio.h>

 

 
#define MDIO_OATC14_PLCA_IDVER	0xca00   
#define MDIO_OATC14_PLCA_CTRL0	0xca01	 
#define MDIO_OATC14_PLCA_CTRL1	0xca02	 
#define MDIO_OATC14_PLCA_STATUS	0xca03	 
#define MDIO_OATC14_PLCA_TOTMR	0xca04	 
#define MDIO_OATC14_PLCA_BURST	0xca05	 

 
#define MDIO_OATC14_PLCA_IDM	0xff00	 
#define MDIO_OATC14_PLCA_VER	0x00ff	 

 
#define MDIO_OATC14_PLCA_EN	BIT(15)  
#define MDIO_OATC14_PLCA_RST	BIT(14)  

 
#define MDIO_OATC14_PLCA_NCNT	0xff00	 
#define MDIO_OATC14_PLCA_ID	0x00ff	 

 
#define MDIO_OATC14_PLCA_PST	BIT(15)	 

 
#define MDIO_OATC14_PLCA_TOT	0x00ff

 
#define MDIO_OATC14_PLCA_MAXBC	0xff00
#define MDIO_OATC14_PLCA_BTMR	0x00ff

 
#define OATC14_IDM		0x0a00

#endif  
