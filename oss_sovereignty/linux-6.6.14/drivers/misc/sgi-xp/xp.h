 

 

#ifndef _DRIVERS_MISC_SGIXP_XP_H
#define _DRIVERS_MISC_SGIXP_XP_H

#include <linux/mutex.h>

#if defined CONFIG_X86_UV || defined CONFIG_IA64_SGI_UV
#include <asm/uv/uv.h>
#endif

#ifdef USE_DBUG_ON
#define DBUG_ON(condition)	BUG_ON(condition)
#else
#define DBUG_ON(condition)
#endif

 
#define XP_MAX_NPARTITIONS_SN2	64
#define XP_MAX_NPARTITIONS_UV	256

 
#define XPC_MEM_CHANNEL		0	 
#define	XPC_NET_CHANNEL		1	 

#define XPC_MAX_NCHANNELS	2	 

#if XPC_MAX_NCHANNELS > 8
#error	XPC_MAX_NCHANNELS exceeds absolute MAXIMUM possible.
#endif

 
#define XPC_MSG_MAX_SIZE	128
#define XPC_MSG_HDR_MAX_SIZE	16
#define XPC_MSG_PAYLOAD_MAX_SIZE (XPC_MSG_MAX_SIZE - XPC_MSG_HDR_MAX_SIZE)

#define XPC_MSG_SIZE(_payload_size) \
				ALIGN(XPC_MSG_HDR_MAX_SIZE + (_payload_size), \
				      is_uv_system() ? 64 : 128)


 
enum xp_retval {
	xpSuccess = 0,

	xpNotConnected,		 
	xpConnected,		 
	xpRETIRED1,		 

	xpMsgReceived,		 
	xpMsgDelivered,		 

	xpRETIRED2,		 

	xpNoWait,		 
	xpRetry,		 
	xpTimeout,		 
	xpInterrupted,		 

	xpUnequalMsgSizes,	 
	xpInvalidAddress,	 

	xpNoMemory,		 
	xpLackOfResources,	 
	xpUnregistered,		 
	xpAlreadyRegistered,	 

	xpPartitionDown,	 
	xpNotLoaded,		 
	xpUnloading,		 

	xpBadMagic,		 

	xpReactivating,		 

	xpUnregistering,	 
	xpOtherUnregistering,	 

	xpCloneKThread,		 
	xpCloneKThreadFailed,	 

	xpNoHeartbeat,		 

	xpPioReadError,		 
	xpPhysAddrRegFailed,	 

	xpRETIRED3,		 
	xpRETIRED4,		 
	xpRETIRED5,		 
	xpRETIRED6,		 
	xpRETIRED7,		 
	xpRETIRED8,		 
	xpRETIRED9,		 
	xpRETIRED10,		 
	xpRETIRED11,		 
	xpRETIRED12,		 

	xpBadVersion,		 
	xpVarsNotSet,		 
	xpNoRsvdPageAddr,	 
	xpInvalidPartid,	 
	xpLocalPartid,		 

	xpOtherGoingDown,	 
	xpSystemGoingDown,	 
	xpSystemHalt,		 
	xpSystemReboot,		 
	xpSystemPoweroff,	 

	xpDisconnecting,	 

	xpOpenCloseError,	 

	xpDisconnected,		 

	xpBteCopyError,		 
	xpSalError,		 
	xpRsvdPageNotSet,	 
	xpPayloadTooBig,	 

	xpUnsupported,		 
	xpNeedMoreInfo,		 

	xpGruCopyError,		 
	xpGruSendMqError,	 

	xpBadChannelNumber,	 
	xpBadMsgType,		 
	xpBiosError,		 

	xpUnknownReason		 
};

 
typedef void (*xpc_channel_func) (enum xp_retval reason, short partid,
				  int ch_number, void *data, void *key);

 
typedef void (*xpc_notify_func) (enum xp_retval reason, short partid,
				 int ch_number, void *key);

 
struct xpc_registration {
	struct mutex mutex;
	xpc_channel_func func;	 
	void *key;		 
	u16 nentries;		 
	u16 entry_size;		 
	u32 assigned_limit;	 
	u32 idle_limit;		 
} ____cacheline_aligned;

#define XPC_CHANNEL_REGISTERED(_c)	(xpc_registrations[_c].func != NULL)

 
#define XPC_WAIT	0	 
#define XPC_NOWAIT	1	 

struct xpc_interface {
	void (*connect) (int);
	void (*disconnect) (int);
	enum xp_retval (*send) (short, int, u32, void *, u16);
	enum xp_retval (*send_notify) (short, int, u32, void *, u16,
					xpc_notify_func, void *);
	void (*received) (short, int, void *);
	enum xp_retval (*partid_to_nasids) (short, void *);
};

extern struct xpc_interface xpc_interface;

extern void xpc_set_interface(void (*)(int),
			      void (*)(int),
			      enum xp_retval (*)(short, int, u32, void *, u16),
			      enum xp_retval (*)(short, int, u32, void *, u16,
						 xpc_notify_func, void *),
			      void (*)(short, int, void *),
			      enum xp_retval (*)(short, void *));
extern void xpc_clear_interface(void);

extern enum xp_retval xpc_connect(int, xpc_channel_func, void *, u16,
				   u16, u32, u32);
extern void xpc_disconnect(int);

static inline enum xp_retval
xpc_send(short partid, int ch_number, u32 flags, void *payload,
	 u16 payload_size)
{
	if (!xpc_interface.send)
		return xpNotLoaded;

	return xpc_interface.send(partid, ch_number, flags, payload,
				  payload_size);
}

static inline enum xp_retval
xpc_send_notify(short partid, int ch_number, u32 flags, void *payload,
		u16 payload_size, xpc_notify_func func, void *key)
{
	if (!xpc_interface.send_notify)
		return xpNotLoaded;

	return xpc_interface.send_notify(partid, ch_number, flags, payload,
					 payload_size, func, key);
}

static inline void
xpc_received(short partid, int ch_number, void *payload)
{
	if (xpc_interface.received)
		xpc_interface.received(partid, ch_number, payload);
}

static inline enum xp_retval
xpc_partid_to_nasids(short partid, void *nasids)
{
	if (!xpc_interface.partid_to_nasids)
		return xpNotLoaded;

	return xpc_interface.partid_to_nasids(partid, nasids);
}

extern short xp_max_npartitions;
extern short xp_partition_id;
extern u8 xp_region_size;

extern unsigned long (*xp_pa) (void *);
extern unsigned long (*xp_socket_pa) (unsigned long);
extern enum xp_retval (*xp_remote_memcpy) (unsigned long, const unsigned long,
		       size_t);
extern int (*xp_cpu_to_nasid) (int);
extern enum xp_retval (*xp_expand_memprotect) (unsigned long, unsigned long);
extern enum xp_retval (*xp_restrict_memprotect) (unsigned long, unsigned long);

extern struct device *xp;
extern enum xp_retval xp_init_uv(void);
extern void xp_exit_uv(void);

#endif  
