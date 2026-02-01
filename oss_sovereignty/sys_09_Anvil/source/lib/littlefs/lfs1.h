 
#ifndef LFS1_H
#define LFS1_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif







#define LFS1_VERSION 0x00010007
#define LFS1_VERSION_MAJOR (0xffff & (LFS1_VERSION >> 16))
#define LFS1_VERSION_MINOR (0xffff & (LFS1_VERSION >>  0))




#define LFS1_DISK_VERSION 0x00010001
#define LFS1_DISK_VERSION_MAJOR (0xffff & (LFS1_DISK_VERSION >> 16))
#define LFS1_DISK_VERSION_MINOR (0xffff & (LFS1_DISK_VERSION >>  0))





typedef uint32_t lfs1_size_t;
typedef uint32_t lfs1_off_t;

typedef int32_t  lfs1_ssize_t;
typedef int32_t  lfs1_soff_t;

typedef uint32_t lfs1_block_t;


#ifndef LFS1_NAME_MAX
#define LFS1_NAME_MAX 255
#endif


#ifndef LFS1_FILE_MAX
#define LFS1_FILE_MAX 2147483647
#endif



enum lfs1_error {
    LFS1_ERR_OK       = 0,    
    LFS1_ERR_IO       = -5,   
    LFS1_ERR_CORRUPT  = -52,  
    LFS1_ERR_NOENT    = -2,   
    LFS1_ERR_EXIST    = -17,  
    LFS1_ERR_NOTDIR   = -20,  
    LFS1_ERR_ISDIR    = -21,  
    LFS1_ERR_NOTEMPTY = -39,  
    LFS1_ERR_BADF     = -9,   
    LFS1_ERR_FBIG     = -27,  
    LFS1_ERR_INVAL    = -22,  
    LFS1_ERR_NOSPC    = -28,  
    LFS1_ERR_NOMEM    = -12,  
};


enum lfs1_type {
    LFS1_TYPE_REG        = 0x11,
    LFS1_TYPE_DIR        = 0x22,
    LFS1_TYPE_SUPERBLOCK = 0x2e,
};


enum lfs1_open_flags {
    
    LFS1_O_RDONLY = 1,        
    LFS1_O_WRONLY = 2,        
    LFS1_O_RDWR   = 3,        
    LFS1_O_CREAT  = 0x0100,   
    LFS1_O_EXCL   = 0x0200,   
    LFS1_O_TRUNC  = 0x0400,   
    LFS1_O_APPEND = 0x0800,   

    
    LFS1_F_DIRTY   = 0x10000, 
    LFS1_F_WRITING = 0x20000, 
    LFS1_F_READING = 0x40000, 
    LFS1_F_ERRED   = 0x80000, 
};


enum lfs1_whence_flags {
    LFS1_SEEK_SET = 0,   
    LFS1_SEEK_CUR = 1,   
    LFS1_SEEK_END = 2,   
};



struct lfs1_config {
    
    
    void *context;

    
    
    int (*read)(const struct lfs1_config *c, lfs1_block_t block,
            lfs1_off_t off, void *buffer, lfs1_size_t size);

    
    
    
    int (*prog)(const struct lfs1_config *c, lfs1_block_t block,
            lfs1_off_t off, const void *buffer, lfs1_size_t size);

    
    
    
    
    int (*erase)(const struct lfs1_config *c, lfs1_block_t block);

    
    
    int (*sync)(const struct lfs1_config *c);

    
    
    
    lfs1_size_t read_size;

    
    
    
    
    lfs1_size_t prog_size;

    
    
    
    
    lfs1_size_t block_size;

    
    lfs1_size_t block_count;

    
    
    
    
    lfs1_size_t lookahead;

    
    void *read_buffer;

    
    void *prog_buffer;

    
    
    void *lookahead_buffer;

    
    
    void *file_buffer;
};


struct lfs1_file_config {
    
    
    void *buffer;
};


struct lfs1_info {
    
    uint8_t type;

    
    lfs1_size_t size;

    
    char name[LFS1_NAME_MAX+1];
};



typedef struct lfs1_entry {
    lfs1_off_t off;

    struct lfs1_disk_entry {
        uint8_t type;
        uint8_t elen;
        uint8_t alen;
        uint8_t nlen;
        union {
            struct {
                lfs1_block_t head;
                lfs1_size_t size;
            } file;
            lfs1_block_t dir[2];
        } u;
    } d;
} lfs1_entry_t;

typedef struct lfs1_cache {
    lfs1_block_t block;
    lfs1_off_t off;
    uint8_t *buffer;
} lfs1_cache_t;

typedef struct lfs1_file {
    struct lfs1_file *next;
    lfs1_block_t pair[2];
    lfs1_off_t poff;

    lfs1_block_t head;
    lfs1_size_t size;

    const struct lfs1_file_config *cfg;
    uint32_t flags;
    lfs1_off_t pos;
    lfs1_block_t block;
    lfs1_off_t off;
    lfs1_cache_t cache;
} lfs1_file_t;

typedef struct lfs1_dir {
    struct lfs1_dir *next;
    lfs1_block_t pair[2];
    lfs1_off_t off;

    lfs1_block_t head[2];
    lfs1_off_t pos;

    struct lfs1_disk_dir {
        uint32_t rev;
        lfs1_size_t size;
        lfs1_block_t tail[2];
    } d;
} lfs1_dir_t;

typedef struct lfs1_superblock {
    lfs1_off_t off;

    struct lfs1_disk_superblock {
        uint8_t type;
        uint8_t elen;
        uint8_t alen;
        uint8_t nlen;
        lfs1_block_t root[2];
        uint32_t block_size;
        uint32_t block_count;
        uint32_t version;
        char magic[8];
    } d;
} lfs1_superblock_t;

typedef struct lfs1_free {
    lfs1_block_t off;
    lfs1_block_t size;
    lfs1_block_t i;
    lfs1_block_t ack;
    uint32_t *buffer;
} lfs1_free_t;


typedef struct lfs1 {
    const struct lfs1_config *cfg;

    lfs1_block_t root[2];
    lfs1_file_t *files;
    lfs1_dir_t *dirs;

    lfs1_cache_t rcache;
    lfs1_cache_t pcache;

    lfs1_free_t free;
    bool deorphaned;
    bool moving;
} lfs1_t;











int lfs1_format(lfs1_t *lfs1, const struct lfs1_config *config);









int lfs1_mount(lfs1_t *lfs1, const struct lfs1_config *config);





int lfs1_unmount(lfs1_t *lfs1);







int lfs1_remove(lfs1_t *lfs1, const char *path);







int lfs1_rename(lfs1_t *lfs1, const char *oldpath, const char *newpath);





int lfs1_stat(lfs1_t *lfs1, const char *path, struct lfs1_info *info);










int lfs1_file_open(lfs1_t *lfs1, lfs1_file_t *file,
        const char *path, int flags);











int lfs1_file_opencfg(lfs1_t *lfs1, lfs1_file_t *file,
        const char *path, int flags,
        const struct lfs1_file_config *config);







int lfs1_file_close(lfs1_t *lfs1, lfs1_file_t *file);





int lfs1_file_sync(lfs1_t *lfs1, lfs1_file_t *file);





lfs1_ssize_t lfs1_file_read(lfs1_t *lfs1, lfs1_file_t *file,
        void *buffer, lfs1_size_t size);







lfs1_ssize_t lfs1_file_write(lfs1_t *lfs1, lfs1_file_t *file,
        const void *buffer, lfs1_size_t size);





lfs1_soff_t lfs1_file_seek(lfs1_t *lfs1, lfs1_file_t *file,
        lfs1_soff_t off, int whence);




int lfs1_file_truncate(lfs1_t *lfs1, lfs1_file_t *file, lfs1_off_t size);





lfs1_soff_t lfs1_file_tell(lfs1_t *lfs1, lfs1_file_t *file);





int lfs1_file_rewind(lfs1_t *lfs1, lfs1_file_t *file);





lfs1_soff_t lfs1_file_size(lfs1_t *lfs1, lfs1_file_t *file);







int lfs1_mkdir(lfs1_t *lfs1, const char *path);





int lfs1_dir_open(lfs1_t *lfs1, lfs1_dir_t *dir, const char *path);





int lfs1_dir_close(lfs1_t *lfs1, lfs1_dir_t *dir);





int lfs1_dir_read(lfs1_t *lfs1, lfs1_dir_t *dir, struct lfs1_info *info);







int lfs1_dir_seek(lfs1_t *lfs1, lfs1_dir_t *dir, lfs1_off_t off);







lfs1_soff_t lfs1_dir_tell(lfs1_t *lfs1, lfs1_dir_t *dir);




int lfs1_dir_rewind(lfs1_t *lfs1, lfs1_dir_t *dir);











int lfs1_traverse(lfs1_t *lfs1, int (*cb)(void*, lfs1_block_t), void *data);








int lfs1_deorphan(lfs1_t *lfs1);


#ifdef __cplusplus
}  
#endif

#endif
