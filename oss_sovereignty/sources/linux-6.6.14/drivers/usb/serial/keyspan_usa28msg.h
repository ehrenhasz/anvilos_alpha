


#ifndef	__USA28MSG__
#define	__USA28MSG__


struct keyspan_usa28_portControlMessage
{
	
	u8	setBaudRate,	
		baudLo,			
		baudHi;			

	
	u8	parity,			
		ctsFlowControl,	        
					
		xonFlowControl,	
		rts,			
		dtr;			

	
	u8	forwardingLength,  
		forwardMs,		
		breakThreshold,	
		xonChar,		
		xoffChar;		

	
	u8	_txOn,			
		_txOff,			
		txFlush,		
		txForceXoff,	
		txBreak,		
		rxOn,			
		rxOff,			
		rxFlush,		
		rxForward,		
		returnStatus,	
		resetDataToggle;
	
};

struct keyspan_usa28_portStatusMessage
{
	u8	port,			
		cts,
		dsr,			
		dcd,

		ri,				
		_txOff,			
		_txXoff,		
		dataLost,		

		rxEnabled,		
		rxBreak,		
		rs232invalid,	
		controlResponse;
};


#define	TX_OFF			0x01	
#define	TX_XOFF			0x02	

struct keyspan_usa28_globalControlMessage
{
	u8	sendGlobalStatus,	
		resetStatusToggle,	
		resetStatusCount;	
};

struct keyspan_usa28_globalStatusMessage
{
	u8	port,				
		sendGlobalStatus,	
		resetStatusCount;	
};

struct keyspan_usa28_globalDebugMessage
{
	u8	port,				
		n,					
		b;					
};


#define	MAX_DATA_LEN			64


#define	RX_PARITY_BIT			0x04
#define	TX_PARITY_BIT			0x01


#define	STATUS_UPDATE_INTERVAL	16

#endif

