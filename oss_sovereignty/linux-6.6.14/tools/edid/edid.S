 


 
#define MFG_LNX1 'L'
#define MFG_LNX2 'N'
#define MFG_LNX3 'X'
#define SERIAL 0
#define YEAR 2012
#define WEEK 5

 
#define XY_RATIO_16_10	0b00
#define XY_RATIO_4_3	0b01
#define XY_RATIO_5_4	0b10
#define XY_RATIO_16_9	0b11

 
#ifndef ESTABLISHED_TIMING1_BITS
#define ESTABLISHED_TIMING1_BITS 0x00
#endif
#ifndef ESTABLISHED_TIMING2_BITS
#define ESTABLISHED_TIMING2_BITS 0x00
#endif
#ifndef ESTABLISHED_TIMING3_BITS
#define ESTABLISHED_TIMING3_BITS 0x00
#endif

#define mfgname2id(v1,v2,v3) \
	((((v1-'@')&0x1f)<<10)+(((v2-'@')&0x1f)<<5)+((v3-'@')&0x1f))
#define swap16(v1) ((v1>>8)+((v1&0xff)<<8))
#define lsbs2(v1,v2) (((v1&0x0f)<<4)+(v2&0x0f))
#define msbs2(v1,v2) ((((v1>>8)&0x0f)<<4)+((v2>>8)&0x0f))
#define msbs4(v1,v2,v3,v4) \
	((((v1>>8)&0x03)<<6)+(((v2>>8)&0x03)<<4)+\
	(((v3>>4)&0x03)<<2)+((v4>>4)&0x03))
#define pixdpi2mm(pix,dpi) ((pix*25)/dpi)
#define xsize pixdpi2mm(XPIX,DPI)
#define ysize pixdpi2mm(YPIX,DPI)

		.data

 
header:		.byte	0x00,0xff,0xff,0xff,0xff,0xff,0xff,0x00

mfg_id:		.hword	swap16(mfgname2id(MFG_LNX1, MFG_LNX2, MFG_LNX3))

prod_code:	.hword	0

 
serial_number:	.long	SERIAL

 
week:		.byte	WEEK

 
year:		.byte	YEAR-1990

version:	.byte	VERSION 	 
revision:	.byte	REVISION	 

 
video_parms:	.byte	0x6d

 
max_hor_size:	.byte	xsize/10

 
max_vert_size:	.byte	ysize/10

 
gamma:		.byte	120

 
dsp_features:	.byte	0xea

 
 
red_green_lsb:	.byte	0x5e

 
blue_white_lsb:	.byte	0xc0

 
red_x_msb:	.byte	0xa4

 
red_y_msb:	.byte	0x59

 
green_x_y_msb:	.byte	0x4a,0x98

 
blue_x_y_msb:	.byte	0x25,0x20

 
white_x_y_msb:	.byte	0x50,0x54

 
 
estbl_timing1:	.byte	ESTABLISHED_TIMING1_BITS

 
estbl_timing2:	.byte	ESTABLISHED_TIMING2_BITS

 
estbl_timing3:	.byte	ESTABLISHED_TIMING3_BITS

 
 
std_xres:	.byte	(XPIX/8)-31
 
std_vres:	.byte	(XY_RATIO<<6)+VFREQ-60
		.fill	7,2,0x0101	 

descriptor1:
 
clock:		.hword	CLOCK/10

 
x_act_lsb:	.byte	XPIX&0xff
 
x_blk_lsb:	.byte	XBLANK&0xff
 
x_msbs:		.byte	msbs2(XPIX,XBLANK)

 
y_act_lsb:	.byte	YPIX&0xff
 
y_blk_lsb:	.byte	YBLANK&0xff
 
y_msbs:		.byte	msbs2(YPIX,YBLANK)

 
x_snc_off_lsb:	.byte	XOFFSET&0xff
 
x_snc_pls_lsb:	.byte	XPULSE&0xff
 
y_snc_lsb:	.byte	lsbs2(YOFFSET, YPULSE)
 
xy_snc_msbs:	.byte	msbs4(XOFFSET,XPULSE,YOFFSET,YPULSE)

 
x_dsp_size:	.byte	xsize&0xff

 
y_dsp_size:	.byte	ysize&0xff

 
dsp_size_mbsb:	.byte	msbs2(xsize,ysize)

 
x_border:	.byte	0
 
y_border:	.byte	0

 
features:	.byte	0x18+(VSYNC_POL<<2)+(HSYNC_POL<<1)

descriptor2:	.byte	0,0	 
		.byte	0	 
		.byte	0xff	 
		.byte	0	 
start1:		.ascii	"Linux #0"
end1:		.byte	0x0a	 
		.fill	12-(end1-start1), 1, 0x20  
descriptor3:	.byte	0,0	 
		.byte	0	 
		.byte	0xfd	 
		.byte	0	 
start2:		.byte	VFREQ-1	 
		.byte	VFREQ+1	 
		.byte	(CLOCK/(XPIX+XBLANK))-1  
		.byte	(CLOCK/(XPIX+XBLANK))+1  
		.byte	(CLOCK/10000)+1	 
		.byte	0	 
end2:		.byte	0x0a	 
		.fill	12-(end2-start2), 1, 0x20  
descriptor4:	.byte	0,0	 
		.byte	0	 
		.byte	0xfc	 
		.byte	0	 
start3:		.ascii	TIMING_NAME
end3:		.byte	0x0a	 
		.fill	12-(end3-start3), 1, 0x20  
extensions:	.byte	0	 
checksum:	.byte	CRC	 
