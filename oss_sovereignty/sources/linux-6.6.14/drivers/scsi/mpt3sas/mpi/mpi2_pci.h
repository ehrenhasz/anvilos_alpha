

#ifndef MPI2_PCI_H
#define MPI2_PCI_H



#define MPI26_PCIE_DEVINFO_DIRECT_ATTACH        (0x00000010)

#define MPI26_PCIE_DEVINFO_MASK_DEVICE_TYPE     (0x0000000F)
#define MPI26_PCIE_DEVINFO_NO_DEVICE            (0x00000000)
#define MPI26_PCIE_DEVINFO_PCI_SWITCH           (0x00000001)
#define MPI26_PCIE_DEVINFO_NVME                 (0x00000003)
#define MPI26_PCIE_DEVINFO_SCSI                 (0x00000004)




typedef struct _MPI26_NVME_ENCAPSULATED_REQUEST {
	U16	DevHandle;                      
	U8	ChainOffset;                    
	U8	Function;                       
	U16	EncapsulatedCommandLength;      
	U8	Reserved1;                      
	U8	MsgFlags;                       
	U8	VP_ID;                          
	U8	VF_ID;                          
	U16	Reserved2;                      
	U32	Reserved3;                      
	U64	ErrorResponseBaseAddress;       
	U16	ErrorResponseAllocationLength;  
	U16	Flags;                          
	U32	DataLength;                     
	U8	NVMe_Command[4];                

} MPI26_NVME_ENCAPSULATED_REQUEST, *PTR_MPI26_NVME_ENCAPSULATED_REQUEST,
	Mpi26NVMeEncapsulatedRequest_t, *pMpi26NVMeEncapsulatedRequest_t;


#define MPI26_NVME_FLAGS_FORCE_ADMIN_ERR_RESP       (0x0020)

#define MPI26_NVME_FLAGS_SUBMISSIONQ_MASK           (0x0010)
#define MPI26_NVME_FLAGS_SUBMISSIONQ_IO             (0x0000)
#define MPI26_NVME_FLAGS_SUBMISSIONQ_ADMIN          (0x0010)

#define MPI26_NVME_FLAGS_ERR_RSP_ADDR_MASK          (0x000C)
#define MPI26_NVME_FLAGS_ERR_RSP_ADDR_SYSTEM        (0x0000)
#define MPI26_NVME_FLAGS_ERR_RSP_ADDR_IOCTL         (0x0008)

#define MPI26_NVME_FLAGS_DATADIRECTION_MASK         (0x0003)
#define MPI26_NVME_FLAGS_NODATATRANSFER             (0x0000)
#define MPI26_NVME_FLAGS_WRITE                      (0x0001)
#define MPI26_NVME_FLAGS_READ                       (0x0002)
#define MPI26_NVME_FLAGS_BIDIRECTIONAL              (0x0003)



typedef struct _MPI26_NVME_ENCAPSULATED_ERROR_REPLY {
	U16	DevHandle;                      
	U8	MsgLength;                      
	U8	Function;                       
	U16	EncapsulatedCommandLength;      
	U8	Reserved1;                      
	U8	MsgFlags;                       
	U8	VP_ID;                          
	U8	VF_ID;                          
	U16	Reserved2;                      
	U16	Reserved3;                      
	U16	IOCStatus;                      
	U32	IOCLogInfo;                     
	U16	ErrorResponseCount;             
	U16	Reserved4;                      
} MPI26_NVME_ENCAPSULATED_ERROR_REPLY,
	*PTR_MPI26_NVME_ENCAPSULATED_ERROR_REPLY,
	Mpi26NVMeEncapsulatedErrorReply_t,
	*pMpi26NVMeEncapsulatedErrorReply_t;


#endif
