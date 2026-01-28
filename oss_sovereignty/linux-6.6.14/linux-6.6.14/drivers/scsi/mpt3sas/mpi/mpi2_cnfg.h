#ifndef MPI2_CNFG_H
#define MPI2_CNFG_H
typedef struct _MPI2_CONFIG_PAGE_HEADER {
	U8                 PageVersion;                 
	U8                 PageLength;                  
	U8                 PageNumber;                  
	U8                 PageType;                    
} MPI2_CONFIG_PAGE_HEADER, *PTR_MPI2_CONFIG_PAGE_HEADER,
	Mpi2ConfigPageHeader_t, *pMpi2ConfigPageHeader_t;
typedef union _MPI2_CONFIG_PAGE_HEADER_UNION {
	MPI2_CONFIG_PAGE_HEADER  Struct;
	U8                       Bytes[4];
	U16                      Word16[2];
	U32                      Word32;
} MPI2_CONFIG_PAGE_HEADER_UNION, *PTR_MPI2_CONFIG_PAGE_HEADER_UNION,
	Mpi2ConfigPageHeaderUnion, *pMpi2ConfigPageHeaderUnion;
typedef struct _MPI2_CONFIG_EXTENDED_PAGE_HEADER {
	U8                  PageVersion;                 
	U8                  Reserved1;                   
	U8                  PageNumber;                  
	U8                  PageType;                    
	U16                 ExtPageLength;               
	U8                  ExtPageType;                 
	U8                  Reserved2;                   
} MPI2_CONFIG_EXTENDED_PAGE_HEADER,
	*PTR_MPI2_CONFIG_EXTENDED_PAGE_HEADER,
	Mpi2ConfigExtendedPageHeader_t,
	*pMpi2ConfigExtendedPageHeader_t;
typedef union _MPI2_CONFIG_EXT_PAGE_HEADER_UNION {
	MPI2_CONFIG_PAGE_HEADER          Struct;
	MPI2_CONFIG_EXTENDED_PAGE_HEADER Ext;
	U8                               Bytes[8];
	U16                              Word16[4];
	U32                              Word32[2];
} MPI2_CONFIG_EXT_PAGE_HEADER_UNION,
	*PTR_MPI2_CONFIG_EXT_PAGE_HEADER_UNION,
	Mpi2ConfigPageExtendedHeaderUnion,
	*pMpi2ConfigPageExtendedHeaderUnion;
#define MPI2_CONFIG_PAGEATTR_READ_ONLY              (0x00)
#define MPI2_CONFIG_PAGEATTR_CHANGEABLE             (0x10)
#define MPI2_CONFIG_PAGEATTR_PERSISTENT             (0x20)
#define MPI2_CONFIG_PAGEATTR_MASK                   (0xF0)
#define MPI2_CONFIG_PAGETYPE_IO_UNIT                (0x00)
#define MPI2_CONFIG_PAGETYPE_IOC                    (0x01)
#define MPI2_CONFIG_PAGETYPE_BIOS                   (0x02)
#define MPI2_CONFIG_PAGETYPE_RAID_VOLUME            (0x08)
#define MPI2_CONFIG_PAGETYPE_MANUFACTURING          (0x09)
#define MPI2_CONFIG_PAGETYPE_RAID_PHYSDISK          (0x0A)
#define MPI2_CONFIG_PAGETYPE_EXTENDED               (0x0F)
#define MPI2_CONFIG_PAGETYPE_MASK                   (0x0F)
#define MPI2_CONFIG_TYPENUM_MASK                    (0x0FFF)
#define MPI2_CONFIG_EXTPAGETYPE_SAS_IO_UNIT         (0x10)
#define MPI2_CONFIG_EXTPAGETYPE_SAS_EXPANDER        (0x11)
#define MPI2_CONFIG_EXTPAGETYPE_SAS_DEVICE          (0x12)
#define MPI2_CONFIG_EXTPAGETYPE_SAS_PHY             (0x13)
#define MPI2_CONFIG_EXTPAGETYPE_LOG                 (0x14)
#define MPI2_CONFIG_EXTPAGETYPE_ENCLOSURE           (0x15)
#define MPI2_CONFIG_EXTPAGETYPE_RAID_CONFIG         (0x16)
#define MPI2_CONFIG_EXTPAGETYPE_DRIVER_MAPPING      (0x17)
#define MPI2_CONFIG_EXTPAGETYPE_SAS_PORT            (0x18)
#define MPI2_CONFIG_EXTPAGETYPE_ETHERNET            (0x19)
#define MPI2_CONFIG_EXTPAGETYPE_EXT_MANUFACTURING   (0x1A)
#define MPI2_CONFIG_EXTPAGETYPE_PCIE_IO_UNIT        (0x1B)
#define MPI2_CONFIG_EXTPAGETYPE_PCIE_SWITCH         (0x1C)
#define MPI2_CONFIG_EXTPAGETYPE_PCIE_DEVICE         (0x1D)
#define MPI2_CONFIG_EXTPAGETYPE_PCIE_LINK           (0x1E)
#define MPI2_RAID_VOLUME_PGAD_FORM_MASK             (0xF0000000)
#define MPI2_RAID_VOLUME_PGAD_FORM_GET_NEXT_HANDLE  (0x00000000)
#define MPI2_RAID_VOLUME_PGAD_FORM_HANDLE           (0x10000000)
#define MPI2_RAID_VOLUME_PGAD_HANDLE_MASK           (0x0000FFFF)
#define MPI2_PHYSDISK_PGAD_FORM_MASK                    (0xF0000000)
#define MPI2_PHYSDISK_PGAD_FORM_GET_NEXT_PHYSDISKNUM    (0x00000000)
#define MPI2_PHYSDISK_PGAD_FORM_PHYSDISKNUM             (0x10000000)
#define MPI2_PHYSDISK_PGAD_FORM_DEVHANDLE               (0x20000000)
#define MPI2_PHYSDISK_PGAD_PHYSDISKNUM_MASK             (0x000000FF)
#define MPI2_PHYSDISK_PGAD_DEVHANDLE_MASK               (0x0000FFFF)
#define MPI2_SAS_EXPAND_PGAD_FORM_MASK              (0xF0000000)
#define MPI2_SAS_EXPAND_PGAD_FORM_GET_NEXT_HNDL     (0x00000000)
#define MPI2_SAS_EXPAND_PGAD_FORM_HNDL_PHY_NUM      (0x10000000)
#define MPI2_SAS_EXPAND_PGAD_FORM_HNDL              (0x20000000)
#define MPI2_SAS_EXPAND_PGAD_HANDLE_MASK            (0x0000FFFF)
#define MPI2_SAS_EXPAND_PGAD_PHYNUM_MASK            (0x00FF0000)
#define MPI2_SAS_EXPAND_PGAD_PHYNUM_SHIFT           (16)
#define MPI2_SAS_DEVICE_PGAD_FORM_MASK              (0xF0000000)
#define MPI2_SAS_DEVICE_PGAD_FORM_GET_NEXT_HANDLE   (0x00000000)
#define MPI2_SAS_DEVICE_PGAD_FORM_HANDLE            (0x20000000)
#define MPI2_SAS_DEVICE_PGAD_HANDLE_MASK            (0x0000FFFF)
#define MPI2_SAS_PHY_PGAD_FORM_MASK                 (0xF0000000)
#define MPI2_SAS_PHY_PGAD_FORM_PHY_NUMBER           (0x00000000)
#define MPI2_SAS_PHY_PGAD_FORM_PHY_TBL_INDEX        (0x10000000)
#define MPI2_SAS_PHY_PGAD_PHY_NUMBER_MASK           (0x000000FF)
#define MPI2_SAS_PHY_PGAD_PHY_TBL_INDEX_MASK        (0x0000FFFF)
#define MPI2_SASPORT_PGAD_FORM_MASK                 (0xF0000000)
#define MPI2_SASPORT_PGAD_FORM_GET_NEXT_PORT        (0x00000000)
#define MPI2_SASPORT_PGAD_FORM_PORT_NUM             (0x10000000)
#define MPI2_SASPORT_PGAD_PORTNUMBER_MASK           (0x00000FFF)
#define MPI2_SAS_ENCLOS_PGAD_FORM_MASK              (0xF0000000)
#define MPI2_SAS_ENCLOS_PGAD_FORM_GET_NEXT_HANDLE   (0x00000000)
#define MPI2_SAS_ENCLOS_PGAD_FORM_HANDLE            (0x10000000)
#define MPI2_SAS_ENCLOS_PGAD_HANDLE_MASK            (0x0000FFFF)
#define MPI26_ENCLOS_PGAD_FORM_MASK                 (0xF0000000)
#define MPI26_ENCLOS_PGAD_FORM_GET_NEXT_HANDLE      (0x00000000)
#define MPI26_ENCLOS_PGAD_FORM_HANDLE               (0x10000000)
#define MPI26_ENCLOS_PGAD_HANDLE_MASK               (0x0000FFFF)
#define MPI2_RAID_PGAD_FORM_MASK                    (0xF0000000)
#define MPI2_RAID_PGAD_FORM_GET_NEXT_CONFIGNUM      (0x00000000)
#define MPI2_RAID_PGAD_FORM_CONFIGNUM               (0x10000000)
#define MPI2_RAID_PGAD_FORM_ACTIVE_CONFIG           (0x20000000)
#define MPI2_RAID_PGAD_CONFIGNUM_MASK               (0x000000FF)
#define MPI2_DPM_PGAD_FORM_MASK                     (0xF0000000)
#define MPI2_DPM_PGAD_FORM_ENTRY_RANGE              (0x00000000)
#define MPI2_DPM_PGAD_ENTRY_COUNT_MASK              (0x0FFF0000)
#define MPI2_DPM_PGAD_ENTRY_COUNT_SHIFT             (16)
#define MPI2_DPM_PGAD_START_ENTRY_MASK              (0x0000FFFF)
#define MPI2_ETHERNET_PGAD_FORM_MASK                (0xF0000000)
#define MPI2_ETHERNET_PGAD_FORM_IF_NUM              (0x00000000)
#define MPI2_ETHERNET_PGAD_IF_NUMBER_MASK           (0x000000FF)
#define MPI26_PCIE_SWITCH_PGAD_FORM_MASK            (0xF0000000)
#define MPI26_PCIE_SWITCH_PGAD_FORM_GET_NEXT_HNDL   (0x00000000)
#define MPI26_PCIE_SWITCH_PGAD_FORM_HNDL_PORTNUM    (0x10000000)
#define MPI26_PCIE_SWITCH_EXPAND_PGAD_FORM_HNDL     (0x20000000)
#define MPI26_PCIE_SWITCH_PGAD_HANDLE_MASK          (0x0000FFFF)
#define MPI26_PCIE_SWITCH_PGAD_PORTNUM_MASK         (0x00FF0000)
#define MPI26_PCIE_SWITCH_PGAD_PORTNUM_SHIFT        (16)
#define MPI26_PCIE_DEVICE_PGAD_FORM_MASK            (0xF0000000)
#define MPI26_PCIE_DEVICE_PGAD_FORM_GET_NEXT_HANDLE (0x00000000)
#define MPI26_PCIE_DEVICE_PGAD_FORM_HANDLE          (0x20000000)
#define MPI26_PCIE_DEVICE_PGAD_HANDLE_MASK          (0x0000FFFF)
#define MPI26_PCIE_LINK_PGAD_FORM_MASK            (0xF0000000)
#define MPI26_PCIE_LINK_PGAD_FORM_GET_NEXT_LINK   (0x00000000)
#define MPI26_PCIE_LINK_PGAD_FORM_LINK_NUM        (0x10000000)
#define MPI26_PCIE_DEVICE_PGAD_LINKNUM_MASK       (0x000000FF)
typedef struct _MPI2_CONFIG_REQUEST {
	U8                      Action;                      
	U8                      SGLFlags;                    
	U8                      ChainOffset;                 
	U8                      Function;                    
	U16                     ExtPageLength;               
	U8                      ExtPageType;                 
	U8                      MsgFlags;                    
	U8                      VP_ID;                       
	U8                      VF_ID;                       
	U16                     Reserved1;                   
	U8                      Reserved2;                   
	U8                      ProxyVF_ID;                  
	U16                     Reserved4;                   
	U32                     Reserved3;                   
	MPI2_CONFIG_PAGE_HEADER Header;                      
	U32                     PageAddress;                 
	MPI2_SGE_IO_UNION       PageBufferSGE;               
} MPI2_CONFIG_REQUEST, *PTR_MPI2_CONFIG_REQUEST,
	Mpi2ConfigRequest_t, *pMpi2ConfigRequest_t;
#define MPI2_CONFIG_ACTION_PAGE_HEADER              (0x00)
#define MPI2_CONFIG_ACTION_PAGE_READ_CURRENT        (0x01)
#define MPI2_CONFIG_ACTION_PAGE_WRITE_CURRENT       (0x02)
#define MPI2_CONFIG_ACTION_PAGE_DEFAULT             (0x03)
#define MPI2_CONFIG_ACTION_PAGE_WRITE_NVRAM         (0x04)
#define MPI2_CONFIG_ACTION_PAGE_READ_DEFAULT        (0x05)
#define MPI2_CONFIG_ACTION_PAGE_READ_NVRAM          (0x06)
#define MPI2_CONFIG_ACTION_PAGE_GET_CHANGEABLE      (0x07)
typedef struct _MPI2_CONFIG_REPLY {
	U8                      Action;                      
	U8                      SGLFlags;                    
	U8                      MsgLength;                   
	U8                      Function;                    
	U16                     ExtPageLength;               
	U8                      ExtPageType;                 
	U8                      MsgFlags;                    
	U8                      VP_ID;                       
	U8                      VF_ID;                       
	U16                     Reserved1;                   
	U16                     Reserved2;                   
	U16                     IOCStatus;                   
	U32                     IOCLogInfo;                  
	MPI2_CONFIG_PAGE_HEADER Header;                      
} MPI2_CONFIG_REPLY, *PTR_MPI2_CONFIG_REPLY,
	Mpi2ConfigReply_t, *pMpi2ConfigReply_t;
#define MPI2_MFGPAGE_VENDORID_LSI                   (0x1000)
#define MPI2_MFGPAGE_VENDORID_ATTO                  (0x117C)
#define MPI2_MFGPAGE_DEVID_SAS2004                  (0x0070)
#define MPI2_MFGPAGE_DEVID_SAS2008                  (0x0072)
#define MPI2_MFGPAGE_DEVID_SAS2108_1                (0x0074)
#define MPI2_MFGPAGE_DEVID_SAS2108_2                (0x0076)
#define MPI2_MFGPAGE_DEVID_SAS2108_3                (0x0077)
#define MPI2_MFGPAGE_DEVID_SAS2116_1                (0x0064)
#define MPI2_MFGPAGE_DEVID_SAS2116_2                (0x0065)
#define MPI2_MFGPAGE_DEVID_SSS6200                  (0x007E)
#define MPI2_MFGPAGE_DEVID_SAS2208_1                (0x0080)
#define MPI2_MFGPAGE_DEVID_SAS2208_2                (0x0081)
#define MPI2_MFGPAGE_DEVID_SAS2208_3                (0x0082)
#define MPI2_MFGPAGE_DEVID_SAS2208_4                (0x0083)
#define MPI2_MFGPAGE_DEVID_SAS2208_5                (0x0084)
#define MPI2_MFGPAGE_DEVID_SAS2208_6                (0x0085)
#define MPI2_MFGPAGE_DEVID_SAS2308_1                (0x0086)
#define MPI2_MFGPAGE_DEVID_SAS2308_2                (0x0087)
#define MPI2_MFGPAGE_DEVID_SAS2308_3                (0x006E)
#define MPI2_MFGPAGE_DEVID_SWITCH_MPI_EP            (0x02B0)
#define MPI2_MFGPAGE_DEVID_SWITCH_MPI_EP_1          (0x02B1)
#define MPI25_MFGPAGE_DEVID_SAS3004                 (0x0096)
#define MPI25_MFGPAGE_DEVID_SAS3008                 (0x0097)
#define MPI25_MFGPAGE_DEVID_SAS3108_1               (0x0090)
#define MPI25_MFGPAGE_DEVID_SAS3108_2               (0x0091)
#define MPI25_MFGPAGE_DEVID_SAS3108_5               (0x0094)
#define MPI25_MFGPAGE_DEVID_SAS3108_6               (0x0095)
#define MPI26_MFGPAGE_DEVID_SAS3216                 (0x00C9)
#define MPI26_MFGPAGE_DEVID_SAS3224                 (0x00C4)
#define MPI26_MFGPAGE_DEVID_SAS3316_1               (0x00C5)
#define MPI26_MFGPAGE_DEVID_SAS3316_2               (0x00C6)
#define MPI26_MFGPAGE_DEVID_SAS3316_3               (0x00C7)
#define MPI26_MFGPAGE_DEVID_SAS3316_4               (0x00C8)
#define MPI26_MFGPAGE_DEVID_SAS3324_1               (0x00C0)
#define MPI26_MFGPAGE_DEVID_SAS3324_2               (0x00C1)
#define MPI26_MFGPAGE_DEVID_SAS3324_3               (0x00C2)
#define MPI26_MFGPAGE_DEVID_SAS3324_4               (0x00C3)
#define MPI26_MFGPAGE_DEVID_SAS3516                 (0x00AA)
#define MPI26_MFGPAGE_DEVID_SAS3516_1               (0x00AB)
#define MPI26_MFGPAGE_DEVID_SAS3416                 (0x00AC)
#define MPI26_MFGPAGE_DEVID_SAS3508                 (0x00AD)
#define MPI26_MFGPAGE_DEVID_SAS3508_1               (0x00AE)
#define MPI26_MFGPAGE_DEVID_SAS3408                 (0x00AF)
#define MPI26_MFGPAGE_DEVID_SAS3716                 (0x00D0)
#define MPI26_MFGPAGE_DEVID_SAS3616                 (0x00D1)
#define MPI26_MFGPAGE_DEVID_SAS3708                 (0x00D2)
#define MPI26_MFGPAGE_DEVID_SEC_MASK_3916           (0x0003)
#define MPI26_MFGPAGE_DEVID_INVALID0_3916           (0x00E0)
#define MPI26_MFGPAGE_DEVID_CFG_SEC_3916            (0x00E1)
#define MPI26_MFGPAGE_DEVID_HARD_SEC_3916           (0x00E2)
#define MPI26_MFGPAGE_DEVID_INVALID1_3916           (0x00E3)
#define MPI26_MFGPAGE_DEVID_SEC_MASK_3816           (0x0003)
#define MPI26_MFGPAGE_DEVID_INVALID0_3816           (0x00E4)
#define MPI26_MFGPAGE_DEVID_CFG_SEC_3816            (0x00E5)
#define MPI26_MFGPAGE_DEVID_HARD_SEC_3816           (0x00E6)
#define MPI26_MFGPAGE_DEVID_INVALID1_3816           (0x00E7)
typedef struct _MPI2_CONFIG_PAGE_MAN_0 {
	MPI2_CONFIG_PAGE_HEADER Header;                      
	U8                      ChipName[16];                
	U8                      ChipRevision[8];             
	U8                      BoardName[16];               
	U8                      BoardAssembly[16];           
	U8                      BoardTracerNumber[16];       
} MPI2_CONFIG_PAGE_MAN_0,
	*PTR_MPI2_CONFIG_PAGE_MAN_0,
	Mpi2ManufacturingPage0_t,
	*pMpi2ManufacturingPage0_t;
#define MPI2_MANUFACTURING0_PAGEVERSION                (0x00)
typedef struct _MPI2_CONFIG_PAGE_MAN_1 {
	MPI2_CONFIG_PAGE_HEADER Header;                      
	U8                      VPD[256];                    
} MPI2_CONFIG_PAGE_MAN_1,
	*PTR_MPI2_CONFIG_PAGE_MAN_1,
	Mpi2ManufacturingPage1_t,
	*pMpi2ManufacturingPage1_t;
#define MPI2_MANUFACTURING1_PAGEVERSION                (0x00)
typedef struct _MPI2_CHIP_REVISION_ID {
	U16 DeviceID;                                        
	U8  PCIRevisionID;                                   
	U8  Reserved;                                        
} MPI2_CHIP_REVISION_ID, *PTR_MPI2_CHIP_REVISION_ID,
	Mpi2ChipRevisionId_t, *pMpi2ChipRevisionId_t;
#ifndef MPI2_MAN_PAGE_2_HW_SETTINGS_WORDS
#define MPI2_MAN_PAGE_2_HW_SETTINGS_WORDS   (1)
#endif
typedef struct _MPI2_CONFIG_PAGE_MAN_2 {
	MPI2_CONFIG_PAGE_HEADER Header;                      
	MPI2_CHIP_REVISION_ID   ChipId;                      
	U32
		HwSettings[MPI2_MAN_PAGE_2_HW_SETTINGS_WORDS]; 
} MPI2_CONFIG_PAGE_MAN_2,
	*PTR_MPI2_CONFIG_PAGE_MAN_2,
	Mpi2ManufacturingPage2_t,
	*pMpi2ManufacturingPage2_t;
#define MPI2_MANUFACTURING2_PAGEVERSION                 (0x00)
#ifndef MPI2_MAN_PAGE_3_INFO_WORDS
#define MPI2_MAN_PAGE_3_INFO_WORDS          (1)
#endif
typedef struct _MPI2_CONFIG_PAGE_MAN_3 {
	MPI2_CONFIG_PAGE_HEADER             Header;          
	MPI2_CHIP_REVISION_ID               ChipId;          
	U32
		Info[MPI2_MAN_PAGE_3_INFO_WORDS]; 
} MPI2_CONFIG_PAGE_MAN_3,
	*PTR_MPI2_CONFIG_PAGE_MAN_3,
	Mpi2ManufacturingPage3_t,
	*pMpi2ManufacturingPage3_t;
#define MPI2_MANUFACTURING3_PAGEVERSION                 (0x00)
typedef struct _MPI2_MANPAGE4_PWR_SAVE_SETTINGS {
	U8                          PowerSaveFlags;                  
	U8                          InternalOperationsSleepTime;     
	U8                          InternalOperationsRunTime;       
	U8                          HostIdleTime;                    
} MPI2_MANPAGE4_PWR_SAVE_SETTINGS,
	*PTR_MPI2_MANPAGE4_PWR_SAVE_SETTINGS,
	Mpi2ManPage4PwrSaveSettings_t,
	*pMpi2ManPage4PwrSaveSettings_t;
#define MPI2_MANPAGE4_MASK_POWERSAVE_MODE               (0x03)
#define MPI2_MANPAGE4_POWERSAVE_MODE_DISABLED           (0x00)
#define MPI2_MANPAGE4_CUSTOM_POWERSAVE_MODE             (0x01)
#define MPI2_MANPAGE4_FULL_POWERSAVE_MODE               (0x02)
typedef struct _MPI2_CONFIG_PAGE_MAN_4 {
	MPI2_CONFIG_PAGE_HEADER             Header;                  
	U32                                 Reserved1;               
	U32                                 Flags;                   
	U8                                  InquirySize;             
	U8                                  Reserved2;               
	U16                                 Reserved3;               
	U8                                  InquiryData[56];         
	U32                                 RAID0VolumeSettings;     
	U32                                 RAID1EVolumeSettings;    
	U32                                 RAID1VolumeSettings;     
	U32                                 RAID10VolumeSettings;    
	U32                                 Reserved4;               
	U32                                 Reserved5;               
	MPI2_MANPAGE4_PWR_SAVE_SETTINGS     PowerSaveSettings;       
	U8                                  MaxOCEDisks;             
	U8                                  ResyncRate;              
	U16                                 DataScrubDuration;       
	U8                                  MaxHotSpares;            
	U8                                  MaxPhysDisksPerVol;      
	U8                                  MaxPhysDisks;            
	U8                                  MaxVolumes;              
} MPI2_CONFIG_PAGE_MAN_4,
	*PTR_MPI2_CONFIG_PAGE_MAN_4,
	Mpi2ManufacturingPage4_t,
	*pMpi2ManufacturingPage4_t;
#define MPI2_MANUFACTURING4_PAGEVERSION                 (0x0A)
#define MPI2_MANPAGE4_METADATA_SIZE_MASK                (0x00030000)
#define MPI2_MANPAGE4_METADATA_512MB                    (0x00000000)
#define MPI2_MANPAGE4_MIX_SSD_SAS_SATA                  (0x00008000)
#define MPI2_MANPAGE4_MIX_SSD_AND_NON_SSD               (0x00004000)
#define MPI2_MANPAGE4_HIDE_PHYSDISK_NON_IR              (0x00002000)
#define MPI2_MANPAGE4_MASK_PHYSDISK_COERCION            (0x00001C00)
#define MPI2_MANPAGE4_PHYSDISK_COERCION_1GB             (0x00000000)
#define MPI2_MANPAGE4_PHYSDISK_128MB_COERCION           (0x00000400)
#define MPI2_MANPAGE4_PHYSDISK_ADAPTIVE_COERCION        (0x00000800)
#define MPI2_MANPAGE4_PHYSDISK_ZERO_COERCION            (0x00000C00)
#define MPI2_MANPAGE4_MASK_BAD_BLOCK_MARKING            (0x00000300)
#define MPI2_MANPAGE4_DEFAULT_BAD_BLOCK_MARKING         (0x00000000)
#define MPI2_MANPAGE4_TABLE_BAD_BLOCK_MARKING           (0x00000100)
#define MPI2_MANPAGE4_WRITE_LONG_BAD_BLOCK_MARKING      (0x00000200)
#define MPI2_MANPAGE4_FORCE_OFFLINE_FAILOVER            (0x00000080)
#define MPI2_MANPAGE4_RAID10_DISABLE                    (0x00000040)
#define MPI2_MANPAGE4_RAID1E_DISABLE                    (0x00000020)
#define MPI2_MANPAGE4_RAID1_DISABLE                     (0x00000010)
#define MPI2_MANPAGE4_RAID0_DISABLE                     (0x00000008)
#define MPI2_MANPAGE4_IR_MODEPAGE8_DISABLE              (0x00000004)
#define MPI2_MANPAGE4_IM_RESYNC_CACHE_ENABLE            (0x00000002)
#define MPI2_MANPAGE4_IR_NO_MIX_SAS_SATA                (0x00000001)
#ifndef MPI2_MAN_PAGE_5_PHY_ENTRIES
#define MPI2_MAN_PAGE_5_PHY_ENTRIES         (1)
#endif
typedef struct _MPI2_MANUFACTURING5_ENTRY {
	U64                                 WWID;            
	U64                                 DeviceName;      
} MPI2_MANUFACTURING5_ENTRY,
	*PTR_MPI2_MANUFACTURING5_ENTRY,
	Mpi2Manufacturing5Entry_t,
	*pMpi2Manufacturing5Entry_t;
typedef struct _MPI2_CONFIG_PAGE_MAN_5 {
	MPI2_CONFIG_PAGE_HEADER             Header;          
	U8                                  NumPhys;         
	U8                                  Reserved1;       
	U16                                 Reserved2;       
	U32                                 Reserved3;       
	U32                                 Reserved4;       
	MPI2_MANUFACTURING5_ENTRY
		Phy[MPI2_MAN_PAGE_5_PHY_ENTRIES]; 
} MPI2_CONFIG_PAGE_MAN_5,
	*PTR_MPI2_CONFIG_PAGE_MAN_5,
	Mpi2ManufacturingPage5_t,
	*pMpi2ManufacturingPage5_t;
#define MPI2_MANUFACTURING5_PAGEVERSION                 (0x03)
typedef struct _MPI2_CONFIG_PAGE_MAN_6 {
	MPI2_CONFIG_PAGE_HEADER         Header;              
	U32                             ProductSpecificInfo; 
} MPI2_CONFIG_PAGE_MAN_6,
	*PTR_MPI2_CONFIG_PAGE_MAN_6,
	Mpi2ManufacturingPage6_t,
	*pMpi2ManufacturingPage6_t;
#define MPI2_MANUFACTURING6_PAGEVERSION                 (0x00)
typedef struct _MPI2_MANPAGE7_CONNECTOR_INFO {
	U32                         Pinout;                  
	U8                          Connector[16];           
	U8                          Location;                
	U8                          ReceptacleID;            
	U16                         Slot;                    
	U16                         Slotx2;                  
	U16                         Slotx4;                  
} MPI2_MANPAGE7_CONNECTOR_INFO,
	*PTR_MPI2_MANPAGE7_CONNECTOR_INFO,
	Mpi2ManPage7ConnectorInfo_t,
	*pMpi2ManPage7ConnectorInfo_t;
#define MPI2_MANPAGE7_PINOUT_LANE_MASK                  (0x0000FF00)
#define MPI2_MANPAGE7_PINOUT_LANE_SHIFT                 (8)
#define MPI2_MANPAGE7_PINOUT_TYPE_MASK                  (0x000000FF)
#define MPI2_MANPAGE7_PINOUT_TYPE_UNKNOWN               (0x00)
#define MPI2_MANPAGE7_PINOUT_SATA_SINGLE                (0x01)
#define MPI2_MANPAGE7_PINOUT_SFF_8482                   (0x02)
#define MPI2_MANPAGE7_PINOUT_SFF_8486                   (0x03)
#define MPI2_MANPAGE7_PINOUT_SFF_8484                   (0x04)
#define MPI2_MANPAGE7_PINOUT_SFF_8087                   (0x05)
#define MPI2_MANPAGE7_PINOUT_SFF_8643_4I                (0x06)
#define MPI2_MANPAGE7_PINOUT_SFF_8643_8I                (0x07)
#define MPI2_MANPAGE7_PINOUT_SFF_8470                   (0x08)
#define MPI2_MANPAGE7_PINOUT_SFF_8088                   (0x09)
#define MPI2_MANPAGE7_PINOUT_SFF_8644_4X                (0x0A)
#define MPI2_MANPAGE7_PINOUT_SFF_8644_8X                (0x0B)
#define MPI2_MANPAGE7_PINOUT_SFF_8644_16X               (0x0C)
#define MPI2_MANPAGE7_PINOUT_SFF_8436                   (0x0D)
#define MPI2_MANPAGE7_PINOUT_SFF_8088_A                 (0x0E)
#define MPI2_MANPAGE7_PINOUT_SFF_8643_16i               (0x0F)
#define MPI2_MANPAGE7_PINOUT_SFF_8654_4i                (0x10)
#define MPI2_MANPAGE7_PINOUT_SFF_8654_8i                (0x11)
#define MPI2_MANPAGE7_PINOUT_SFF_8611_4i                (0x12)
#define MPI2_MANPAGE7_PINOUT_SFF_8611_8i                (0x13)
#define MPI2_MANPAGE7_LOCATION_UNKNOWN                  (0x01)
#define MPI2_MANPAGE7_LOCATION_INTERNAL                 (0x02)
#define MPI2_MANPAGE7_LOCATION_EXTERNAL                 (0x04)
#define MPI2_MANPAGE7_LOCATION_SWITCHABLE               (0x08)
#define MPI2_MANPAGE7_LOCATION_AUTO                     (0x10)
#define MPI2_MANPAGE7_LOCATION_NOT_PRESENT              (0x20)
#define MPI2_MANPAGE7_LOCATION_NOT_CONNECTED            (0x80)
#define MPI2_MANPAGE7_SLOT_UNKNOWN                      (0xFFFF)
#ifndef MPI2_MANPAGE7_CONNECTOR_INFO_MAX
#define MPI2_MANPAGE7_CONNECTOR_INFO_MAX  (1)
#endif
typedef struct _MPI2_CONFIG_PAGE_MAN_7 {
	MPI2_CONFIG_PAGE_HEADER         Header;              
	U32                             Reserved1;           
	U32                             Reserved2;           
	U32                             Flags;               
	U8                              EnclosureName[16];   
	U8                              NumPhys;             
	U8                              Reserved3;           
	U16                             Reserved4;           
	MPI2_MANPAGE7_CONNECTOR_INFO
	ConnectorInfo[MPI2_MANPAGE7_CONNECTOR_INFO_MAX];  
} MPI2_CONFIG_PAGE_MAN_7,
	*PTR_MPI2_CONFIG_PAGE_MAN_7,
	Mpi2ManufacturingPage7_t,
	*pMpi2ManufacturingPage7_t;
#define MPI2_MANUFACTURING7_PAGEVERSION                 (0x01)
#define MPI2_MANPAGE7_FLAG_BASE_ENCLOSURE_LEVEL         (0x00000008)
#define MPI2_MANPAGE7_FLAG_EVENTREPLAY_SLOT_ORDER       (0x00000002)
#define MPI2_MANPAGE7_FLAG_USE_SLOT_INFO                (0x00000001)
#define MPI26_MANPAGE7_FLAG_CONN_LANE_USE_PINOUT        (0x00000020)
#define MPI26_MANPAGE7_FLAG_X2_X4_SLOT_INFO_VALID       (0x00000010)
typedef struct _MPI2_CONFIG_PAGE_MAN_PS {
	MPI2_CONFIG_PAGE_HEADER         Header;              
	U32                             ProductSpecificInfo; 
} MPI2_CONFIG_PAGE_MAN_PS,
	*PTR_MPI2_CONFIG_PAGE_MAN_PS,
	Mpi2ManufacturingPagePS_t,
	*pMpi2ManufacturingPagePS_t;
#define MPI2_MANUFACTURING8_PAGEVERSION                 (0x00)
#define MPI2_MANUFACTURING9_PAGEVERSION                 (0x00)
#define MPI2_MANUFACTURING10_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING11_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING12_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING13_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING14_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING15_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING16_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING17_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING18_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING19_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING20_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING21_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING22_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING23_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING24_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING25_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING26_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING27_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING28_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING29_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING30_PAGEVERSION                (0x00)
#define MPI2_MANUFACTURING31_PAGEVERSION                (0x00)
typedef struct _MPI2_CONFIG_PAGE_IO_UNIT_0 {
	MPI2_CONFIG_PAGE_HEADER Header;                      
	U64                     UniqueValue;                 
	MPI2_VERSION_UNION      NvdataVersionDefault;        
	MPI2_VERSION_UNION      NvdataVersionPersistent;     
} MPI2_CONFIG_PAGE_IO_UNIT_0,
	*PTR_MPI2_CONFIG_PAGE_IO_UNIT_0,
	Mpi2IOUnitPage0_t, *pMpi2IOUnitPage0_t;
#define MPI2_IOUNITPAGE0_PAGEVERSION                    (0x02)
typedef struct _MPI2_CONFIG_PAGE_IO_UNIT_1 {
	MPI2_CONFIG_PAGE_HEADER Header;                      
	U32                     Flags;                       
} MPI2_CONFIG_PAGE_IO_UNIT_1,
	*PTR_MPI2_CONFIG_PAGE_IO_UNIT_1,
	Mpi2IOUnitPage1_t, *pMpi2IOUnitPage1_t;
#define MPI2_IOUNITPAGE1_PAGEVERSION                    (0x04)
#define MPI26_IOUNITPAGE1_NVME_WRCACHE_MASK             (0x00030000)
#define MPI26_IOUNITPAGE1_NVME_WRCACHE_SHIFT            (16)
#define MPI26_IOUNITPAGE1_NVME_WRCACHE_NO_CHANGE        (0x00000000)
#define MPI26_IOUNITPAGE1_NVME_WRCACHE_ENABLE           (0x00010000)
#define MPI26_IOUNITPAGE1_NVME_WRCACHE_DISABLE          (0x00020000)
#define MPI2_IOUNITPAGE1_ATA_SECURITY_FREEZE_LOCK       (0x00004000)
#define MPI25_IOUNITPAGE1_NEW_DEVICE_FAST_PATH_DISABLE  (0x00002000)
#define MPI25_IOUNITPAGE1_DISABLE_FAST_PATH             (0x00001000)
#define MPI2_IOUNITPAGE1_ENABLE_HOST_BASED_DISCOVERY    (0x00000800)
#define MPI2_IOUNITPAGE1_MASK_SATA_WRITE_CACHE          (0x00000600)
#define MPI2_IOUNITPAGE1_SATA_WRITE_CACHE_SHIFT         (9)
#define MPI2_IOUNITPAGE1_ENABLE_SATA_WRITE_CACHE        (0x00000000)
#define MPI2_IOUNITPAGE1_DISABLE_SATA_WRITE_CACHE       (0x00000200)
#define MPI2_IOUNITPAGE1_UNCHANGED_SATA_WRITE_CACHE     (0x00000400)
#define MPI2_IOUNITPAGE1_NATIVE_COMMAND_Q_DISABLE       (0x00000100)
#define MPI2_IOUNITPAGE1_DISABLE_IR                     (0x00000040)
#define MPI2_IOUNITPAGE1_DISABLE_TASK_SET_FULL_HANDLING (0x00000020)
#define MPI2_IOUNITPAGE1_IR_USE_STATIC_VOLUME_ID        (0x00000004)
#ifndef MPI2_IO_UNIT_PAGE_3_GPIO_VAL_MAX
#define MPI2_IO_UNIT_PAGE_3_GPIO_VAL_MAX    (36)
#endif
typedef struct _MPI2_CONFIG_PAGE_IO_UNIT_3 {
	MPI2_CONFIG_PAGE_HEADER Header;			  
	U8                      GPIOCount;		  
	U8                      Reserved1;		  
	U16                     Reserved2;		  
	U16
		GPIOVal[MPI2_IO_UNIT_PAGE_3_GPIO_VAL_MAX]; 
} MPI2_CONFIG_PAGE_IO_UNIT_3,
	*PTR_MPI2_CONFIG_PAGE_IO_UNIT_3,
	Mpi2IOUnitPage3_t, *pMpi2IOUnitPage3_t;
#define MPI2_IOUNITPAGE3_PAGEVERSION                    (0x01)
#define MPI2_IOUNITPAGE3_GPIO_FUNCTION_MASK             (0xFFFC)
#define MPI2_IOUNITPAGE3_GPIO_FUNCTION_SHIFT            (2)
#define MPI2_IOUNITPAGE3_GPIO_SETTING_OFF               (0x0000)
#define MPI2_IOUNITPAGE3_GPIO_SETTING_ON                (0x0001)
#ifndef MPI2_IOUNITPAGE5_DMAENGINE_ENTRIES
#define MPI2_IOUNITPAGE5_DMAENGINE_ENTRIES      (1)
#endif
typedef struct _MPI2_CONFIG_PAGE_IO_UNIT_5 {
	MPI2_CONFIG_PAGE_HEADER Header;                      
	U64
		RaidAcceleratorBufferBaseAddress;            
	U64
		RaidAcceleratorBufferSize;                   
	U64
		RaidAcceleratorControlBaseAddress;           
	U8                      RAControlSize;               
	U8                      NumDmaEngines;               
	U8                      RAMinControlSize;            
	U8                      RAMaxControlSize;            
	U32                     Reserved1;                   
	U32                     Reserved2;                   
	U32                     Reserved3;                   
	U32
	DmaEngineCapabilities[MPI2_IOUNITPAGE5_DMAENGINE_ENTRIES];  
} MPI2_CONFIG_PAGE_IO_UNIT_5,
	*PTR_MPI2_CONFIG_PAGE_IO_UNIT_5,
	Mpi2IOUnitPage5_t, *pMpi2IOUnitPage5_t;
#define MPI2_IOUNITPAGE5_PAGEVERSION                    (0x00)
#define MPI2_IOUNITPAGE5_DMA_CAP_MASK_MAX_REQUESTS      (0xFFFF0000)
#define MPI2_IOUNITPAGE5_DMA_CAP_SHIFT_MAX_REQUESTS     (16)
#define MPI2_IOUNITPAGE5_DMA_CAP_EEDP                   (0x0008)
#define MPI2_IOUNITPAGE5_DMA_CAP_PARITY_GENERATION      (0x0004)
#define MPI2_IOUNITPAGE5_DMA_CAP_HASHING                (0x0002)
#define MPI2_IOUNITPAGE5_DMA_CAP_ENCRYPTION             (0x0001)
typedef struct _MPI2_CONFIG_PAGE_IO_UNIT_6 {
	MPI2_CONFIG_PAGE_HEADER Header;                  
	U16                     Flags;                   
	U8                      RAHostControlSize;       
	U8                      Reserved0;               
	U64
		RaidAcceleratorHostControlBaseAddress;   
	U32                     Reserved1;               
	U32                     Reserved2;               
	U32                     Reserved3;               
} MPI2_CONFIG_PAGE_IO_UNIT_6,
	*PTR_MPI2_CONFIG_PAGE_IO_UNIT_6,
	Mpi2IOUnitPage6_t, *pMpi2IOUnitPage6_t;
#define MPI2_IOUNITPAGE6_PAGEVERSION                    (0x00)
#define MPI2_IOUNITPAGE6_FLAGS_ENABLE_RAID_ACCELERATOR  (0x0001)
typedef struct _MPI2_CONFIG_PAGE_IO_UNIT_7 {
	MPI2_CONFIG_PAGE_HEADER Header;                  
	U8                      CurrentPowerMode;        
	U8                      PreviousPowerMode;       
	U8                      PCIeWidth;               
	U8                      PCIeSpeed;               
	U32                     ProcessorState;          
	U32
		PowerManagementCapabilities;             
	U16                     IOCTemperature;          
	U8
		IOCTemperatureUnits;                     
	U8                      IOCSpeed;                
	U16                     BoardTemperature;        
	U8
		BoardTemperatureUnits;                   
	U8                      Reserved3;               
	U32			BoardPowerRequirement;	 
	U32			PCISlotPowerAllocation;	 
	U8		Flags;			 
	U8		Reserved6;			 
	U16		Reserved7;			 
	U32		Reserved8;			 
} MPI2_CONFIG_PAGE_IO_UNIT_7,
	*PTR_MPI2_CONFIG_PAGE_IO_UNIT_7,
	Mpi2IOUnitPage7_t, *pMpi2IOUnitPage7_t;
#define MPI2_IOUNITPAGE7_PAGEVERSION			(0x05)
#define MPI25_IOUNITPAGE7_PM_INIT_MASK              (0xC0)
#define MPI25_IOUNITPAGE7_PM_INIT_UNAVAILABLE       (0x00)
#define MPI25_IOUNITPAGE7_PM_INIT_HOST              (0x40)
#define MPI25_IOUNITPAGE7_PM_INIT_IO_UNIT           (0x80)
#define MPI25_IOUNITPAGE7_PM_INIT_PCIE_DPA          (0xC0)
#define MPI25_IOUNITPAGE7_PM_MODE_MASK              (0x07)
#define MPI25_IOUNITPAGE7_PM_MODE_UNAVAILABLE       (0x00)
#define MPI25_IOUNITPAGE7_PM_MODE_UNKNOWN           (0x01)
#define MPI25_IOUNITPAGE7_PM_MODE_FULL_POWER        (0x04)
#define MPI25_IOUNITPAGE7_PM_MODE_REDUCED_POWER     (0x05)
#define MPI25_IOUNITPAGE7_PM_MODE_STANDBY           (0x06)
#define MPI2_IOUNITPAGE7_PCIE_WIDTH_X1              (0x01)
#define MPI2_IOUNITPAGE7_PCIE_WIDTH_X2              (0x02)
#define MPI2_IOUNITPAGE7_PCIE_WIDTH_X4              (0x04)
#define MPI2_IOUNITPAGE7_PCIE_WIDTH_X8              (0x08)
#define MPI2_IOUNITPAGE7_PCIE_WIDTH_X16             (0x10)
#define MPI2_IOUNITPAGE7_PCIE_SPEED_2_5_GBPS        (0x00)
#define MPI2_IOUNITPAGE7_PCIE_SPEED_5_0_GBPS        (0x01)
#define MPI2_IOUNITPAGE7_PCIE_SPEED_8_0_GBPS        (0x02)
#define MPI2_IOUNITPAGE7_PCIE_SPEED_16_0_GBPS       (0x03)
#define MPI2_IOUNITPAGE7_PSTATE_MASK_SECOND         (0x0000000F)
#define MPI2_IOUNITPAGE7_PSTATE_SHIFT_SECOND        (0)
#define MPI2_IOUNITPAGE7_PSTATE_NOT_PRESENT         (0x00)
#define MPI2_IOUNITPAGE7_PSTATE_DISABLED            (0x01)
#define MPI2_IOUNITPAGE7_PSTATE_ENABLED             (0x02)
#define MPI25_IOUNITPAGE7_PMCAP_DPA_FULL_PWR_MODE       (0x00400000)
#define MPI25_IOUNITPAGE7_PMCAP_DPA_REDUCED_PWR_MODE    (0x00200000)
#define MPI25_IOUNITPAGE7_PMCAP_DPA_STANDBY_MODE        (0x00100000)
#define MPI25_IOUNITPAGE7_PMCAP_HOST_FULL_PWR_MODE      (0x00040000)
#define MPI25_IOUNITPAGE7_PMCAP_HOST_REDUCED_PWR_MODE   (0x00020000)
#define MPI25_IOUNITPAGE7_PMCAP_HOST_STANDBY_MODE       (0x00010000)
#define MPI25_IOUNITPAGE7_PMCAP_IO_FULL_PWR_MODE        (0x00004000)
#define MPI25_IOUNITPAGE7_PMCAP_IO_REDUCED_PWR_MODE     (0x00002000)
#define MPI25_IOUNITPAGE7_PMCAP_IO_STANDBY_MODE         (0x00001000)
#define MPI2_IOUNITPAGE7_PMCAP_HOST_12_5_PCT_IOCSPEED   (0x00000400)
#define MPI2_IOUNITPAGE7_PMCAP_HOST_25_0_PCT_IOCSPEED   (0x00000200)
#define MPI2_IOUNITPAGE7_PMCAP_HOST_50_0_PCT_IOCSPEED   (0x00000100)
#define MPI25_IOUNITPAGE7_PMCAP_IO_12_5_PCT_IOCSPEED    (0x00000040)
#define MPI25_IOUNITPAGE7_PMCAP_IO_25_0_PCT_IOCSPEED    (0x00000020)
#define MPI25_IOUNITPAGE7_PMCAP_IO_50_0_PCT_IOCSPEED    (0x00000010)
#define MPI2_IOUNITPAGE7_PMCAP_HOST_WIDTH_CHANGE_PCIE   (0x00000008)
#define MPI2_IOUNITPAGE7_PMCAP_HOST_SPEED_CHANGE_PCIE   (0x00000004)
#define MPI25_IOUNITPAGE7_PMCAP_IO_WIDTH_CHANGE_PCIE    (0x00000002)
#define MPI25_IOUNITPAGE7_PMCAP_IO_SPEED_CHANGE_PCIE    (0x00000001)
#define MPI2_IOUNITPAGE7_PMCAP_12_5_PCT_IOCSPEED    (0x00000400)
#define MPI2_IOUNITPAGE7_PMCAP_25_0_PCT_IOCSPEED    (0x00000200)
#define MPI2_IOUNITPAGE7_PMCAP_50_0_PCT_IOCSPEED    (0x00000100)
#define MPI2_IOUNITPAGE7_PMCAP_PCIE_WIDTH_CHANGE    (0x00000008)  
#define MPI2_IOUNITPAGE7_PMCAP_PCIE_SPEED_CHANGE    (0x00000004)  
#define MPI2_IOUNITPAGE7_IOC_TEMP_NOT_PRESENT       (0x00)
#define MPI2_IOUNITPAGE7_IOC_TEMP_FAHRENHEIT        (0x01)
#define MPI2_IOUNITPAGE7_IOC_TEMP_CELSIUS           (0x02)
#define MPI2_IOUNITPAGE7_IOC_SPEED_FULL             (0x01)
#define MPI2_IOUNITPAGE7_IOC_SPEED_HALF             (0x02)
#define MPI2_IOUNITPAGE7_IOC_SPEED_QUARTER          (0x04)
#define MPI2_IOUNITPAGE7_IOC_SPEED_EIGHTH           (0x08)
#define MPI2_IOUNITPAGE7_BOARD_TEMP_NOT_PRESENT     (0x00)
#define MPI2_IOUNITPAGE7_BOARD_TEMP_FAHRENHEIT      (0x01)
#define MPI2_IOUNITPAGE7_BOARD_TEMP_CELSIUS         (0x02)
#define MPI2_IOUNITPAGE7_FLAG_CABLE_POWER_EXC       (0x01)
#define MPI2_IOUNIT8_NUM_THRESHOLDS     (4)
typedef struct _MPI2_IOUNIT8_SENSOR {
	U16                     Flags;                   
	U16                     Reserved1;               
	U16
		Threshold[MPI2_IOUNIT8_NUM_THRESHOLDS];  
	U32                     Reserved2;               
	U32                     Reserved3;               
	U32                     Reserved4;               
} MPI2_IOUNIT8_SENSOR, *PTR_MPI2_IOUNIT8_SENSOR,
	Mpi2IOUnit8Sensor_t, *pMpi2IOUnit8Sensor_t;
#define MPI2_IOUNIT8_SENSOR_FLAGS_T3_ENABLE         (0x0008)
#define MPI2_IOUNIT8_SENSOR_FLAGS_T2_ENABLE         (0x0004)
#define MPI2_IOUNIT8_SENSOR_FLAGS_T1_ENABLE         (0x0002)
#define MPI2_IOUNIT8_SENSOR_FLAGS_T0_ENABLE         (0x0001)
#ifndef MPI2_IOUNITPAGE8_SENSOR_ENTRIES
#define MPI2_IOUNITPAGE8_SENSOR_ENTRIES     (1)
#endif
typedef struct _MPI2_CONFIG_PAGE_IO_UNIT_8 {
	MPI2_CONFIG_PAGE_HEADER Header;                  
	U32                     Reserved1;               
	U32                     Reserved2;               
	U8                      NumSensors;              
	U8                      PollingInterval;         
	U16                     Reserved3;               
	MPI2_IOUNIT8_SENSOR
		Sensor[MPI2_IOUNITPAGE8_SENSOR_ENTRIES]; 
} MPI2_CONFIG_PAGE_IO_UNIT_8,
	*PTR_MPI2_CONFIG_PAGE_IO_UNIT_8,
	Mpi2IOUnitPage8_t, *pMpi2IOUnitPage8_t;
#define MPI2_IOUNITPAGE8_PAGEVERSION                    (0x00)
typedef struct _MPI2_IOUNIT9_SENSOR {
	U16                     CurrentTemperature;      
	U16                     Reserved1;               
	U8                      Flags;                   
	U8                      Reserved2;               
	U16                     Reserved3;               
	U32                     Reserved4;               
	U32                     Reserved5;               
} MPI2_IOUNIT9_SENSOR, *PTR_MPI2_IOUNIT9_SENSOR,
	Mpi2IOUnit9Sensor_t, *pMpi2IOUnit9Sensor_t;
#define MPI2_IOUNIT9_SENSOR_FLAGS_TEMP_VALID        (0x01)
#ifndef MPI2_IOUNITPAGE9_SENSOR_ENTRIES
#define MPI2_IOUNITPAGE9_SENSOR_ENTRIES     (1)
#endif
typedef struct _MPI2_CONFIG_PAGE_IO_UNIT_9 {
	MPI2_CONFIG_PAGE_HEADER Header;                  
	U32                     Reserved1;               
	U32                     Reserved2;               
	U8                      NumSensors;              
	U8                      Reserved4;               
	U16                     Reserved3;               
	MPI2_IOUNIT9_SENSOR
		Sensor[MPI2_IOUNITPAGE9_SENSOR_ENTRIES]; 
} MPI2_CONFIG_PAGE_IO_UNIT_9,
	*PTR_MPI2_CONFIG_PAGE_IO_UNIT_9,
	Mpi2IOUnitPage9_t, *pMpi2IOUnitPage9_t;
#define MPI2_IOUNITPAGE9_PAGEVERSION                    (0x00)
typedef struct _MPI2_IOUNIT10_FUNCTION {
	U8                      CreditPercent;       
	U8                      Reserved1;           
	U16                     Reserved2;           
} MPI2_IOUNIT10_FUNCTION,
	*PTR_MPI2_IOUNIT10_FUNCTION,
	Mpi2IOUnit10Function_t,
	*pMpi2IOUnit10Function_t;
#ifndef MPI2_IOUNITPAGE10_FUNCTION_ENTRIES
#define MPI2_IOUNITPAGE10_FUNCTION_ENTRIES      (1)
#endif
typedef struct _MPI2_CONFIG_PAGE_IO_UNIT_10 {
	MPI2_CONFIG_PAGE_HEADER Header;                       
	U8                      NumFunctions;                 
	U8                      Reserved1;                    
	U16                     Reserved2;                    
	U32                     Reserved3;                    
	U32                     Reserved4;                    
	MPI2_IOUNIT10_FUNCTION
		Function[MPI2_IOUNITPAGE10_FUNCTION_ENTRIES]; 
} MPI2_CONFIG_PAGE_IO_UNIT_10,
	*PTR_MPI2_CONFIG_PAGE_IO_UNIT_10,
	Mpi2IOUnitPage10_t, *pMpi2IOUnitPage10_t;
#define MPI2_IOUNITPAGE10_PAGEVERSION                   (0x01)
typedef struct _MPI26_IOUNIT11_SPINUP_GROUP {
	U8          MaxTargetSpinup;             
	U8          SpinupDelay;                 
	U8          SpinupFlags;                 
	U8          Reserved1;                   
} MPI26_IOUNIT11_SPINUP_GROUP,
	*PTR_MPI26_IOUNIT11_SPINUP_GROUP,
	Mpi26IOUnit11SpinupGroup_t,
	*pMpi26IOUnit11SpinupGroup_t;
#define MPI26_IOUNITPAGE11_SPINUP_DISABLE_FLAG          (0x01)
#ifndef MPI26_IOUNITPAGE11_PHY_MAX
#define MPI26_IOUNITPAGE11_PHY_MAX        (4)
#endif
typedef struct _MPI26_CONFIG_PAGE_IO_UNIT_11 {
	MPI2_CONFIG_PAGE_HEADER       Header;			        
	U32                           Reserved1;                       
	MPI26_IOUNIT11_SPINUP_GROUP   SpinupGroupParameters[4];        
	U32                           Reserved2;                       
	U32                           Reserved3;                       
	U32                           Reserved4;                       
	U8                            BootDeviceWaitTime;              
	U8                            Reserved5;                       
	U16                           Reserved6;                       
	U8                            NumPhys;                         
	U8                            PEInitialSpinupDelay;            
	U8                            PEReplyDelay;                    
	U8                            Flags;                           
	U8			      PHY[MPI26_IOUNITPAGE11_PHY_MAX]; 
} MPI26_CONFIG_PAGE_IO_UNIT_11,
	*PTR_MPI26_CONFIG_PAGE_IO_UNIT_11,
	Mpi26IOUnitPage11_t,
	*pMpi26IOUnitPage11_t;
#define MPI26_IOUNITPAGE11_PAGEVERSION                  (0x00)
#define MPI26_IOUNITPAGE11_FLAGS_AUTO_PORTENABLE        (0x01)
#define MPI26_IOUNITPAGE11_PHY_SPINUP_GROUP_MASK        (0x03)
typedef struct _MPI2_CONFIG_PAGE_IOC_0 {
	MPI2_CONFIG_PAGE_HEADER Header;                      
	U32                     Reserved1;                   
	U32                     Reserved2;                   
	U16                     VendorID;                    
	U16                     DeviceID;                    
	U8                      RevisionID;                  
	U8                      Reserved3;                   
	U16                     Reserved4;                   
	U32                     ClassCode;                   
	U16                     SubsystemVendorID;           
	U16                     SubsystemID;                 
} MPI2_CONFIG_PAGE_IOC_0,
	*PTR_MPI2_CONFIG_PAGE_IOC_0,
	Mpi2IOCPage0_t, *pMpi2IOCPage0_t;
#define MPI2_IOCPAGE0_PAGEVERSION                       (0x02)
typedef struct _MPI2_CONFIG_PAGE_IOC_1 {
	MPI2_CONFIG_PAGE_HEADER Header;                      
	U32                     Flags;                       
	U32                     CoalescingTimeout;           
	U8                      CoalescingDepth;             
	U8                      PCISlotNum;                  
	U8                      PCIBusNum;                   
	U8                      PCIDomainSegment;            
	U32                     Reserved1;                   
	U32                     ProductSpecific;             
} MPI2_CONFIG_PAGE_IOC_1,
	*PTR_MPI2_CONFIG_PAGE_IOC_1,
	Mpi2IOCPage1_t, *pMpi2IOCPage1_t;
#define MPI2_IOCPAGE1_PAGEVERSION                       (0x05)
#define MPI2_IOCPAGE1_REPLY_COALESCING                  (0x00000001)
#define MPI2_IOCPAGE1_PCISLOTNUM_UNKNOWN                (0xFF)
#define MPI2_IOCPAGE1_PCIBUSNUM_UNKNOWN                 (0xFF)
#define MPI2_IOCPAGE1_PCIDOMAIN_UNKNOWN                 (0xFF)
typedef struct _MPI2_CONFIG_PAGE_IOC_6 {
	MPI2_CONFIG_PAGE_HEADER Header;          
	U32
		CapabilitiesFlags;               
	U8                      MaxDrivesRAID0;  
	U8                      MaxDrivesRAID1;  
	U8
		 MaxDrivesRAID1E;                 
	U8
		 MaxDrivesRAID10;		 
	U8                      MinDrivesRAID0;  
	U8                      MinDrivesRAID1;  
	U8
		 MinDrivesRAID1E;                 
	U8
		 MinDrivesRAID10;                 
	U32                     Reserved1;       
	U8
		 MaxGlobalHotSpares;              
	U8                      MaxPhysDisks;    
	U8                      MaxVolumes;      
	U8                      MaxConfigs;      
	U8                      MaxOCEDisks;     
	U8                      Reserved2;       
	U16                     Reserved3;       
	U32
		SupportedStripeSizeMapRAID0;     
	U32
		SupportedStripeSizeMapRAID1E;    
	U32
		SupportedStripeSizeMapRAID10;    
	U32                     Reserved4;       
	U32                     Reserved5;       
	U16
		DefaultMetadataSize;             
	U16                     Reserved6;       
	U16
		MaxBadBlockTableEntries;         
	U16                     Reserved7;       
	U32
		IRNvsramVersion;                 
} MPI2_CONFIG_PAGE_IOC_6,
	*PTR_MPI2_CONFIG_PAGE_IOC_6,
	Mpi2IOCPage6_t, *pMpi2IOCPage6_t;
#define MPI2_IOCPAGE6_PAGEVERSION                       (0x05)
#define MPI2_IOCPAGE6_CAP_FLAGS_4K_SECTORS_SUPPORT      (0x00000020)
#define MPI2_IOCPAGE6_CAP_FLAGS_RAID10_SUPPORT          (0x00000010)
#define MPI2_IOCPAGE6_CAP_FLAGS_RAID1_SUPPORT           (0x00000008)
#define MPI2_IOCPAGE6_CAP_FLAGS_RAID1E_SUPPORT          (0x00000004)
#define MPI2_IOCPAGE6_CAP_FLAGS_RAID0_SUPPORT           (0x00000002)
#define MPI2_IOCPAGE6_CAP_FLAGS_GLOBAL_HOT_SPARE        (0x00000001)
#define MPI2_IOCPAGE7_EVENTMASK_WORDS       (4)
typedef struct _MPI2_CONFIG_PAGE_IOC_7 {
	MPI2_CONFIG_PAGE_HEADER Header;                      
	U32                     Reserved1;                   
	U32
		EventMasks[MPI2_IOCPAGE7_EVENTMASK_WORDS]; 
	U16                     SASBroadcastPrimitiveMasks;  
	U16                     SASNotifyPrimitiveMasks;     
	U32                     Reserved3;                   
} MPI2_CONFIG_PAGE_IOC_7,
	*PTR_MPI2_CONFIG_PAGE_IOC_7,
	Mpi2IOCPage7_t, *pMpi2IOCPage7_t;
#define MPI2_IOCPAGE7_PAGEVERSION                       (0x02)
typedef struct _MPI2_CONFIG_PAGE_IOC_8 {
	MPI2_CONFIG_PAGE_HEADER Header;                      
	U8                      NumDevsPerEnclosure;         
	U8                      Reserved1;                   
	U16                     Reserved2;                   
	U16                     MaxPersistentEntries;        
	U16                     MaxNumPhysicalMappedIDs;     
	U16                     Flags;                       
	U16                     Reserved3;                   
	U16                     IRVolumeMappingFlags;        
	U16                     Reserved4;                   
	U32                     Reserved5;                   
} MPI2_CONFIG_PAGE_IOC_8,
	*PTR_MPI2_CONFIG_PAGE_IOC_8,
	Mpi2IOCPage8_t, *pMpi2IOCPage8_t;
#define MPI2_IOCPAGE8_PAGEVERSION                       (0x00)
#define MPI2_IOCPAGE8_FLAGS_DA_START_SLOT_1             (0x00000020)
#define MPI2_IOCPAGE8_FLAGS_RESERVED_TARGETID_0         (0x00000010)
#define MPI2_IOCPAGE8_FLAGS_MASK_MAPPING_MODE           (0x0000000E)
#define MPI2_IOCPAGE8_FLAGS_DEVICE_PERSISTENCE_MAPPING  (0x00000000)
#define MPI2_IOCPAGE8_FLAGS_ENCLOSURE_SLOT_MAPPING      (0x00000002)
#define MPI2_IOCPAGE8_FLAGS_DISABLE_PERSISTENT_MAPPING  (0x00000001)
#define MPI2_IOCPAGE8_FLAGS_ENABLE_PERSISTENT_MAPPING   (0x00000000)
#define MPI2_IOCPAGE8_IRFLAGS_MASK_VOLUME_MAPPING_MODE  (0x00000003)
#define MPI2_IOCPAGE8_IRFLAGS_LOW_VOLUME_MAPPING        (0x00000000)
#define MPI2_IOCPAGE8_IRFLAGS_HIGH_VOLUME_MAPPING       (0x00000001)
typedef struct _MPI2_CONFIG_PAGE_BIOS_1 {
	MPI2_CONFIG_PAGE_HEADER Header;                      
	U32                     BiosOptions;                 
	U32                     IOCSettings;                 
	U8                      SSUTimeout;                  
	U8                      MaxEnclosureLevel;           
	U16                     Reserved2;                   
	U32                     DeviceSettings;              
	U16                     NumberOfDevices;             
	U16                     UEFIVersion;                 
	U16                     IOTimeoutBlockDevicesNonRM;  
	U16                     IOTimeoutSequential;         
	U16                     IOTimeoutOther;              
	U16                     IOTimeoutBlockDevicesRM;     
} MPI2_CONFIG_PAGE_BIOS_1,
	*PTR_MPI2_CONFIG_PAGE_BIOS_1,
	Mpi2BiosPage1_t, *pMpi2BiosPage1_t;
#define MPI2_BIOSPAGE1_PAGEVERSION                      (0x07)
#define MPI2_BIOSPAGE1_OPTIONS_BOOT_LIST_ADD_ALT_BOOT_DEVICE    (0x00008000)
#define MPI2_BIOSPAGE1_OPTIONS_ADVANCED_CONFIG                  (0x00004000)
#define MPI2_BIOSPAGE1_OPTIONS_PNS_MASK                         (0x00003800)
#define MPI2_BIOSPAGE1_OPTIONS_PNS_PBDHL                        (0x00000000)
#define MPI2_BIOSPAGE1_OPTIONS_PNS_ENCSLOSURE                   (0x00000800)
#define MPI2_BIOSPAGE1_OPTIONS_PNS_LWWID                        (0x00001000)
#define MPI2_BIOSPAGE1_OPTIONS_PNS_PSENS                        (0x00001800)
#define MPI2_BIOSPAGE1_OPTIONS_PNS_ESPHY                        (0x00002000)
#define MPI2_BIOSPAGE1_OPTIONS_X86_DISABLE_BIOS		(0x00000400)
#define MPI2_BIOSPAGE1_OPTIONS_MASK_REGISTRATION_UEFI_BSD	(0x00000300)
#define MPI2_BIOSPAGE1_OPTIONS_USE_BIT0_REGISTRATION_UEFI_BSD	(0x00000000)
#define MPI2_BIOSPAGE1_OPTIONS_FULL_REGISTRATION_UEFI_BSD	(0x00000100)
#define MPI2_BIOSPAGE1_OPTIONS_ADAPTER_REGISTRATION_UEFI_BSD	(0x00000200)
#define MPI2_BIOSPAGE1_OPTIONS_DISABLE_REGISTRATION_UEFI_BSD	(0x00000300)
#define MPI2_BIOSPAGE1_OPTIONS_MASK_OEM_ID                  (0x000000F0)
#define MPI2_BIOSPAGE1_OPTIONS_LSI_OEM_ID                   (0x00000000)
#define MPI2_BIOSPAGE1_OPTIONS_MASK_UEFI_HII_REGISTRATION   (0x00000006)
#define MPI2_BIOSPAGE1_OPTIONS_ENABLE_UEFI_HII              (0x00000000)
#define MPI2_BIOSPAGE1_OPTIONS_DISABLE_UEFI_HII             (0x00000002)
#define MPI2_BIOSPAGE1_OPTIONS_VERSION_CHECK_UEFI_HII       (0x00000004)
#define MPI2_BIOSPAGE1_OPTIONS_DISABLE_BIOS                 (0x00000001)
#define MPI2_BIOSPAGE1_IOCSET_MASK_BOOT_PREFERENCE      (0x00030000)
#define MPI2_BIOSPAGE1_IOCSET_ENCLOSURE_SLOT_BOOT       (0x00000000)
#define MPI2_BIOSPAGE1_IOCSET_SAS_ADDRESS_BOOT          (0x00010000)
#define MPI2_BIOSPAGE1_IOCSET_MASK_RM_SETTING           (0x000000C0)
#define MPI2_BIOSPAGE1_IOCSET_NONE_RM_SETTING           (0x00000000)
#define MPI2_BIOSPAGE1_IOCSET_BOOT_RM_SETTING           (0x00000040)
#define MPI2_BIOSPAGE1_IOCSET_MEDIA_RM_SETTING          (0x00000080)
#define MPI2_BIOSPAGE1_IOCSET_MASK_ADAPTER_SUPPORT      (0x00000030)
#define MPI2_BIOSPAGE1_IOCSET_NO_SUPPORT                (0x00000000)
#define MPI2_BIOSPAGE1_IOCSET_BIOS_SUPPORT              (0x00000010)
#define MPI2_BIOSPAGE1_IOCSET_OS_SUPPORT                (0x00000020)
#define MPI2_BIOSPAGE1_IOCSET_ALL_SUPPORT               (0x00000030)
#define MPI2_BIOSPAGE1_IOCSET_ALTERNATE_CHS             (0x00000008)
#define MPI2_BIOSPAGE1_DEVSET_DISABLE_SMART_POLLING     (0x00000010)
#define MPI2_BIOSPAGE1_DEVSET_DISABLE_SEQ_LUN           (0x00000008)
#define MPI2_BIOSPAGE1_DEVSET_DISABLE_RM_LUN            (0x00000004)
#define MPI2_BIOSPAGE1_DEVSET_DISABLE_NON_RM_LUN        (0x00000002)
#define MPI2_BIOSPAGE1_DEVSET_DISABLE_OTHER_LUN         (0x00000001)
#define MPI2_BIOSPAGE1_UEFI_VER_MAJOR_MASK              (0xFF00)
#define MPI2_BIOSPAGE1_UEFI_VER_MAJOR_SHIFT             (8)
#define MPI2_BIOSPAGE1_UEFI_VER_MINOR_MASK              (0x00FF)
#define MPI2_BIOSPAGE1_UEFI_VER_MINOR_SHIFT             (0)
typedef struct _MPI2_BOOT_DEVICE_ADAPTER_ORDER {
	U32         Reserved1;                               
	U32         Reserved2;                               
	U32         Reserved3;                               
	U32         Reserved4;                               
	U32         Reserved5;                               
	U32         Reserved6;                               
} MPI2_BOOT_DEVICE_ADAPTER_ORDER,
	*PTR_MPI2_BOOT_DEVICE_ADAPTER_ORDER,
	Mpi2BootDeviceAdapterOrder_t,
	*pMpi2BootDeviceAdapterOrder_t;
typedef struct _MPI2_BOOT_DEVICE_SAS_WWID {
	U64         SASAddress;                              
	U8          LUN[8];                                  
	U32         Reserved1;                               
	U32         Reserved2;                               
} MPI2_BOOT_DEVICE_SAS_WWID,
	*PTR_MPI2_BOOT_DEVICE_SAS_WWID,
	Mpi2BootDeviceSasWwid_t,
	*pMpi2BootDeviceSasWwid_t;
typedef struct _MPI2_BOOT_DEVICE_ENCLOSURE_SLOT {
	U64         EnclosureLogicalID;                      
	U32         Reserved1;                               
	U32         Reserved2;                               
	U16         SlotNumber;                              
	U16         Reserved3;                               
	U32         Reserved4;                               
} MPI2_BOOT_DEVICE_ENCLOSURE_SLOT,
	*PTR_MPI2_BOOT_DEVICE_ENCLOSURE_SLOT,
	Mpi2BootDeviceEnclosureSlot_t,
	*pMpi2BootDeviceEnclosureSlot_t;
typedef struct _MPI2_BOOT_DEVICE_DEVICE_NAME {
	U64         DeviceName;                              
	U8          LUN[8];                                  
	U32         Reserved1;                               
	U32         Reserved2;                               
} MPI2_BOOT_DEVICE_DEVICE_NAME,
	*PTR_MPI2_BOOT_DEVICE_DEVICE_NAME,
	Mpi2BootDeviceDeviceName_t,
	*pMpi2BootDeviceDeviceName_t;
typedef union _MPI2_MPI2_BIOSPAGE2_BOOT_DEVICE {
	MPI2_BOOT_DEVICE_ADAPTER_ORDER  AdapterOrder;
	MPI2_BOOT_DEVICE_SAS_WWID       SasWwid;
	MPI2_BOOT_DEVICE_ENCLOSURE_SLOT EnclosureSlot;
	MPI2_BOOT_DEVICE_DEVICE_NAME    DeviceName;
} MPI2_BIOSPAGE2_BOOT_DEVICE,
	*PTR_MPI2_BIOSPAGE2_BOOT_DEVICE,
	Mpi2BiosPage2BootDevice_t,
	*pMpi2BiosPage2BootDevice_t;
typedef struct _MPI2_CONFIG_PAGE_BIOS_2 {
	MPI2_CONFIG_PAGE_HEADER     Header;                  
	U32                         Reserved1;               
	U32                         Reserved2;               
	U32                         Reserved3;               
	U32                         Reserved4;               
	U32                         Reserved5;               
	U32                         Reserved6;               
	U8                          ReqBootDeviceForm;       
	U8                          Reserved7;               
	U16                         Reserved8;               
	MPI2_BIOSPAGE2_BOOT_DEVICE  RequestedBootDevice;     
	U8                          ReqAltBootDeviceForm;    
	U8                          Reserved9;               
	U16                         Reserved10;              
	MPI2_BIOSPAGE2_BOOT_DEVICE  RequestedAltBootDevice;  
	U8                          CurrentBootDeviceForm;   
	U8                          Reserved11;              
	U16                         Reserved12;              
	MPI2_BIOSPAGE2_BOOT_DEVICE  CurrentBootDevice;       
} MPI2_CONFIG_PAGE_BIOS_2, *PTR_MPI2_CONFIG_PAGE_BIOS_2,
	Mpi2BiosPage2_t, *pMpi2BiosPage2_t;
#define MPI2_BIOSPAGE2_PAGEVERSION                      (0x04)
#define MPI2_BIOSPAGE2_FORM_MASK                        (0x0F)
#define MPI2_BIOSPAGE2_FORM_NO_DEVICE_SPECIFIED         (0x00)
#define MPI2_BIOSPAGE2_FORM_SAS_WWID                    (0x05)
#define MPI2_BIOSPAGE2_FORM_ENCLOSURE_SLOT              (0x06)
#define MPI2_BIOSPAGE2_FORM_DEVICE_NAME                 (0x07)
#define MPI2_BIOSPAGE3_NUM_ADAPTER      (4)
typedef struct _MPI2_ADAPTER_INFO {
	U8      PciBusNumber;                         
	U8      PciDeviceAndFunctionNumber;           
	U16     AdapterFlags;                         
} MPI2_ADAPTER_INFO, *PTR_MPI2_ADAPTER_INFO,
	Mpi2AdapterInfo_t, *pMpi2AdapterInfo_t;
#define MPI2_ADAPTER_INFO_FLAGS_EMBEDDED                (0x0001)
#define MPI2_ADAPTER_INFO_FLAGS_INIT_STATUS             (0x0002)
typedef struct _MPI2_ADAPTER_ORDER_AUX {
	U64     WWID;					 
	U32     Reserved1;				 
	U32     Reserved2;				 
} MPI2_ADAPTER_ORDER_AUX, *PTR_MPI2_ADAPTER_ORDER_AUX,
	Mpi2AdapterOrderAux_t, *pMpi2AdapterOrderAux_t;
typedef struct _MPI2_CONFIG_PAGE_BIOS_3 {
	MPI2_CONFIG_PAGE_HEADER Header;               
	U32                     GlobalFlags;          
	U32                     BiosVersion;          
	MPI2_ADAPTER_INFO       AdapterOrder[MPI2_BIOSPAGE3_NUM_ADAPTER];
	U32                     Reserved1;            
	MPI2_ADAPTER_ORDER_AUX  AdapterOrderAux[MPI2_BIOSPAGE3_NUM_ADAPTER];
} MPI2_CONFIG_PAGE_BIOS_3,
	*PTR_MPI2_CONFIG_PAGE_BIOS_3,
	Mpi2BiosPage3_t, *pMpi2BiosPage3_t;
#define MPI2_BIOSPAGE3_PAGEVERSION                      (0x01)
#define MPI2_BIOSPAGE3_FLAGS_PAUSE_ON_ERROR             (0x00000002)
#define MPI2_BIOSPAGE3_FLAGS_VERBOSE_ENABLE             (0x00000004)
#define MPI2_BIOSPAGE3_FLAGS_HOOK_INT_40_DISABLE        (0x00000010)
#define MPI2_BIOSPAGE3_FLAGS_DEV_LIST_DISPLAY_MASK      (0x000000E0)
#define MPI2_BIOSPAGE3_FLAGS_INSTALLED_DEV_DISPLAY      (0x00000000)
#define MPI2_BIOSPAGE3_FLAGS_ADAPTER_DISPLAY            (0x00000020)
#define MPI2_BIOSPAGE3_FLAGS_ADAPTER_DEV_DISPLAY        (0x00000040)
#ifndef MPI2_BIOS_PAGE_4_PHY_ENTRIES
#define MPI2_BIOS_PAGE_4_PHY_ENTRIES        (1)
#endif
typedef struct _MPI2_BIOS4_ENTRY {
	U64                     ReassignmentWWID;        
	U64                     ReassignmentDeviceName;  
} MPI2_BIOS4_ENTRY, *PTR_MPI2_BIOS4_ENTRY,
	Mpi2MBios4Entry_t, *pMpi2Bios4Entry_t;
typedef struct _MPI2_CONFIG_PAGE_BIOS_4 {
	MPI2_CONFIG_PAGE_HEADER Header;              
	U8                      NumPhys;             
	U8                      Reserved1;           
	U16                     Reserved2;           
	MPI2_BIOS4_ENTRY
		Phy[MPI2_BIOS_PAGE_4_PHY_ENTRIES];   
} MPI2_CONFIG_PAGE_BIOS_4, *PTR_MPI2_CONFIG_PAGE_BIOS_4,
	Mpi2BiosPage4_t, *pMpi2BiosPage4_t;
#define MPI2_BIOSPAGE4_PAGEVERSION                      (0x01)
typedef struct _MPI2_RAIDVOL0_PHYS_DISK {
	U8                      RAIDSetNum;         
	U8                      PhysDiskMap;        
	U8                      PhysDiskNum;        
	U8                      Reserved;           
} MPI2_RAIDVOL0_PHYS_DISK, *PTR_MPI2_RAIDVOL0_PHYS_DISK,
	Mpi2RaidVol0PhysDisk_t, *pMpi2RaidVol0PhysDisk_t;
#define MPI2_RAIDVOL0_PHYSDISK_PRIMARY                  (0x01)
#define MPI2_RAIDVOL0_PHYSDISK_SECONDARY                (0x02)
typedef struct _MPI2_RAIDVOL0_SETTINGS {
	U16                     Settings;           
	U8                      HotSparePool;       
	U8                      Reserved;           
} MPI2_RAIDVOL0_SETTINGS, *PTR_MPI2_RAIDVOL0_SETTINGS,
	Mpi2RaidVol0Settings_t,
	*pMpi2RaidVol0Settings_t;
#define MPI2_RAID_HOT_SPARE_POOL_0                      (0x01)
#define MPI2_RAID_HOT_SPARE_POOL_1                      (0x02)
#define MPI2_RAID_HOT_SPARE_POOL_2                      (0x04)
#define MPI2_RAID_HOT_SPARE_POOL_3                      (0x08)
#define MPI2_RAID_HOT_SPARE_POOL_4                      (0x10)
#define MPI2_RAID_HOT_SPARE_POOL_5                      (0x20)
#define MPI2_RAID_HOT_SPARE_POOL_6                      (0x40)
#define MPI2_RAID_HOT_SPARE_POOL_7                      (0x80)
#define MPI2_RAIDVOL0_SETTING_USE_PRODUCT_ID_SUFFIX     (0x0008)
#define MPI2_RAIDVOL0_SETTING_AUTO_CONFIG_HSWAP_DISABLE (0x0004)
#define MPI2_RAIDVOL0_SETTING_MASK_WRITE_CACHING        (0x0003)
#define MPI2_RAIDVOL0_SETTING_UNCHANGED                 (0x0000)
#define MPI2_RAIDVOL0_SETTING_DISABLE_WRITE_CACHING     (0x0001)
#define MPI2_RAIDVOL0_SETTING_ENABLE_WRITE_CACHING      (0x0002)
#ifndef MPI2_RAID_VOL_PAGE_0_PHYSDISK_MAX
#define MPI2_RAID_VOL_PAGE_0_PHYSDISK_MAX       (1)
#endif
typedef struct _MPI2_CONFIG_PAGE_RAID_VOL_0 {
	MPI2_CONFIG_PAGE_HEADER Header;             
	U16                     DevHandle;          
	U8                      VolumeState;        
	U8                      VolumeType;         
	U32                     VolumeStatusFlags;  
	MPI2_RAIDVOL0_SETTINGS  VolumeSettings;     
	U64                     MaxLBA;             
	U32                     StripeSize;         
	U16                     BlockSize;          
	U16                     Reserved1;          
	U8                      SupportedPhysDisks; 
	U8                      ResyncRate;         
	U16                     DataScrubDuration;  
	U8                      NumPhysDisks;       
	U8                      Reserved2;          
	U8                      Reserved3;          
	U8                      InactiveStatus;     
	MPI2_RAIDVOL0_PHYS_DISK
	PhysDisk[MPI2_RAID_VOL_PAGE_0_PHYSDISK_MAX];  
} MPI2_CONFIG_PAGE_RAID_VOL_0,
	*PTR_MPI2_CONFIG_PAGE_RAID_VOL_0,
	Mpi2RaidVolPage0_t, *pMpi2RaidVolPage0_t;
#define MPI2_RAIDVOLPAGE0_PAGEVERSION           (0x0A)
#define MPI2_RAID_VOL_STATE_MISSING                         (0x00)
#define MPI2_RAID_VOL_STATE_FAILED                          (0x01)
#define MPI2_RAID_VOL_STATE_INITIALIZING                    (0x02)
#define MPI2_RAID_VOL_STATE_ONLINE                          (0x03)
#define MPI2_RAID_VOL_STATE_DEGRADED                        (0x04)
#define MPI2_RAID_VOL_STATE_OPTIMAL                         (0x05)
#define MPI2_RAID_VOL_TYPE_RAID0                            (0x00)
#define MPI2_RAID_VOL_TYPE_RAID1E                           (0x01)
#define MPI2_RAID_VOL_TYPE_RAID1                            (0x02)
#define MPI2_RAID_VOL_TYPE_RAID10                           (0x05)
#define MPI2_RAID_VOL_TYPE_UNKNOWN                          (0xFF)
#define MPI2_RAIDVOL0_STATUS_FLAG_PENDING_RESYNC            (0x02000000)
#define MPI2_RAIDVOL0_STATUS_FLAG_BACKG_INIT_PENDING        (0x01000000)
#define MPI2_RAIDVOL0_STATUS_FLAG_MDC_PENDING               (0x00800000)
#define MPI2_RAIDVOL0_STATUS_FLAG_USER_CONSIST_PENDING      (0x00400000)
#define MPI2_RAIDVOL0_STATUS_FLAG_MAKE_DATA_CONSISTENT      (0x00200000)
#define MPI2_RAIDVOL0_STATUS_FLAG_DATA_SCRUB                (0x00100000)
#define MPI2_RAIDVOL0_STATUS_FLAG_CONSISTENCY_CHECK         (0x00080000)
#define MPI2_RAIDVOL0_STATUS_FLAG_CAPACITY_EXPANSION        (0x00040000)
#define MPI2_RAIDVOL0_STATUS_FLAG_BACKGROUND_INIT           (0x00020000)
#define MPI2_RAIDVOL0_STATUS_FLAG_RESYNC_IN_PROGRESS        (0x00010000)
#define MPI2_RAIDVOL0_STATUS_FLAG_VOL_NOT_CONSISTENT        (0x00000080)
#define MPI2_RAIDVOL0_STATUS_FLAG_OCE_ALLOWED               (0x00000040)
#define MPI2_RAIDVOL0_STATUS_FLAG_BGI_COMPLETE              (0x00000020)
#define MPI2_RAIDVOL0_STATUS_FLAG_1E_OFFSET_MIRROR          (0x00000000)
#define MPI2_RAIDVOL0_STATUS_FLAG_1E_ADJACENT_MIRROR        (0x00000010)
#define MPI2_RAIDVOL0_STATUS_FLAG_BAD_BLOCK_TABLE_FULL      (0x00000008)
#define MPI2_RAIDVOL0_STATUS_FLAG_VOLUME_INACTIVE           (0x00000004)
#define MPI2_RAIDVOL0_STATUS_FLAG_QUIESCED                  (0x00000002)
#define MPI2_RAIDVOL0_STATUS_FLAG_ENABLED                   (0x00000001)
#define MPI2_RAIDVOL0_SUPPORT_SOLID_STATE_DISKS             (0x08)
#define MPI2_RAIDVOL0_SUPPORT_HARD_DISKS                    (0x04)
#define MPI2_RAIDVOL0_SUPPORT_SAS_PROTOCOL                  (0x02)
#define MPI2_RAIDVOL0_SUPPORT_SATA_PROTOCOL                 (0x01)
#define MPI2_RAIDVOLPAGE0_UNKNOWN_INACTIVE                  (0x00)
#define MPI2_RAIDVOLPAGE0_STALE_METADATA_INACTIVE           (0x01)
#define MPI2_RAIDVOLPAGE0_FOREIGN_VOLUME_INACTIVE           (0x02)
#define MPI2_RAIDVOLPAGE0_INSUFFICIENT_RESOURCE_INACTIVE    (0x03)
#define MPI2_RAIDVOLPAGE0_CLONE_VOLUME_INACTIVE             (0x04)
#define MPI2_RAIDVOLPAGE0_INSUFFICIENT_METADATA_INACTIVE    (0x05)
#define MPI2_RAIDVOLPAGE0_PREVIOUSLY_DELETED                (0x06)
typedef struct _MPI2_CONFIG_PAGE_RAID_VOL_1 {
	MPI2_CONFIG_PAGE_HEADER Header;                      
	U16                     DevHandle;                   
	U16                     Reserved0;                   
	U8                      GUID[24];                    
	U8                      Name[16];                    
	U64                     WWID;                        
	U32                     Reserved1;                   
	U32                     Reserved2;                   
} MPI2_CONFIG_PAGE_RAID_VOL_1,
	*PTR_MPI2_CONFIG_PAGE_RAID_VOL_1,
	Mpi2RaidVolPage1_t, *pMpi2RaidVolPage1_t;
#define MPI2_RAIDVOLPAGE1_PAGEVERSION           (0x03)
typedef struct _MPI2_RAIDPHYSDISK0_SETTINGS {
	U16                     Reserved1;                   
	U8                      HotSparePool;                
	U8                      Reserved2;                   
} MPI2_RAIDPHYSDISK0_SETTINGS,
	*PTR_MPI2_RAIDPHYSDISK0_SETTINGS,
	Mpi2RaidPhysDisk0Settings_t,
	*pMpi2RaidPhysDisk0Settings_t;
typedef struct _MPI2_RAIDPHYSDISK0_INQUIRY_DATA {
	U8                      VendorID[8];                 
	U8                      ProductID[16];               
	U8                      ProductRevLevel[4];          
	U8                      SerialNum[32];               
} MPI2_RAIDPHYSDISK0_INQUIRY_DATA,
	*PTR_MPI2_RAIDPHYSDISK0_INQUIRY_DATA,
	Mpi2RaidPhysDisk0InquiryData_t,
	*pMpi2RaidPhysDisk0InquiryData_t;
typedef struct _MPI2_CONFIG_PAGE_RD_PDISK_0 {
	MPI2_CONFIG_PAGE_HEADER         Header;              
	U16                             DevHandle;           
	U8                              Reserved1;           
	U8                              PhysDiskNum;         
	MPI2_RAIDPHYSDISK0_SETTINGS     PhysDiskSettings;    
	U32                             Reserved2;           
	MPI2_RAIDPHYSDISK0_INQUIRY_DATA InquiryData;         
	U32                             Reserved3;           
	U8                              PhysDiskState;       
	U8                              OfflineReason;       
	U8                              IncompatibleReason;  
	U8                              PhysDiskAttributes;  
	U32                             PhysDiskStatusFlags; 
	U64                             DeviceMaxLBA;        
	U64                             HostMaxLBA;          
	U64                             CoercedMaxLBA;       
	U16                             BlockSize;           
	U16                             Reserved5;           
	U32                             Reserved6;           
} MPI2_CONFIG_PAGE_RD_PDISK_0,
	*PTR_MPI2_CONFIG_PAGE_RD_PDISK_0,
	Mpi2RaidPhysDiskPage0_t,
	*pMpi2RaidPhysDiskPage0_t;
#define MPI2_RAIDPHYSDISKPAGE0_PAGEVERSION          (0x05)
#define MPI2_RAID_PD_STATE_NOT_CONFIGURED               (0x00)
#define MPI2_RAID_PD_STATE_NOT_COMPATIBLE               (0x01)
#define MPI2_RAID_PD_STATE_OFFLINE                      (0x02)
#define MPI2_RAID_PD_STATE_ONLINE                       (0x03)
#define MPI2_RAID_PD_STATE_HOT_SPARE                    (0x04)
#define MPI2_RAID_PD_STATE_DEGRADED                     (0x05)
#define MPI2_RAID_PD_STATE_REBUILDING                   (0x06)
#define MPI2_RAID_PD_STATE_OPTIMAL                      (0x07)
#define MPI2_PHYSDISK0_ONLINE                           (0x00)
#define MPI2_PHYSDISK0_OFFLINE_MISSING                  (0x01)
#define MPI2_PHYSDISK0_OFFLINE_FAILED                   (0x03)
#define MPI2_PHYSDISK0_OFFLINE_INITIALIZING             (0x04)
#define MPI2_PHYSDISK0_OFFLINE_REQUESTED                (0x05)
#define MPI2_PHYSDISK0_OFFLINE_FAILED_REQUESTED         (0x06)
#define MPI2_PHYSDISK0_OFFLINE_OTHER                    (0xFF)
#define MPI2_PHYSDISK0_COMPATIBLE                       (0x00)
#define MPI2_PHYSDISK0_INCOMPATIBLE_PROTOCOL            (0x01)
#define MPI2_PHYSDISK0_INCOMPATIBLE_BLOCKSIZE           (0x02)
#define MPI2_PHYSDISK0_INCOMPATIBLE_MAX_LBA             (0x03)
#define MPI2_PHYSDISK0_INCOMPATIBLE_SATA_EXTENDED_CMD   (0x04)
#define MPI2_PHYSDISK0_INCOMPATIBLE_REMOVEABLE_MEDIA    (0x05)
#define MPI2_PHYSDISK0_INCOMPATIBLE_MEDIA_TYPE          (0x06)
#define MPI2_PHYSDISK0_INCOMPATIBLE_UNKNOWN             (0xFF)
#define MPI2_PHYSDISK0_ATTRIB_MEDIA_MASK                (0x0C)
#define MPI2_PHYSDISK0_ATTRIB_SOLID_STATE_DRIVE         (0x08)
#define MPI2_PHYSDISK0_ATTRIB_HARD_DISK_DRIVE           (0x04)
#define MPI2_PHYSDISK0_ATTRIB_PROTOCOL_MASK             (0x03)
#define MPI2_PHYSDISK0_ATTRIB_SAS_PROTOCOL              (0x02)
#define MPI2_PHYSDISK0_ATTRIB_SATA_PROTOCOL             (0x01)
#define MPI2_PHYSDISK0_STATUS_FLAG_NOT_CERTIFIED        (0x00000040)
#define MPI2_PHYSDISK0_STATUS_FLAG_OCE_TARGET           (0x00000020)
#define MPI2_PHYSDISK0_STATUS_FLAG_WRITE_CACHE_ENABLED  (0x00000010)
#define MPI2_PHYSDISK0_STATUS_FLAG_OPTIMAL_PREVIOUS     (0x00000000)
#define MPI2_PHYSDISK0_STATUS_FLAG_NOT_OPTIMAL_PREVIOUS (0x00000008)
#define MPI2_PHYSDISK0_STATUS_FLAG_INACTIVE_VOLUME      (0x00000004)
#define MPI2_PHYSDISK0_STATUS_FLAG_QUIESCED             (0x00000002)
#define MPI2_PHYSDISK0_STATUS_FLAG_OUT_OF_SYNC          (0x00000001)
#ifndef MPI2_RAID_PHYS_DISK1_PATH_MAX
#define MPI2_RAID_PHYS_DISK1_PATH_MAX   (1)
#endif
typedef struct _MPI2_RAIDPHYSDISK1_PATH {
	U16             DevHandle;           
	U16             Reserved1;           
	U64             WWID;                
	U64             OwnerWWID;           
	U8              OwnerIdentifier;     
	U8              Reserved2;           
	U16             Flags;               
} MPI2_RAIDPHYSDISK1_PATH, *PTR_MPI2_RAIDPHYSDISK1_PATH,
	Mpi2RaidPhysDisk1Path_t,
	*pMpi2RaidPhysDisk1Path_t;
#define MPI2_RAID_PHYSDISK1_FLAG_PRIMARY        (0x0004)
#define MPI2_RAID_PHYSDISK1_FLAG_BROKEN         (0x0002)
#define MPI2_RAID_PHYSDISK1_FLAG_INVALID        (0x0001)
typedef struct _MPI2_CONFIG_PAGE_RD_PDISK_1 {
	MPI2_CONFIG_PAGE_HEADER         Header;              
	U8                              NumPhysDiskPaths;    
	U8                              PhysDiskNum;         
	U16                             Reserved1;           
	U32                             Reserved2;           
	MPI2_RAIDPHYSDISK1_PATH
		PhysicalDiskPath[MPI2_RAID_PHYS_DISK1_PATH_MAX]; 
} MPI2_CONFIG_PAGE_RD_PDISK_1,
	*PTR_MPI2_CONFIG_PAGE_RD_PDISK_1,
	Mpi2RaidPhysDiskPage1_t,
	*pMpi2RaidPhysDiskPage1_t;
#define MPI2_RAIDPHYSDISKPAGE1_PAGEVERSION          (0x02)
#define MPI2_SAS_NEG_LINK_RATE_MASK_LOGICAL             (0xF0)
#define MPI2_SAS_NEG_LINK_RATE_SHIFT_LOGICAL            (4)
#define MPI2_SAS_NEG_LINK_RATE_MASK_PHYSICAL            (0x0F)
#define MPI2_SAS_NEG_LINK_RATE_UNKNOWN_LINK_RATE        (0x00)
#define MPI2_SAS_NEG_LINK_RATE_PHY_DISABLED             (0x01)
#define MPI2_SAS_NEG_LINK_RATE_NEGOTIATION_FAILED       (0x02)
#define MPI2_SAS_NEG_LINK_RATE_SATA_OOB_COMPLETE        (0x03)
#define MPI2_SAS_NEG_LINK_RATE_PORT_SELECTOR            (0x04)
#define MPI2_SAS_NEG_LINK_RATE_SMP_RESET_IN_PROGRESS    (0x05)
#define MPI2_SAS_NEG_LINK_RATE_UNSUPPORTED_PHY          (0x06)
#define MPI2_SAS_NEG_LINK_RATE_1_5                      (0x08)
#define MPI2_SAS_NEG_LINK_RATE_3_0                      (0x09)
#define MPI2_SAS_NEG_LINK_RATE_6_0                      (0x0A)
#define MPI25_SAS_NEG_LINK_RATE_12_0                    (0x0B)
#define MPI26_SAS_NEG_LINK_RATE_22_5                    (0x0C)
#define MPI2_SAS_APHYINFO_INSIDE_ZPSDS_PERSISTENT       (0x00000040)
#define MPI2_SAS_APHYINFO_REQUESTED_INSIDE_ZPSDS        (0x00000020)
#define MPI2_SAS_APHYINFO_BREAK_REPLY_CAPABLE           (0x00000010)
#define MPI2_SAS_APHYINFO_REASON_MASK                   (0x0000000F)
#define MPI2_SAS_APHYINFO_REASON_UNKNOWN                (0x00000000)
#define MPI2_SAS_APHYINFO_REASON_POWER_ON               (0x00000001)
#define MPI2_SAS_APHYINFO_REASON_HARD_RESET             (0x00000002)
#define MPI2_SAS_APHYINFO_REASON_SMP_PHY_CONTROL        (0x00000003)
#define MPI2_SAS_APHYINFO_REASON_LOSS_OF_SYNC           (0x00000004)
#define MPI2_SAS_APHYINFO_REASON_MULTIPLEXING_SEQ       (0x00000005)
#define MPI2_SAS_APHYINFO_REASON_IT_NEXUS_LOSS_TIMER    (0x00000006)
#define MPI2_SAS_APHYINFO_REASON_BREAK_TIMEOUT          (0x00000007)
#define MPI2_SAS_APHYINFO_REASON_PHY_TEST_STOPPED       (0x00000008)
#define MPI2_SAS_PHYINFO_PHY_VACANT                     (0x80000000)
#define MPI2_SAS_PHYINFO_PHY_POWER_CONDITION_MASK       (0x18000000)
#define MPI2_SAS_PHYINFO_SHIFT_PHY_POWER_CONDITION      (27)
#define MPI2_SAS_PHYINFO_PHY_POWER_ACTIVE               (0x00000000)
#define MPI2_SAS_PHYINFO_PHY_POWER_PARTIAL              (0x08000000)
#define MPI2_SAS_PHYINFO_PHY_POWER_SLUMBER              (0x10000000)
#define MPI2_SAS_PHYINFO_CHANGED_REQ_INSIDE_ZPSDS       (0x04000000)
#define MPI2_SAS_PHYINFO_INSIDE_ZPSDS_PERSISTENT        (0x02000000)
#define MPI2_SAS_PHYINFO_REQ_INSIDE_ZPSDS               (0x01000000)
#define MPI2_SAS_PHYINFO_ZONE_GROUP_PERSISTENT          (0x00400000)
#define MPI2_SAS_PHYINFO_INSIDE_ZPSDS                   (0x00200000)
#define MPI2_SAS_PHYINFO_ZONING_ENABLED                 (0x00100000)
#define MPI2_SAS_PHYINFO_REASON_MASK                    (0x000F0000)
#define MPI2_SAS_PHYINFO_REASON_UNKNOWN                 (0x00000000)
#define MPI2_SAS_PHYINFO_REASON_POWER_ON                (0x00010000)
#define MPI2_SAS_PHYINFO_REASON_HARD_RESET              (0x00020000)
#define MPI2_SAS_PHYINFO_REASON_SMP_PHY_CONTROL         (0x00030000)
#define MPI2_SAS_PHYINFO_REASON_LOSS_OF_SYNC            (0x00040000)
#define MPI2_SAS_PHYINFO_REASON_MULTIPLEXING_SEQ        (0x00050000)
#define MPI2_SAS_PHYINFO_REASON_IT_NEXUS_LOSS_TIMER     (0x00060000)
#define MPI2_SAS_PHYINFO_REASON_BREAK_TIMEOUT           (0x00070000)
#define MPI2_SAS_PHYINFO_REASON_PHY_TEST_STOPPED        (0x00080000)
#define MPI2_SAS_PHYINFO_MULTIPLEXING_SUPPORTED         (0x00008000)
#define MPI2_SAS_PHYINFO_SATA_PORT_ACTIVE               (0x00004000)
#define MPI2_SAS_PHYINFO_SATA_PORT_SELECTOR_PRESENT     (0x00002000)
#define MPI2_SAS_PHYINFO_VIRTUAL_PHY                    (0x00001000)
#define MPI2_SAS_PHYINFO_MASK_PARTIAL_PATHWAY_TIME      (0x00000F00)
#define MPI2_SAS_PHYINFO_SHIFT_PARTIAL_PATHWAY_TIME     (8)
#define MPI2_SAS_PHYINFO_MASK_ROUTING_ATTRIBUTE         (0x000000F0)
#define MPI2_SAS_PHYINFO_DIRECT_ROUTING                 (0x00000000)
#define MPI2_SAS_PHYINFO_SUBTRACTIVE_ROUTING            (0x00000010)
#define MPI2_SAS_PHYINFO_TABLE_ROUTING                  (0x00000020)
#define MPI2_SAS_PRATE_MAX_RATE_MASK                    (0xF0)
#define MPI2_SAS_PRATE_MAX_RATE_NOT_PROGRAMMABLE        (0x00)
#define MPI2_SAS_PRATE_MAX_RATE_1_5                     (0x80)
#define MPI2_SAS_PRATE_MAX_RATE_3_0                     (0x90)
#define MPI2_SAS_PRATE_MAX_RATE_6_0                     (0xA0)
#define MPI25_SAS_PRATE_MAX_RATE_12_0                   (0xB0)
#define MPI26_SAS_PRATE_MAX_RATE_22_5                   (0xC0)
#define MPI2_SAS_PRATE_MIN_RATE_MASK                    (0x0F)
#define MPI2_SAS_PRATE_MIN_RATE_NOT_PROGRAMMABLE        (0x00)
#define MPI2_SAS_PRATE_MIN_RATE_1_5                     (0x08)
#define MPI2_SAS_PRATE_MIN_RATE_3_0                     (0x09)
#define MPI2_SAS_PRATE_MIN_RATE_6_0                     (0x0A)
#define MPI25_SAS_PRATE_MIN_RATE_12_0                   (0x0B)
#define MPI26_SAS_PRATE_MIN_RATE_22_5                   (0x0C)
#define MPI2_SAS_HWRATE_MAX_RATE_MASK                   (0xF0)
#define MPI2_SAS_HWRATE_MAX_RATE_1_5                    (0x80)
#define MPI2_SAS_HWRATE_MAX_RATE_3_0                    (0x90)
#define MPI2_SAS_HWRATE_MAX_RATE_6_0                    (0xA0)
#define MPI25_SAS_HWRATE_MAX_RATE_12_0                  (0xB0)
#define MPI26_SAS_HWRATE_MAX_RATE_22_5                  (0xC0)
#define MPI2_SAS_HWRATE_MIN_RATE_MASK                   (0x0F)
#define MPI2_SAS_HWRATE_MIN_RATE_1_5                    (0x08)
#define MPI2_SAS_HWRATE_MIN_RATE_3_0                    (0x09)
#define MPI2_SAS_HWRATE_MIN_RATE_6_0                    (0x0A)
#define MPI25_SAS_HWRATE_MIN_RATE_12_0                  (0x0B)
#define MPI26_SAS_HWRATE_MIN_RATE_22_5                  (0x0C)
typedef struct _MPI2_SAS_IO_UNIT0_PHY_DATA {
	U8          Port;                    
	U8          PortFlags;               
	U8          PhyFlags;                
	U8          NegotiatedLinkRate;      
	U32         ControllerPhyDeviceInfo; 
	U16         AttachedDevHandle;       
	U16         ControllerDevHandle;     
	U32         DiscoveryStatus;         
	U32         Reserved;                
} MPI2_SAS_IO_UNIT0_PHY_DATA,
	*PTR_MPI2_SAS_IO_UNIT0_PHY_DATA,
	Mpi2SasIOUnit0PhyData_t,
	*pMpi2SasIOUnit0PhyData_t;
#ifndef MPI2_SAS_IOUNIT0_PHY_MAX
#define MPI2_SAS_IOUNIT0_PHY_MAX        (1)
#endif
typedef struct _MPI2_CONFIG_PAGE_SASIOUNIT_0 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER    Header;    
	U32                                 Reserved1; 
	U8                                  NumPhys;   
	U8                                  Reserved2; 
	U16                                 Reserved3; 
	MPI2_SAS_IO_UNIT0_PHY_DATA
		PhyData[MPI2_SAS_IOUNIT0_PHY_MAX];     
} MPI2_CONFIG_PAGE_SASIOUNIT_0,
	*PTR_MPI2_CONFIG_PAGE_SASIOUNIT_0,
	Mpi2SasIOUnitPage0_t, *pMpi2SasIOUnitPage0_t;
#define MPI2_SASIOUNITPAGE0_PAGEVERSION                     (0x05)
#define MPI2_SASIOUNIT0_PORTFLAGS_DISCOVERY_IN_PROGRESS     (0x08)
#define MPI2_SASIOUNIT0_PORTFLAGS_AUTO_PORT_CONFIG          (0x01)
#define MPI2_SASIOUNIT0_PHYFLAGS_INIT_PERSIST_CONNECT       (0x40)
#define MPI2_SASIOUNIT0_PHYFLAGS_TARG_PERSIST_CONNECT       (0x20)
#define MPI2_SASIOUNIT0_PHYFLAGS_ZONING_ENABLED             (0x10)
#define MPI2_SASIOUNIT0_PHYFLAGS_PHY_DISABLED               (0x08)
#define MPI2_SASIOUNIT0_DS_MAX_ENCLOSURES_EXCEED            (0x80000000)
#define MPI2_SASIOUNIT0_DS_MAX_EXPANDERS_EXCEED             (0x40000000)
#define MPI2_SASIOUNIT0_DS_MAX_DEVICES_EXCEED               (0x20000000)
#define MPI2_SASIOUNIT0_DS_MAX_TOPO_PHYS_EXCEED             (0x10000000)
#define MPI2_SASIOUNIT0_DS_DOWNSTREAM_INITIATOR             (0x08000000)
#define MPI2_SASIOUNIT0_DS_MULTI_SUBTRACTIVE_SUBTRACTIVE    (0x00008000)
#define MPI2_SASIOUNIT0_DS_EXP_MULTI_SUBTRACTIVE            (0x00004000)
#define MPI2_SASIOUNIT0_DS_MULTI_PORT_DOMAIN                (0x00002000)
#define MPI2_SASIOUNIT0_DS_TABLE_TO_SUBTRACTIVE_LINK        (0x00001000)
#define MPI2_SASIOUNIT0_DS_UNSUPPORTED_DEVICE               (0x00000800)
#define MPI2_SASIOUNIT0_DS_TABLE_LINK                       (0x00000400)
#define MPI2_SASIOUNIT0_DS_SUBTRACTIVE_LINK                 (0x00000200)
#define MPI2_SASIOUNIT0_DS_SMP_CRC_ERROR                    (0x00000100)
#define MPI2_SASIOUNIT0_DS_SMP_FUNCTION_FAILED              (0x00000080)
#define MPI2_SASIOUNIT0_DS_INDEX_NOT_EXIST                  (0x00000040)
#define MPI2_SASIOUNIT0_DS_OUT_ROUTE_ENTRIES                (0x00000020)
#define MPI2_SASIOUNIT0_DS_SMP_TIMEOUT                      (0x00000010)
#define MPI2_SASIOUNIT0_DS_MULTIPLE_PORTS                   (0x00000004)
#define MPI2_SASIOUNIT0_DS_UNADDRESSABLE_DEVICE             (0x00000002)
#define MPI2_SASIOUNIT0_DS_LOOP_DETECTED                    (0x00000001)
typedef struct _MPI2_SAS_IO_UNIT1_PHY_DATA {
	U8          Port;                        
	U8          PortFlags;                   
	U8          PhyFlags;                    
	U8          MaxMinLinkRate;              
	U32         ControllerPhyDeviceInfo;     
	U16         MaxTargetPortConnectTime;    
	U16         Reserved1;                   
} MPI2_SAS_IO_UNIT1_PHY_DATA,
	*PTR_MPI2_SAS_IO_UNIT1_PHY_DATA,
	Mpi2SasIOUnit1PhyData_t,
	*pMpi2SasIOUnit1PhyData_t;
#ifndef MPI2_SAS_IOUNIT1_PHY_MAX
#define MPI2_SAS_IOUNIT1_PHY_MAX        (1)
#endif
typedef struct _MPI2_CONFIG_PAGE_SASIOUNIT_1 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER    Header;  
	U16
		ControlFlags;                        
	U16
		SASNarrowMaxQueueDepth;              
	U16
		AdditionalControlFlags;              
	U16
		SASWideMaxQueueDepth;                
	U8
		NumPhys;                             
	U8
		SATAMaxQDepth;                       
	U8
		ReportDeviceMissingDelay;            
	U8
		IODeviceMissingDelay;                
	MPI2_SAS_IO_UNIT1_PHY_DATA
		PhyData[MPI2_SAS_IOUNIT1_PHY_MAX];   
} MPI2_CONFIG_PAGE_SASIOUNIT_1,
	*PTR_MPI2_CONFIG_PAGE_SASIOUNIT_1,
	Mpi2SasIOUnitPage1_t, *pMpi2SasIOUnitPage1_t;
#define MPI2_SASIOUNITPAGE1_PAGEVERSION     (0x09)
#define MPI2_SASIOUNIT1_CONTROL_DEVICE_SELF_TEST                    (0x8000)
#define MPI2_SASIOUNIT1_CONTROL_SATA_3_0_MAX                        (0x4000)
#define MPI2_SASIOUNIT1_CONTROL_SATA_1_5_MAX                        (0x2000)
#define MPI2_SASIOUNIT1_CONTROL_SATA_SW_PRESERVE                    (0x1000)
#define MPI2_SASIOUNIT1_CONTROL_MASK_DEV_SUPPORT                    (0x0600)
#define MPI2_SASIOUNIT1_CONTROL_SHIFT_DEV_SUPPORT                   (9)
#define MPI2_SASIOUNIT1_CONTROL_DEV_SUPPORT_BOTH                    (0x0)
#define MPI2_SASIOUNIT1_CONTROL_DEV_SAS_SUPPORT                     (0x1)
#define MPI2_SASIOUNIT1_CONTROL_DEV_SATA_SUPPORT                    (0x2)
#define MPI2_SASIOUNIT1_CONTROL_SATA_48BIT_LBA_REQUIRED             (0x0080)
#define MPI2_SASIOUNIT1_CONTROL_SATA_SMART_REQUIRED                 (0x0040)
#define MPI2_SASIOUNIT1_CONTROL_SATA_NCQ_REQUIRED                   (0x0020)
#define MPI2_SASIOUNIT1_CONTROL_SATA_FUA_REQUIRED                   (0x0010)
#define MPI2_SASIOUNIT1_CONTROL_TABLE_SUBTRACTIVE_ILLEGAL           (0x0008)
#define MPI2_SASIOUNIT1_CONTROL_SUBTRACTIVE_ILLEGAL                 (0x0004)
#define MPI2_SASIOUNIT1_CONTROL_FIRST_LVL_DISC_ONLY                 (0x0002)
#define MPI2_SASIOUNIT1_CONTROL_CLEAR_AFFILIATION                   (0x0001)
#define MPI2_SASIOUNIT1_ACONTROL_DA_PERSIST_CONNECT                 (0x0100)
#define MPI2_SASIOUNIT1_ACONTROL_MULTI_PORT_DOMAIN_ILLEGAL          (0x0080)
#define MPI2_SASIOUNIT1_ACONTROL_SATA_ASYNCHROUNOUS_NOTIFICATION    (0x0040)
#define MPI2_SASIOUNIT1_ACONTROL_INVALID_TOPOLOGY_CORRECTION        (0x0020)
#define MPI2_SASIOUNIT1_ACONTROL_PORT_ENABLE_ONLY_SATA_LINK_RESET   (0x0010)
#define MPI2_SASIOUNIT1_ACONTROL_OTHER_AFFILIATION_SATA_LINK_RESET  (0x0008)
#define MPI2_SASIOUNIT1_ACONTROL_SELF_AFFILIATION_SATA_LINK_RESET   (0x0004)
#define MPI2_SASIOUNIT1_ACONTROL_NO_AFFILIATION_SATA_LINK_RESET     (0x0002)
#define MPI2_SASIOUNIT1_ACONTROL_ALLOW_TABLE_TO_TABLE               (0x0001)
#define MPI2_SASIOUNIT1_REPORT_MISSING_TIMEOUT_MASK                 (0x7F)
#define MPI2_SASIOUNIT1_REPORT_MISSING_UNIT_16                      (0x80)
#define MPI2_SASIOUNIT1_PORT_FLAGS_AUTO_PORT_CONFIG                 (0x01)
#define MPI2_SASIOUNIT1_PHYFLAGS_INIT_PERSIST_CONNECT               (0x40)
#define MPI2_SASIOUNIT1_PHYFLAGS_TARG_PERSIST_CONNECT               (0x20)
#define MPI2_SASIOUNIT1_PHYFLAGS_ZONING_ENABLE                      (0x10)
#define MPI2_SASIOUNIT1_PHYFLAGS_PHY_DISABLE                        (0x08)
#define MPI2_SASIOUNIT1_MAX_RATE_MASK                               (0xF0)
#define MPI2_SASIOUNIT1_MAX_RATE_1_5                                (0x80)
#define MPI2_SASIOUNIT1_MAX_RATE_3_0                                (0x90)
#define MPI2_SASIOUNIT1_MAX_RATE_6_0                                (0xA0)
#define MPI25_SASIOUNIT1_MAX_RATE_12_0                              (0xB0)
#define MPI26_SASIOUNIT1_MAX_RATE_22_5                              (0xC0)
#define MPI2_SASIOUNIT1_MIN_RATE_MASK                               (0x0F)
#define MPI2_SASIOUNIT1_MIN_RATE_1_5                                (0x08)
#define MPI2_SASIOUNIT1_MIN_RATE_3_0                                (0x09)
#define MPI2_SASIOUNIT1_MIN_RATE_6_0                                (0x0A)
#define MPI25_SASIOUNIT1_MIN_RATE_12_0                              (0x0B)
#define MPI26_SASIOUNIT1_MIN_RATE_22_5                              (0x0C)
typedef struct _MPI2_SAS_IOUNIT4_SPINUP_GROUP {
	U8          MaxTargetSpinup;             
	U8          SpinupDelay;                 
	U8          SpinupFlags;                 
	U8          Reserved1;                   
} MPI2_SAS_IOUNIT4_SPINUP_GROUP,
	*PTR_MPI2_SAS_IOUNIT4_SPINUP_GROUP,
	Mpi2SasIOUnit4SpinupGroup_t,
	*pMpi2SasIOUnit4SpinupGroup_t;
#define MPI2_SASIOUNIT4_SPINUP_DISABLE_FLAG         (0x01)
#ifndef MPI2_SAS_IOUNIT4_PHY_MAX
#define MPI2_SAS_IOUNIT4_PHY_MAX        (4)
#endif
typedef struct _MPI2_CONFIG_PAGE_SASIOUNIT_4 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER    Header; 
	MPI2_SAS_IOUNIT4_SPINUP_GROUP
		SpinupGroupParameters[4];        
	U32
		Reserved1;                       
	U32
		Reserved2;                       
	U32
		Reserved3;                       
	U8
		BootDeviceWaitTime;              
	U8
		SATADeviceWaitTime;		 
	U16
		Reserved5;                       
	U8
		NumPhys;                         
	U8
		PEInitialSpinupDelay;            
	U8
		PEReplyDelay;                    
	U8
		Flags;                           
	U8
		PHY[MPI2_SAS_IOUNIT4_PHY_MAX];   
} MPI2_CONFIG_PAGE_SASIOUNIT_4,
	*PTR_MPI2_CONFIG_PAGE_SASIOUNIT_4,
	Mpi2SasIOUnitPage4_t, *pMpi2SasIOUnitPage4_t;
#define MPI2_SASIOUNITPAGE4_PAGEVERSION     (0x02)
#define MPI2_SASIOUNIT4_FLAGS_AUTO_PORTENABLE               (0x01)
#define MPI2_SASIOUNIT4_PHY_SPINUP_GROUP_MASK               (0x03)
typedef struct _MPI2_SAS_IO_UNIT5_PHY_PM_SETTINGS {
	U8          ControlFlags;                
	U8          PortWidthModGroup;           
	U16         InactivityTimerExponent;     
	U8          SATAPartialTimeout;          
	U8          Reserved2;                   
	U8          SATASlumberTimeout;          
	U8          Reserved3;                   
	U8          SASPartialTimeout;           
	U8          Reserved4;                   
	U8          SASSlumberTimeout;           
	U8          Reserved5;                   
} MPI2_SAS_IO_UNIT5_PHY_PM_SETTINGS,
	*PTR_MPI2_SAS_IO_UNIT5_PHY_PM_SETTINGS,
	Mpi2SasIOUnit5PhyPmSettings_t,
	*pMpi2SasIOUnit5PhyPmSettings_t;
#define MPI2_SASIOUNIT5_CONTROL_SAS_SLUMBER_ENABLE      (0x08)
#define MPI2_SASIOUNIT5_CONTROL_SAS_PARTIAL_ENABLE      (0x04)
#define MPI2_SASIOUNIT5_CONTROL_SATA_SLUMBER_ENABLE     (0x02)
#define MPI2_SASIOUNIT5_CONTROL_SATA_PARTIAL_ENABLE     (0x01)
#define MPI2_SASIOUNIT5_PWMG_DISABLE                    (0xFF)
#define MPI2_SASIOUNIT5_ITE_MASK_SAS_SLUMBER            (0x7000)
#define MPI2_SASIOUNIT5_ITE_SHIFT_SAS_SLUMBER           (12)
#define MPI2_SASIOUNIT5_ITE_MASK_SAS_PARTIAL            (0x0700)
#define MPI2_SASIOUNIT5_ITE_SHIFT_SAS_PARTIAL           (8)
#define MPI2_SASIOUNIT5_ITE_MASK_SATA_SLUMBER           (0x0070)
#define MPI2_SASIOUNIT5_ITE_SHIFT_SATA_SLUMBER          (4)
#define MPI2_SASIOUNIT5_ITE_MASK_SATA_PARTIAL           (0x0007)
#define MPI2_SASIOUNIT5_ITE_SHIFT_SATA_PARTIAL          (0)
#define MPI2_SASIOUNIT5_ITE_TEN_SECONDS                 (7)
#define MPI2_SASIOUNIT5_ITE_ONE_SECOND                  (6)
#define MPI2_SASIOUNIT5_ITE_HUNDRED_MILLISECONDS        (5)
#define MPI2_SASIOUNIT5_ITE_TEN_MILLISECONDS            (4)
#define MPI2_SASIOUNIT5_ITE_ONE_MILLISECOND             (3)
#define MPI2_SASIOUNIT5_ITE_HUNDRED_MICROSECONDS        (2)
#define MPI2_SASIOUNIT5_ITE_TEN_MICROSECONDS            (1)
#define MPI2_SASIOUNIT5_ITE_ONE_MICROSECOND             (0)
#ifndef MPI2_SAS_IOUNIT5_PHY_MAX
#define MPI2_SAS_IOUNIT5_PHY_MAX        (1)
#endif
typedef struct _MPI2_CONFIG_PAGE_SASIOUNIT_5 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER    Header;    
	U8                                  NumPhys;   
	U8                                  Reserved1; 
	U16                                 Reserved2; 
	U32                                 Reserved3; 
	MPI2_SAS_IO_UNIT5_PHY_PM_SETTINGS
	SASPhyPowerManagementSettings[MPI2_SAS_IOUNIT5_PHY_MAX]; 
} MPI2_CONFIG_PAGE_SASIOUNIT_5,
	*PTR_MPI2_CONFIG_PAGE_SASIOUNIT_5,
	Mpi2SasIOUnitPage5_t, *pMpi2SasIOUnitPage5_t;
#define MPI2_SASIOUNITPAGE5_PAGEVERSION     (0x01)
typedef struct _MPI2_SAS_IO_UNIT6_PORT_WIDTH_MOD_GROUP_STATUS {
	U8          CurrentStatus;               
	U8          CurrentModulation;           
	U8          CurrentUtilization;          
	U8          Reserved1;                   
	U32         Reserved2;                   
} MPI2_SAS_IO_UNIT6_PORT_WIDTH_MOD_GROUP_STATUS,
	*PTR_MPI2_SAS_IO_UNIT6_PORT_WIDTH_MOD_GROUP_STATUS,
	Mpi2SasIOUnit6PortWidthModGroupStatus_t,
	*pMpi2SasIOUnit6PortWidthModGroupStatus_t;
#define MPI2_SASIOUNIT6_STATUS_UNAVAILABLE                      (0x00)
#define MPI2_SASIOUNIT6_STATUS_UNCONFIGURED                     (0x01)
#define MPI2_SASIOUNIT6_STATUS_INVALID_CONFIG                   (0x02)
#define MPI2_SASIOUNIT6_STATUS_LINK_DOWN                        (0x03)
#define MPI2_SASIOUNIT6_STATUS_OBSERVATION_ONLY                 (0x04)
#define MPI2_SASIOUNIT6_STATUS_INACTIVE                         (0x05)
#define MPI2_SASIOUNIT6_STATUS_ACTIVE_IOUNIT                    (0x06)
#define MPI2_SASIOUNIT6_STATUS_ACTIVE_HOST                      (0x07)
#define MPI2_SASIOUNIT6_MODULATION_25_PERCENT                   (0x00)
#define MPI2_SASIOUNIT6_MODULATION_50_PERCENT                   (0x01)
#define MPI2_SASIOUNIT6_MODULATION_75_PERCENT                   (0x02)
#define MPI2_SASIOUNIT6_MODULATION_100_PERCENT                  (0x03)
#ifndef MPI2_SAS_IOUNIT6_GROUP_MAX
#define MPI2_SAS_IOUNIT6_GROUP_MAX      (1)
#endif
typedef struct _MPI2_CONFIG_PAGE_SASIOUNIT_6 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER    Header;                  
	U32                                 Reserved1;               
	U32                                 Reserved2;               
	U8                                  NumGroups;               
	U8                                  Reserved3;               
	U16                                 Reserved4;               
	MPI2_SAS_IO_UNIT6_PORT_WIDTH_MOD_GROUP_STATUS
	PortWidthModulationGroupStatus[MPI2_SAS_IOUNIT6_GROUP_MAX];  
} MPI2_CONFIG_PAGE_SASIOUNIT_6,
	*PTR_MPI2_CONFIG_PAGE_SASIOUNIT_6,
	Mpi2SasIOUnitPage6_t, *pMpi2SasIOUnitPage6_t;
#define MPI2_SASIOUNITPAGE6_PAGEVERSION     (0x00)
typedef struct _MPI2_SAS_IO_UNIT7_PORT_WIDTH_MOD_GROUP_SETTINGS {
	U8          Flags;                       
	U8          Reserved1;                   
	U16         Reserved2;                   
	U8          Threshold75Pct;              
	U8          Threshold50Pct;              
	U8          Threshold25Pct;              
	U8          Reserved3;                   
} MPI2_SAS_IO_UNIT7_PORT_WIDTH_MOD_GROUP_SETTINGS,
	*PTR_MPI2_SAS_IO_UNIT7_PORT_WIDTH_MOD_GROUP_SETTINGS,
	Mpi2SasIOUnit7PortWidthModGroupSettings_t,
	*pMpi2SasIOUnit7PortWidthModGroupSettings_t;
#define MPI2_SASIOUNIT7_FLAGS_ENABLE_PORT_WIDTH_MODULATION  (0x01)
#ifndef MPI2_SAS_IOUNIT7_GROUP_MAX
#define MPI2_SAS_IOUNIT7_GROUP_MAX      (1)
#endif
typedef struct _MPI2_CONFIG_PAGE_SASIOUNIT_7 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER Header;              
	U8                               SamplingInterval;    
	U8                               WindowLength;        
	U16                              Reserved1;           
	U32                              Reserved2;           
	U32                              Reserved3;           
	U8                               NumGroups;           
	U8                               Reserved4;           
	U16                              Reserved5;           
	MPI2_SAS_IO_UNIT7_PORT_WIDTH_MOD_GROUP_SETTINGS
	PortWidthModulationGroupSettings[MPI2_SAS_IOUNIT7_GROUP_MAX]; 
} MPI2_CONFIG_PAGE_SASIOUNIT_7,
	*PTR_MPI2_CONFIG_PAGE_SASIOUNIT_7,
	Mpi2SasIOUnitPage7_t, *pMpi2SasIOUnitPage7_t;
#define MPI2_SASIOUNITPAGE7_PAGEVERSION     (0x00)
typedef struct _MPI2_CONFIG_PAGE_SASIOUNIT_8 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER
		Header;                          
	U32
		Reserved1;                       
	U32
		PowerManagementCapabilities;     
	U8
		TxRxSleepStatus;                 
	U8
		Reserved2;                       
	U16
		Reserved3;                       
} MPI2_CONFIG_PAGE_SASIOUNIT_8,
	*PTR_MPI2_CONFIG_PAGE_SASIOUNIT_8,
	Mpi2SasIOUnitPage8_t, *pMpi2SasIOUnitPage8_t;
#define MPI2_SASIOUNITPAGE8_PAGEVERSION     (0x00)
#define MPI2_SASIOUNIT8_PM_HOST_PORT_WIDTH_MOD          (0x00001000)
#define MPI2_SASIOUNIT8_PM_HOST_SAS_SLUMBER_MODE        (0x00000800)
#define MPI2_SASIOUNIT8_PM_HOST_SAS_PARTIAL_MODE        (0x00000400)
#define MPI2_SASIOUNIT8_PM_HOST_SATA_SLUMBER_MODE       (0x00000200)
#define MPI2_SASIOUNIT8_PM_HOST_SATA_PARTIAL_MODE       (0x00000100)
#define MPI2_SASIOUNIT8_PM_IOUNIT_PORT_WIDTH_MOD        (0x00000010)
#define MPI2_SASIOUNIT8_PM_IOUNIT_SAS_SLUMBER_MODE      (0x00000008)
#define MPI2_SASIOUNIT8_PM_IOUNIT_SAS_PARTIAL_MODE      (0x00000004)
#define MPI2_SASIOUNIT8_PM_IOUNIT_SATA_SLUMBER_MODE     (0x00000002)
#define MPI2_SASIOUNIT8_PM_IOUNIT_SATA_PARTIAL_MODE     (0x00000001)
#define MPI25_SASIOUNIT8_TXRXSLEEP_UNSUPPORTED          (0x00)
#define MPI25_SASIOUNIT8_TXRXSLEEP_DISENGAGED           (0x01)
#define MPI25_SASIOUNIT8_TXRXSLEEP_ACTIVE               (0x02)
#define MPI25_SASIOUNIT8_TXRXSLEEP_SHUTDOWN             (0x03)
typedef struct _MPI2_CONFIG_PAGE_SASIOUNIT16 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER
		Header;                              
	U64
		TimeStamp;                           
	U32
		Reserved1;                           
	U32
		Reserved2;                           
	U32
		FastPathPendedRequests;              
	U32
		FastPathUnPendedRequests;            
	U32
		FastPathHostRequestStarts;           
	U32
		FastPathFirmwareRequestStarts;       
	U32
		FastPathHostCompletions;             
	U32
		FastPathFirmwareCompletions;         
	U32
		NonFastPathRequestStarts;            
	U32
		NonFastPathHostCompletions;          
} MPI2_CONFIG_PAGE_SASIOUNIT16,
	*PTR_MPI2_CONFIG_PAGE_SASIOUNIT16,
	Mpi2SasIOUnitPage16_t, *pMpi2SasIOUnitPage16_t;
#define MPI2_SASIOUNITPAGE16_PAGEVERSION    (0x00)
typedef struct _MPI2_CONFIG_PAGE_EXPANDER_0 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER
		Header;                      
	U8
		PhysicalPort;                
	U8
		ReportGenLength;             
	U16
		EnclosureHandle;             
	U64
		SASAddress;                  
	U32
		DiscoveryStatus;             
	U16
		DevHandle;                   
	U16
		ParentDevHandle;             
	U16
		ExpanderChangeCount;         
	U16
		ExpanderRouteIndexes;        
	U8
		NumPhys;                     
	U8
		SASLevel;                    
	U16
		Flags;                       
	U16
		STPBusInactivityTimeLimit;   
	U16
		STPMaxConnectTimeLimit;      
	U16
		STP_SMP_NexusLossTime;       
	U16
		MaxNumRoutedSasAddresses;    
	U64
		ActiveZoneManagerSASAddress; 
	U16
		ZoneLockInactivityLimit;     
	U16
		Reserved1;                   
	U8
		TimeToReducedFunc;           
	U8
		InitialTimeToReducedFunc;    
	U8
		MaxReducedFuncTime;          
	U8
		Reserved2;                   
} MPI2_CONFIG_PAGE_EXPANDER_0,
	*PTR_MPI2_CONFIG_PAGE_EXPANDER_0,
	Mpi2ExpanderPage0_t, *pMpi2ExpanderPage0_t;
#define MPI2_SASEXPANDER0_PAGEVERSION       (0x06)
#define MPI2_SAS_EXPANDER0_DS_MAX_ENCLOSURES_EXCEED         (0x80000000)
#define MPI2_SAS_EXPANDER0_DS_MAX_EXPANDERS_EXCEED          (0x40000000)
#define MPI2_SAS_EXPANDER0_DS_MAX_DEVICES_EXCEED            (0x20000000)
#define MPI2_SAS_EXPANDER0_DS_MAX_TOPO_PHYS_EXCEED          (0x10000000)
#define MPI2_SAS_EXPANDER0_DS_DOWNSTREAM_INITIATOR          (0x08000000)
#define MPI2_SAS_EXPANDER0_DS_MULTI_SUBTRACTIVE_SUBTRACTIVE (0x00008000)
#define MPI2_SAS_EXPANDER0_DS_EXP_MULTI_SUBTRACTIVE         (0x00004000)
#define MPI2_SAS_EXPANDER0_DS_MULTI_PORT_DOMAIN             (0x00002000)
#define MPI2_SAS_EXPANDER0_DS_TABLE_TO_SUBTRACTIVE_LINK     (0x00001000)
#define MPI2_SAS_EXPANDER0_DS_UNSUPPORTED_DEVICE            (0x00000800)
#define MPI2_SAS_EXPANDER0_DS_TABLE_LINK                    (0x00000400)
#define MPI2_SAS_EXPANDER0_DS_SUBTRACTIVE_LINK              (0x00000200)
#define MPI2_SAS_EXPANDER0_DS_SMP_CRC_ERROR                 (0x00000100)
#define MPI2_SAS_EXPANDER0_DS_SMP_FUNCTION_FAILED           (0x00000080)
#define MPI2_SAS_EXPANDER0_DS_INDEX_NOT_EXIST               (0x00000040)
#define MPI2_SAS_EXPANDER0_DS_OUT_ROUTE_ENTRIES             (0x00000020)
#define MPI2_SAS_EXPANDER0_DS_SMP_TIMEOUT                   (0x00000010)
#define MPI2_SAS_EXPANDER0_DS_MULTIPLE_PORTS                (0x00000004)
#define MPI2_SAS_EXPANDER0_DS_UNADDRESSABLE_DEVICE          (0x00000002)
#define MPI2_SAS_EXPANDER0_DS_LOOP_DETECTED                 (0x00000001)
#define MPI2_SAS_EXPANDER0_FLAGS_REDUCED_FUNCTIONALITY      (0x2000)
#define MPI2_SAS_EXPANDER0_FLAGS_ZONE_LOCKED                (0x1000)
#define MPI2_SAS_EXPANDER0_FLAGS_SUPPORTED_PHYSICAL_PRES    (0x0800)
#define MPI2_SAS_EXPANDER0_FLAGS_ASSERTED_PHYSICAL_PRES     (0x0400)
#define MPI2_SAS_EXPANDER0_FLAGS_ZONING_SUPPORT             (0x0200)
#define MPI2_SAS_EXPANDER0_FLAGS_ENABLED_ZONING             (0x0100)
#define MPI2_SAS_EXPANDER0_FLAGS_TABLE_TO_TABLE_SUPPORT     (0x0080)
#define MPI2_SAS_EXPANDER0_FLAGS_CONNECTOR_END_DEVICE       (0x0010)
#define MPI2_SAS_EXPANDER0_FLAGS_OTHERS_CONFIG              (0x0004)
#define MPI2_SAS_EXPANDER0_FLAGS_CONFIG_IN_PROGRESS         (0x0002)
#define MPI2_SAS_EXPANDER0_FLAGS_ROUTE_TABLE_CONFIG         (0x0001)
typedef struct _MPI2_CONFIG_PAGE_EXPANDER_1 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER
		Header;                      
	U8
		PhysicalPort;                
	U8
		Reserved1;                   
	U16
		Reserved2;                   
	U8
		NumPhys;                     
	U8
		Phy;                         
	U16
		NumTableEntriesProgrammed;   
	U8
		ProgrammedLinkRate;          
	U8
		HwLinkRate;                  
	U16
		AttachedDevHandle;           
	U32
		PhyInfo;                     
	U32
		AttachedDeviceInfo;          
	U16
		ExpanderDevHandle;           
	U8
		ChangeCount;                 
	U8
		NegotiatedLinkRate;          
	U8
		PhyIdentifier;               
	U8
		AttachedPhyIdentifier;       
	U8
		Reserved3;                   
	U8
		DiscoveryInfo;               
	U32
		AttachedPhyInfo;             
	U8
		ZoneGroup;                   
	U8
		SelfConfigStatus;            
	U16
		Reserved4;                   
} MPI2_CONFIG_PAGE_EXPANDER_1,
	*PTR_MPI2_CONFIG_PAGE_EXPANDER_1,
	Mpi2ExpanderPage1_t, *pMpi2ExpanderPage1_t;
#define MPI2_SASEXPANDER1_PAGEVERSION       (0x02)
#define MPI2_SAS_EXPANDER1_DISCINFO_BAD_PHY_DISABLED    (0x04)
#define MPI2_SAS_EXPANDER1_DISCINFO_LINK_STATUS_CHANGE  (0x02)
#define MPI2_SAS_EXPANDER1_DISCINFO_NO_ROUTING_ENTRIES  (0x01)
typedef struct _MPI2_CONFIG_PAGE_SAS_DEV_0 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER
		Header;                  
	U16
		Slot;                    
	U16
		EnclosureHandle;         
	U64
		SASAddress;              
	U16
		ParentDevHandle;         
	U8
		PhyNum;                  
	U8
		AccessStatus;            
	U16
		DevHandle;               
	U8
		AttachedPhyIdentifier;   
	U8
		ZoneGroup;               
	U32
		DeviceInfo;              
	U16
		Flags;                   
	U8
		PhysicalPort;            
	U8
		MaxPortConnections;      
	U64
		DeviceName;              
	U8
		PortGroups;              
	U8
		DmaGroup;                
	U8
		ControlGroup;            
	U8
		EnclosureLevel;		 
	U32
		ConnectorName[4];	 
	U32
		Reserved3;               
} MPI2_CONFIG_PAGE_SAS_DEV_0,
	*PTR_MPI2_CONFIG_PAGE_SAS_DEV_0,
	Mpi2SasDevicePage0_t,
	*pMpi2SasDevicePage0_t;
#define MPI2_SASDEVICE0_PAGEVERSION         (0x09)
#define MPI2_SAS_DEVICE0_ASTATUS_NO_ERRORS                  (0x00)
#define MPI2_SAS_DEVICE0_ASTATUS_SATA_INIT_FAILED           (0x01)
#define MPI2_SAS_DEVICE0_ASTATUS_SATA_CAPABILITY_FAILED     (0x02)
#define MPI2_SAS_DEVICE0_ASTATUS_SATA_AFFILIATION_CONFLICT  (0x03)
#define MPI2_SAS_DEVICE0_ASTATUS_SATA_NEEDS_INITIALIZATION  (0x04)
#define MPI2_SAS_DEVICE0_ASTATUS_ROUTE_NOT_ADDRESSABLE      (0x05)
#define MPI2_SAS_DEVICE0_ASTATUS_SMP_ERROR_NOT_ADDRESSABLE  (0x06)
#define MPI2_SAS_DEVICE0_ASTATUS_DEVICE_BLOCKED             (0x07)
#define MPI2_SAS_DEVICE0_ASTATUS_SIF_UNKNOWN                (0x10)
#define MPI2_SAS_DEVICE0_ASTATUS_SIF_AFFILIATION_CONFLICT   (0x11)
#define MPI2_SAS_DEVICE0_ASTATUS_SIF_DIAG                   (0x12)
#define MPI2_SAS_DEVICE0_ASTATUS_SIF_IDENTIFICATION         (0x13)
#define MPI2_SAS_DEVICE0_ASTATUS_SIF_CHECK_POWER            (0x14)
#define MPI2_SAS_DEVICE0_ASTATUS_SIF_PIO_SN                 (0x15)
#define MPI2_SAS_DEVICE0_ASTATUS_SIF_MDMA_SN                (0x16)
#define MPI2_SAS_DEVICE0_ASTATUS_SIF_UDMA_SN                (0x17)
#define MPI2_SAS_DEVICE0_ASTATUS_SIF_ZONING_VIOLATION       (0x18)
#define MPI2_SAS_DEVICE0_ASTATUS_SIF_NOT_ADDRESSABLE        (0x19)
#define MPI2_SAS_DEVICE0_ASTATUS_SIF_MAX                    (0x1F)
#define MPI2_SAS_DEVICE0_FLAGS_UNAUTHORIZED_DEVICE          (0x8000)
#define MPI25_SAS_DEVICE0_FLAGS_ENABLED_FAST_PATH           (0x4000)
#define MPI25_SAS_DEVICE0_FLAGS_FAST_PATH_CAPABLE           (0x2000)
#define MPI2_SAS_DEVICE0_FLAGS_SLUMBER_PM_CAPABLE           (0x1000)
#define MPI2_SAS_DEVICE0_FLAGS_PARTIAL_PM_CAPABLE           (0x0800)
#define MPI2_SAS_DEVICE0_FLAGS_SATA_ASYNCHRONOUS_NOTIFY     (0x0400)
#define MPI2_SAS_DEVICE0_FLAGS_SATA_SW_PRESERVE             (0x0200)
#define MPI2_SAS_DEVICE0_FLAGS_UNSUPPORTED_DEVICE           (0x0100)
#define MPI2_SAS_DEVICE0_FLAGS_SATA_48BIT_LBA_SUPPORTED     (0x0080)
#define MPI2_SAS_DEVICE0_FLAGS_SATA_SMART_SUPPORTED         (0x0040)
#define MPI2_SAS_DEVICE0_FLAGS_SATA_NCQ_SUPPORTED           (0x0020)
#define MPI2_SAS_DEVICE0_FLAGS_SATA_FUA_SUPPORTED           (0x0010)
#define MPI2_SAS_DEVICE0_FLAGS_PORT_SELECTOR_ATTACH         (0x0008)
#define MPI2_SAS_DEVICE0_FLAGS_PERSIST_CAPABLE              (0x0004)
#define MPI2_SAS_DEVICE0_FLAGS_ENCL_LEVEL_VALID             (0x0002)
#define MPI2_SAS_DEVICE0_FLAGS_DEVICE_PRESENT               (0x0001)
typedef struct _MPI2_CONFIG_PAGE_SAS_DEV_1 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER
		Header;                  
	U32
		Reserved1;               
	U64
		SASAddress;              
	U32
		Reserved2;               
	U16
		DevHandle;               
	U16
		Reserved3;               
	U8
		InitialRegDeviceFIS[20]; 
} MPI2_CONFIG_PAGE_SAS_DEV_1,
	*PTR_MPI2_CONFIG_PAGE_SAS_DEV_1,
	Mpi2SasDevicePage1_t,
	*pMpi2SasDevicePage1_t;
#define MPI2_SASDEVICE1_PAGEVERSION         (0x01)
typedef struct _MPI2_CONFIG_PAGE_SAS_PHY_0 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER
		Header;                  
	U16
		OwnerDevHandle;          
	U16
		Reserved1;               
	U16
		AttachedDevHandle;       
	U8
		AttachedPhyIdentifier;   
	U8
		Reserved2;               
	U32
		AttachedPhyInfo;         
	U8
		ProgrammedLinkRate;      
	U8
		HwLinkRate;              
	U8
		ChangeCount;             
	U8
		Flags;                   
	U32
		PhyInfo;                 
	U8
		NegotiatedLinkRate;      
	U8
		Reserved3;               
	U16
		Reserved4;               
} MPI2_CONFIG_PAGE_SAS_PHY_0,
	*PTR_MPI2_CONFIG_PAGE_SAS_PHY_0,
	Mpi2SasPhyPage0_t, *pMpi2SasPhyPage0_t;
#define MPI2_SASPHY0_PAGEVERSION            (0x03)
#define MPI2_SAS_PHY0_FLAGS_SGPIO_DIRECT_ATTACH_ENC             (0x01)
typedef struct _MPI2_CONFIG_PAGE_SAS_PHY_1 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER
		Header;                      
	U32
		Reserved1;                   
	U32
		InvalidDwordCount;           
	U32
		RunningDisparityErrorCount;  
	U32
		LossDwordSynchCount;         
	U32
		PhyResetProblemCount;        
} MPI2_CONFIG_PAGE_SAS_PHY_1,
	*PTR_MPI2_CONFIG_PAGE_SAS_PHY_1,
	Mpi2SasPhyPage1_t, *pMpi2SasPhyPage1_t;
#define MPI2_SASPHY1_PAGEVERSION            (0x01)
typedef struct _MPI2_SASPHY2_PHY_EVENT {
	U8          PhyEventCode;        
	U8          Reserved1;           
	U16         Reserved2;           
	U32         PhyEventInfo;        
} MPI2_SASPHY2_PHY_EVENT, *PTR_MPI2_SASPHY2_PHY_EVENT,
	Mpi2SasPhy2PhyEvent_t, *pMpi2SasPhy2PhyEvent_t;
#ifndef MPI2_SASPHY2_PHY_EVENT_MAX
#define MPI2_SASPHY2_PHY_EVENT_MAX      (1)
#endif
typedef struct _MPI2_CONFIG_PAGE_SAS_PHY_2 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER
		Header;                      
	U32
		Reserved1;                   
	U8
		NumPhyEvents;                
	U8
		Reserved2;                   
	U16
		Reserved3;                   
	MPI2_SASPHY2_PHY_EVENT
		PhyEvent[MPI2_SASPHY2_PHY_EVENT_MAX];  
} MPI2_CONFIG_PAGE_SAS_PHY_2,
	*PTR_MPI2_CONFIG_PAGE_SAS_PHY_2,
	Mpi2SasPhyPage2_t,
	*pMpi2SasPhyPage2_t;
#define MPI2_SASPHY2_PAGEVERSION            (0x00)
typedef struct _MPI2_SASPHY3_PHY_EVENT_CONFIG {
	U8          PhyEventCode;        
	U8          Reserved1;           
	U16         Reserved2;           
	U8          CounterType;         
	U8          ThresholdWindow;     
	U8          TimeUnits;           
	U8          Reserved3;           
	U32         EventThreshold;      
	U16         ThresholdFlags;      
	U16         Reserved4;           
} MPI2_SASPHY3_PHY_EVENT_CONFIG,
	*PTR_MPI2_SASPHY3_PHY_EVENT_CONFIG,
	Mpi2SasPhy3PhyEventConfig_t,
	*pMpi2SasPhy3PhyEventConfig_t;
#define MPI2_SASPHY3_EVENT_CODE_NO_EVENT                    (0x00)
#define MPI2_SASPHY3_EVENT_CODE_INVALID_DWORD               (0x01)
#define MPI2_SASPHY3_EVENT_CODE_RUNNING_DISPARITY_ERROR     (0x02)
#define MPI2_SASPHY3_EVENT_CODE_LOSS_DWORD_SYNC             (0x03)
#define MPI2_SASPHY3_EVENT_CODE_PHY_RESET_PROBLEM           (0x04)
#define MPI2_SASPHY3_EVENT_CODE_ELASTICITY_BUF_OVERFLOW     (0x05)
#define MPI2_SASPHY3_EVENT_CODE_RX_ERROR                    (0x06)
#define MPI2_SASPHY3_EVENT_CODE_RX_ADDR_FRAME_ERROR         (0x20)
#define MPI2_SASPHY3_EVENT_CODE_TX_AC_OPEN_REJECT           (0x21)
#define MPI2_SASPHY3_EVENT_CODE_RX_AC_OPEN_REJECT           (0x22)
#define MPI2_SASPHY3_EVENT_CODE_TX_RC_OPEN_REJECT           (0x23)
#define MPI2_SASPHY3_EVENT_CODE_RX_RC_OPEN_REJECT           (0x24)
#define MPI2_SASPHY3_EVENT_CODE_RX_AIP_PARTIAL_WAITING_ON   (0x25)
#define MPI2_SASPHY3_EVENT_CODE_RX_AIP_CONNECT_WAITING_ON   (0x26)
#define MPI2_SASPHY3_EVENT_CODE_TX_BREAK                    (0x27)
#define MPI2_SASPHY3_EVENT_CODE_RX_BREAK                    (0x28)
#define MPI2_SASPHY3_EVENT_CODE_BREAK_TIMEOUT               (0x29)
#define MPI2_SASPHY3_EVENT_CODE_CONNECTION                  (0x2A)
#define MPI2_SASPHY3_EVENT_CODE_PEAKTX_PATHWAY_BLOCKED      (0x2B)
#define MPI2_SASPHY3_EVENT_CODE_PEAKTX_ARB_WAIT_TIME        (0x2C)
#define MPI2_SASPHY3_EVENT_CODE_PEAK_ARB_WAIT_TIME          (0x2D)
#define MPI2_SASPHY3_EVENT_CODE_PEAK_CONNECT_TIME           (0x2E)
#define MPI2_SASPHY3_EVENT_CODE_TX_SSP_FRAMES               (0x40)
#define MPI2_SASPHY3_EVENT_CODE_RX_SSP_FRAMES               (0x41)
#define MPI2_SASPHY3_EVENT_CODE_TX_SSP_ERROR_FRAMES         (0x42)
#define MPI2_SASPHY3_EVENT_CODE_RX_SSP_ERROR_FRAMES         (0x43)
#define MPI2_SASPHY3_EVENT_CODE_TX_CREDIT_BLOCKED           (0x44)
#define MPI2_SASPHY3_EVENT_CODE_RX_CREDIT_BLOCKED           (0x45)
#define MPI2_SASPHY3_EVENT_CODE_TX_SATA_FRAMES              (0x50)
#define MPI2_SASPHY3_EVENT_CODE_RX_SATA_FRAMES              (0x51)
#define MPI2_SASPHY3_EVENT_CODE_SATA_OVERFLOW               (0x52)
#define MPI2_SASPHY3_EVENT_CODE_TX_SMP_FRAMES               (0x60)
#define MPI2_SASPHY3_EVENT_CODE_RX_SMP_FRAMES               (0x61)
#define MPI2_SASPHY3_EVENT_CODE_RX_SMP_ERROR_FRAMES         (0x63)
#define MPI2_SASPHY3_EVENT_CODE_HOTPLUG_TIMEOUT             (0xD0)
#define MPI2_SASPHY3_EVENT_CODE_MISALIGNED_MUX_PRIMITIVE    (0xD1)
#define MPI2_SASPHY3_EVENT_CODE_RX_AIP                      (0xD2)
#define MPI2_SASPHY3_EVENT_CODE_LCARB_WAIT_TIME		    (0xD3)
#define MPI2_SASPHY3_EVENT_CODE_RCVD_CONN_RESP_WAIT_TIME    (0xD4)
#define MPI2_SASPHY3_EVENT_CODE_LCCONN_TIME	            (0xD5)
#define MPI2_SASPHY3_EVENT_CODE_SSP_TX_START_TRANSMIT	    (0xD6)
#define MPI2_SASPHY3_EVENT_CODE_SATA_TX_START	            (0xD7)
#define MPI2_SASPHY3_EVENT_CODE_SMP_TX_START_TRANSMT	    (0xD8)
#define MPI2_SASPHY3_EVENT_CODE_TX_SMP_BREAK_CONN	    (0xD9)
#define MPI2_SASPHY3_EVENT_CODE_SSP_RX_START_RECEIVE	    (0xDA)
#define MPI2_SASPHY3_EVENT_CODE_SATA_RX_START_RECEIVE	    (0xDB)
#define MPI2_SASPHY3_EVENT_CODE_SMP_RX_START_RECEIVE	    (0xDC)
#define MPI2_SASPHY3_COUNTER_TYPE_WRAPPING                  (0x00)
#define MPI2_SASPHY3_COUNTER_TYPE_SATURATING                (0x01)
#define MPI2_SASPHY3_COUNTER_TYPE_PEAK_VALUE                (0x02)
#define MPI2_SASPHY3_TIME_UNITS_10_MICROSECONDS             (0x00)
#define MPI2_SASPHY3_TIME_UNITS_100_MICROSECONDS            (0x01)
#define MPI2_SASPHY3_TIME_UNITS_1_MILLISECOND               (0x02)
#define MPI2_SASPHY3_TIME_UNITS_10_MILLISECONDS             (0x03)
#define MPI2_SASPHY3_TFLAGS_PHY_RESET                       (0x0002)
#define MPI2_SASPHY3_TFLAGS_EVENT_NOTIFY                    (0x0001)
#ifndef MPI2_SASPHY3_PHY_EVENT_MAX
#define MPI2_SASPHY3_PHY_EVENT_MAX      (1)
#endif
typedef struct _MPI2_CONFIG_PAGE_SAS_PHY_3 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER
		Header;                      
	U32
		Reserved1;                   
	U8
		NumPhyEvents;                
	U8
		Reserved2;                   
	U16
		Reserved3;                   
	MPI2_SASPHY3_PHY_EVENT_CONFIG
		PhyEventConfig[MPI2_SASPHY3_PHY_EVENT_MAX];  
} MPI2_CONFIG_PAGE_SAS_PHY_3,
	*PTR_MPI2_CONFIG_PAGE_SAS_PHY_3,
	Mpi2SasPhyPage3_t, *pMpi2SasPhyPage3_t;
#define MPI2_SASPHY3_PAGEVERSION            (0x00)
typedef struct _MPI2_CONFIG_PAGE_SAS_PHY_4 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER
		Header;                      
	U16
		Reserved1;                   
	U8
		Reserved2;                   
	U8
		Flags;                       
	U8
		InitialFrame[28];            
} MPI2_CONFIG_PAGE_SAS_PHY_4,
	*PTR_MPI2_CONFIG_PAGE_SAS_PHY_4,
	Mpi2SasPhyPage4_t, *pMpi2SasPhyPage4_t;
#define MPI2_SASPHY4_PAGEVERSION            (0x00)
#define MPI2_SASPHY4_FLAGS_FRAME_VALID        (0x02)
#define MPI2_SASPHY4_FLAGS_SATA_FRAME         (0x01)
typedef struct _MPI2_CONFIG_PAGE_SAS_PORT_0 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER
		Header;                      
	U8
		PortNumber;                  
	U8
		PhysicalPort;                
	U8
		PortWidth;                   
	U8
		PhysicalPortWidth;           
	U8
		ZoneGroup;                   
	U8
		Reserved1;                   
	U16
		Reserved2;                   
	U64
		SASAddress;                  
	U32
		DeviceInfo;                  
	U32
		Reserved3;                   
	U32
		Reserved4;                   
} MPI2_CONFIG_PAGE_SAS_PORT_0,
	*PTR_MPI2_CONFIG_PAGE_SAS_PORT_0,
	Mpi2SasPortPage0_t, *pMpi2SasPortPage0_t;
#define MPI2_SASPORT0_PAGEVERSION           (0x00)
typedef struct _MPI2_CONFIG_PAGE_SAS_ENCLOSURE_0 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER	Header;	 
	U32	Reserved1;			 
	U64	EnclosureLogicalID;		 
	U16	Flags;				 
	U16	EnclosureHandle;		 
	U16	NumSlots;			 
	U16	StartSlot;			 
	U8	ChassisSlot;			 
	U8	EnclosureLevel;			 
	U16	SEPDevHandle;			 
	U8	OEMRD;				 
	U8	Reserved1a;			 
	U16	Reserved2;			 
	U32	Reserved3;			 
} MPI2_CONFIG_PAGE_SAS_ENCLOSURE_0,
	*PTR_MPI2_CONFIG_PAGE_SAS_ENCLOSURE_0,
	Mpi2SasEnclosurePage0_t, *pMpi2SasEnclosurePage0_t,
	MPI26_CONFIG_PAGE_ENCLOSURE_0,
	*PTR_MPI26_CONFIG_PAGE_ENCLOSURE_0,
	Mpi26EnclosurePage0_t, *pMpi26EnclosurePage0_t;
#define MPI2_SASENCLOSURE0_PAGEVERSION      (0x04)
#define MPI26_SAS_ENCLS0_FLAGS_OEMRD_VALID          (0x0080)
#define MPI26_SAS_ENCLS0_FLAGS_OEMRD_COLLECTING     (0x0040)
#define MPI2_SAS_ENCLS0_FLAGS_CHASSIS_SLOT_VALID    (0x0020)
#define MPI2_SAS_ENCLS0_FLAGS_ENCL_LEVEL_VALID      (0x0010)
#define MPI2_SAS_ENCLS0_FLAGS_MNG_MASK              (0x000F)
#define MPI2_SAS_ENCLS0_FLAGS_MNG_UNKNOWN           (0x0000)
#define MPI2_SAS_ENCLS0_FLAGS_MNG_IOC_SES           (0x0001)
#define MPI2_SAS_ENCLS0_FLAGS_MNG_IOC_SGPIO         (0x0002)
#define MPI2_SAS_ENCLS0_FLAGS_MNG_EXP_SGPIO         (0x0003)
#define MPI2_SAS_ENCLS0_FLAGS_MNG_SES_ENCLOSURE     (0x0004)
#define MPI2_SAS_ENCLS0_FLAGS_MNG_IOC_GPIO          (0x0005)
#define MPI26_ENCLOSURE0_PAGEVERSION        (0x04)
#define MPI26_ENCLS0_FLAGS_OEMRD_VALID              (0x0080)
#define MPI26_ENCLS0_FLAGS_OEMRD_COLLECTING         (0x0040)
#define MPI26_ENCLS0_FLAGS_CHASSIS_SLOT_VALID       (0x0020)
#define MPI26_ENCLS0_FLAGS_ENCL_LEVEL_VALID         (0x0010)
#define MPI26_ENCLS0_FLAGS_MNG_MASK                 (0x000F)
#define MPI26_ENCLS0_FLAGS_MNG_UNKNOWN              (0x0000)
#define MPI26_ENCLS0_FLAGS_MNG_IOC_SES              (0x0001)
#define MPI26_ENCLS0_FLAGS_MNG_IOC_SGPIO            (0x0002)
#define MPI26_ENCLS0_FLAGS_MNG_EXP_SGPIO            (0x0003)
#define MPI26_ENCLS0_FLAGS_MNG_SES_ENCLOSURE        (0x0004)
#define MPI26_ENCLS0_FLAGS_MNG_IOC_GPIO             (0x0005)
#ifndef MPI2_LOG_0_NUM_LOG_ENTRIES
#define MPI2_LOG_0_NUM_LOG_ENTRIES          (1)
#endif
#define MPI2_LOG_0_LOG_DATA_LENGTH          (0x1C)
typedef struct _MPI2_LOG_0_ENTRY {
	U64         TimeStamp;                       
	U32         Reserved1;                       
	U16         LogSequence;                     
	U16         LogEntryQualifier;               
	U8          VP_ID;                           
	U8          VF_ID;                           
	U16         Reserved2;                       
	U8
		LogData[MPI2_LOG_0_LOG_DATA_LENGTH]; 
} MPI2_LOG_0_ENTRY, *PTR_MPI2_LOG_0_ENTRY,
	Mpi2Log0Entry_t, *pMpi2Log0Entry_t;
#define MPI2_LOG_0_ENTRY_QUAL_ENTRY_UNUSED          (0x0000)
#define MPI2_LOG_0_ENTRY_QUAL_POWER_ON_RESET        (0x0001)
#define MPI2_LOG_0_ENTRY_QUAL_TIMESTAMP_UPDATE      (0x0002)
#define MPI2_LOG_0_ENTRY_QUAL_MIN_IMPLEMENT_SPEC    (0x8000)
#define MPI2_LOG_0_ENTRY_QUAL_MAX_IMPLEMENT_SPEC    (0xFFFF)
typedef struct _MPI2_CONFIG_PAGE_LOG_0 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER    Header;        
	U32                                 Reserved1;     
	U32                                 Reserved2;     
	U16                                 NumLogEntries; 
	U16                                 Reserved3;     
	MPI2_LOG_0_ENTRY
		LogEntry[MPI2_LOG_0_NUM_LOG_ENTRIES];  
} MPI2_CONFIG_PAGE_LOG_0, *PTR_MPI2_CONFIG_PAGE_LOG_0,
	Mpi2LogPage0_t, *pMpi2LogPage0_t;
#define MPI2_LOG_0_PAGEVERSION              (0x02)
#ifndef MPI2_RAIDCONFIG0_MAX_ELEMENTS
#define MPI2_RAIDCONFIG0_MAX_ELEMENTS       (1)
#endif
typedef struct _MPI2_RAIDCONFIG0_CONFIG_ELEMENT {
	U16                     ElementFlags;              
	U16                     VolDevHandle;              
	U8                      HotSparePool;              
	U8                      PhysDiskNum;               
	U16                     PhysDiskDevHandle;         
} MPI2_RAIDCONFIG0_CONFIG_ELEMENT,
	*PTR_MPI2_RAIDCONFIG0_CONFIG_ELEMENT,
	Mpi2RaidConfig0ConfigElement_t,
	*pMpi2RaidConfig0ConfigElement_t;
#define MPI2_RAIDCONFIG0_EFLAGS_MASK_ELEMENT_TYPE       (0x000F)
#define MPI2_RAIDCONFIG0_EFLAGS_VOLUME_ELEMENT          (0x0000)
#define MPI2_RAIDCONFIG0_EFLAGS_VOL_PHYS_DISK_ELEMENT   (0x0001)
#define MPI2_RAIDCONFIG0_EFLAGS_HOT_SPARE_ELEMENT       (0x0002)
#define MPI2_RAIDCONFIG0_EFLAGS_OCE_ELEMENT             (0x0003)
typedef struct _MPI2_CONFIG_PAGE_RAID_CONFIGURATION_0 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER    Header;          
	U8                                  NumHotSpares;    
	U8                                  NumPhysDisks;    
	U8                                  NumVolumes;      
	U8                                  ConfigNum;       
	U32                                 Flags;           
	U8                                  ConfigGUID[24];  
	U32                                 Reserved1;       
	U8                                  NumElements;     
	U8                                  Reserved2;       
	U16                                 Reserved3;       
	MPI2_RAIDCONFIG0_CONFIG_ELEMENT
		ConfigElement[MPI2_RAIDCONFIG0_MAX_ELEMENTS];  
} MPI2_CONFIG_PAGE_RAID_CONFIGURATION_0,
	*PTR_MPI2_CONFIG_PAGE_RAID_CONFIGURATION_0,
	Mpi2RaidConfigurationPage0_t,
	*pMpi2RaidConfigurationPage0_t;
#define MPI2_RAIDCONFIG0_PAGEVERSION            (0x00)
#define MPI2_RAIDCONFIG0_FLAG_FOREIGN_CONFIG        (0x00000001)
typedef struct _MPI2_CONFIG_PAGE_DRIVER_MAP0_ENTRY {
	U64	PhysicalIdentifier;          
	U16	MappingInformation;          
	U16	DeviceIndex;                 
	U32	PhysicalBitsMapping;         
	U32	Reserved1;                   
} MPI2_CONFIG_PAGE_DRIVER_MAP0_ENTRY,
	*PTR_MPI2_CONFIG_PAGE_DRIVER_MAP0_ENTRY,
	Mpi2DriverMap0Entry_t, *pMpi2DriverMap0Entry_t;
typedef struct _MPI2_CONFIG_PAGE_DRIVER_MAPPING_0 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER    Header;  
	MPI2_CONFIG_PAGE_DRIVER_MAP0_ENTRY  Entry;   
} MPI2_CONFIG_PAGE_DRIVER_MAPPING_0,
	*PTR_MPI2_CONFIG_PAGE_DRIVER_MAPPING_0,
	Mpi2DriverMappingPage0_t, *pMpi2DriverMappingPage0_t;
#define MPI2_DRIVERMAPPING0_PAGEVERSION         (0x00)
#define MPI2_DRVMAP0_MAPINFO_SLOT_MASK              (0x07F0)
#define MPI2_DRVMAP0_MAPINFO_SLOT_SHIFT             (4)
#define MPI2_DRVMAP0_MAPINFO_MISSING_MASK           (0x000F)
typedef union _MPI2_ETHERNET_IP_ADDR {
	U32     IPv4Addr;
	U32     IPv6Addr[4];
} MPI2_ETHERNET_IP_ADDR, *PTR_MPI2_ETHERNET_IP_ADDR,
	Mpi2EthernetIpAddr_t, *pMpi2EthernetIpAddr_t;
#define MPI2_ETHERNET_HOST_NAME_LENGTH          (32)
typedef struct _MPI2_CONFIG_PAGE_ETHERNET_0 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER    Header;           
	U8                                  NumInterfaces;    
	U8                                  Reserved0;        
	U16                                 Reserved1;        
	U32                                 Status;           
	U8                                  MediaState;       
	U8                                  Reserved2;        
	U16                                 Reserved3;        
	U8                                  MacAddress[6];    
	U8                                  Reserved4;        
	U8                                  Reserved5;        
	MPI2_ETHERNET_IP_ADDR               IpAddress;        
	MPI2_ETHERNET_IP_ADDR               SubnetMask;       
	MPI2_ETHERNET_IP_ADDR               GatewayIpAddress; 
	MPI2_ETHERNET_IP_ADDR               DNS1IpAddress;    
	MPI2_ETHERNET_IP_ADDR               DNS2IpAddress;    
	MPI2_ETHERNET_IP_ADDR               DhcpIpAddress;    
	U8
		HostName[MPI2_ETHERNET_HOST_NAME_LENGTH]; 
} MPI2_CONFIG_PAGE_ETHERNET_0,
	*PTR_MPI2_CONFIG_PAGE_ETHERNET_0,
	Mpi2EthernetPage0_t, *pMpi2EthernetPage0_t;
#define MPI2_ETHERNETPAGE0_PAGEVERSION   (0x00)
#define MPI2_ETHPG0_STATUS_IPV6_CAPABLE             (0x80000000)
#define MPI2_ETHPG0_STATUS_IPV4_CAPABLE             (0x40000000)
#define MPI2_ETHPG0_STATUS_CONSOLE_CONNECTED        (0x20000000)
#define MPI2_ETHPG0_STATUS_DEFAULT_IF               (0x00000100)
#define MPI2_ETHPG0_STATUS_FW_DWNLD_ENABLED         (0x00000080)
#define MPI2_ETHPG0_STATUS_TELNET_ENABLED           (0x00000040)
#define MPI2_ETHPG0_STATUS_SSH2_ENABLED             (0x00000020)
#define MPI2_ETHPG0_STATUS_DHCP_CLIENT_ENABLED      (0x00000010)
#define MPI2_ETHPG0_STATUS_IPV6_ENABLED             (0x00000008)
#define MPI2_ETHPG0_STATUS_IPV4_ENABLED             (0x00000004)
#define MPI2_ETHPG0_STATUS_IPV6_ADDRESSES           (0x00000002)
#define MPI2_ETHPG0_STATUS_ETH_IF_ENABLED           (0x00000001)
#define MPI2_ETHPG0_MS_DUPLEX_MASK                  (0x80)
#define MPI2_ETHPG0_MS_HALF_DUPLEX                  (0x00)
#define MPI2_ETHPG0_MS_FULL_DUPLEX                  (0x80)
#define MPI2_ETHPG0_MS_CONNECT_SPEED_MASK           (0x07)
#define MPI2_ETHPG0_MS_NOT_CONNECTED                (0x00)
#define MPI2_ETHPG0_MS_10MBIT                       (0x01)
#define MPI2_ETHPG0_MS_100MBIT                      (0x02)
#define MPI2_ETHPG0_MS_1GBIT                        (0x03)
typedef struct _MPI2_CONFIG_PAGE_ETHERNET_1 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER
		Header;                  
	U32
		Reserved0;               
	U32
		Flags;                   
	U8
		MediaState;              
	U8
		Reserved1;               
	U16
		Reserved2;               
	U8
		MacAddress[6];           
	U8
		Reserved3;               
	U8
		Reserved4;               
	MPI2_ETHERNET_IP_ADDR
		StaticIpAddress;         
	MPI2_ETHERNET_IP_ADDR
		StaticSubnetMask;        
	MPI2_ETHERNET_IP_ADDR
		StaticGatewayIpAddress;  
	MPI2_ETHERNET_IP_ADDR
		StaticDNS1IpAddress;     
	MPI2_ETHERNET_IP_ADDR
		StaticDNS2IpAddress;     
	U32
		Reserved5;               
	U32
		Reserved6;               
	U32
		Reserved7;               
	U32
		Reserved8;               
	U8
		HostName[MPI2_ETHERNET_HOST_NAME_LENGTH]; 
} MPI2_CONFIG_PAGE_ETHERNET_1,
	*PTR_MPI2_CONFIG_PAGE_ETHERNET_1,
	Mpi2EthernetPage1_t, *pMpi2EthernetPage1_t;
#define MPI2_ETHERNETPAGE1_PAGEVERSION   (0x00)
#define MPI2_ETHPG1_FLAG_SET_DEFAULT_IF             (0x00000100)
#define MPI2_ETHPG1_FLAG_ENABLE_FW_DOWNLOAD         (0x00000080)
#define MPI2_ETHPG1_FLAG_ENABLE_TELNET              (0x00000040)
#define MPI2_ETHPG1_FLAG_ENABLE_SSH2                (0x00000020)
#define MPI2_ETHPG1_FLAG_ENABLE_DHCP_CLIENT         (0x00000010)
#define MPI2_ETHPG1_FLAG_ENABLE_IPV6                (0x00000008)
#define MPI2_ETHPG1_FLAG_ENABLE_IPV4                (0x00000004)
#define MPI2_ETHPG1_FLAG_USE_IPV6_ADDRESSES         (0x00000002)
#define MPI2_ETHPG1_FLAG_ENABLE_ETH_IF              (0x00000001)
#define MPI2_ETHPG1_MS_DUPLEX_MASK                  (0x80)
#define MPI2_ETHPG1_MS_HALF_DUPLEX                  (0x00)
#define MPI2_ETHPG1_MS_FULL_DUPLEX                  (0x80)
#define MPI2_ETHPG1_MS_DATA_RATE_MASK               (0x07)
#define MPI2_ETHPG1_MS_DATA_RATE_AUTO               (0x00)
#define MPI2_ETHPG1_MS_DATA_RATE_10MBIT             (0x01)
#define MPI2_ETHPG1_MS_DATA_RATE_100MBIT            (0x02)
#define MPI2_ETHPG1_MS_DATA_RATE_1GBIT              (0x03)
typedef struct _MPI2_CONFIG_PAGE_EXT_MAN_PS {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER
		Header;                  
	U32
		ProductSpecificInfo;     
} MPI2_CONFIG_PAGE_EXT_MAN_PS,
	*PTR_MPI2_CONFIG_PAGE_EXT_MAN_PS,
	Mpi2ExtManufacturingPagePS_t,
	*pMpi2ExtManufacturingPagePS_t;
#define MPI26_PCIE_NEG_LINK_RATE_MASK_PHYSICAL          (0x0F)
#define MPI26_PCIE_NEG_LINK_RATE_UNKNOWN                (0x00)
#define MPI26_PCIE_NEG_LINK_RATE_PHY_DISABLED           (0x01)
#define MPI26_PCIE_NEG_LINK_RATE_2_5                    (0x02)
#define MPI26_PCIE_NEG_LINK_RATE_5_0                    (0x03)
#define MPI26_PCIE_NEG_LINK_RATE_8_0                    (0x04)
#define MPI26_PCIE_NEG_LINK_RATE_16_0                   (0x05)
typedef struct _MPI26_PCIE_IO_UNIT0_PHY_DATA {
	U8	Link;                    
	U8	LinkFlags;               
	U8	PhyFlags;                
	U8	NegotiatedLinkRate;      
	U32	ControllerPhyDeviceInfo; 
	U16	AttachedDevHandle;       
	U16	ControllerDevHandle;     
	U32	EnumerationStatus;       
	U32	Reserved1;               
} MPI26_PCIE_IO_UNIT0_PHY_DATA,
	*PTR_MPI26_PCIE_IO_UNIT0_PHY_DATA,
	Mpi26PCIeIOUnit0PhyData_t, *pMpi26PCIeIOUnit0PhyData_t;
#ifndef MPI26_PCIE_IOUNIT0_PHY_MAX
#define MPI26_PCIE_IOUNIT0_PHY_MAX      (1)
#endif
typedef struct _MPI26_CONFIG_PAGE_PIOUNIT_0 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER	Header;  
	U32	Reserved1;                               
	U8	NumPhys;                                 
	U8	InitStatus;                              
	U16	Reserved3;                               
	MPI26_PCIE_IO_UNIT0_PHY_DATA
		PhyData[MPI26_PCIE_IOUNIT0_PHY_MAX];     
} MPI26_CONFIG_PAGE_PIOUNIT_0,
	*PTR_MPI26_CONFIG_PAGE_PIOUNIT_0,
	Mpi26PCIeIOUnitPage0_t, *pMpi26PCIeIOUnitPage0_t;
#define MPI26_PCIEIOUNITPAGE0_PAGEVERSION                   (0x00)
#define MPI26_PCIEIOUNIT0_LINKFLAGS_ENUMERATION_IN_PROGRESS (0x08)
#define MPI26_PCIEIOUNIT0_PHYFLAGS_PHY_DISABLED             (0x08)
#define MPI26_PCIEIOUNIT0_ES_MAX_SWITCHES_EXCEEDED          (0x40000000)
#define MPI26_PCIEIOUNIT0_ES_MAX_DEVICES_EXCEEDED           (0x20000000)
typedef struct _MPI26_PCIE_IO_UNIT1_PHY_DATA {
	U8	Link;                        
	U8	LinkFlags;                   
	U8	PhyFlags;                    
	U8	MaxMinLinkRate;              
	U32	ControllerPhyDeviceInfo;     
	U32	Reserved1;                   
} MPI26_PCIE_IO_UNIT1_PHY_DATA,
	*PTR_MPI26_PCIE_IO_UNIT1_PHY_DATA,
	Mpi26PCIeIOUnit1PhyData_t, *pMpi26PCIeIOUnit1PhyData_t;
#define MPI26_PCIEIOUNIT1_LINKFLAGS_DIS_SEPARATE_REFCLK     (0x00)
#define MPI26_PCIEIOUNIT1_LINKFLAGS_SRIS_EN                 (0x01)
#define MPI26_PCIEIOUNIT1_LINKFLAGS_SRNS_EN                 (0x02)
#ifndef MPI26_PCIE_IOUNIT1_PHY_MAX
#define MPI26_PCIE_IOUNIT1_PHY_MAX      (1)
#endif
typedef struct _MPI26_CONFIG_PAGE_PIOUNIT_1 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER	Header;	 
	U16	ControlFlags;                        
	U16	Reserved;                            
	U16	AdditionalControlFlags;              
	U16	NVMeMaxQueueDepth;                   
	U8	NumPhys;                             
	U8	DMDReportPCIe;                       
	U16	Reserved2;                           
	MPI26_PCIE_IO_UNIT1_PHY_DATA
		PhyData[MPI26_PCIE_IOUNIT1_PHY_MAX]; 
} MPI26_CONFIG_PAGE_PIOUNIT_1,
	*PTR_MPI26_CONFIG_PAGE_PIOUNIT_1,
	Mpi26PCIeIOUnitPage1_t, *pMpi26PCIeIOUnitPage1_t;
#define MPI26_PCIEIOUNITPAGE1_PAGEVERSION   (0x00)
#define MPI26_PCIEIOUNIT1_PHYFLAGS_PHY_DISABLE                      (0x08)
#define MPI26_PCIEIOUNIT1_PHYFLAGS_ENDPOINT_ONLY                    (0x01)
#define MPI26_PCIEIOUNIT1_MAX_RATE_MASK                             (0xF0)
#define MPI26_PCIEIOUNIT1_MAX_RATE_SHIFT                            (4)
#define MPI26_PCIEIOUNIT1_MAX_RATE_2_5                              (0x20)
#define MPI26_PCIEIOUNIT1_MAX_RATE_5_0                              (0x30)
#define MPI26_PCIEIOUNIT1_MAX_RATE_8_0                              (0x40)
#define MPI26_PCIEIOUNIT1_MAX_RATE_16_0                             (0x50)
#define MPI26_PCIEIOUNIT1_DMDRPT_UNIT_MASK                          (0x80)
#define MPI26_PCIEIOUNIT1_DMDRPT_UNIT_1_SEC                         (0x00)
#define MPI26_PCIEIOUNIT1_DMDRPT_UNIT_16_SEC                        (0x80)
#define MPI26_PCIEIOUNIT1_DMDRPT_DELAY_TIME_MASK                    (0x7F)
typedef struct _MPI26_CONFIG_PAGE_PSWITCH_0 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER	Header;	 
	U8	PhysicalPort;                
	U8	Reserved1;                   
	U16	Reserved2;                   
	U16	DevHandle;                   
	U16	ParentDevHandle;             
	U8	NumPorts;                    
	U8	PCIeLevel;                   
	U16	Reserved3;                   
	U32	Reserved4;                   
	U32	Reserved5;                   
	U32	Reserved6;                   
} MPI26_CONFIG_PAGE_PSWITCH_0, *PTR_MPI26_CONFIG_PAGE_PSWITCH_0,
	Mpi26PCIeSwitchPage0_t, *pMpi26PCIeSwitchPage0_t;
#define MPI26_PCIESWITCH0_PAGEVERSION       (0x00)
typedef struct _MPI26_CONFIG_PAGE_PSWITCH_1 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER	Header;	 
	U8	PhysicalPort;                
	U8	Reserved1;                   
	U16	Reserved2;                   
	U8	NumPorts;                    
	U8	PortNum;                     
	U16	AttachedDevHandle;           
	U16	SwitchDevHandle;             
	U8	NegotiatedPortWidth;         
	U8	NegotiatedLinkRate;          
	U32	Reserved4;                   
	U32	Reserved5;                   
} MPI26_CONFIG_PAGE_PSWITCH_1, *PTR_MPI26_CONFIG_PAGE_PSWITCH_1,
	Mpi26PCIeSwitchPage1_t, *pMpi26PCIeSwitchPage1_t;
#define MPI26_PCIESWITCH1_PAGEVERSION       (0x00)
#define MPI26_PCIESWITCH1_2_RETIMER_PRESENCE         (0x0002)
#define MPI26_PCIESWITCH1_RETIMER_PRESENCE           (0x0001)
typedef struct _MPI26_CONFIG_PAGE_PCIEDEV_0 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER	Header;	 
	U16	Slot;                    
	U16	EnclosureHandle;         
	U64	WWID;                    
	U16	ParentDevHandle;         
	U8	PortNum;                 
	U8	AccessStatus;            
	U16	DevHandle;               
	U8	PhysicalPort;            
	U8	Reserved1;               
	U32	DeviceInfo;              
	U32	Flags;                   
	U8	SupportedLinkRates;      
	U8	MaxPortWidth;            
	U8	NegotiatedPortWidth;     
	U8	NegotiatedLinkRate;      
	U8	EnclosureLevel;          
	U8	Reserved2;               
	U16	Reserved3;               
	U8	ConnectorName[4];        
	U32	Reserved4;               
	U32	Reserved5;               
} MPI26_CONFIG_PAGE_PCIEDEV_0, *PTR_MPI26_CONFIG_PAGE_PCIEDEV_0,
	Mpi26PCIeDevicePage0_t, *pMpi26PCIeDevicePage0_t;
#define MPI26_PCIEDEVICE0_PAGEVERSION       (0x01)
#define MPI26_PCIEDEV0_ASTATUS_NO_ERRORS                    (0x00)
#define MPI26_PCIEDEV0_ASTATUS_NEEDS_INITIALIZATION         (0x04)
#define MPI26_PCIEDEV0_ASTATUS_CAPABILITY_FAILED            (0x02)
#define MPI26_PCIEDEV0_ASTATUS_DEVICE_BLOCKED               (0x07)
#define MPI26_PCIEDEV0_ASTATUS_MEMORY_SPACE_ACCESS_FAILED   (0x08)
#define MPI26_PCIEDEV0_ASTATUS_UNSUPPORTED_DEVICE           (0x09)
#define MPI26_PCIEDEV0_ASTATUS_MSIX_REQUIRED                (0x0A)
#define MPI26_PCIEDEV0_ASTATUS_UNKNOWN                      (0x10)
#define MPI26_PCIEDEV0_ASTATUS_NVME_READY_TIMEOUT           (0x30)
#define MPI26_PCIEDEV0_ASTATUS_NVME_DEVCFG_UNSUPPORTED      (0x31)
#define MPI26_PCIEDEV0_ASTATUS_NVME_IDENTIFY_FAILED         (0x32)
#define MPI26_PCIEDEV0_ASTATUS_NVME_QCONFIG_FAILED          (0x33)
#define MPI26_PCIEDEV0_ASTATUS_NVME_QCREATION_FAILED        (0x34)
#define MPI26_PCIEDEV0_ASTATUS_NVME_EVENTCFG_FAILED         (0x35)
#define MPI26_PCIEDEV0_ASTATUS_NVME_GET_FEATURE_STAT_FAILED (0x36)
#define MPI26_PCIEDEV0_ASTATUS_NVME_IDLE_TIMEOUT            (0x37)
#define MPI26_PCIEDEV0_ASTATUS_NVME_FAILURE_STATUS          (0x38)
#define MPI26_PCIEDEV0_ASTATUS_INIT_FAIL_MAX                (0x3F)
#define MPI26_PCIEDEV0_FLAGS_2_RETIMER_PRESENCE             (0x00020000)
#define MPI26_PCIEDEV0_FLAGS_RETIMER_PRESENCE               (0x00010000)
#define MPI26_PCIEDEV0_FLAGS_UNAUTHORIZED_DEVICE            (0x00008000)
#define MPI26_PCIEDEV0_FLAGS_ENABLED_FAST_PATH              (0x00004000)
#define MPI26_PCIEDEV0_FLAGS_FAST_PATH_CAPABLE              (0x00002000)
#define MPI26_PCIEDEV0_FLAGS_ASYNCHRONOUS_NOTIFICATION      (0x00000400)
#define MPI26_PCIEDEV0_FLAGS_ATA_SW_PRESERVATION            (0x00000200)
#define MPI26_PCIEDEV0_FLAGS_UNSUPPORTED_DEVICE             (0x00000100)
#define MPI26_PCIEDEV0_FLAGS_ATA_48BIT_LBA_SUPPORTED        (0x00000080)
#define MPI26_PCIEDEV0_FLAGS_ATA_SMART_SUPPORTED            (0x00000040)
#define MPI26_PCIEDEV0_FLAGS_ATA_NCQ_SUPPORTED              (0x00000020)
#define MPI26_PCIEDEV0_FLAGS_ATA_FUA_SUPPORTED              (0x00000010)
#define MPI26_PCIEDEV0_FLAGS_ENCL_LEVEL_VALID               (0x00000002)
#define MPI26_PCIEDEV0_FLAGS_DEVICE_PRESENT                 (0x00000001)
#define MPI26_PCIEDEV0_LINK_RATE_16_0_SUPPORTED             (0x08)
#define MPI26_PCIEDEV0_LINK_RATE_8_0_SUPPORTED              (0x04)
#define MPI26_PCIEDEV0_LINK_RATE_5_0_SUPPORTED              (0x02)
#define MPI26_PCIEDEV0_LINK_RATE_2_5_SUPPORTED              (0x01)
typedef struct _MPI26_CONFIG_PAGE_PCIEDEV_2 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER	Header;	 
	U16	DevHandle;		 
	U8	ControllerResetTO;		 
	U8	Reserved1;		 
	U32	MaximumDataTransferSize;	 
	U32	Capabilities;		 
	U16	NOIOB;		 
	U16     ShutdownLatency;         
	U16     VendorID;                
	U16     DeviceID;                
	U16     SubsystemVendorID;       
	U16     SubsystemID;             
	U8      RevisionID;              
	U8      Reserved21[3];           
} MPI26_CONFIG_PAGE_PCIEDEV_2, *PTR_MPI26_CONFIG_PAGE_PCIEDEV_2,
	Mpi26PCIeDevicePage2_t, *pMpi26PCIeDevicePage2_t;
#define MPI26_PCIEDEVICE2_PAGEVERSION       (0x01)
#define MPI26_PCIEDEV2_CAP_DATA_BLK_ALIGN_AND_GRAN     (0x00000008)
#define MPI26_PCIEDEV2_CAP_SGL_FORMAT                  (0x00000004)
#define MPI26_PCIEDEV2_CAP_BIT_BUCKET_SUPPORT          (0x00000002)
#define MPI26_PCIEDEV2_CAP_SGL_SUPPORT                 (0x00000001)
#define MPI26_PCIEDEV2_NOIOB_UNSUPPORTED                (0x0000)
typedef struct _MPI26_CONFIG_PAGE_PCIELINK_1 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER	Header;	 
	U8	Link;				 
	U8	Reserved1;			 
	U16	Reserved2;			 
	U32	CorrectableErrorCount;		 
	U16	NonFatalErrorCount;		 
	U16	Reserved3;			 
	U16	FatalErrorCount;		 
	U16	Reserved4;			 
} MPI26_CONFIG_PAGE_PCIELINK_1, *PTR_MPI26_CONFIG_PAGE_PCIELINK_1,
	Mpi26PcieLinkPage1_t, *pMpi26PcieLinkPage1_t;
#define MPI26_PCIELINK1_PAGEVERSION            (0x00)
typedef struct _MPI26_PCIELINK2_LINK_EVENT {
	U8	LinkEventCode;		 
	U8	Reserved1;		 
	U16	Reserved2;		 
	U32	LinkEventInfo;		 
} MPI26_PCIELINK2_LINK_EVENT, *PTR_MPI26_PCIELINK2_LINK_EVENT,
	Mpi26PcieLink2LinkEvent_t, *pMpi26PcieLink2LinkEvent_t;
#ifndef MPI26_PCIELINK2_LINK_EVENT_MAX
#define MPI26_PCIELINK2_LINK_EVENT_MAX      (1)
#endif
typedef struct _MPI26_CONFIG_PAGE_PCIELINK_2 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER	Header;	 
	U8	Link;                        
	U8	Reserved1;                   
	U16	Reserved2;                   
	U8	NumLinkEvents;               
	U8	Reserved3;                   
	U16	Reserved4;                   
	MPI26_PCIELINK2_LINK_EVENT
		LinkEvent[MPI26_PCIELINK2_LINK_EVENT_MAX];	 
} MPI26_CONFIG_PAGE_PCIELINK_2, *PTR_MPI26_CONFIG_PAGE_PCIELINK_2,
	Mpi26PcieLinkPage2_t, *pMpi26PcieLinkPage2_t;
#define MPI26_PCIELINK2_PAGEVERSION            (0x00)
typedef struct _MPI26_PCIELINK3_LINK_EVENT_CONFIG {
	U8	LinkEventCode;       
	U8	Reserved1;           
	U16	Reserved2;           
	U8	CounterType;         
	U8	ThresholdWindow;     
	U8	TimeUnits;           
	U8	Reserved3;           
	U32	EventThreshold;      
	U16	ThresholdFlags;      
	U16	Reserved4;           
} MPI26_PCIELINK3_LINK_EVENT_CONFIG, *PTR_MPI26_PCIELINK3_LINK_EVENT_CONFIG,
	Mpi26PcieLink3LinkEventConfig_t, *pMpi26PcieLink3LinkEventConfig_t;
#define MPI26_PCIELINK3_EVTCODE_NO_EVENT                              (0x00)
#define MPI26_PCIELINK3_EVTCODE_CORRECTABLE_ERROR_RECEIVED            (0x01)
#define MPI26_PCIELINK3_EVTCODE_NON_FATAL_ERROR_RECEIVED              (0x02)
#define MPI26_PCIELINK3_EVTCODE_FATAL_ERROR_RECEIVED                  (0x03)
#define MPI26_PCIELINK3_EVTCODE_DATA_LINK_ERROR_DETECTED              (0x04)
#define MPI26_PCIELINK3_EVTCODE_TRANSACTION_LAYER_ERROR_DETECTED      (0x05)
#define MPI26_PCIELINK3_EVTCODE_TLP_ECRC_ERROR_DETECTED               (0x06)
#define MPI26_PCIELINK3_EVTCODE_POISONED_TLP                          (0x07)
#define MPI26_PCIELINK3_EVTCODE_RECEIVED_NAK_DLLP                     (0x08)
#define MPI26_PCIELINK3_EVTCODE_SENT_NAK_DLLP                         (0x09)
#define MPI26_PCIELINK3_EVTCODE_LTSSM_RECOVERY_STATE                  (0x0A)
#define MPI26_PCIELINK3_EVTCODE_LTSSM_RXL0S_STATE                     (0x0B)
#define MPI26_PCIELINK3_EVTCODE_LTSSM_TXL0S_STATE                     (0x0C)
#define MPI26_PCIELINK3_EVTCODE_LTSSM_L1_STATE                        (0x0D)
#define MPI26_PCIELINK3_EVTCODE_LTSSM_DISABLED_STATE                  (0x0E)
#define MPI26_PCIELINK3_EVTCODE_LTSSM_HOT_RESET_STATE                 (0x0F)
#define MPI26_PCIELINK3_EVTCODE_SYSTEM_ERROR                          (0x10)
#define MPI26_PCIELINK3_EVTCODE_DECODE_ERROR                          (0x11)
#define MPI26_PCIELINK3_EVTCODE_DISPARITY_ERROR                       (0x12)
#define MPI26_PCIELINK3_COUNTER_TYPE_WRAPPING               (0x00)
#define MPI26_PCIELINK3_COUNTER_TYPE_SATURATING             (0x01)
#define MPI26_PCIELINK3_COUNTER_TYPE_PEAK_VALUE             (0x02)
#define MPI26_PCIELINK3_TM_UNITS_10_MICROSECONDS            (0x00)
#define MPI26_PCIELINK3_TM_UNITS_100_MICROSECONDS           (0x01)
#define MPI26_PCIELINK3_TM_UNITS_1_MILLISECOND              (0x02)
#define MPI26_PCIELINK3_TM_UNITS_10_MILLISECONDS            (0x03)
#define MPI26_PCIELINK3_TFLAGS_EVENT_NOTIFY                 (0x0001)
#ifndef MPI26_PCIELINK3_LINK_EVENT_MAX
#define MPI26_PCIELINK3_LINK_EVENT_MAX      (1)
#endif
typedef struct _MPI26_CONFIG_PAGE_PCIELINK_3 {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER	Header;	 
	U8	Link;                        
	U8	Reserved1;                   
	U16	Reserved2;                   
	U8	NumLinkEvents;               
	U8	Reserved3;                   
	U16	Reserved4;                   
	MPI26_PCIELINK3_LINK_EVENT_CONFIG
		LinkEventConfig[MPI26_PCIELINK3_LINK_EVENT_MAX];  
} MPI26_CONFIG_PAGE_PCIELINK_3, *PTR_MPI26_CONFIG_PAGE_PCIELINK_3,
	Mpi26PcieLinkPage3_t, *pMpi26PcieLinkPage3_t;
#define MPI26_PCIELINK3_PAGEVERSION            (0x00)
#endif
