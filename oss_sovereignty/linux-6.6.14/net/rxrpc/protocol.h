 
 

#ifndef _LINUX_RXRPC_PACKET_H
#define _LINUX_RXRPC_PACKET_H

typedef u32	rxrpc_seq_t;	 
typedef u32	rxrpc_serial_t;	 
typedef __be32	rxrpc_seq_net_t;  
typedef __be32	rxrpc_serial_net_t;  

 
 
struct rxrpc_wire_header {
	__be32		epoch;		 
#define RXRPC_RANDOM_EPOCH	0x80000000	 

	__be32		cid;		 
#define RXRPC_MAXCALLS		4			 
#define RXRPC_CHANNELMASK	(RXRPC_MAXCALLS-1)	 
#define RXRPC_CIDMASK		(~RXRPC_CHANNELMASK)	 
#define RXRPC_CIDSHIFT		ilog2(RXRPC_MAXCALLS)	 
#define RXRPC_CID_INC		(1 << RXRPC_CIDSHIFT)	 

	__be32		callNumber;	 
	__be32		seq;		 
	__be32		serial;		 

	uint8_t		type;		 
#define RXRPC_PACKET_TYPE_DATA		1	 
#define RXRPC_PACKET_TYPE_ACK		2	 
#define RXRPC_PACKET_TYPE_BUSY		3	 
#define RXRPC_PACKET_TYPE_ABORT		4	 
#define RXRPC_PACKET_TYPE_ACKALL	5	 
#define RXRPC_PACKET_TYPE_CHALLENGE	6	 
#define RXRPC_PACKET_TYPE_RESPONSE	7	 
#define RXRPC_PACKET_TYPE_DEBUG		8	 
#define RXRPC_PACKET_TYPE_PARAMS	9	 
#define RXRPC_PACKET_TYPE_10		10	 
#define RXRPC_PACKET_TYPE_11		11	 
#define RXRPC_PACKET_TYPE_VERSION	13	 

	uint8_t		flags;		 
#define RXRPC_CLIENT_INITIATED	0x01		 
#define RXRPC_REQUEST_ACK	0x02		 
#define RXRPC_LAST_PACKET	0x04		 
#define RXRPC_MORE_PACKETS	0x08		 
#define RXRPC_JUMBO_PACKET	0x20		 
#define RXRPC_SLOW_START_OK	0x20		 

	uint8_t		userStatus;	 
#define RXRPC_USERSTATUS_SERVICE_UPGRADE 0x01	 

	uint8_t		securityIndex;	 
	union {
		__be16	_rsvd;		 
		__be16	cksum;		 
	};
	__be16		serviceId;	 

} __packed;

 
 
struct rxrpc_jumbo_header {
	uint8_t		flags;		 
	uint8_t		pad;
	union {
		__be16	_rsvd;		 
		__be16	cksum;		 
	};
} __packed;

#define RXRPC_JUMBO_DATALEN	1412	 
#define RXRPC_JUMBO_SUBPKTLEN	(RXRPC_JUMBO_DATALEN + sizeof(struct rxrpc_jumbo_header))

 
#define RXRPC_MAX_NR_JUMBO	47

 
 
struct rxrpc_ackpacket {
	__be16		bufferSpace;	 
	__be16		maxSkew;	 
	__be32		firstPacket;	 
	__be32		previousPacket;	 
	__be32		serial;		 

	uint8_t		reason;		 
#define RXRPC_ACK_REQUESTED		1	 
#define RXRPC_ACK_DUPLICATE		2	 
#define RXRPC_ACK_OUT_OF_SEQUENCE	3	 
#define RXRPC_ACK_EXCEEDS_WINDOW	4	 
#define RXRPC_ACK_NOSPACE		5	 
#define RXRPC_ACK_PING			6	 
#define RXRPC_ACK_PING_RESPONSE		7	 
#define RXRPC_ACK_DELAY			8	 
#define RXRPC_ACK_IDLE			9	 
#define RXRPC_ACK__INVALID		10	 

	uint8_t		nAcks;		 
#define RXRPC_MAXACKS	255

	uint8_t		acks[];		 
#define RXRPC_ACK_TYPE_NACK		0
#define RXRPC_ACK_TYPE_ACK		1

} __packed;

 
struct rxrpc_ackinfo {
	__be32		rxMTU;		 
	__be32		maxMTU;		 
	__be32		rwind;		 
	__be32		jumbo_max;	 
};

 
 
struct rxkad_challenge {
	__be32		version;	 
	__be32		nonce;		 
	__be32		min_level;	 
	__be32		__padding;	 
} __packed;

 
 
struct rxkad_response {
	__be32		version;	 
	__be32		__pad;

	 
	struct {
		__be32		epoch;		 
		__be32		cid;		 
		__be32		checksum;	 
		__be32		securityIndex;	 
		__be32		call_id[4];	 
		__be32		inc_nonce;	 
		__be32		level;		 
	} encrypted;

	__be32		kvno;		 
	__be32		ticket_len;	 
} __packed;

#endif  
