#ifndef MPT3SAS_TRIGGER_DIAG_H_INCLUDED
#define MPT3SAS_TRIGGER_DIAG_H_INCLUDED
#define NUM_VALID_ENTRIES               (20)
#define MPT3SAS_TRIGGER_MASTER          (1)
#define MPT3SAS_TRIGGER_EVENT           (2)
#define MPT3SAS_TRIGGER_SCSI            (3)
#define MPT3SAS_TRIGGER_MPI             (4)
#define MASTER_TRIGGER_FILE_NAME        "diag_trigger_master"
#define EVENT_TRIGGERS_FILE_NAME        "diag_trigger_event"
#define SCSI_TRIGGERS_FILE_NAME         "diag_trigger_scsi"
#define MPI_TRIGGER_FILE_NAME           "diag_trigger_mpi"
#define MASTER_TRIGGER_FW_FAULT         (0x00000001)
#define MASTER_TRIGGER_ADAPTER_RESET    (0x00000002)
#define MASTER_TRIGGER_TASK_MANAGMENT   (0x00000004)
#define MASTER_TRIGGER_DEVICE_REMOVAL   (0x00000008)
#define MPI3_EVENT_DIAGNOSTIC_TRIGGER_FIRED	(0x6E)
struct SL_WH_MASTER_TRIGGER_T {
	uint32_t MasterData;
};
struct SL_WH_EVENT_TRIGGER_T {
	uint16_t EventValue;
	uint16_t LogEntryQualifier;
};
struct SL_WH_EVENT_TRIGGERS_T {
	uint32_t ValidEntries;
	struct SL_WH_EVENT_TRIGGER_T EventTriggerEntry[NUM_VALID_ENTRIES];
};
struct SL_WH_SCSI_TRIGGER_T {
	U8 ASCQ;
	U8 ASC;
	U8 SenseKey;
	U8 Reserved;
};
struct SL_WH_SCSI_TRIGGERS_T {
	uint32_t ValidEntries;
	struct SL_WH_SCSI_TRIGGER_T SCSITriggerEntry[NUM_VALID_ENTRIES];
};
struct SL_WH_MPI_TRIGGER_T {
	uint16_t IOCStatus;
	uint16_t Reserved;
	uint32_t IocLogInfo;
};
struct SL_WH_MPI_TRIGGERS_T {
	uint32_t ValidEntries;
	struct SL_WH_MPI_TRIGGER_T MPITriggerEntry[NUM_VALID_ENTRIES];
};
struct SL_WH_TRIGGERS_EVENT_DATA_T {
	uint32_t trigger_type;
	union {
		struct SL_WH_MASTER_TRIGGER_T master;
		struct SL_WH_EVENT_TRIGGER_T event;
		struct SL_WH_SCSI_TRIGGER_T scsi;
		struct SL_WH_MPI_TRIGGER_T mpi;
	} u;
};
#endif  
