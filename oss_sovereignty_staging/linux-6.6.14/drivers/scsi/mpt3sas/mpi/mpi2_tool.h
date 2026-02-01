 
 

#ifndef MPI2_TOOL_H
#define MPI2_TOOL_H

 

 
#define MPI2_TOOLBOX_CLEAN_TOOL                     (0x00)
#define MPI2_TOOLBOX_MEMORY_MOVE_TOOL               (0x01)
#define MPI2_TOOLBOX_DIAG_DATA_UPLOAD_TOOL          (0x02)
#define MPI2_TOOLBOX_ISTWI_READ_WRITE_TOOL          (0x03)
#define MPI2_TOOLBOX_BEACON_TOOL                    (0x05)
#define MPI2_TOOLBOX_DIAGNOSTIC_CLI_TOOL            (0x06)
#define MPI2_TOOLBOX_TEXT_DISPLAY_TOOL              (0x07)
#define MPI26_TOOLBOX_BACKEND_PCIE_LANE_MARGIN      (0x08)

 

typedef struct _MPI2_TOOLBOX_REPLY {
	U8 Tool;		 
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
} MPI2_TOOLBOX_REPLY, *PTR_MPI2_TOOLBOX_REPLY,
	Mpi2ToolboxReply_t, *pMpi2ToolboxReply_t;

 

typedef struct _MPI2_TOOLBOX_CLEAN_REQUEST {
	U8 Tool;		 
	U8 Reserved1;		 
	U8 ChainOffset;		 
	U8 Function;		 
	U16 Reserved2;		 
	U8 Reserved3;		 
	U8 MsgFlags;		 
	U8 VP_ID;		 
	U8 VF_ID;		 
	U16 Reserved4;		 
	U32 Flags;		 
} MPI2_TOOLBOX_CLEAN_REQUEST, *PTR_MPI2_TOOLBOX_CLEAN_REQUEST,
	Mpi2ToolboxCleanRequest_t, *pMpi2ToolboxCleanRequest_t;

 
#define MPI2_TOOLBOX_CLEAN_BOOT_SERVICES            (0x80000000)
#define MPI2_TOOLBOX_CLEAN_PERSIST_MANUFACT_PAGES   (0x40000000)
#define MPI2_TOOLBOX_CLEAN_OTHER_PERSIST_PAGES      (0x20000000)
#define MPI2_TOOLBOX_CLEAN_FW_CURRENT               (0x10000000)
#define MPI2_TOOLBOX_CLEAN_FW_BACKUP                (0x08000000)
#define MPI2_TOOLBOX_CLEAN_BIT26_PRODUCT_SPECIFIC   (0x04000000)
#define MPI2_TOOLBOX_CLEAN_MEGARAID                 (0x02000000)
#define MPI2_TOOLBOX_CLEAN_INITIALIZATION           (0x01000000)
#define MPI2_TOOLBOX_CLEAN_SBR                      (0x00800000)
#define MPI2_TOOLBOX_CLEAN_SBR_BACKUP               (0x00400000)
#define MPI2_TOOLBOX_CLEAN_HIIM                     (0x00200000)
#define MPI2_TOOLBOX_CLEAN_HIIA                     (0x00100000)
#define MPI2_TOOLBOX_CLEAN_CTLR                     (0x00080000)
#define MPI2_TOOLBOX_CLEAN_IMR_FIRMWARE             (0x00040000)
#define MPI2_TOOLBOX_CLEAN_MR_NVDATA                (0x00020000)
#define MPI2_TOOLBOX_CLEAN_RESERVED_5_16            (0x0001FFE0)
#define MPI2_TOOLBOX_CLEAN_ALL_BUT_MPB              (0x00000010)
#define MPI2_TOOLBOX_CLEAN_ENTIRE_FLASH             (0x00000008)
#define MPI2_TOOLBOX_CLEAN_FLASH                    (0x00000004)
#define MPI2_TOOLBOX_CLEAN_SEEPROM                  (0x00000002)
#define MPI2_TOOLBOX_CLEAN_NVSRAM                   (0x00000001)

 

typedef struct _MPI2_TOOLBOX_MEM_MOVE_REQUEST {
	U8 Tool;		 
	U8 Reserved1;		 
	U8 ChainOffset;		 
	U8 Function;		 
	U16 Reserved2;		 
	U8 Reserved3;		 
	U8 MsgFlags;		 
	U8 VP_ID;		 
	U8 VF_ID;		 
	U16 Reserved4;		 
	MPI2_SGE_SIMPLE_UNION SGL;	 
} MPI2_TOOLBOX_MEM_MOVE_REQUEST, *PTR_MPI2_TOOLBOX_MEM_MOVE_REQUEST,
	Mpi2ToolboxMemMoveRequest_t, *pMpi2ToolboxMemMoveRequest_t;

 

typedef struct _MPI2_TOOLBOX_DIAG_DATA_UPLOAD_REQUEST {
	U8 Tool;		 
	U8 Reserved1;		 
	U8 ChainOffset;		 
	U8 Function;		 
	U16 Reserved2;		 
	U8 Reserved3;		 
	U8 MsgFlags;		 
	U8 VP_ID;		 
	U8 VF_ID;		 
	U16 Reserved4;		 
	U8 SGLFlags;		 
	U8 Reserved5;		 
	U16 Reserved6;		 
	U32 Flags;		 
	U32 DataLength;		 
	MPI2_SGE_SIMPLE_UNION SGL;	 
} MPI2_TOOLBOX_DIAG_DATA_UPLOAD_REQUEST,
	*PTR_MPI2_TOOLBOX_DIAG_DATA_UPLOAD_REQUEST,
	Mpi2ToolboxDiagDataUploadRequest_t,
	*pMpi2ToolboxDiagDataUploadRequest_t;

 

typedef struct _MPI2_DIAG_DATA_UPLOAD_HEADER {
	U32 DiagDataLength;	 
	U8 FormatCode;		 
	U8 Reserved1;		 
	U16 Reserved2;		 
} MPI2_DIAG_DATA_UPLOAD_HEADER, *PTR_MPI2_DIAG_DATA_UPLOAD_HEADER,
	Mpi2DiagDataUploadHeader_t, *pMpi2DiagDataUploadHeader_t;

 

 
typedef struct _MPI2_TOOLBOX_ISTWI_READ_WRITE_REQUEST {
	U8 Tool;		 
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
	U8 DevIndex;		 
	U8 Action;		 
	U8 SGLFlags;		 
	U8 Flags;		 
	U16 TxDataLength;	 
	U16 RxDataLength;	 
	U32 Reserved8;		 
	U32 Reserved9;		 
	U32 Reserved10;		 
	U32 Reserved11;		 
	U32 Reserved12;		 
	MPI2_SGE_SIMPLE_UNION SGL;	 
} MPI2_TOOLBOX_ISTWI_READ_WRITE_REQUEST,
	*PTR_MPI2_TOOLBOX_ISTWI_READ_WRITE_REQUEST,
	Mpi2ToolboxIstwiReadWriteRequest_t,
	*pMpi2ToolboxIstwiReadWriteRequest_t;

 
#define MPI2_TOOL_ISTWI_ACTION_READ_DATA            (0x01)
#define MPI2_TOOL_ISTWI_ACTION_WRITE_DATA           (0x02)
#define MPI2_TOOL_ISTWI_ACTION_SEQUENCE             (0x03)
#define MPI2_TOOL_ISTWI_ACTION_RESERVE_BUS          (0x10)
#define MPI2_TOOL_ISTWI_ACTION_RELEASE_BUS          (0x11)
#define MPI2_TOOL_ISTWI_ACTION_RESET                (0x12)

 

 
#define MPI2_TOOL_ISTWI_FLAG_AUTO_RESERVE_RELEASE   (0x80)
#define MPI2_TOOL_ISTWI_FLAG_PAGE_ADDR_MASK         (0x07)

 
#define MPI26_TOOL_ISTWI_MSGFLG_ADDR_MASK           (0x01)
 
#define MPI26_TOOL_ISTWI_MSGFLG_ADDR_INDEX          (0x00)
 
#define MPI26_TOOL_ISTWI_MSGFLG_ADDR_INFO           (0x01)

 
typedef struct _MPI2_TOOLBOX_ISTWI_REPLY {
	U8 Tool;		 
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
	U8 DevIndex;		 
	U8 Action;		 
	U8 IstwiStatus;		 
	U8 Reserved6;		 
	U16 TxDataCount;	 
	U16 RxDataCount;	 
} MPI2_TOOLBOX_ISTWI_REPLY, *PTR_MPI2_TOOLBOX_ISTWI_REPLY,
	Mpi2ToolboxIstwiReply_t, *pMpi2ToolboxIstwiReply_t;

 

typedef struct _MPI2_TOOLBOX_BEACON_REQUEST {
	U8 Tool;		 
	U8 Reserved1;		 
	U8 ChainOffset;		 
	U8 Function;		 
	U16 Reserved2;		 
	U8 Reserved3;		 
	U8 MsgFlags;		 
	U8 VP_ID;		 
	U8 VF_ID;		 
	U16 Reserved4;		 
	U8 Reserved5;		 
	U8 PhysicalPort;	 
	U8 Reserved6;		 
	U8 Flags;		 
} MPI2_TOOLBOX_BEACON_REQUEST, *PTR_MPI2_TOOLBOX_BEACON_REQUEST,
	Mpi2ToolboxBeaconRequest_t, *pMpi2ToolboxBeaconRequest_t;

 
#define MPI2_TOOLBOX_FLAGS_BEACONMODE_OFF       (0x00)
#define MPI2_TOOLBOX_FLAGS_BEACONMODE_ON        (0x01)

 

#define MPI2_TOOLBOX_DIAG_CLI_CMD_LENGTH    (0x5C)

 
typedef struct _MPI2_TOOLBOX_DIAGNOSTIC_CLI_REQUEST {
	U8 Tool;		 
	U8 Reserved1;		 
	U8 ChainOffset;		 
	U8 Function;		 
	U16 Reserved2;		 
	U8 Reserved3;		 
	U8 MsgFlags;		 
	U8 VP_ID;		 
	U8 VF_ID;		 
	U16 Reserved4;		 
	U8 SGLFlags;		 
	U8 Reserved5;		 
	U16 Reserved6;		 
	U32 DataLength;		 
	U8 DiagnosticCliCommand[MPI2_TOOLBOX_DIAG_CLI_CMD_LENGTH]; 
	MPI2_MPI_SGE_IO_UNION SGL;	 
} MPI2_TOOLBOX_DIAGNOSTIC_CLI_REQUEST,
	*PTR_MPI2_TOOLBOX_DIAGNOSTIC_CLI_REQUEST,
	Mpi2ToolboxDiagnosticCliRequest_t,
	*pMpi2ToolboxDiagnosticCliRequest_t;

 

 
typedef struct _MPI25_TOOLBOX_DIAGNOSTIC_CLI_REQUEST {
	U8 Tool;		 
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
	U32 DataLength;		 
	U8 DiagnosticCliCommand[MPI2_TOOLBOX_DIAG_CLI_CMD_LENGTH]; 
	MPI25_SGE_IO_UNION      SGL;                         
} MPI25_TOOLBOX_DIAGNOSTIC_CLI_REQUEST,
	*PTR_MPI25_TOOLBOX_DIAGNOSTIC_CLI_REQUEST,
	Mpi25ToolboxDiagnosticCliRequest_t,
	*pMpi25ToolboxDiagnosticCliRequest_t;

 
typedef struct _MPI2_TOOLBOX_DIAGNOSTIC_CLI_REPLY {
	U8 Tool;		 
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
	U32 ReturnedDataLength;	 
} MPI2_TOOLBOX_DIAGNOSTIC_CLI_REPLY,
	*PTR_MPI2_TOOLBOX_DIAG_CLI_REPLY,
	Mpi2ToolboxDiagnosticCliReply_t,
	*pMpi2ToolboxDiagnosticCliReply_t;


 

 
typedef struct _MPI2_TOOLBOX_TEXT_DISPLAY_REQUEST {
	U8			Tool;			 
	U8			Reserved1;		 
	U8			ChainOffset;		 
	U8			Function;		 
	U16			Reserved2;		 
	U8			Reserved3;		 
	U8			MsgFlags;		 
	U8			VP_ID;			 
	U8			VF_ID;			 
	U16			Reserved4;		 
	U8			Console;		 
	U8			Flags;			 
	U16			Reserved6;		 
	U8			TextToDisplay[4];	 
} MPI2_TOOLBOX_TEXT_DISPLAY_REQUEST,
*PTR_MPI2_TOOLBOX_TEXT_DISPLAY_REQUEST,
Mpi2ToolboxTextDisplayRequest_t,
*pMpi2ToolboxTextDisplayRequest_t;

 
#define MPI2_TOOLBOX_CONSOLE_TYPE_MASK          (0xF0)
#define MPI2_TOOLBOX_CONSOLE_TYPE_DEFAULT       (0x00)
#define MPI2_TOOLBOX_CONSOLE_TYPE_UART          (0x10)
#define MPI2_TOOLBOX_CONSOLE_TYPE_ETHERNET      (0x20)

#define MPI2_TOOLBOX_CONSOLE_NUMBER_MASK        (0x0F)

 
#define MPI2_TOOLBOX_CONSOLE_FLAG_TIMESTAMP     (0x01)


 

 
typedef struct _MPI26_TOOLBOX_LANE_MARGIN_REQUEST {
	U8 Tool;			 
	U8 Reserved1;			 
	U8 ChainOffset;			 
	U8 Function;			 
	U16 Reserved2;			 
	U8 Reserved3;			 
	U8 MsgFlags;			 
	U8 VP_ID;			 
	U8 VF_ID;			 
	U16 Reserved4;			 
	U8 Command;			 
	U8 SwitchPort;			 
	U16 DevHandle;			 
	U8 RegisterOffset;		 
	U8 Reserved5;			 
	U16 DataLength;			 
	MPI25_SGE_IO_UNION SGL;		 
} MPI26_TOOLBOX_LANE_MARGINING_REQUEST,
	*PTR_MPI2_TOOLBOX_LANE_MARGINING_REQUEST,
	Mpi26ToolboxLaneMarginingRequest_t,
	*pMpi2ToolboxLaneMarginingRequest_t;

 
#define MPI26_TOOL_MARGIN_COMMAND_ENTER_MARGIN_MODE        (0x01)
#define MPI26_TOOL_MARGIN_COMMAND_READ_REGISTER_DATA       (0x02)
#define MPI26_TOOL_MARGIN_COMMAND_WRITE_REGISTER_DATA      (0x03)
#define MPI26_TOOL_MARGIN_COMMAND_EXIT_MARGIN_MODE         (0x04)


 
typedef struct _MPI26_TOOLBOX_LANE_MARGIN_REPLY {
	U8 Tool;			 
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
	U16 ReturnedDataLength;		 
	U16 Reserved6;			 
} MPI26_TOOLBOX_LANE_MARGINING_REPLY,
	*PTR_MPI26_TOOLBOX_LANE_MARGINING_REPLY,
	Mpi26ToolboxLaneMarginingReply_t,
	*pMpi26ToolboxLaneMarginingReply_t;


 

 

typedef struct _MPI2_DIAG_BUFFER_POST_REQUEST {
	U8 ExtendedType;	 
	U8 BufferType;		 
	U8 ChainOffset;		 
	U8 Function;		 
	U16 Reserved2;		 
	U8 Reserved3;		 
	U8 MsgFlags;		 
	U8 VP_ID;		 
	U8 VF_ID;		 
	U16 Reserved4;		 
	U64 BufferAddress;	 
	U32 BufferLength;	 
	U32 Reserved5;		 
	U32 Reserved6;		 
	U32 Flags;		 
	U32 ProductSpecific[23];	 
} MPI2_DIAG_BUFFER_POST_REQUEST, *PTR_MPI2_DIAG_BUFFER_POST_REQUEST,
	Mpi2DiagBufferPostRequest_t, *pMpi2DiagBufferPostRequest_t;

 
#define MPI2_DIAG_EXTENDED_TYPE_UTILIZATION         (0x02)

 
#define MPI2_DIAG_BUF_TYPE_TRACE                    (0x00)
#define MPI2_DIAG_BUF_TYPE_SNAPSHOT                 (0x01)
#define MPI2_DIAG_BUF_TYPE_EXTENDED                 (0x02)
 
#define MPI2_DIAG_BUF_TYPE_COUNT                    (0x03)

 
#define MPI2_DIAG_BUF_FLAG_RELEASE_ON_FULL          (0x00000002)
#define MPI2_DIAG_BUF_FLAG_IMMEDIATE_RELEASE        (0x00000001)

 

typedef struct _MPI2_DIAG_BUFFER_POST_REPLY {
	U8 ExtendedType;	 
	U8 BufferType;		 
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
	U32 TransferLength;	 
} MPI2_DIAG_BUFFER_POST_REPLY, *PTR_MPI2_DIAG_BUFFER_POST_REPLY,
	Mpi2DiagBufferPostReply_t, *pMpi2DiagBufferPostReply_t;

 

typedef struct _MPI2_DIAG_RELEASE_REQUEST {
	U8 Reserved1;		 
	U8 BufferType;		 
	U8 ChainOffset;		 
	U8 Function;		 
	U16 Reserved2;		 
	U8 Reserved3;		 
	U8 MsgFlags;		 
	U8 VP_ID;		 
	U8 VF_ID;		 
	U16 Reserved4;		 
} MPI2_DIAG_RELEASE_REQUEST, *PTR_MPI2_DIAG_RELEASE_REQUEST,
	Mpi2DiagReleaseRequest_t, *pMpi2DiagReleaseRequest_t;

 

typedef struct _MPI2_DIAG_RELEASE_REPLY {
	U8 Reserved1;		 
	U8 BufferType;		 
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
} MPI2_DIAG_RELEASE_REPLY, *PTR_MPI2_DIAG_RELEASE_REPLY,
	Mpi2DiagReleaseReply_t, *pMpi2DiagReleaseReply_t;

#endif
