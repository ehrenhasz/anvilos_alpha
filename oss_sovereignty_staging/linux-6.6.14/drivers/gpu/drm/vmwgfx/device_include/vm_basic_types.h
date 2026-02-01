 
 
#ifndef VM_BASIC_TYPES_H
#define VM_BASIC_TYPES_H

#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/page.h>

typedef u32 uint32;
typedef s32 int32;
typedef u64 uint64;
typedef u16 uint16;
typedef s16 int16;
typedef u8  uint8;
typedef s8  int8;

typedef uint64 PA;
typedef uint32 PPN;
typedef uint32 PPN32;
typedef uint64 PPN64;

typedef bool Bool;

#define MAX_UINT64 U64_MAX
#define MAX_UINT32 U32_MAX
#define MAX_UINT16 U16_MAX

#define CONST64U(x) x##ULL

#ifndef MBYTES_SHIFT
#define MBYTES_SHIFT 20
#endif
#ifndef MBYTES_2_BYTES
#define MBYTES_2_BYTES(_nbytes) ((uint64)(_nbytes) << MBYTES_SHIFT)
#endif

 

typedef struct MKSGuestStatCounter {
	atomic64_t count;
} MKSGuestStatCounter;

typedef struct MKSGuestStatCounterTime {
	MKSGuestStatCounter counter;
	atomic64_t selfCycles;
	atomic64_t totalCycles;
} MKSGuestStatCounterTime;

 

#define MKS_GUEST_STAT_FLAG_NONE    0
#define MKS_GUEST_STAT_FLAG_TIME    (1U << 0)

typedef __attribute__((aligned(32))) struct MKSGuestStatInfoEntry {
	union {
		const char *s;
		uint64 u;
	} name;
	union {
		const char *s;
		uint64 u;
	} description;
	uint64 flags;
	union {
		MKSGuestStatCounter *counter;
		MKSGuestStatCounterTime *counterTime;
		uint64 u;
	} stat;
} MKSGuestStatInfoEntry;

#define INVALID_PPN64       ((PPN64)0x000fffffffffffffULL)

#define MKS_GUEST_STAT_INSTANCE_DESC_LENGTH 1024
#define MKS_GUEST_STAT_INSTANCE_MAX_STATS   4096
#define MKS_GUEST_STAT_INSTANCE_MAX_STAT_PPNS        \
	(PFN_UP(MKS_GUEST_STAT_INSTANCE_MAX_STATS *  \
		sizeof(MKSGuestStatCounterTime)))
#define MKS_GUEST_STAT_INSTANCE_MAX_INFO_PPNS        \
	(PFN_UP(MKS_GUEST_STAT_INSTANCE_MAX_STATS *  \
		sizeof(MKSGuestStatInfoEntry)))
#define MKS_GUEST_STAT_AVERAGE_NAME_LENGTH  40
#define MKS_GUEST_STAT_INSTANCE_MAX_STRS_PPNS        \
	(PFN_UP(MKS_GUEST_STAT_INSTANCE_MAX_STATS *  \
		MKS_GUEST_STAT_AVERAGE_NAME_LENGTH))

 

typedef struct MKSGuestStatInstanceDescriptor {
	uint64 reservedMBZ;  
	uint64 statStartVA;  
	uint64 strsStartVA;  
	uint64 statLength;   
	uint64 infoLength;   
	uint64 strsLength;   
	PPN64  statPPNs[MKS_GUEST_STAT_INSTANCE_MAX_STAT_PPNS];  
	PPN64  infoPPNs[MKS_GUEST_STAT_INSTANCE_MAX_INFO_PPNS];  
	PPN64  strsPPNs[MKS_GUEST_STAT_INSTANCE_MAX_STRS_PPNS];  
	char   description[MKS_GUEST_STAT_INSTANCE_DESC_LENGTH];
} MKSGuestStatInstanceDescriptor;

#endif
