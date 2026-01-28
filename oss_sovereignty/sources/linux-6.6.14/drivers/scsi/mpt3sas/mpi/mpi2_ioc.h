


#ifndef MPI2_IOC_H
#define MPI2_IOC_H






typedef struct _MPI2_IOC_INIT_REQUEST {
	U8 WhoInit;		
	U8 Reserved1;		
	U8 ChainOffset;		
	U8 Function;		
	U16 Reserved2;		
	U8 Reserved3;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved4;		
	U16 MsgVersion;		
	U16 HeaderVersion;	
	U32 Reserved5;		
	U16 ConfigurationFlags;	
	U8 HostPageSize;	
	U8 HostMSIxVectors;	
	U16 Reserved8;		
	U16 SystemRequestFrameSize;	
	U16 ReplyDescriptorPostQueueDepth;	
	U16 ReplyFreeQueueDepth;	
	U32 SenseBufferAddressHigh;	
	U32 SystemReplyAddressHigh;	
	U64 SystemRequestFrameBaseAddress;	
	U64 ReplyDescriptorPostQueueAddress;	
	U64 ReplyFreeQueueAddress;	
	U64 TimeStamp;		
} MPI2_IOC_INIT_REQUEST, *PTR_MPI2_IOC_INIT_REQUEST,
	Mpi2IOCInitRequest_t, *pMpi2IOCInitRequest_t;


#define MPI2_WHOINIT_NOT_INITIALIZED            (0x00)
#define MPI2_WHOINIT_SYSTEM_BIOS                (0x01)
#define MPI2_WHOINIT_ROM_BIOS                   (0x02)
#define MPI2_WHOINIT_PCI_PEER                   (0x03)
#define MPI2_WHOINIT_HOST_DRIVER                (0x04)
#define MPI2_WHOINIT_MANUFACTURER               (0x05)


#define MPI2_IOCINIT_MSGFLAG_RDPQ_ARRAY_MODE    (0x01)



#define MPI2_IOCINIT_MSGVERSION_MAJOR_MASK      (0xFF00)
#define MPI2_IOCINIT_MSGVERSION_MAJOR_SHIFT     (8)
#define MPI2_IOCINIT_MSGVERSION_MINOR_MASK      (0x00FF)
#define MPI2_IOCINIT_MSGVERSION_MINOR_SHIFT     (0)


#define MPI2_IOCINIT_HDRVERSION_UNIT_MASK       (0xFF00)
#define MPI2_IOCINIT_HDRVERSION_UNIT_SHIFT      (8)
#define MPI2_IOCINIT_HDRVERSION_DEV_MASK        (0x00FF)
#define MPI2_IOCINIT_HDRVERSION_DEV_SHIFT       (0)


#define MPI26_IOCINIT_CFGFLAGS_NVME_SGL_FORMAT  (0x0001)
#define MPI26_IOCINIT_CFGFLAGS_COREDUMP_ENABLE  (0x0002)


#define MPI2_RDPQ_DEPTH_MIN                     (16)


typedef struct _MPI2_IOC_INIT_RDPQ_ARRAY_ENTRY {
	U64                 RDPQBaseAddress;                    
	U32                 Reserved1;                          
	U32                 Reserved2;                          
} MPI2_IOC_INIT_RDPQ_ARRAY_ENTRY,
*PTR_MPI2_IOC_INIT_RDPQ_ARRAY_ENTRY,
Mpi2IOCInitRDPQArrayEntry, *pMpi2IOCInitRDPQArrayEntry;



typedef struct _MPI2_IOC_INIT_REPLY {
	U8 WhoInit;		
	U8 Reserved1;		
	U8 MsgLength;		
	U8 Function;		
	U16 Reserved2;		
	U8 Reserved3;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved4;		
	U16 Reserved5;		
	U16 IOCStatus;		
	U32 IOCLogInfo;		
} MPI2_IOC_INIT_REPLY, *PTR_MPI2_IOC_INIT_REPLY,
	Mpi2IOCInitReply_t, *pMpi2IOCInitReply_t;




typedef struct _MPI2_IOC_FACTS_REQUEST {
	U16 Reserved1;		
	U8 ChainOffset;		
	U8 Function;		
	U16 Reserved2;		
	U8 Reserved3;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved4;		
} MPI2_IOC_FACTS_REQUEST, *PTR_MPI2_IOC_FACTS_REQUEST,
	Mpi2IOCFactsRequest_t, *pMpi2IOCFactsRequest_t;


typedef struct _MPI2_IOC_FACTS_REPLY {
	U16 MsgVersion;		
	U8 MsgLength;		
	U8 Function;		
	U16 HeaderVersion;	
	U8 IOCNumber;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved1;		
	U16 IOCExceptions;	
	U16 IOCStatus;		
	U32 IOCLogInfo;		
	U8 MaxChainDepth;	
	U8 WhoInit;		
	U8 NumberOfPorts;	
	U8 MaxMSIxVectors;	
	U16 RequestCredit;	
	U16 ProductID;		
	U32 IOCCapabilities;	
	MPI2_VERSION_UNION FWVersion;	
	U16 IOCRequestFrameSize;	
	U16 IOCMaxChainSegmentSize;	
	U16 MaxInitiators;	
	U16 MaxTargets;		
	U16 MaxSasExpanders;	
	U16 MaxEnclosures;	
	U16 ProtocolFlags;	
	U16 HighPriorityCredit;	
	U16 MaxReplyDescriptorPostQueueDepth;	
	U8 ReplyFrameSize;	
	U8 MaxVolumes;		
	U16 MaxDevHandle;	
	U16 MaxPersistentEntries;	
	U16 MinDevHandle;	
	U8 CurrentHostPageSize;	
	U8 Reserved4;		
	U8 SGEModifierMask;	
	U8 SGEModifierValue;	
	U8 SGEModifierShift;	
	U8 Reserved5;		
} MPI2_IOC_FACTS_REPLY, *PTR_MPI2_IOC_FACTS_REPLY,
	Mpi2IOCFactsReply_t, *pMpi2IOCFactsReply_t;


#define MPI2_IOCFACTS_MSGVERSION_MAJOR_MASK             (0xFF00)
#define MPI2_IOCFACTS_MSGVERSION_MAJOR_SHIFT            (8)
#define MPI2_IOCFACTS_MSGVERSION_MINOR_MASK             (0x00FF)
#define MPI2_IOCFACTS_MSGVERSION_MINOR_SHIFT            (0)


#define MPI2_IOCFACTS_HDRVERSION_UNIT_MASK              (0xFF00)
#define MPI2_IOCFACTS_HDRVERSION_UNIT_SHIFT             (8)
#define MPI2_IOCFACTS_HDRVERSION_DEV_MASK               (0x00FF)
#define MPI2_IOCFACTS_HDRVERSION_DEV_SHIFT              (0)


#define MPI2_IOCFACTS_EXCEPT_PCIE_DISABLED              (0x0400)
#define MPI2_IOCFACTS_EXCEPT_PARTIAL_MEMORY_FAILURE     (0x0200)
#define MPI2_IOCFACTS_EXCEPT_IR_FOREIGN_CONFIG_MAX      (0x0100)

#define MPI2_IOCFACTS_EXCEPT_BOOTSTAT_MASK              (0x00E0)
#define MPI2_IOCFACTS_EXCEPT_BOOTSTAT_GOOD              (0x0000)
#define MPI2_IOCFACTS_EXCEPT_BOOTSTAT_BACKUP            (0x0020)
#define MPI2_IOCFACTS_EXCEPT_BOOTSTAT_RESTORED          (0x0040)
#define MPI2_IOCFACTS_EXCEPT_BOOTSTAT_CORRUPT_BACKUP    (0x0060)

#define MPI2_IOCFACTS_EXCEPT_METADATA_UNSUPPORTED       (0x0010)
#define MPI2_IOCFACTS_EXCEPT_MANUFACT_CHECKSUM_FAIL     (0x0008)
#define MPI2_IOCFACTS_EXCEPT_FW_CHECKSUM_FAIL           (0x0004)
#define MPI2_IOCFACTS_EXCEPT_RAID_CONFIG_INVALID        (0x0002)
#define MPI2_IOCFACTS_EXCEPT_CONFIG_CHECKSUM_FAIL       (0x0001)






#define MPI26_IOCFACTS_CAPABILITY_COREDUMP_ENABLED      (0x00200000)
#define MPI26_IOCFACTS_CAPABILITY_PCIE_SRIOV            (0x00100000)
#define MPI26_IOCFACTS_CAPABILITY_ATOMIC_REQ            (0x00080000)
#define MPI2_IOCFACTS_CAPABILITY_RDPQ_ARRAY_CAPABLE     (0x00040000)
#define MPI25_IOCFACTS_CAPABILITY_FAST_PATH_CAPABLE     (0x00020000)
#define MPI2_IOCFACTS_CAPABILITY_HOST_BASED_DISCOVERY   (0x00010000)
#define MPI2_IOCFACTS_CAPABILITY_MSI_X_INDEX            (0x00008000)
#define MPI2_IOCFACTS_CAPABILITY_RAID_ACCELERATOR       (0x00004000)
#define MPI2_IOCFACTS_CAPABILITY_EVENT_REPLAY           (0x00002000)
#define MPI2_IOCFACTS_CAPABILITY_INTEGRATED_RAID        (0x00001000)
#define MPI2_IOCFACTS_CAPABILITY_TLR                    (0x00000800)
#define MPI2_IOCFACTS_CAPABILITY_MULTICAST              (0x00000100)
#define MPI2_IOCFACTS_CAPABILITY_BIDIRECTIONAL_TARGET   (0x00000080)
#define MPI2_IOCFACTS_CAPABILITY_EEDP                   (0x00000040)
#define MPI2_IOCFACTS_CAPABILITY_EXTENDED_BUFFER        (0x00000020)
#define MPI2_IOCFACTS_CAPABILITY_SNAPSHOT_BUFFER        (0x00000010)
#define MPI2_IOCFACTS_CAPABILITY_DIAG_TRACE_BUFFER      (0x00000008)
#define MPI2_IOCFACTS_CAPABILITY_TASK_SET_FULL_HANDLING (0x00000004)


#define MPI2_IOCFACTS_PROTOCOL_NVME_DEVICES             (0x0008)
#define MPI2_IOCFACTS_PROTOCOL_SCSI_INITIATOR           (0x0002)
#define MPI2_IOCFACTS_PROTOCOL_SCSI_TARGET              (0x0001)




typedef struct _MPI2_PORT_FACTS_REQUEST {
	U16 Reserved1;		
	U8 ChainOffset;		
	U8 Function;		
	U16 Reserved2;		
	U8 PortNumber;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved3;		
} MPI2_PORT_FACTS_REQUEST, *PTR_MPI2_PORT_FACTS_REQUEST,
	Mpi2PortFactsRequest_t, *pMpi2PortFactsRequest_t;


typedef struct _MPI2_PORT_FACTS_REPLY {
	U16 Reserved1;		
	U8 MsgLength;		
	U8 Function;		
	U16 Reserved2;		
	U8 PortNumber;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved3;		
	U16 Reserved4;		
	U16 IOCStatus;		
	U32 IOCLogInfo;		
	U8 Reserved5;		
	U8 PortType;		
	U16 Reserved6;		
	U16 MaxPostedCmdBuffers;	
	U16 Reserved7;		
} MPI2_PORT_FACTS_REPLY, *PTR_MPI2_PORT_FACTS_REPLY,
	Mpi2PortFactsReply_t, *pMpi2PortFactsReply_t;


#define MPI2_PORTFACTS_PORTTYPE_INACTIVE            (0x00)
#define MPI2_PORTFACTS_PORTTYPE_FC                  (0x10)
#define MPI2_PORTFACTS_PORTTYPE_ISCSI               (0x20)
#define MPI2_PORTFACTS_PORTTYPE_SAS_PHYSICAL        (0x30)
#define MPI2_PORTFACTS_PORTTYPE_SAS_VIRTUAL         (0x31)
#define MPI2_PORTFACTS_PORTTYPE_TRI_MODE            (0x40)





typedef struct _MPI2_PORT_ENABLE_REQUEST {
	U16 Reserved1;		
	U8 ChainOffset;		
	U8 Function;		
	U8 Reserved2;		
	U8 PortFlags;		
	U8 Reserved3;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved4;		
} MPI2_PORT_ENABLE_REQUEST, *PTR_MPI2_PORT_ENABLE_REQUEST,
	Mpi2PortEnableRequest_t, *pMpi2PortEnableRequest_t;


typedef struct _MPI2_PORT_ENABLE_REPLY {
	U16 Reserved1;		
	U8 MsgLength;		
	U8 Function;		
	U8 Reserved2;		
	U8 PortFlags;		
	U8 Reserved3;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved4;		
	U16 Reserved5;		
	U16 IOCStatus;		
	U32 IOCLogInfo;		
} MPI2_PORT_ENABLE_REPLY, *PTR_MPI2_PORT_ENABLE_REPLY,
	Mpi2PortEnableReply_t, *pMpi2PortEnableReply_t;




#define MPI2_EVENT_NOTIFY_EVENTMASK_WORDS           (4)

typedef struct _MPI2_EVENT_NOTIFICATION_REQUEST {
	U16 Reserved1;		
	U8 ChainOffset;		
	U8 Function;		
	U16 Reserved2;		
	U8 Reserved3;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved4;		
	U32 Reserved5;		
	U32 Reserved6;		
	U32 EventMasks[MPI2_EVENT_NOTIFY_EVENTMASK_WORDS];	
	U16 SASBroadcastPrimitiveMasks;	
	U16 SASNotifyPrimitiveMasks;	
	U32 Reserved8;		
} MPI2_EVENT_NOTIFICATION_REQUEST,
	*PTR_MPI2_EVENT_NOTIFICATION_REQUEST,
	Mpi2EventNotificationRequest_t,
	*pMpi2EventNotificationRequest_t;


typedef struct _MPI2_EVENT_NOTIFICATION_REPLY {
	U16 EventDataLength;	
	U8 MsgLength;		
	U8 Function;		
	U16 Reserved1;		
	U8 AckRequired;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved2;		
	U16 Reserved3;		
	U16 IOCStatus;		
	U32 IOCLogInfo;		
	U16 Event;		
	U16 Reserved4;		
	U32 EventContext;	
	U32 EventData[];	
} MPI2_EVENT_NOTIFICATION_REPLY, *PTR_MPI2_EVENT_NOTIFICATION_REPLY,
	Mpi2EventNotificationReply_t,
	*pMpi2EventNotificationReply_t;


#define MPI2_EVENT_NOTIFICATION_ACK_NOT_REQUIRED    (0x00)
#define MPI2_EVENT_NOTIFICATION_ACK_REQUIRED        (0x01)


#define MPI2_EVENT_LOG_DATA                         (0x0001)
#define MPI2_EVENT_STATE_CHANGE                     (0x0002)
#define MPI2_EVENT_HARD_RESET_RECEIVED              (0x0005)
#define MPI2_EVENT_EVENT_CHANGE                     (0x000A)
#define MPI2_EVENT_TASK_SET_FULL                    (0x000E)	
#define MPI2_EVENT_SAS_DEVICE_STATUS_CHANGE         (0x000F)
#define MPI2_EVENT_IR_OPERATION_STATUS              (0x0014)
#define MPI2_EVENT_SAS_DISCOVERY                    (0x0016)
#define MPI2_EVENT_SAS_BROADCAST_PRIMITIVE          (0x0017)
#define MPI2_EVENT_SAS_INIT_DEVICE_STATUS_CHANGE    (0x0018)
#define MPI2_EVENT_SAS_INIT_TABLE_OVERFLOW          (0x0019)
#define MPI2_EVENT_SAS_TOPOLOGY_CHANGE_LIST         (0x001C)
#define MPI2_EVENT_SAS_ENCL_DEVICE_STATUS_CHANGE    (0x001D)
#define MPI2_EVENT_ENCL_DEVICE_STATUS_CHANGE        (0x001D)
#define MPI2_EVENT_IR_VOLUME                        (0x001E)
#define MPI2_EVENT_IR_PHYSICAL_DISK                 (0x001F)
#define MPI2_EVENT_IR_CONFIGURATION_CHANGE_LIST     (0x0020)
#define MPI2_EVENT_LOG_ENTRY_ADDED                  (0x0021)
#define MPI2_EVENT_SAS_PHY_COUNTER                  (0x0022)
#define MPI2_EVENT_GPIO_INTERRUPT                   (0x0023)
#define MPI2_EVENT_HOST_BASED_DISCOVERY_PHY         (0x0024)
#define MPI2_EVENT_SAS_QUIESCE                      (0x0025)
#define MPI2_EVENT_SAS_NOTIFY_PRIMITIVE             (0x0026)
#define MPI2_EVENT_TEMP_THRESHOLD                   (0x0027)
#define MPI2_EVENT_HOST_MESSAGE                     (0x0028)
#define MPI2_EVENT_POWER_PERFORMANCE_CHANGE         (0x0029)
#define MPI2_EVENT_PCIE_DEVICE_STATUS_CHANGE        (0x0030)
#define MPI2_EVENT_PCIE_ENUMERATION                 (0x0031)
#define MPI2_EVENT_PCIE_TOPOLOGY_CHANGE_LIST        (0x0032)
#define MPI2_EVENT_PCIE_LINK_COUNTER                (0x0033)
#define MPI2_EVENT_ACTIVE_CABLE_EXCEPTION           (0x0034)
#define MPI2_EVENT_SAS_DEVICE_DISCOVERY_ERROR       (0x0035)
#define MPI2_EVENT_MIN_PRODUCT_SPECIFIC             (0x006E)
#define MPI2_EVENT_MAX_PRODUCT_SPECIFIC             (0x007F)




#define MPI2_EVENT_DATA_LOG_DATA_LENGTH             (0x1C)

typedef struct _MPI2_EVENT_DATA_LOG_ENTRY_ADDED {
	U64 TimeStamp;		
	U32 Reserved1;		
	U16 LogSequence;	
	U16 LogEntryQualifier;	
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved2;		
	U8 LogData[MPI2_EVENT_DATA_LOG_DATA_LENGTH];	
} MPI2_EVENT_DATA_LOG_ENTRY_ADDED,
	*PTR_MPI2_EVENT_DATA_LOG_ENTRY_ADDED,
	Mpi2EventDataLogEntryAdded_t,
	*pMpi2EventDataLogEntryAdded_t;



typedef struct _MPI2_EVENT_DATA_GPIO_INTERRUPT {
	U8 GPIONum;		
	U8 Reserved1;		
	U16 Reserved2;		
} MPI2_EVENT_DATA_GPIO_INTERRUPT,
	*PTR_MPI2_EVENT_DATA_GPIO_INTERRUPT,
	Mpi2EventDataGpioInterrupt_t,
	*pMpi2EventDataGpioInterrupt_t;



typedef struct _MPI2_EVENT_DATA_TEMPERATURE {
	U16 Status;		
	U8 SensorNum;		
	U8 Reserved1;		
	U16 CurrentTemperature;	
	U16 Reserved2;		
	U32 Reserved3;		
	U32 Reserved4;		
} MPI2_EVENT_DATA_TEMPERATURE,
	*PTR_MPI2_EVENT_DATA_TEMPERATURE,
	Mpi2EventDataTemperature_t, *pMpi2EventDataTemperature_t;


#define MPI2_EVENT_TEMPERATURE3_EXCEEDED            (0x0008)
#define MPI2_EVENT_TEMPERATURE2_EXCEEDED            (0x0004)
#define MPI2_EVENT_TEMPERATURE1_EXCEEDED            (0x0002)
#define MPI2_EVENT_TEMPERATURE0_EXCEEDED            (0x0001)



typedef struct _MPI2_EVENT_DATA_HOST_MESSAGE {
	U8 SourceVF_ID;		
	U8 Reserved1;		
	U16 Reserved2;		
	U32 Reserved3;		
	U32 HostData[];		
} MPI2_EVENT_DATA_HOST_MESSAGE, *PTR_MPI2_EVENT_DATA_HOST_MESSAGE,
	Mpi2EventDataHostMessage_t, *pMpi2EventDataHostMessage_t;



typedef struct _MPI2_EVENT_DATA_POWER_PERF_CHANGE {
	U8 CurrentPowerMode;	
	U8 PreviousPowerMode;	
	U16 Reserved1;		
} MPI2_EVENT_DATA_POWER_PERF_CHANGE,
	*PTR_MPI2_EVENT_DATA_POWER_PERF_CHANGE,
	Mpi2EventDataPowerPerfChange_t,
	*pMpi2EventDataPowerPerfChange_t;


#define MPI2_EVENT_PM_INIT_MASK              (0xC0)
#define MPI2_EVENT_PM_INIT_UNAVAILABLE       (0x00)
#define MPI2_EVENT_PM_INIT_HOST              (0x40)
#define MPI2_EVENT_PM_INIT_IO_UNIT           (0x80)
#define MPI2_EVENT_PM_INIT_PCIE_DPA          (0xC0)

#define MPI2_EVENT_PM_MODE_MASK              (0x07)
#define MPI2_EVENT_PM_MODE_UNAVAILABLE       (0x00)
#define MPI2_EVENT_PM_MODE_UNKNOWN           (0x01)
#define MPI2_EVENT_PM_MODE_FULL_POWER        (0x04)
#define MPI2_EVENT_PM_MODE_REDUCED_POWER     (0x05)
#define MPI2_EVENT_PM_MODE_STANDBY           (0x06)



typedef struct _MPI26_EVENT_DATA_ACTIVE_CABLE_EXCEPT {
	U32         ActiveCablePowerRequirement;        
	U8          ReasonCode;                         
	U8          ReceptacleID;                       
	U16         Reserved1;                          
} MPI25_EVENT_DATA_ACTIVE_CABLE_EXCEPT,
	*PTR_MPI25_EVENT_DATA_ACTIVE_CABLE_EXCEPT,
	Mpi25EventDataActiveCableExcept_t,
	*pMpi25EventDataActiveCableExcept_t,
	MPI26_EVENT_DATA_ACTIVE_CABLE_EXCEPT,
	*PTR_MPI26_EVENT_DATA_ACTIVE_CABLE_EXCEPT,
	Mpi26EventDataActiveCableExcept_t,
	*pMpi26EventDataActiveCableExcept_t;


#define MPI25_EVENT_ACTIVE_CABLE_INSUFFICIENT_POWER     (0x00)
#define MPI25_EVENT_ACTIVE_CABLE_PRESENT                (0x01)
#define MPI25_EVENT_ACTIVE_CABLE_DEGRADED               (0x02)


#define MPI26_EVENT_ACTIVE_CABLE_INSUFFICIENT_POWER     (0x00)
#define MPI26_EVENT_ACTIVE_CABLE_PRESENT                (0x01)
#define MPI26_EVENT_ACTIVE_CABLE_DEGRADED               (0x02)



typedef struct _MPI2_EVENT_DATA_HARD_RESET_RECEIVED {
	U8 Reserved1;		
	U8 Port;		
	U16 Reserved2;		
} MPI2_EVENT_DATA_HARD_RESET_RECEIVED,
	*PTR_MPI2_EVENT_DATA_HARD_RESET_RECEIVED,
	Mpi2EventDataHardResetReceived_t,
	*pMpi2EventDataHardResetReceived_t;




typedef struct _MPI2_EVENT_DATA_TASK_SET_FULL {
	U16 DevHandle;		
	U16 CurrentDepth;	
} MPI2_EVENT_DATA_TASK_SET_FULL, *PTR_MPI2_EVENT_DATA_TASK_SET_FULL,
	Mpi2EventDataTaskSetFull_t, *pMpi2EventDataTaskSetFull_t;



typedef struct _MPI2_EVENT_DATA_SAS_DEVICE_STATUS_CHANGE {
	U16 TaskTag;		
	U8 ReasonCode;		
	U8 PhysicalPort;	
	U8 ASC;			
	U8 ASCQ;		
	U16 DevHandle;		
	U32 Reserved2;		
	U64 SASAddress;		
	U8 LUN[8];		
} MPI2_EVENT_DATA_SAS_DEVICE_STATUS_CHANGE,
	*PTR_MPI2_EVENT_DATA_SAS_DEVICE_STATUS_CHANGE,
	Mpi2EventDataSasDeviceStatusChange_t,
	*pMpi2EventDataSasDeviceStatusChange_t;


#define MPI2_EVENT_SAS_DEV_STAT_RC_SMART_DATA                           (0x05)
#define MPI2_EVENT_SAS_DEV_STAT_RC_UNSUPPORTED                          (0x07)
#define MPI2_EVENT_SAS_DEV_STAT_RC_INTERNAL_DEVICE_RESET                (0x08)
#define MPI2_EVENT_SAS_DEV_STAT_RC_TASK_ABORT_INTERNAL                  (0x09)
#define MPI2_EVENT_SAS_DEV_STAT_RC_ABORT_TASK_SET_INTERNAL              (0x0A)
#define MPI2_EVENT_SAS_DEV_STAT_RC_CLEAR_TASK_SET_INTERNAL              (0x0B)
#define MPI2_EVENT_SAS_DEV_STAT_RC_QUERY_TASK_INTERNAL                  (0x0C)
#define MPI2_EVENT_SAS_DEV_STAT_RC_ASYNC_NOTIFICATION                   (0x0D)
#define MPI2_EVENT_SAS_DEV_STAT_RC_CMP_INTERNAL_DEV_RESET               (0x0E)
#define MPI2_EVENT_SAS_DEV_STAT_RC_CMP_TASK_ABORT_INTERNAL              (0x0F)
#define MPI2_EVENT_SAS_DEV_STAT_RC_SATA_INIT_FAILURE                    (0x10)
#define MPI2_EVENT_SAS_DEV_STAT_RC_EXPANDER_REDUCED_FUNCTIONALITY       (0x11)
#define MPI2_EVENT_SAS_DEV_STAT_RC_CMP_EXPANDER_REDUCED_FUNCTIONALITY   (0x12)



typedef struct _MPI2_EVENT_DATA_IR_OPERATION_STATUS {
	U16 VolDevHandle;	
	U16 Reserved1;		
	U8 RAIDOperation;	
	U8 PercentComplete;	
	U16 Reserved2;		
	U32 ElapsedSeconds;	
} MPI2_EVENT_DATA_IR_OPERATION_STATUS,
	*PTR_MPI2_EVENT_DATA_IR_OPERATION_STATUS,
	Mpi2EventDataIrOperationStatus_t,
	*pMpi2EventDataIrOperationStatus_t;


#define MPI2_EVENT_IR_RAIDOP_RESYNC                     (0x00)
#define MPI2_EVENT_IR_RAIDOP_ONLINE_CAP_EXPANSION       (0x01)
#define MPI2_EVENT_IR_RAIDOP_CONSISTENCY_CHECK          (0x02)
#define MPI2_EVENT_IR_RAIDOP_BACKGROUND_INIT            (0x03)
#define MPI2_EVENT_IR_RAIDOP_MAKE_DATA_CONSISTENT       (0x04)



typedef struct _MPI2_EVENT_DATA_IR_VOLUME {
	U16 VolDevHandle;	
	U8 ReasonCode;		
	U8 Reserved1;		
	U32 NewValue;		
	U32 PreviousValue;	
} MPI2_EVENT_DATA_IR_VOLUME, *PTR_MPI2_EVENT_DATA_IR_VOLUME,
	Mpi2EventDataIrVolume_t, *pMpi2EventDataIrVolume_t;


#define MPI2_EVENT_IR_VOLUME_RC_SETTINGS_CHANGED        (0x01)
#define MPI2_EVENT_IR_VOLUME_RC_STATUS_FLAGS_CHANGED    (0x02)
#define MPI2_EVENT_IR_VOLUME_RC_STATE_CHANGED           (0x03)



typedef struct _MPI2_EVENT_DATA_IR_PHYSICAL_DISK {
	U16 Reserved1;		
	U8 ReasonCode;		
	U8 PhysDiskNum;		
	U16 PhysDiskDevHandle;	
	U16 Reserved2;		
	U16 Slot;		
	U16 EnclosureHandle;	
	U32 NewValue;		
	U32 PreviousValue;	
} MPI2_EVENT_DATA_IR_PHYSICAL_DISK,
	*PTR_MPI2_EVENT_DATA_IR_PHYSICAL_DISK,
	Mpi2EventDataIrPhysicalDisk_t,
	*pMpi2EventDataIrPhysicalDisk_t;


#define MPI2_EVENT_IR_PHYSDISK_RC_SETTINGS_CHANGED      (0x01)
#define MPI2_EVENT_IR_PHYSDISK_RC_STATUS_FLAGS_CHANGED  (0x02)
#define MPI2_EVENT_IR_PHYSDISK_RC_STATE_CHANGED         (0x03)




#ifndef MPI2_EVENT_IR_CONFIG_ELEMENT_COUNT
#define MPI2_EVENT_IR_CONFIG_ELEMENT_COUNT          (1)
#endif

typedef struct _MPI2_EVENT_IR_CONFIG_ELEMENT {
	U16 ElementFlags;	
	U16 VolDevHandle;	
	U8 ReasonCode;		
	U8 PhysDiskNum;		
	U16 PhysDiskDevHandle;	
} MPI2_EVENT_IR_CONFIG_ELEMENT, *PTR_MPI2_EVENT_IR_CONFIG_ELEMENT,
	Mpi2EventIrConfigElement_t, *pMpi2EventIrConfigElement_t;


#define MPI2_EVENT_IR_CHANGE_EFLAGS_ELEMENT_TYPE_MASK   (0x000F)
#define MPI2_EVENT_IR_CHANGE_EFLAGS_VOLUME_ELEMENT      (0x0000)
#define MPI2_EVENT_IR_CHANGE_EFLAGS_VOLPHYSDISK_ELEMENT (0x0001)
#define MPI2_EVENT_IR_CHANGE_EFLAGS_HOTSPARE_ELEMENT    (0x0002)


#define MPI2_EVENT_IR_CHANGE_RC_ADDED                   (0x01)
#define MPI2_EVENT_IR_CHANGE_RC_REMOVED                 (0x02)
#define MPI2_EVENT_IR_CHANGE_RC_NO_CHANGE               (0x03)
#define MPI2_EVENT_IR_CHANGE_RC_HIDE                    (0x04)
#define MPI2_EVENT_IR_CHANGE_RC_UNHIDE                  (0x05)
#define MPI2_EVENT_IR_CHANGE_RC_VOLUME_CREATED          (0x06)
#define MPI2_EVENT_IR_CHANGE_RC_VOLUME_DELETED          (0x07)
#define MPI2_EVENT_IR_CHANGE_RC_PD_CREATED              (0x08)
#define MPI2_EVENT_IR_CHANGE_RC_PD_DELETED              (0x09)

typedef struct _MPI2_EVENT_DATA_IR_CONFIG_CHANGE_LIST {
	U8 NumElements;		
	U8 Reserved1;		
	U8 Reserved2;		
	U8 ConfigNum;		
	U32 Flags;		
	MPI2_EVENT_IR_CONFIG_ELEMENT
		ConfigElement[MPI2_EVENT_IR_CONFIG_ELEMENT_COUNT];
} MPI2_EVENT_DATA_IR_CONFIG_CHANGE_LIST,
	*PTR_MPI2_EVENT_DATA_IR_CONFIG_CHANGE_LIST,
	Mpi2EventDataIrConfigChangeList_t,
	*pMpi2EventDataIrConfigChangeList_t;


#define MPI2_EVENT_IR_CHANGE_FLAGS_FOREIGN_CONFIG   (0x00000001)



typedef struct _MPI2_EVENT_DATA_SAS_DISCOVERY {
	U8 Flags;		
	U8 ReasonCode;		
	U8 PhysicalPort;	
	U8 Reserved1;		
	U32 DiscoveryStatus;	
} MPI2_EVENT_DATA_SAS_DISCOVERY,
	*PTR_MPI2_EVENT_DATA_SAS_DISCOVERY,
	Mpi2EventDataSasDiscovery_t, *pMpi2EventDataSasDiscovery_t;


#define MPI2_EVENT_SAS_DISC_DEVICE_CHANGE                   (0x02)
#define MPI2_EVENT_SAS_DISC_IN_PROGRESS                     (0x01)


#define MPI2_EVENT_SAS_DISC_RC_STARTED                      (0x01)
#define MPI2_EVENT_SAS_DISC_RC_COMPLETED                    (0x02)


#define MPI2_EVENT_SAS_DISC_DS_MAX_ENCLOSURES_EXCEED            (0x80000000)
#define MPI2_EVENT_SAS_DISC_DS_MAX_EXPANDERS_EXCEED             (0x40000000)
#define MPI2_EVENT_SAS_DISC_DS_MAX_DEVICES_EXCEED               (0x20000000)
#define MPI2_EVENT_SAS_DISC_DS_MAX_TOPO_PHYS_EXCEED             (0x10000000)
#define MPI2_EVENT_SAS_DISC_DS_DOWNSTREAM_INITIATOR             (0x08000000)
#define MPI2_EVENT_SAS_DISC_DS_MULTI_SUBTRACTIVE_SUBTRACTIVE    (0x00008000)
#define MPI2_EVENT_SAS_DISC_DS_EXP_MULTI_SUBTRACTIVE            (0x00004000)
#define MPI2_EVENT_SAS_DISC_DS_MULTI_PORT_DOMAIN                (0x00002000)
#define MPI2_EVENT_SAS_DISC_DS_TABLE_TO_SUBTRACTIVE_LINK        (0x00001000)
#define MPI2_EVENT_SAS_DISC_DS_UNSUPPORTED_DEVICE               (0x00000800)
#define MPI2_EVENT_SAS_DISC_DS_TABLE_LINK                       (0x00000400)
#define MPI2_EVENT_SAS_DISC_DS_SUBTRACTIVE_LINK                 (0x00000200)
#define MPI2_EVENT_SAS_DISC_DS_SMP_CRC_ERROR                    (0x00000100)
#define MPI2_EVENT_SAS_DISC_DS_SMP_FUNCTION_FAILED              (0x00000080)
#define MPI2_EVENT_SAS_DISC_DS_INDEX_NOT_EXIST                  (0x00000040)
#define MPI2_EVENT_SAS_DISC_DS_OUT_ROUTE_ENTRIES                (0x00000020)
#define MPI2_EVENT_SAS_DISC_DS_SMP_TIMEOUT                      (0x00000010)
#define MPI2_EVENT_SAS_DISC_DS_MULTIPLE_PORTS                   (0x00000004)
#define MPI2_EVENT_SAS_DISC_DS_UNADDRESSABLE_DEVICE             (0x00000002)
#define MPI2_EVENT_SAS_DISC_DS_LOOP_DETECTED                    (0x00000001)



typedef struct _MPI2_EVENT_DATA_SAS_BROADCAST_PRIMITIVE {
	U8 PhyNum;		
	U8 Port;		
	U8 PortWidth;		
	U8 Primitive;		
} MPI2_EVENT_DATA_SAS_BROADCAST_PRIMITIVE,
	*PTR_MPI2_EVENT_DATA_SAS_BROADCAST_PRIMITIVE,
	Mpi2EventDataSasBroadcastPrimitive_t,
	*pMpi2EventDataSasBroadcastPrimitive_t;


#define MPI2_EVENT_PRIMITIVE_CHANGE                         (0x01)
#define MPI2_EVENT_PRIMITIVE_SES                            (0x02)
#define MPI2_EVENT_PRIMITIVE_EXPANDER                       (0x03)
#define MPI2_EVENT_PRIMITIVE_ASYNCHRONOUS_EVENT             (0x04)
#define MPI2_EVENT_PRIMITIVE_RESERVED3                      (0x05)
#define MPI2_EVENT_PRIMITIVE_RESERVED4                      (0x06)
#define MPI2_EVENT_PRIMITIVE_CHANGE0_RESERVED               (0x07)
#define MPI2_EVENT_PRIMITIVE_CHANGE1_RESERVED               (0x08)



typedef struct _MPI2_EVENT_DATA_SAS_NOTIFY_PRIMITIVE {
	U8 PhyNum;		
	U8 Port;		
	U8 Reserved1;		
	U8 Primitive;		
} MPI2_EVENT_DATA_SAS_NOTIFY_PRIMITIVE,
	*PTR_MPI2_EVENT_DATA_SAS_NOTIFY_PRIMITIVE,
	Mpi2EventDataSasNotifyPrimitive_t,
	*pMpi2EventDataSasNotifyPrimitive_t;


#define MPI2_EVENT_NOTIFY_ENABLE_SPINUP                     (0x01)
#define MPI2_EVENT_NOTIFY_POWER_LOSS_EXPECTED               (0x02)
#define MPI2_EVENT_NOTIFY_RESERVED1                         (0x03)
#define MPI2_EVENT_NOTIFY_RESERVED2                         (0x04)



typedef struct _MPI2_EVENT_DATA_SAS_INIT_DEV_STATUS_CHANGE {
	U8 ReasonCode;		
	U8 PhysicalPort;	
	U16 DevHandle;		
	U64 SASAddress;		
} MPI2_EVENT_DATA_SAS_INIT_DEV_STATUS_CHANGE,
	*PTR_MPI2_EVENT_DATA_SAS_INIT_DEV_STATUS_CHANGE,
	Mpi2EventDataSasInitDevStatusChange_t,
	*pMpi2EventDataSasInitDevStatusChange_t;


#define MPI2_EVENT_SAS_INIT_RC_ADDED                (0x01)
#define MPI2_EVENT_SAS_INIT_RC_NOT_RESPONDING       (0x02)



typedef struct _MPI2_EVENT_DATA_SAS_INIT_TABLE_OVERFLOW {
	U16 MaxInit;		
	U16 CurrentInit;	
	U64 SASAddress;		
} MPI2_EVENT_DATA_SAS_INIT_TABLE_OVERFLOW,
	*PTR_MPI2_EVENT_DATA_SAS_INIT_TABLE_OVERFLOW,
	Mpi2EventDataSasInitTableOverflow_t,
	*pMpi2EventDataSasInitTableOverflow_t;




#ifndef MPI2_EVENT_SAS_TOPO_PHY_COUNT
#define MPI2_EVENT_SAS_TOPO_PHY_COUNT           (1)
#endif

typedef struct _MPI2_EVENT_SAS_TOPO_PHY_ENTRY {
	U16 AttachedDevHandle;	
	U8 LinkRate;		
	U8 PhyStatus;		
} MPI2_EVENT_SAS_TOPO_PHY_ENTRY, *PTR_MPI2_EVENT_SAS_TOPO_PHY_ENTRY,
	Mpi2EventSasTopoPhyEntry_t, *pMpi2EventSasTopoPhyEntry_t;

typedef struct _MPI2_EVENT_DATA_SAS_TOPOLOGY_CHANGE_LIST {
	U16 EnclosureHandle;	
	U16 ExpanderDevHandle;	
	U8 NumPhys;		
	U8 Reserved1;		
	U16 Reserved2;		
	U8 NumEntries;		
	U8 StartPhyNum;		
	U8 ExpStatus;		
	U8 PhysicalPort;	
	MPI2_EVENT_SAS_TOPO_PHY_ENTRY
	PHY[MPI2_EVENT_SAS_TOPO_PHY_COUNT];	
} MPI2_EVENT_DATA_SAS_TOPOLOGY_CHANGE_LIST,
	*PTR_MPI2_EVENT_DATA_SAS_TOPOLOGY_CHANGE_LIST,
	Mpi2EventDataSasTopologyChangeList_t,
	*pMpi2EventDataSasTopologyChangeList_t;


#define MPI2_EVENT_SAS_TOPO_ES_NO_EXPANDER                  (0x00)
#define MPI2_EVENT_SAS_TOPO_ES_ADDED                        (0x01)
#define MPI2_EVENT_SAS_TOPO_ES_NOT_RESPONDING               (0x02)
#define MPI2_EVENT_SAS_TOPO_ES_RESPONDING                   (0x03)
#define MPI2_EVENT_SAS_TOPO_ES_DELAY_NOT_RESPONDING         (0x04)


#define MPI2_EVENT_SAS_TOPO_LR_CURRENT_MASK                 (0xF0)
#define MPI2_EVENT_SAS_TOPO_LR_CURRENT_SHIFT                (4)
#define MPI2_EVENT_SAS_TOPO_LR_PREV_MASK                    (0x0F)
#define MPI2_EVENT_SAS_TOPO_LR_PREV_SHIFT                   (0)

#define MPI2_EVENT_SAS_TOPO_LR_UNKNOWN_LINK_RATE            (0x00)
#define MPI2_EVENT_SAS_TOPO_LR_PHY_DISABLED                 (0x01)
#define MPI2_EVENT_SAS_TOPO_LR_NEGOTIATION_FAILED           (0x02)
#define MPI2_EVENT_SAS_TOPO_LR_SATA_OOB_COMPLETE            (0x03)
#define MPI2_EVENT_SAS_TOPO_LR_PORT_SELECTOR                (0x04)
#define MPI2_EVENT_SAS_TOPO_LR_SMP_RESET_IN_PROGRESS        (0x05)
#define MPI2_EVENT_SAS_TOPO_LR_UNSUPPORTED_PHY              (0x06)
#define MPI2_EVENT_SAS_TOPO_LR_RATE_1_5                     (0x08)
#define MPI2_EVENT_SAS_TOPO_LR_RATE_3_0                     (0x09)
#define MPI2_EVENT_SAS_TOPO_LR_RATE_6_0                     (0x0A)
#define MPI25_EVENT_SAS_TOPO_LR_RATE_12_0                   (0x0B)
#define MPI26_EVENT_SAS_TOPO_LR_RATE_22_5                   (0x0C)


#define MPI2_EVENT_SAS_TOPO_PHYSTATUS_VACANT                (0x80)
#define MPI2_EVENT_SAS_TOPO_PS_MULTIPLEX_CHANGE             (0x10)

#define MPI2_EVENT_SAS_TOPO_RC_MASK                         (0x0F)
#define MPI2_EVENT_SAS_TOPO_RC_TARG_ADDED                   (0x01)
#define MPI2_EVENT_SAS_TOPO_RC_TARG_NOT_RESPONDING          (0x02)
#define MPI2_EVENT_SAS_TOPO_RC_PHY_CHANGED                  (0x03)
#define MPI2_EVENT_SAS_TOPO_RC_NO_CHANGE                    (0x04)
#define MPI2_EVENT_SAS_TOPO_RC_DELAY_NOT_RESPONDING         (0x05)



typedef struct _MPI2_EVENT_DATA_SAS_ENCL_DEV_STATUS_CHANGE {
	U16 EnclosureHandle;	
	U8 ReasonCode;		
	U8 PhysicalPort;	
	U64 EnclosureLogicalID;	
	U16 NumSlots;		
	U16 StartSlot;		
	U32 PhyBits;		
} MPI2_EVENT_DATA_SAS_ENCL_DEV_STATUS_CHANGE,
	*PTR_MPI2_EVENT_DATA_SAS_ENCL_DEV_STATUS_CHANGE,
	Mpi2EventDataSasEnclDevStatusChange_t,
	*pMpi2EventDataSasEnclDevStatusChange_t,
	MPI26_EVENT_DATA_ENCL_DEV_STATUS_CHANGE,
	*PTR_MPI26_EVENT_DATA_ENCL_DEV_STATUS_CHANGE,
	Mpi26EventDataEnclDevStatusChange_t,
	*pMpi26EventDataEnclDevStatusChange_t;


#define MPI2_EVENT_SAS_ENCL_RC_ADDED                (0x01)
#define MPI2_EVENT_SAS_ENCL_RC_NOT_RESPONDING       (0x02)


#define MPI26_EVENT_ENCL_RC_ADDED                   (0x01)
#define MPI26_EVENT_ENCL_RC_NOT_RESPONDING          (0x02)


typedef struct _MPI25_EVENT_DATA_SAS_DEVICE_DISCOVERY_ERROR {
	U16	DevHandle;                  
	U8	ReasonCode;                 
	U8	PhysicalPort;               
	U32	Reserved1[2];               
	U64	SASAddress;                 
	U32	Reserved2[2];               
} MPI25_EVENT_DATA_SAS_DEVICE_DISCOVERY_ERROR,
	*PTR_MPI25_EVENT_DATA_SAS_DEVICE_DISCOVERY_ERROR,
	Mpi25EventDataSasDeviceDiscoveryError_t,
	*pMpi25EventDataSasDeviceDiscoveryError_t;


#define MPI25_EVENT_SAS_DISC_ERR_SMP_FAILED         (0x01)
#define MPI25_EVENT_SAS_DISC_ERR_SMP_TIMEOUT        (0x02)



typedef struct _MPI2_EVENT_DATA_SAS_PHY_COUNTER {
	U64 TimeStamp;		
	U32 Reserved1;		
	U8 PhyEventCode;	
	U8 PhyNum;		
	U16 Reserved2;		
	U32 PhyEventInfo;	
	U8 CounterType;		
	U8 ThresholdWindow;	
	U8 TimeUnits;		
	U8 Reserved3;		
	U32 EventThreshold;	
	U16 ThresholdFlags;	
	U16 Reserved4;		
} MPI2_EVENT_DATA_SAS_PHY_COUNTER,
	*PTR_MPI2_EVENT_DATA_SAS_PHY_COUNTER,
	Mpi2EventDataSasPhyCounter_t,
	*pMpi2EventDataSasPhyCounter_t;











typedef struct _MPI2_EVENT_DATA_SAS_QUIESCE {
	U8 ReasonCode;		
	U8 Reserved1;		
	U16 Reserved2;		
	U32 Reserved3;		
} MPI2_EVENT_DATA_SAS_QUIESCE,
	*PTR_MPI2_EVENT_DATA_SAS_QUIESCE,
	Mpi2EventDataSasQuiesce_t, *pMpi2EventDataSasQuiesce_t;


#define MPI2_EVENT_SAS_QUIESCE_RC_STARTED                   (0x01)
#define MPI2_EVENT_SAS_QUIESCE_RC_COMPLETED                 (0x02)



typedef struct _MPI2_EVENT_HBD_PHY_SAS {
	U8 Flags;		
	U8 NegotiatedLinkRate;	
	U8 PhyNum;		
	U8 PhysicalPort;	
	U32 Reserved1;		
	U8 InitialFrame[28];	
} MPI2_EVENT_HBD_PHY_SAS, *PTR_MPI2_EVENT_HBD_PHY_SAS,
	Mpi2EventHbdPhySas_t, *pMpi2EventHbdPhySas_t;


#define MPI2_EVENT_HBD_SAS_FLAGS_FRAME_VALID        (0x02)
#define MPI2_EVENT_HBD_SAS_FLAGS_SATA_FRAME         (0x01)



typedef union _MPI2_EVENT_HBD_DESCRIPTOR {
	MPI2_EVENT_HBD_PHY_SAS Sas;
} MPI2_EVENT_HBD_DESCRIPTOR, *PTR_MPI2_EVENT_HBD_DESCRIPTOR,
	Mpi2EventHbdDescriptor_t, *pMpi2EventHbdDescriptor_t;

typedef struct _MPI2_EVENT_DATA_HBD_PHY {
	U8 DescriptorType;	
	U8 Reserved1;		
	U16 Reserved2;		
	U32 Reserved3;		
	MPI2_EVENT_HBD_DESCRIPTOR Descriptor;	
} MPI2_EVENT_DATA_HBD_PHY, *PTR_MPI2_EVENT_DATA_HBD_PHY,
	Mpi2EventDataHbdPhy_t,
	*pMpi2EventDataMpi2EventDataHbdPhy_t;


#define MPI2_EVENT_HBD_DT_SAS               (0x01)




typedef struct _MPI26_EVENT_DATA_PCIE_DEVICE_STATUS_CHANGE {
	U16	TaskTag;                        
	U8	ReasonCode;                     
	U8	PhysicalPort;                   
	U8	ASC;                            
	U8	ASCQ;                           
	U16	DevHandle;                      
	U32	Reserved2;                      
	U64	WWID;                           
	U8	LUN[8];                         
} MPI26_EVENT_DATA_PCIE_DEVICE_STATUS_CHANGE,
	*PTR_MPI26_EVENT_DATA_PCIE_DEVICE_STATUS_CHANGE,
	Mpi26EventDataPCIeDeviceStatusChange_t,
	*pMpi26EventDataPCIeDeviceStatusChange_t;


#define MPI26_EVENT_PCIDEV_STAT_RC_SMART_DATA                           (0x05)
#define MPI26_EVENT_PCIDEV_STAT_RC_UNSUPPORTED                          (0x07)
#define MPI26_EVENT_PCIDEV_STAT_RC_INTERNAL_DEVICE_RESET                (0x08)
#define MPI26_EVENT_PCIDEV_STAT_RC_TASK_ABORT_INTERNAL                  (0x09)
#define MPI26_EVENT_PCIDEV_STAT_RC_ABORT_TASK_SET_INTERNAL              (0x0A)
#define MPI26_EVENT_PCIDEV_STAT_RC_CLEAR_TASK_SET_INTERNAL              (0x0B)
#define MPI26_EVENT_PCIDEV_STAT_RC_QUERY_TASK_INTERNAL                  (0x0C)
#define MPI26_EVENT_PCIDEV_STAT_RC_ASYNC_NOTIFICATION                   (0x0D)
#define MPI26_EVENT_PCIDEV_STAT_RC_CMP_INTERNAL_DEV_RESET               (0x0E)
#define MPI26_EVENT_PCIDEV_STAT_RC_CMP_TASK_ABORT_INTERNAL              (0x0F)
#define MPI26_EVENT_PCIDEV_STAT_RC_DEV_INIT_FAILURE                     (0x10)
#define MPI26_EVENT_PCIDEV_STAT_RC_PCIE_HOT_RESET_FAILED                (0x11)




typedef struct _MPI26_EVENT_DATA_PCIE_ENUMERATION {
	U8	Flags;                      
	U8	ReasonCode;                 
	U8	PhysicalPort;               
	U8	Reserved1;                  
	U32	EnumerationStatus;          
} MPI26_EVENT_DATA_PCIE_ENUMERATION,
	*PTR_MPI26_EVENT_DATA_PCIE_ENUMERATION,
	Mpi26EventDataPCIeEnumeration_t,
	*pMpi26EventDataPCIeEnumeration_t;


#define MPI26_EVENT_PCIE_ENUM_DEVICE_CHANGE                 (0x02)
#define MPI26_EVENT_PCIE_ENUM_IN_PROGRESS                   (0x01)


#define MPI26_EVENT_PCIE_ENUM_RC_STARTED                    (0x01)
#define MPI26_EVENT_PCIE_ENUM_RC_COMPLETED                  (0x02)


#define MPI26_EVENT_PCIE_ENUM_ES_MAX_SWITCHES_EXCEED            (0x40000000)
#define MPI26_EVENT_PCIE_ENUM_ES_MAX_DEVICES_EXCEED             (0x20000000)
#define MPI26_EVENT_PCIE_ENUM_ES_RESOURCES_EXHAUSTED            (0x10000000)





#ifndef MPI26_EVENT_PCIE_TOPO_PORT_COUNT
#define MPI26_EVENT_PCIE_TOPO_PORT_COUNT        (1)
#endif

typedef struct _MPI26_EVENT_PCIE_TOPO_PORT_ENTRY {
	U16	AttachedDevHandle;      
	U8	PortStatus;             
	U8	Reserved1;              
	U8	CurrentPortInfo;        
	U8	Reserved2;              
	U8	PreviousPortInfo;       
	U8	Reserved3;              
} MPI26_EVENT_PCIE_TOPO_PORT_ENTRY,
	*PTR_MPI26_EVENT_PCIE_TOPO_PORT_ENTRY,
	Mpi26EventPCIeTopoPortEntry_t,
	*pMpi26EventPCIeTopoPortEntry_t;


#define MPI26_EVENT_PCIE_TOPO_PS_DEV_ADDED                  (0x01)
#define MPI26_EVENT_PCIE_TOPO_PS_NOT_RESPONDING             (0x02)
#define MPI26_EVENT_PCIE_TOPO_PS_PORT_CHANGED               (0x03)
#define MPI26_EVENT_PCIE_TOPO_PS_NO_CHANGE                  (0x04)
#define MPI26_EVENT_PCIE_TOPO_PS_DELAY_NOT_RESPONDING       (0x05)


#define MPI26_EVENT_PCIE_TOPO_PI_LANE_MASK                  (0xF0)
#define MPI26_EVENT_PCIE_TOPO_PI_LANES_UNKNOWN              (0x00)
#define MPI26_EVENT_PCIE_TOPO_PI_1_LANE                     (0x10)
#define MPI26_EVENT_PCIE_TOPO_PI_2_LANES                    (0x20)
#define MPI26_EVENT_PCIE_TOPO_PI_4_LANES                    (0x30)
#define MPI26_EVENT_PCIE_TOPO_PI_8_LANES                    (0x40)
#define MPI26_EVENT_PCIE_TOPO_PI_16_LANES                   (0x50)

#define MPI26_EVENT_PCIE_TOPO_PI_RATE_MASK                  (0x0F)
#define MPI26_EVENT_PCIE_TOPO_PI_RATE_UNKNOWN               (0x00)
#define MPI26_EVENT_PCIE_TOPO_PI_RATE_DISABLED              (0x01)
#define MPI26_EVENT_PCIE_TOPO_PI_RATE_2_5                   (0x02)
#define MPI26_EVENT_PCIE_TOPO_PI_RATE_5_0                   (0x03)
#define MPI26_EVENT_PCIE_TOPO_PI_RATE_8_0                   (0x04)
#define MPI26_EVENT_PCIE_TOPO_PI_RATE_16_0                  (0x05)

typedef struct _MPI26_EVENT_DATA_PCIE_TOPOLOGY_CHANGE_LIST {
	U16	EnclosureHandle;        
	U16	SwitchDevHandle;        
	U8	NumPorts;               
	U8	Reserved1;              
	U16	Reserved2;              
	U8	NumEntries;             
	U8	StartPortNum;           
	U8	SwitchStatus;           
	U8	PhysicalPort;           
	MPI26_EVENT_PCIE_TOPO_PORT_ENTRY
		PortEntry[MPI26_EVENT_PCIE_TOPO_PORT_COUNT]; 
} MPI26_EVENT_DATA_PCIE_TOPOLOGY_CHANGE_LIST,
	*PTR_MPI26_EVENT_DATA_PCIE_TOPOLOGY_CHANGE_LIST,
	Mpi26EventDataPCIeTopologyChangeList_t,
	*pMpi26EventDataPCIeTopologyChangeList_t;


#define MPI26_EVENT_PCIE_TOPO_SS_NO_PCIE_SWITCH             (0x00)
#define MPI26_EVENT_PCIE_TOPO_SS_ADDED                      (0x01)
#define MPI26_EVENT_PCIE_TOPO_SS_NOT_RESPONDING             (0x02)
#define MPI26_EVENT_PCIE_TOPO_SS_RESPONDING                 (0x03)
#define MPI26_EVENT_PCIE_TOPO_SS_DELAY_NOT_RESPONDING       (0x04)



typedef struct _MPI26_EVENT_DATA_PCIE_LINK_COUNTER {
	U64	TimeStamp;          
	U32	Reserved1;          
	U8	LinkEventCode;      
	U8	LinkNum;            
	U16	Reserved2;          
	U32	LinkEventInfo;      
	U8	CounterType;        
	U8	ThresholdWindow;    
	U8	TimeUnits;          
	U8	Reserved3;          
	U32	EventThreshold;     
	U16	ThresholdFlags;     
	U16	Reserved4;          
} MPI26_EVENT_DATA_PCIE_LINK_COUNTER,
	*PTR_MPI26_EVENT_DATA_PCIE_LINK_COUNTER,
	Mpi26EventDataPcieLinkCounter_t, *pMpi26EventDataPcieLinkCounter_t;













typedef struct _MPI2_EVENT_ACK_REQUEST {
	U16 Reserved1;		
	U8 ChainOffset;		
	U8 Function;		
	U16 Reserved2;		
	U8 Reserved3;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved4;		
	U16 Event;		
	U16 Reserved5;		
	U32 EventContext;	
} MPI2_EVENT_ACK_REQUEST, *PTR_MPI2_EVENT_ACK_REQUEST,
	Mpi2EventAckRequest_t, *pMpi2EventAckRequest_t;


typedef struct _MPI2_EVENT_ACK_REPLY {
	U16 Reserved1;		
	U8 MsgLength;		
	U8 Function;		
	U16 Reserved2;		
	U8 Reserved3;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved4;		
	U16 Reserved5;		
	U16 IOCStatus;		
	U32 IOCLogInfo;		
} MPI2_EVENT_ACK_REPLY, *PTR_MPI2_EVENT_ACK_REPLY,
	Mpi2EventAckReply_t, *pMpi2EventAckReply_t;




typedef struct _MPI2_SEND_HOST_MESSAGE_REQUEST {
	U16 HostDataLength;	
	U8 ChainOffset;		
	U8 Function;		
	U16 Reserved1;		
	U8 Reserved2;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved3;		
	U8 Reserved4;		
	U8 DestVF_ID;		
	U16 Reserved5;		
	U32 Reserved6;		
	U32 Reserved7;		
	U32 Reserved8;		
	U32 Reserved9;		
	U32 Reserved10;		
	U32 HostData[];		
} MPI2_SEND_HOST_MESSAGE_REQUEST,
	*PTR_MPI2_SEND_HOST_MESSAGE_REQUEST,
	Mpi2SendHostMessageRequest_t,
	*pMpi2SendHostMessageRequest_t;


typedef struct _MPI2_SEND_HOST_MESSAGE_REPLY {
	U16 HostDataLength;	
	U8 MsgLength;		
	U8 Function;		
	U16 Reserved1;		
	U8 Reserved2;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved3;		
	U16 Reserved4;		
	U16 IOCStatus;		
	U32 IOCLogInfo;		
} MPI2_SEND_HOST_MESSAGE_REPLY, *PTR_MPI2_SEND_HOST_MESSAGE_REPLY,
	Mpi2SendHostMessageReply_t, *pMpi2SendHostMessageReply_t;




typedef struct _MPI2_FW_DOWNLOAD_REQUEST {
	U8 ImageType;		
	U8 Reserved1;		
	U8 ChainOffset;		
	U8 Function;		
	U16 Reserved2;		
	U8 Reserved3;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved4;		
	U32 TotalImageSize;	
	U32 Reserved5;		
	MPI2_MPI_SGE_UNION SGL;	
} MPI2_FW_DOWNLOAD_REQUEST, *PTR_MPI2_FW_DOWNLOAD_REQUEST,
	Mpi2FWDownloadRequest, *pMpi2FWDownloadRequest;

#define MPI2_FW_DOWNLOAD_MSGFLGS_LAST_SEGMENT   (0x01)

#define MPI2_FW_DOWNLOAD_ITYPE_FW                   (0x01)
#define MPI2_FW_DOWNLOAD_ITYPE_BIOS                 (0x02)
#define MPI2_FW_DOWNLOAD_ITYPE_MANUFACTURING        (0x06)
#define MPI2_FW_DOWNLOAD_ITYPE_CONFIG_1             (0x07)
#define MPI2_FW_DOWNLOAD_ITYPE_CONFIG_2             (0x08)
#define MPI2_FW_DOWNLOAD_ITYPE_MEGARAID             (0x09)
#define MPI2_FW_DOWNLOAD_ITYPE_COMPLETE             (0x0A)
#define MPI2_FW_DOWNLOAD_ITYPE_COMMON_BOOT_BLOCK    (0x0B)
#define MPI2_FW_DOWNLOAD_ITYPE_PUBLIC_KEY           (0x0C)
#define MPI2_FW_DOWNLOAD_ITYPE_CBB_BACKUP           (0x0D)
#define MPI2_FW_DOWNLOAD_ITYPE_SBR                  (0x0E)
#define MPI2_FW_DOWNLOAD_ITYPE_SBR_BACKUP           (0x0F)
#define MPI2_FW_DOWNLOAD_ITYPE_HIIM                 (0x10)
#define MPI2_FW_DOWNLOAD_ITYPE_HIIA                 (0x11)
#define MPI2_FW_DOWNLOAD_ITYPE_CTLR                 (0x12)
#define MPI2_FW_DOWNLOAD_ITYPE_IMR_FIRMWARE         (0x13)
#define MPI2_FW_DOWNLOAD_ITYPE_MR_NVDATA            (0x14)

#define MPI2_FW_DOWNLOAD_ITYPE_CPLD                 (0x15)
#define MPI2_FW_DOWNLOAD_ITYPE_PSOC                 (0x16)
#define MPI2_FW_DOWNLOAD_ITYPE_COREDUMP             (0x17)
#define MPI2_FW_DOWNLOAD_ITYPE_MIN_PRODUCT_SPECIFIC (0xF0)


typedef struct _MPI2_FW_DOWNLOAD_TCSGE {
	U8 Reserved1;		
	U8 ContextSize;		
	U8 DetailsLength;	
	U8 Flags;		
	U32 Reserved2;		
	U32 ImageOffset;	
	U32 ImageSize;		
} MPI2_FW_DOWNLOAD_TCSGE, *PTR_MPI2_FW_DOWNLOAD_TCSGE,
	Mpi2FWDownloadTCSGE_t, *pMpi2FWDownloadTCSGE_t;


typedef struct _MPI25_FW_DOWNLOAD_REQUEST {
	U8 ImageType;		
	U8 Reserved1;		
	U8 ChainOffset;		
	U8 Function;		
	U16 Reserved2;		
	U8 Reserved3;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved4;		
	U32 TotalImageSize;	
	U32 Reserved5;		
	U32 Reserved6;		
	U32 ImageOffset;	
	U32 ImageSize;		
	MPI25_SGE_IO_UNION SGL;	
} MPI25_FW_DOWNLOAD_REQUEST, *PTR_MPI25_FW_DOWNLOAD_REQUEST,
	Mpi25FWDownloadRequest, *pMpi25FWDownloadRequest;


typedef struct _MPI2_FW_DOWNLOAD_REPLY {
	U8 ImageType;		
	U8 Reserved1;		
	U8 MsgLength;		
	U8 Function;		
	U16 Reserved2;		
	U8 Reserved3;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved4;		
	U16 Reserved5;		
	U16 IOCStatus;		
	U32 IOCLogInfo;		
} MPI2_FW_DOWNLOAD_REPLY, *PTR_MPI2_FW_DOWNLOAD_REPLY,
	Mpi2FWDownloadReply_t, *pMpi2FWDownloadReply_t;




typedef struct _MPI2_FW_UPLOAD_REQUEST {
	U8 ImageType;		
	U8 Reserved1;		
	U8 ChainOffset;		
	U8 Function;		
	U16 Reserved2;		
	U8 Reserved3;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved4;		
	U32 Reserved5;		
	U32 Reserved6;		
	MPI2_MPI_SGE_UNION SGL;	
} MPI2_FW_UPLOAD_REQUEST, *PTR_MPI2_FW_UPLOAD_REQUEST,
	Mpi2FWUploadRequest_t, *pMpi2FWUploadRequest_t;

#define MPI2_FW_UPLOAD_ITYPE_FW_CURRENT         (0x00)
#define MPI2_FW_UPLOAD_ITYPE_FW_FLASH           (0x01)
#define MPI2_FW_UPLOAD_ITYPE_BIOS_FLASH         (0x02)
#define MPI2_FW_UPLOAD_ITYPE_FW_BACKUP          (0x05)
#define MPI2_FW_UPLOAD_ITYPE_MANUFACTURING      (0x06)
#define MPI2_FW_UPLOAD_ITYPE_CONFIG_1           (0x07)
#define MPI2_FW_UPLOAD_ITYPE_CONFIG_2           (0x08)
#define MPI2_FW_UPLOAD_ITYPE_MEGARAID           (0x09)
#define MPI2_FW_UPLOAD_ITYPE_COMPLETE           (0x0A)
#define MPI2_FW_UPLOAD_ITYPE_COMMON_BOOT_BLOCK  (0x0B)
#define MPI2_FW_UPLOAD_ITYPE_CBB_BACKUP         (0x0D)
#define MPI2_FW_UPLOAD_ITYPE_SBR                (0x0E)
#define MPI2_FW_UPLOAD_ITYPE_SBR_BACKUP         (0x0F)
#define MPI2_FW_UPLOAD_ITYPE_HIIM               (0x10)
#define MPI2_FW_UPLOAD_ITYPE_HIIA               (0x11)
#define MPI2_FW_UPLOAD_ITYPE_CTLR               (0x12)
#define MPI2_FW_UPLOAD_ITYPE_IMR_FIRMWARE       (0x13)
#define MPI2_FW_UPLOAD_ITYPE_MR_NVDATA          (0x14)



typedef struct _MPI2_FW_UPLOAD_TCSGE {
	U8 Reserved1;		
	U8 ContextSize;		
	U8 DetailsLength;	
	U8 Flags;		
	U32 Reserved2;		
	U32 ImageOffset;	
	U32 ImageSize;		
} MPI2_FW_UPLOAD_TCSGE, *PTR_MPI2_FW_UPLOAD_TCSGE,
	Mpi2FWUploadTCSGE_t, *pMpi2FWUploadTCSGE_t;


typedef struct _MPI25_FW_UPLOAD_REQUEST {
	U8 ImageType;		
	U8 Reserved1;		
	U8 ChainOffset;		
	U8 Function;		
	U16 Reserved2;		
	U8 Reserved3;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved4;		
	U32 Reserved5;		
	U32 Reserved6;		
	U32 Reserved7;		
	U32 ImageOffset;	
	U32 ImageSize;		
	MPI25_SGE_IO_UNION SGL;	
} MPI25_FW_UPLOAD_REQUEST, *PTR_MPI25_FW_UPLOAD_REQUEST,
	Mpi25FWUploadRequest_t, *pMpi25FWUploadRequest_t;


typedef struct _MPI2_FW_UPLOAD_REPLY {
	U8 ImageType;		
	U8 Reserved1;		
	U8 MsgLength;		
	U8 Function;		
	U16 Reserved2;		
	U8 Reserved3;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved4;		
	U16 Reserved5;		
	U16 IOCStatus;		
	U32 IOCLogInfo;		
	U32 ActualImageSize;	
} MPI2_FW_UPLOAD_REPLY, *PTR_MPI2_FW_UPLOAD_REPLY,
	Mpi2FWUploadReply_t, *pMPi2FWUploadReply_t;





typedef struct _MPI2_PWR_MGMT_CONTROL_REQUEST {
	U8 Feature;		
	U8 Reserved1;		
	U8 ChainOffset;		
	U8 Function;		
	U16 Reserved2;		
	U8 Reserved3;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved4;		
	U8 Parameter1;		
	U8 Parameter2;		
	U8 Parameter3;		
	U8 Parameter4;		
	U32 Reserved5;		
	U32 Reserved6;		
} MPI2_PWR_MGMT_CONTROL_REQUEST, *PTR_MPI2_PWR_MGMT_CONTROL_REQUEST,
	Mpi2PwrMgmtControlRequest_t, *pMpi2PwrMgmtControlRequest_t;


#define MPI2_PM_CONTROL_FEATURE_DA_PHY_POWER_COND       (0x01)
#define MPI2_PM_CONTROL_FEATURE_PORT_WIDTH_MODULATION   (0x02)
#define MPI2_PM_CONTROL_FEATURE_PCIE_LINK               (0x03)	
#define MPI2_PM_CONTROL_FEATURE_IOC_SPEED               (0x04)
#define MPI2_PM_CONTROL_FEATURE_GLOBAL_PWR_MGMT_MODE    (0x05)
#define MPI2_PM_CONTROL_FEATURE_MIN_PRODUCT_SPECIFIC    (0x80)
#define MPI2_PM_CONTROL_FEATURE_MAX_PRODUCT_SPECIFIC    (0xFF)




#define MPI2_PM_CONTROL_PARAM2_PARTIAL                  (0x01)
#define MPI2_PM_CONTROL_PARAM2_SLUMBER                  (0x02)
#define MPI2_PM_CONTROL_PARAM2_EXIT_PWR_MGMT            (0x03)





#define MPI2_PM_CONTROL_PARAM2_REQUEST_OWNERSHIP        (0x01)
#define MPI2_PM_CONTROL_PARAM2_CHANGE_MODULATION        (0x02)
#define MPI2_PM_CONTROL_PARAM2_RELINQUISH_OWNERSHIP     (0x03)

#define MPI2_PM_CONTROL_PARAM3_25_PERCENT               (0x00)
#define MPI2_PM_CONTROL_PARAM3_50_PERCENT               (0x01)
#define MPI2_PM_CONTROL_PARAM3_75_PERCENT               (0x02)
#define MPI2_PM_CONTROL_PARAM3_100_PERCENT              (0x03)





#define MPI2_PM_CONTROL_PARAM1_PCIE_2_5_GBPS            (0x00)	
#define MPI2_PM_CONTROL_PARAM1_PCIE_5_0_GBPS            (0x01)	
#define MPI2_PM_CONTROL_PARAM1_PCIE_8_0_GBPS            (0x02)	

#define MPI2_PM_CONTROL_PARAM2_WIDTH_X1                 (0x01)	
#define MPI2_PM_CONTROL_PARAM2_WIDTH_X2                 (0x02)	
#define MPI2_PM_CONTROL_PARAM2_WIDTH_X4                 (0x04)	
#define MPI2_PM_CONTROL_PARAM2_WIDTH_X8                 (0x08)	




#define MPI2_PM_CONTROL_PARAM1_FULL_IOC_SPEED           (0x01)
#define MPI2_PM_CONTROL_PARAM1_HALF_IOC_SPEED           (0x02)
#define MPI2_PM_CONTROL_PARAM1_QUARTER_IOC_SPEED        (0x04)
#define MPI2_PM_CONTROL_PARAM1_EIGHTH_IOC_SPEED         (0x08)




#define MPI2_PM_CONTROL_PARAM1_TAKE_CONTROL             (0x01)
#define MPI2_PM_CONTROL_PARAM1_CHANGE_GLOBAL_MODE       (0x02)
#define MPI2_PM_CONTROL_PARAM1_RELEASE_CONTROL          (0x03)

#define MPI2_PM_CONTROL_PARAM2_FULL_PWR_PERF            (0x01)
#define MPI2_PM_CONTROL_PARAM2_REDUCED_PWR_PERF         (0x08)
#define MPI2_PM_CONTROL_PARAM2_STANDBY                  (0x40)



typedef struct _MPI2_PWR_MGMT_CONTROL_REPLY {
	U8 Feature;		
	U8 Reserved1;		
	U8 MsgLength;		
	U8 Function;		
	U16 Reserved2;		
	U8 Reserved3;		
	U8 MsgFlags;		
	U8 VP_ID;		
	U8 VF_ID;		
	U16 Reserved4;		
	U16 Reserved5;		
	U16 IOCStatus;		
	U32 IOCLogInfo;		
} MPI2_PWR_MGMT_CONTROL_REPLY, *PTR_MPI2_PWR_MGMT_CONTROL_REPLY,
	Mpi2PwrMgmtControlReply_t, *pMpi2PwrMgmtControlReply_t;




typedef struct _MPI26_IOUNIT_CONTROL_REQUEST {
	U8                      Operation;          
	U8                      Reserved1;          
	U8                      ChainOffset;        
	U8                      Function;           
	U16                     DevHandle;          
	U8                      IOCParameter;       
	U8                      MsgFlags;           
	U8                      VP_ID;              
	U8                      VF_ID;              
	U16                     Reserved3;          
	U16                     Reserved4;          
	U8                      PhyNum;             
	U8                      PrimFlags;          
	U32                     Primitive;          
	U8                      LookupMethod;       
	U8                      Reserved5;          
	U16                     SlotNumber;         
	U64                     LookupAddress;      
	U32                     IOCParameterValue;  
	U32                     Reserved7;          
	U32                     Reserved8;          
} MPI26_IOUNIT_CONTROL_REQUEST,
	*PTR_MPI26_IOUNIT_CONTROL_REQUEST,
	Mpi26IoUnitControlRequest_t,
	*pMpi26IoUnitControlRequest_t;


#define MPI26_CTRL_OP_CLEAR_ALL_PERSISTENT              (0x02)
#define MPI26_CTRL_OP_SAS_PHY_LINK_RESET                (0x06)
#define MPI26_CTRL_OP_SAS_PHY_HARD_RESET                (0x07)
#define MPI26_CTRL_OP_PHY_CLEAR_ERROR_LOG               (0x08)
#define MPI26_CTRL_OP_LINK_CLEAR_ERROR_LOG              (0x09)
#define MPI26_CTRL_OP_SAS_SEND_PRIMITIVE                (0x0A)
#define MPI26_CTRL_OP_FORCE_FULL_DISCOVERY              (0x0B)
#define MPI26_CTRL_OP_REMOVE_DEVICE                     (0x0D)
#define MPI26_CTRL_OP_LOOKUP_MAPPING                    (0x0E)
#define MPI26_CTRL_OP_SET_IOC_PARAMETER                 (0x0F)
#define MPI26_CTRL_OP_ENABLE_FP_DEVICE                  (0x10)
#define MPI26_CTRL_OP_DISABLE_FP_DEVICE                 (0x11)
#define MPI26_CTRL_OP_ENABLE_FP_ALL                     (0x12)
#define MPI26_CTRL_OP_DISABLE_FP_ALL                    (0x13)
#define MPI26_CTRL_OP_DEV_ENABLE_NCQ                    (0x14)
#define MPI26_CTRL_OP_DEV_DISABLE_NCQ                   (0x15)
#define MPI26_CTRL_OP_SHUTDOWN                          (0x16)
#define MPI26_CTRL_OP_DEV_ENABLE_PERSIST_CONNECTION     (0x17)
#define MPI26_CTRL_OP_DEV_DISABLE_PERSIST_CONNECTION    (0x18)
#define MPI26_CTRL_OP_DEV_CLOSE_PERSIST_CONNECTION      (0x19)
#define MPI26_CTRL_OP_ENABLE_NVME_SGL_FORMAT            (0x1A)
#define MPI26_CTRL_OP_DISABLE_NVME_SGL_FORMAT           (0x1B)
#define MPI26_CTRL_OP_PRODUCT_SPECIFIC_MIN              (0x80)


#define MPI26_CTRL_PRIMFLAGS_SINGLE                     (0x08)
#define MPI26_CTRL_PRIMFLAGS_TRIPLE                     (0x02)
#define MPI26_CTRL_PRIMFLAGS_REDUNDANT                  (0x01)


#define MPI26_CTRL_LOOKUP_METHOD_WWID_ADDRESS           (0x01)
#define MPI26_CTRL_LOOKUP_METHOD_ENCLOSURE_SLOT         (0x02)
#define MPI26_CTRL_LOOKUP_METHOD_SAS_DEVICE_NAME        (0x03)



typedef struct _MPI26_IOUNIT_CONTROL_REPLY {
	U8                      Operation;          
	U8                      Reserved1;          
	U8                      MsgLength;          
	U8                      Function;           
	U16                     DevHandle;          
	U8                      IOCParameter;       
	U8                      MsgFlags;           
	U8                      VP_ID;              
	U8                      VF_ID;              
	U16                     Reserved3;          
	U16                     Reserved4;          
	U16                     IOCStatus;          
	U32                     IOCLogInfo;         
} MPI26_IOUNIT_CONTROL_REPLY,
	*PTR_MPI26_IOUNIT_CONTROL_REPLY,
	Mpi26IoUnitControlReply_t,
	*pMpi26IoUnitControlReply_t;


#endif
