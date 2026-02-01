 
#ifndef LFS2_H
#define LFS2_H

#include "lfs2_util.h"

#ifdef __cplusplus
extern "C"
{
#endif







#define LFS2_VERSION 0x00020008
#define LFS2_VERSION_MAJOR (0xffff & (LFS2_VERSION >> 16))
#define LFS2_VERSION_MINOR (0xffff & (LFS2_VERSION >>  0))




#define LFS2_DISK_VERSION 0x00020001
#define LFS2_DISK_VERSION_MAJOR (0xffff & (LFS2_DISK_VERSION >> 16))
#define LFS2_DISK_VERSION_MINOR (0xffff & (LFS2_DISK_VERSION >>  0))





typedef uint32_t lfs2_size_t;
typedef uint32_t lfs2_off_t;

typedef int32_t  lfs2_ssize_t;
typedef int32_t  lfs2_soff_t;

typedef uint32_t lfs2_block_t;




#ifndef LFS2_NAME_MAX
#define LFS2_NAME_MAX 255
#endif






#ifndef LFS2_FILE_MAX
#define LFS2_FILE_MAX 2147483647
#endif



#ifndef LFS2_ATTR_MAX
#define LFS2_ATTR_MAX 1022
#endif



enum lfs2_error {
    LFS2_ERR_OK          = 0,    
    LFS2_ERR_IO          = -5,   
    LFS2_ERR_CORRUPT     = -84,  
    LFS2_ERR_NOENT       = -2,   
    LFS2_ERR_EXIST       = -17,  
    LFS2_ERR_NOTDIR      = -20,  
    LFS2_ERR_ISDIR       = -21,  
    LFS2_ERR_NOTEMPTY    = -39,  
    LFS2_ERR_BADF        = -9,   
    LFS2_ERR_FBIG        = -27,  
    LFS2_ERR_INVAL       = -22,  
    LFS2_ERR_NOSPC       = -28,  
    LFS2_ERR_NOMEM       = -12,  
    LFS2_ERR_NOATTR      = -61,  
    LFS2_ERR_NAMETOOLONG = -36,  
};


enum lfs2_type {
    
    LFS2_TYPE_REG            = 0x001,
    LFS2_TYPE_DIR            = 0x002,

    
    LFS2_TYPE_SPLICE         = 0x400,
    LFS2_TYPE_NAME           = 0x000,
    LFS2_TYPE_STRUCT         = 0x200,
    LFS2_TYPE_USERATTR       = 0x300,
    LFS2_TYPE_FROM           = 0x100,
    LFS2_TYPE_TAIL           = 0x600,
    LFS2_TYPE_GLOBALS        = 0x700,
    LFS2_TYPE_CRC            = 0x500,

    
    LFS2_TYPE_CREATE         = 0x401,
    LFS2_TYPE_DELETE         = 0x4ff,
    LFS2_TYPE_SUPERBLOCK     = 0x0ff,
    LFS2_TYPE_DIRSTRUCT      = 0x200,
    LFS2_TYPE_CTZSTRUCT      = 0x202,
    LFS2_TYPE_INLINESTRUCT   = 0x201,
    LFS2_TYPE_SOFTTAIL       = 0x600,
    LFS2_TYPE_HARDTAIL       = 0x601,
    LFS2_TYPE_MOVESTATE      = 0x7ff,
    LFS2_TYPE_CCRC           = 0x500,
    LFS2_TYPE_FCRC           = 0x5ff,

    
    LFS2_FROM_NOOP           = 0x000,
    LFS2_FROM_MOVE           = 0x101,
    LFS2_FROM_USERATTRS      = 0x102,
};


enum lfs2_open_flags {
    
    LFS2_O_RDONLY = 1,         
#ifndef LFS2_READONLY
    LFS2_O_WRONLY = 2,         
    LFS2_O_RDWR   = 3,         
    LFS2_O_CREAT  = 0x0100,    
    LFS2_O_EXCL   = 0x0200,    
    LFS2_O_TRUNC  = 0x0400,    
    LFS2_O_APPEND = 0x0800,    
#endif

    
#ifndef LFS2_READONLY
    LFS2_F_DIRTY   = 0x010000, 
    LFS2_F_WRITING = 0x020000, 
#endif
    LFS2_F_READING = 0x040000, 
#ifndef LFS2_READONLY
    LFS2_F_ERRED   = 0x080000, 
#endif
    LFS2_F_INLINE  = 0x100000, 
};


enum lfs2_whence_flags {
    LFS2_SEEK_SET = 0,   
    LFS2_SEEK_CUR = 1,   
    LFS2_SEEK_END = 2,   
};



struct lfs2_config {
    
    
    void *context;

    
    
    int (*read)(const struct lfs2_config *c, lfs2_block_t block,
            lfs2_off_t off, void *buffer, lfs2_size_t size);

    
    
    
    int (*prog)(const struct lfs2_config *c, lfs2_block_t block,
            lfs2_off_t off, const void *buffer, lfs2_size_t size);

    
    
    
    
    int (*erase)(const struct lfs2_config *c, lfs2_block_t block);

    
    
    int (*sync)(const struct lfs2_config *c);

#ifdef LFS2_THREADSAFE
    
    
    int (*lock)(const struct lfs2_config *c);

    
    
    int (*unlock)(const struct lfs2_config *c);
#endif

    
    
    lfs2_size_t read_size;

    
    
    lfs2_size_t prog_size;

    
    
    
    
    lfs2_size_t block_size;

    
    lfs2_size_t block_count;

    
    
    
    
    
    
    int32_t block_cycles;

    
    
    
    
    
    lfs2_size_t cache_size;

    
    
    
    
    lfs2_size_t lookahead_size;

    
    
    void *read_buffer;

    
    
    void *prog_buffer;

    
    
    
    void *lookahead_buffer;

    
    
    
    
    lfs2_size_t name_max;

    
    
    
    lfs2_size_t file_max;

    
    
    
    lfs2_size_t attr_max;

    
    
    
    
    lfs2_size_t metadata_max;

#ifdef LFS2_MULTIVERSION
    
    
    
    
    uint32_t disk_version;
#endif
};


struct lfs2_info {
    
    uint8_t type;

    
    lfs2_size_t size;

    
    
    
    
    char name[LFS2_NAME_MAX+1];
};


struct lfs2_fsinfo {
    
    uint32_t disk_version;

    
    lfs2_size_t block_size;

    
    lfs2_size_t block_count;

    
    lfs2_size_t name_max;

    
    lfs2_size_t file_max;

    
    lfs2_size_t attr_max;
};



struct lfs2_attr {
    
    
    uint8_t type;

    
    void *buffer;

    
    lfs2_size_t size;
};


struct lfs2_file_config {
    
    
    void *buffer;

    
    
    
    
    
    
    
    
    
    
    
    struct lfs2_attr *attrs;

    
    lfs2_size_t attr_count;
};



typedef struct lfs2_cache {
    lfs2_block_t block;
    lfs2_off_t off;
    lfs2_size_t size;
    uint8_t *buffer;
} lfs2_cache_t;

typedef struct lfs2_mdir {
    lfs2_block_t pair[2];
    uint32_t rev;
    lfs2_off_t off;
    uint32_t etag;
    uint16_t count;
    bool erased;
    bool split;
    lfs2_block_t tail[2];
} lfs2_mdir_t;


typedef struct lfs2_dir {
    struct lfs2_dir *next;
    uint16_t id;
    uint8_t type;
    lfs2_mdir_t m;

    lfs2_off_t pos;
    lfs2_block_t head[2];
} lfs2_dir_t;


typedef struct lfs2_file {
    struct lfs2_file *next;
    uint16_t id;
    uint8_t type;
    lfs2_mdir_t m;

    struct lfs2_ctz {
        lfs2_block_t head;
        lfs2_size_t size;
    } ctz;

    uint32_t flags;
    lfs2_off_t pos;
    lfs2_block_t block;
    lfs2_off_t off;
    lfs2_cache_t cache;

    const struct lfs2_file_config *cfg;
} lfs2_file_t;

typedef struct lfs2_superblock {
    uint32_t version;
    lfs2_size_t block_size;
    lfs2_size_t block_count;
    lfs2_size_t name_max;
    lfs2_size_t file_max;
    lfs2_size_t attr_max;
} lfs2_superblock_t;

typedef struct lfs2_gstate {
    uint32_t tag;
    lfs2_block_t pair[2];
} lfs2_gstate_t;


typedef struct lfs2 {
    lfs2_cache_t rcache;
    lfs2_cache_t pcache;

    lfs2_block_t root[2];
    struct lfs2_mlist {
        struct lfs2_mlist *next;
        uint16_t id;
        uint8_t type;
        lfs2_mdir_t m;
    } *mlist;
    uint32_t seed;

    lfs2_gstate_t gstate;
    lfs2_gstate_t gdisk;
    lfs2_gstate_t gdelta;

    struct lfs2_free {
        lfs2_block_t off;
        lfs2_block_t size;
        lfs2_block_t i;
        lfs2_block_t ack;
        uint32_t *buffer;
    } free;

    const struct lfs2_config *cfg;
    lfs2_size_t block_count;
    lfs2_size_t name_max;
    lfs2_size_t file_max;
    lfs2_size_t attr_max;

#ifdef LFS2_MIGRATE
    struct lfs21 *lfs21;
#endif
} lfs2_t;




#ifndef LFS2_READONLY







int lfs2_format(lfs2_t *lfs2, const struct lfs2_config *config);
#endif









int lfs2_mount(lfs2_t *lfs2, const struct lfs2_config *config);





int lfs2_unmount(lfs2_t *lfs2);



#ifndef LFS2_READONLY




int lfs2_remove(lfs2_t *lfs2, const char *path);
#endif

#ifndef LFS2_READONLY






int lfs2_rename(lfs2_t *lfs2, const char *oldpath, const char *newpath);
#endif





int lfs2_stat(lfs2_t *lfs2, const char *path, struct lfs2_info *info);













lfs2_ssize_t lfs2_getattr(lfs2_t *lfs2, const char *path,
        uint8_t type, void *buffer, lfs2_size_t size);

#ifndef LFS2_READONLY







int lfs2_setattr(lfs2_t *lfs2, const char *path,
        uint8_t type, const void *buffer, lfs2_size_t size);
#endif

#ifndef LFS2_READONLY





int lfs2_removeattr(lfs2_t *lfs2, const char *path, uint8_t type);
#endif




#ifndef LFS2_NO_MALLOC






int lfs2_file_open(lfs2_t *lfs2, lfs2_file_t *file,
        const char *path, int flags);



#endif











int lfs2_file_opencfg(lfs2_t *lfs2, lfs2_file_t *file,
        const char *path, int flags,
        const struct lfs2_file_config *config);







int lfs2_file_close(lfs2_t *lfs2, lfs2_file_t *file);





int lfs2_file_sync(lfs2_t *lfs2, lfs2_file_t *file);





lfs2_ssize_t lfs2_file_read(lfs2_t *lfs2, lfs2_file_t *file,
        void *buffer, lfs2_size_t size);

#ifndef LFS2_READONLY






lfs2_ssize_t lfs2_file_write(lfs2_t *lfs2, lfs2_file_t *file,
        const void *buffer, lfs2_size_t size);
#endif





lfs2_soff_t lfs2_file_seek(lfs2_t *lfs2, lfs2_file_t *file,
        lfs2_soff_t off, int whence);

#ifndef LFS2_READONLY



int lfs2_file_truncate(lfs2_t *lfs2, lfs2_file_t *file, lfs2_off_t size);
#endif





lfs2_soff_t lfs2_file_tell(lfs2_t *lfs2, lfs2_file_t *file);





int lfs2_file_rewind(lfs2_t *lfs2, lfs2_file_t *file);





lfs2_soff_t lfs2_file_size(lfs2_t *lfs2, lfs2_file_t *file);




#ifndef LFS2_READONLY



int lfs2_mkdir(lfs2_t *lfs2, const char *path);
#endif





int lfs2_dir_open(lfs2_t *lfs2, lfs2_dir_t *dir, const char *path);





int lfs2_dir_close(lfs2_t *lfs2, lfs2_dir_t *dir);






int lfs2_dir_read(lfs2_t *lfs2, lfs2_dir_t *dir, struct lfs2_info *info);







int lfs2_dir_seek(lfs2_t *lfs2, lfs2_dir_t *dir, lfs2_off_t off);







lfs2_soff_t lfs2_dir_tell(lfs2_t *lfs2, lfs2_dir_t *dir);




int lfs2_dir_rewind(lfs2_t *lfs2, lfs2_dir_t *dir);








int lfs2_fs_stat(lfs2_t *lfs2, struct lfs2_fsinfo *fsinfo);







lfs2_ssize_t lfs2_fs_size(lfs2_t *lfs2);








int lfs2_fs_traverse(lfs2_t *lfs2, int (*cb)(void*, lfs2_block_t), void *data);











int lfs2_fs_gc(lfs2_t *lfs2);

#ifndef LFS2_READONLY








int lfs2_fs_mkconsistent(lfs2_t *lfs2);
#endif

#ifndef LFS2_READONLY






int lfs2_fs_grow(lfs2_t *lfs2, lfs2_size_t block_count);
#endif

#ifndef LFS2_READONLY
#ifdef LFS2_MIGRATE











int lfs2_migrate(lfs2_t *lfs2, const struct lfs2_config *cfg);
#endif
#endif


#ifdef __cplusplus
}  
#endif

#endif
