
#ifndef __CF_FSI_FW_H
#define __CF_FSI_FW_H





#define HDR_OFFSET	        0x400


#define HDR_SYS_SIG		0x00	
#define  SYS_SIG_SHARED		0x5348
#define  SYS_SIG_SPLIT		0x5350
#define HDR_FW_VERS		0x02	
#define HDR_API_VERS		0x04	
#define  API_VERSION_MAJ	2	
#define  API_VERSION_MIN	1
#define HDR_FW_OPTIONS		0x08	
#define  FW_OPTION_TRACE_EN	0x00000001	
#define	 FW_OPTION_CONT_CLOCK	0x00000002	
#define HDR_FW_SIZE		0x10	


#define HDR_CMD_STAT_AREA	0x80	
#define HDR_FW_CONTROL		0x84	
#define	 FW_CONTROL_CONT_CLOCK	0x00000002	
#define	 FW_CONTROL_DUMMY_RD	0x00000004	
#define	 FW_CONTROL_USE_STOP	0x00000008	
#define HDR_CLOCK_GPIO_VADDR	0x90	
#define HDR_CLOCK_GPIO_DADDR	0x92	
#define HDR_DATA_GPIO_VADDR	0x94	
#define HDR_DATA_GPIO_DADDR	0x96	
#define HDR_TRANS_GPIO_VADDR	0x98	
#define HDR_TRANS_GPIO_DADDR	0x9a	
#define HDR_CLOCK_GPIO_BIT	0x9c	
#define HDR_DATA_GPIO_BIT	0x9d	
#define HDR_TRANS_GPIO_BIT	0x9e	




#define	CMD_STAT_REG	        0x00
#define  CMD_REG_CMD_MASK	0x000000ff
#define  CMD_REG_CMD_SHIFT	0
#define	  CMD_NONE		0x00
#define	  CMD_COMMAND		0x01
#define	  CMD_BREAK		0x02
#define	  CMD_IDLE_CLOCKS	0x03 
#define   CMD_INVALID		0xff
#define  CMD_REG_CLEN_MASK	0x0000ff00
#define  CMD_REG_CLEN_SHIFT	8
#define  CMD_REG_RLEN_MASK	0x00ff0000
#define  CMD_REG_RLEN_SHIFT	16
#define  CMD_REG_STAT_MASK	0xff000000
#define  CMD_REG_STAT_SHIFT	24
#define	  STAT_WORKING		0x00
#define	  STAT_COMPLETE		0x01
#define	  STAT_ERR_INVAL_CMD	0x80
#define	  STAT_ERR_INVAL_IRQ	0x81
#define	  STAT_ERR_MTOE		0x82


#define	STAT_RTAG		0x04


#define	STAT_RCRC		0x05


#define	ECHO_DLY_REG		0x08
#define	SEND_DLY_REG		0x09


#define	CMD_DATA		0x10 


#define	RSP_DATA		0x20 


#define	INT_CNT			0x30 
#define	BAD_INT_VEC		0x34 
#define	CF_STARTED		0x38 
#define	CLK_CNT			0x3c 


#define ARB_REG			0x40
#define  ARB_ARM_REQ		0x01
#define  ARB_ARM_ACK		0x02


#define CF_RESET_D0		0x50
#define CF_RESET_D1		0x54
#define BAD_INT_S0		0x58
#define BAD_INT_S1		0x5c
#define STOP_CNT		0x60




#define	TRACEBUF		0x100
#define	  TR_CLKOBIT0		0xc0
#define	  TR_CLKOBIT1		0xc1
#define	  TR_CLKOSTART		0x82
#define	  TR_OLEN		0x83 
#define	  TR_CLKZ		0x84 
#define	  TR_CLKWSTART		0x85
#define	  TR_CLKTAG		0x86 
#define	  TR_CLKDATA		0x87 
#define	  TR_CLKCRC		0x88 
#define	  TR_CLKIBIT0		0x90
#define	  TR_CLKIBIT1		0x91
#define	  TR_END		0xff

#endif 

