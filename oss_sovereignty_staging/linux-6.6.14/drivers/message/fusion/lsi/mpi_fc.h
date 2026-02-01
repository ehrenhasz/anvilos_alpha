 
 

#ifndef MPI_FC_H
#define MPI_FC_H


 

 
 
 

typedef struct _MSG_LINK_SERVICE_BUFFER_POST_REQUEST
{
    U8                      BufferPostFlags;     
    U8                      BufferCount;         
    U8                      ChainOffset;         
    U8                      Function;            
    U16                     Reserved;            
    U8                      Reserved1;           
    U8                      MsgFlags;            
    U32                     MsgContext;          
    SGE_TRANS_SIMPLE_UNION  SGL;
} MSG_LINK_SERVICE_BUFFER_POST_REQUEST,
 MPI_POINTER PTR_MSG_LINK_SERVICE_BUFFER_POST_REQUEST,
  LinkServiceBufferPostRequest_t, MPI_POINTER pLinkServiceBufferPostRequest_t;

#define LINK_SERVICE_BUFFER_POST_FLAGS_PORT_MASK (0x01)

typedef struct _WWNFORMAT
{
    U32                     PortNameHigh;        
    U32                     PortNameLow;         
    U32                     NodeNameHigh;        
    U32                     NodeNameLow;         
} WWNFORMAT,
  WwnFormat_t;

 
typedef struct _MSG_LINK_SERVICE_BUFFER_POST_REPLY
{
    U8                      Flags;               
    U8                      Reserved;            
    U8                      MsgLength;           
    U8                      Function;            
    U16                     Reserved1;           
    U8                      PortNumber;          
    U8                      MsgFlags;            
    U32                     MsgContext;          
    U16                     Reserved2;           
    U16                     IOCStatus;           
    U32                     IOCLogInfo;          
    U32                     TransferLength;      
    U32                     TransactionContext;  
    U32                     Rctl_Did;            
    U32                     Csctl_Sid;           
    U32                     Type_Fctl;           
    U16                     SeqCnt;              
    U8                      Dfctl;               
    U8                      SeqId;               
    U16                     Rxid;                
    U16                     Oxid;                
    U32                     Parameter;           
    WWNFORMAT               Wwn;                 
} MSG_LINK_SERVICE_BUFFER_POST_REPLY, MPI_POINTER PTR_MSG_LINK_SERVICE_BUFFER_POST_REPLY,
  LinkServiceBufferPostReply_t, MPI_POINTER pLinkServiceBufferPostReply_t;

#define MPI_LS_BUF_POST_REPLY_FLAG_NO_RSP_NEEDED    (0x80)

#define MPI_FC_DID_MASK                             (0x00FFFFFF)
#define MPI_FC_DID_SHIFT                            (0)
#define MPI_FC_RCTL_MASK                            (0xFF000000)
#define MPI_FC_RCTL_SHIFT                           (24)
#define MPI_FC_SID_MASK                             (0x00FFFFFF)
#define MPI_FC_SID_SHIFT                            (0)
#define MPI_FC_CSCTL_MASK                           (0xFF000000)
#define MPI_FC_CSCTL_SHIFT                          (24)
#define MPI_FC_FCTL_MASK                            (0x00FFFFFF)
#define MPI_FC_FCTL_SHIFT                           (0)
#define MPI_FC_TYPE_MASK                            (0xFF000000)
#define MPI_FC_TYPE_SHIFT                           (24)

 
#define FCP_TARGET_DID_MASK                         (0x00FFFFFF)
#define FCP_TARGET_DID_SHIFT                        (0)
#define FCP_TARGET_RCTL_MASK                        (0xFF000000)
#define FCP_TARGET_RCTL_SHIFT                       (24)
#define FCP_TARGET_SID_MASK                         (0x00FFFFFF)
#define FCP_TARGET_SID_SHIFT                        (0)
#define FCP_TARGET_CSCTL_MASK                       (0xFF000000)
#define FCP_TARGET_CSCTL_SHIFT                      (24)
#define FCP_TARGET_FCTL_MASK                        (0x00FFFFFF)
#define FCP_TARGET_FCTL_SHIFT                       (0)
#define FCP_TARGET_TYPE_MASK                        (0xFF000000)
#define FCP_TARGET_TYPE_SHIFT                       (24)


 
 
 

typedef struct _MSG_LINK_SERVICE_RSP_REQUEST
{
    U8                      RspFlags;            
    U8                      RspLength;           
    U8                      ChainOffset;         
    U8                      Function;            
    U16                     Reserved1;           
    U8                      Reserved2;           
    U8                      MsgFlags;            
    U32                     MsgContext;          
    U32                     Rctl_Did;            
    U32                     Csctl_Sid;           
    U32                     Type_Fctl;           
    U16                     SeqCnt;              
    U8                      Dfctl;               
    U8                      SeqId;               
    U16                     Rxid;                
    U16                     Oxid;                
    U32                     Parameter;           
    SGE_SIMPLE_UNION        SGL;                 
} MSG_LINK_SERVICE_RSP_REQUEST, MPI_POINTER PTR_MSG_LINK_SERVICE_RSP_REQUEST,
  LinkServiceRspRequest_t, MPI_POINTER pLinkServiceRspRequest_t;

#define LINK_SERVICE_RSP_FLAGS_IMMEDIATE        (0x80)
#define LINK_SERVICE_RSP_FLAGS_PORT_MASK        (0x01)


 
typedef struct _MSG_LINK_SERVICE_RSP_REPLY
{
    U16                     Reserved;            
    U8                      MsgLength;           
    U8                      Function;            
    U16                     Reserved1;           
    U8                      Reserved_0100_InitiatorIndex;    
    U8                      MsgFlags;            
    U32                     MsgContext;          
    U16                     Reserved3;           
    U16                     IOCStatus;           
    U32                     IOCLogInfo;          
    U32                     InitiatorIndex;      
} MSG_LINK_SERVICE_RSP_REPLY, MPI_POINTER PTR_MSG_LINK_SERVICE_RSP_REPLY,
  LinkServiceRspReply_t, MPI_POINTER pLinkServiceRspReply_t;


 
 
 

typedef struct _MSG_EXLINK_SERVICE_SEND_REQUEST
{
    U8                      SendFlags;           
    U8                      AliasIndex;          
    U8                      ChainOffset;         
    U8                      Function;            
    U32                     MsgFlags_Did;        
    U32                     MsgContext;          
    U32                     ElsCommandCode;      
    SGE_SIMPLE_UNION        SGL;                 
} MSG_EXLINK_SERVICE_SEND_REQUEST, MPI_POINTER PTR_MSG_EXLINK_SERVICE_SEND_REQUEST,
  ExLinkServiceSendRequest_t, MPI_POINTER pExLinkServiceSendRequest_t;

#define EX_LINK_SERVICE_SEND_DID_MASK           (0x00FFFFFF)
#define EX_LINK_SERVICE_SEND_DID_SHIFT          (0)
#define EX_LINK_SERVICE_SEND_MSGFLAGS_MASK      (0xFF000000)
#define EX_LINK_SERVICE_SEND_MSGFLAGS_SHIFT     (24)


 
typedef struct _MSG_EXLINK_SERVICE_SEND_REPLY
{
    U8                      Reserved;            
    U8                      AliasIndex;          
    U8                      MsgLength;           
    U8                      Function;            
    U16                     Reserved1;           
    U8                      Reserved2;           
    U8                      MsgFlags;            
    U32                     MsgContext;          
    U16                     Reserved3;           
    U16                     IOCStatus;           
    U32                     IOCLogInfo;          
    U32                     ResponseLength;      
} MSG_EXLINK_SERVICE_SEND_REPLY, MPI_POINTER PTR_MSG_EXLINK_SERVICE_SEND_REPLY,
  ExLinkServiceSendReply_t, MPI_POINTER pExLinkServiceSendReply_t;

 
 
 

typedef struct _MSG_FC_ABORT_REQUEST
{
    U8                      AbortFlags;                  
    U8                      AbortType;                   
    U8                      ChainOffset;                 
    U8                      Function;                    
    U16                     Reserved1;                   
    U8                      Reserved2;                   
    U8                      MsgFlags;                    
    U32                     MsgContext;                  
    U32                     TransactionContextToAbort;   
} MSG_FC_ABORT_REQUEST, MPI_POINTER PTR_MSG_FC_ABORT_REQUEST,
  FcAbortRequest_t, MPI_POINTER pFcAbortRequest_t;

#define FC_ABORT_FLAG_PORT_MASK                 (0x01)

#define FC_ABORT_TYPE_ALL_FC_BUFFERS            (0x00)
#define FC_ABORT_TYPE_EXACT_FC_BUFFER           (0x01)
#define FC_ABORT_TYPE_CT_SEND_REQUEST           (0x02)
#define FC_ABORT_TYPE_EXLINKSEND_REQUEST        (0x03)

 
typedef struct _MSG_FC_ABORT_REPLY
{
    U16                     Reserved;            
    U8                      MsgLength;           
    U8                      Function;            
    U16                     Reserved1;           
    U8                      Reserved2;           
    U8                      MsgFlags;            
    U32                     MsgContext;          
    U16                     Reserved3;           
    U16                     IOCStatus;           
    U32                     IOCLogInfo;          
} MSG_FC_ABORT_REPLY, MPI_POINTER PTR_MSG_FC_ABORT_REPLY,
  FcAbortReply_t, MPI_POINTER pFcAbortReply_t;


 
 
 

typedef struct _MSG_FC_COMMON_TRANSPORT_SEND_REQUEST
{
    U8                      SendFlags;           
    U8                      AliasIndex;          
    U8                      ChainOffset;         
    U8                      Function;            
    U32                     MsgFlags_Did;        
    U32                     MsgContext;          
    U16                     CTCommandCode;       
    U8                      FsType;              
    U8                      Reserved1;           
    SGE_SIMPLE_UNION        SGL;                 
} MSG_FC_COMMON_TRANSPORT_SEND_REQUEST,
 MPI_POINTER PTR_MSG_FC_COMMON_TRANSPORT_SEND_REQUEST,
  FcCommonTransportSendRequest_t, MPI_POINTER pFcCommonTransportSendRequest_t;

#define MPI_FC_CT_SEND_DID_MASK                 (0x00FFFFFF)
#define MPI_FC_CT_SEND_DID_SHIFT                (0)
#define MPI_FC_CT_SEND_MSGFLAGS_MASK            (0xFF000000)
#define MPI_FC_CT_SEND_MSGFLAGS_SHIFT           (24)


 
typedef struct _MSG_FC_COMMON_TRANSPORT_SEND_REPLY
{
    U8                      Reserved;            
    U8                      AliasIndex;          
    U8                      MsgLength;           
    U8                      Function;            
    U16                     Reserved1;           
    U8                      Reserved2;           
    U8                      MsgFlags;            
    U32                     MsgContext;          
    U16                     Reserved3;           
    U16                     IOCStatus;           
    U32                     IOCLogInfo;          
    U32                     ResponseLength;      
} MSG_FC_COMMON_TRANSPORT_SEND_REPLY, MPI_POINTER PTR_MSG_FC_COMMON_TRANSPORT_SEND_REPLY,
  FcCommonTransportSendReply_t, MPI_POINTER pFcCommonTransportSendReply_t;


 
 
 

typedef struct _MSG_FC_PRIMITIVE_SEND_REQUEST
{
    U8                      SendFlags;           
    U8                      Reserved;            
    U8                      ChainOffset;         
    U8                      Function;            
    U16                     Reserved1;           
    U8                      Reserved2;           
    U8                      MsgFlags;            
    U32                     MsgContext;          
    U8                      FcPrimitive[4];      
} MSG_FC_PRIMITIVE_SEND_REQUEST, MPI_POINTER PTR_MSG_FC_PRIMITIVE_SEND_REQUEST,
  FcPrimitiveSendRequest_t, MPI_POINTER pFcPrimitiveSendRequest_t;

#define MPI_FC_PRIM_SEND_FLAGS_PORT_MASK       (0x01)
#define MPI_FC_PRIM_SEND_FLAGS_ML_RESET_LINK   (0x02)
#define MPI_FC_PRIM_SEND_FLAGS_RESET_LINK      (0x04)
#define MPI_FC_PRIM_SEND_FLAGS_STOP_SEND       (0x08)
#define MPI_FC_PRIM_SEND_FLAGS_SEND_ONCE       (0x10)
#define MPI_FC_PRIM_SEND_FLAGS_SEND_AROUND     (0x20)
#define MPI_FC_PRIM_SEND_FLAGS_UNTIL_FULL      (0x40)
#define MPI_FC_PRIM_SEND_FLAGS_FOREVER         (0x80)

 
typedef struct _MSG_FC_PRIMITIVE_SEND_REPLY
{
    U8                      SendFlags;           
    U8                      Reserved;            
    U8                      MsgLength;           
    U8                      Function;            
    U16                     Reserved1;           
    U8                      Reserved2;           
    U8                      MsgFlags;            
    U32                     MsgContext;          
    U16                     Reserved3;           
    U16                     IOCStatus;           
    U32                     IOCLogInfo;          
} MSG_FC_PRIMITIVE_SEND_REPLY, MPI_POINTER PTR_MSG_FC_PRIMITIVE_SEND_REPLY,
  FcPrimitiveSendReply_t, MPI_POINTER pFcPrimitiveSendReply_t;

#endif

