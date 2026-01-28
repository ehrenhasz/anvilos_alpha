
#ifndef BLKDEV_H
#define BLKDEV_H

#include <sys/types.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_IOCCOM_H
# include <sys/ioccom.h> 
#endif
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>

#ifdef HAVE_SYS_MKDEV_H
# include <sys/mkdev.h>		
#endif

#define DEFAULT_SECTOR_SIZE       512

#ifdef __linux__

# ifndef BLKROSET
#  define BLKROSET   _IO(0x12,93)	
#  define BLKROGET   _IO(0x12,94)	
#  define BLKRRPART  _IO(0x12,95)	
#  define BLKGETSIZE _IO(0x12,96)	
#  define BLKFLSBUF  _IO(0x12,97)	
#  define BLKRASET   _IO(0x12,98)	
#  define BLKRAGET   _IO(0x12,99)	
#  define BLKFRASET  _IO(0x12,100)	
#  define BLKFRAGET  _IO(0x12,101)	
#  define BLKSECTSET _IO(0x12,102)	
#  define BLKSECTGET _IO(0x12,103)	
#  define BLKSSZGET  _IO(0x12,104)	


#  define BLKELVGET  _IOR(0x12,106,size_t) 
#  define BLKELVSET  _IOW(0x12,107,size_t) 

#  define BLKBSZGET  _IOR(0x12,112,size_t)
#  define BLKBSZSET  _IOW(0x12,113,size_t)
# endif 

# ifndef BLKGETSIZE64
#  define BLKGETSIZE64 _IOR(0x12,114,size_t) 
# endif


# ifndef BLKIOMIN
#  define BLKIOMIN   _IO(0x12,120)
#  define BLKIOOPT   _IO(0x12,121)
#  define BLKALIGNOFF _IO(0x12,122)
#  define BLKPBSZGET _IO(0x12,123)
# endif


# ifndef BLKDISCARDZEROES
#  define BLKDISCARDZEROES _IO(0x12,124)
# endif


# ifndef BLKGETDISKSEQ
#  define BLKGETDISKSEQ _IOR(0x12, 128, uint64_t)
# endif


# ifndef FIFREEZE
#  define FIFREEZE   _IOWR('X', 119, int)    
#  define FITHAW     _IOWR('X', 120, int)    
# endif


# ifndef CDROM_GET_CAPABILITY
#  define CDROM_GET_CAPABILITY 0x5331
# endif

#endif 


#ifdef APPLE_DARWIN
# define BLKGETSIZE DKIOCGETBLOCKCOUNT32
#endif

#ifndef HDIO_GETGEO
# ifdef __linux__
#  define HDIO_GETGEO 0x0301
# endif

struct hd_geometry {
	unsigned char heads;
	unsigned char sectors;
	unsigned short cylinders;	
	unsigned long start;
};
#endif 



int is_blkdev(int fd);


int open_blkdev_or_file(const struct stat *st, const char *name, const int oflag);


off_t blkdev_find_size (int fd);


int blkdev_get_size(int fd, unsigned long long *bytes);


int blkdev_get_sectors(int fd, unsigned long long *sectors);


int blkdev_get_sector_size(int fd, int *sector_size);


int blkdev_is_misaligned(int fd);


int blkdev_get_physector_size(int fd, int *sector_size);


int blkdev_is_cdrom(int fd);


int blkdev_get_geometry(int fd, unsigned int *h, unsigned int *s);


#define SCSI_TYPE_DISK			0x00
#define SCSI_TYPE_TAPE			0x01
#define SCSI_TYPE_PRINTER		0x02
#define SCSI_TYPE_PROCESSOR		0x03	
#define SCSI_TYPE_WORM			0x04	
#define SCSI_TYPE_ROM			0x05
#define SCSI_TYPE_SCANNER		0x06
#define SCSI_TYPE_MOD			0x07	
#define SCSI_TYPE_MEDIUM_CHANGER	0x08
#define SCSI_TYPE_COMM			0x09	
#define SCSI_TYPE_RAID			0x0c
#define SCSI_TYPE_ENCLOSURE		0x0d	
#define SCSI_TYPE_RBC			0x0e
#define SCSI_TYPE_OSD			0x11
#define SCSI_TYPE_NO_LUN		0x7f


const char *blkdev_scsi_type_to_name(int type);

int blkdev_lock(int fd, const char *devname, const char *lockmode);
#ifdef HAVE_LINUX_BLKZONED_H
struct blk_zone_report *blkdev_get_zonereport(int fd, uint64_t sector, uint32_t nzones);
#else
static inline struct blk_zone_report *blkdev_get_zonereport(int fd, uint64_t sector, uint32_t nzones)
{
	return NULL;
}
#endif

#endif 
