#ifndef	__USA49MSG__
#define	__USA49MSG__
struct keyspan_usa49_portControlMessage
{
	u8	portNumber,
		setClocking,	 
		baudLo,			 
		baudHi,			 
		prescaler,		 
		txClocking,		 
		rxClocking,		 
		setLcr,			 
		lcr,			 
		setFlowControl,	 
		ctsFlowControl,	 
		xonFlowControl,	 
		xonChar,		 
		xoffChar,		 
		setRts,			 
		rts,			 
		setDtr,			 
		dtr;			 
	u8	forwardingLength,   
		dsrFlowControl,	 
		txAckThreshold,	 
		loopbackMode;	 
	u8	_txOn,			 
		_txOff,			 
		txFlush,		 
		txBreak,		 
		rxOn,			 
		rxOff,			 
		rxFlush,		 
		rxForward,		 
		returnStatus,	 
		resetDataToggle, 
		enablePort,		 
		disablePort;	 
};
#define	USA_DATABITS_5		0x00
#define	USA_DATABITS_6		0x01
#define	USA_DATABITS_7		0x02
#define	USA_DATABITS_8		0x03
#define	STOPBITS_5678_1		0x00	 
#define	STOPBITS_5_1p5		0x04	 
#define	STOPBITS_678_2		0x04	 
#define	USA_PARITY_NONE		0x00
#define	USA_PARITY_ODD		0x08
#define	USA_PARITY_EVEN		0x18
#define	PARITY_1			0x28
#define	PARITY_0			0x38
struct keyspan_usa49_globalControlMessage
{
	u8	portNumber,			 
		sendGlobalStatus,	 
		resetStatusToggle,	 
		resetStatusCount,	 
		remoteWakeupEnable,		 
		disableStatusMessages;	 
};
struct keyspan_usa49_portStatusMessage	 
{
	u8	portNumber,		 
		cts,			 
		dcd,			 
		dsr,			 
		ri,				 
		_txOff,			 
		_txXoff,		 
		rxEnabled,		 
		controlResponse, 
		txAck,			 
		rs232valid;		 
};
#define	RXERROR_OVERRUN	0x02
#define	RXERROR_PARITY	0x04
#define	RXERROR_FRAMING	0x08
#define	RXERROR_BREAK	0x10
struct keyspan_usa49_globalStatusMessage
{
	u8	portNumber,			 
		sendGlobalStatus,	 
		resetStatusCount;	 
};
struct keyspan_usa49_globalDebugMessage
{
	u8	portNumber,			 
		n,					 
		b;					 
};
#define	MAX_DATA_LEN			64
#define	STATUS_UPDATE_INTERVAL	16
#define	STATUS_RATION	10
#endif
