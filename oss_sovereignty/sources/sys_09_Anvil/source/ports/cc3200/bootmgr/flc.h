
#ifndef MICROPY_INCLUDED_CC3200_BOOTMGR_FLC_H
#define MICROPY_INCLUDED_CC3200_BOOTMGR_FLC_H


#ifdef __cplusplus
extern "C"
{
#endif


#define IMG_BOOT_INFO           "/sys/bootinfo.bin"
#define IMG_FACTORY             "/sys/factimg.bin"
#define IMG_UPDATE1             "/sys/updtimg1.bin"
#define IMG_UPDATE2             "/sys/updtimg2.bin"
#define IMG_PREFIX              "/sys/updtimg"

#define IMG_SRVPACK             "/sys/servicepack.ucf"
#define SRVPACK_SIGN            "/sys/servicepack.sig"

#define CA_FILE                 "/cert/ca.pem"
#define CERT_FILE               "/cert/cert.pem"
#define KEY_FILE                "/cert/private.key"


#define IMG_SIZE                (192 * 1024)    
#define SRVPACK_SIZE            (16  * 1024)
#define SIGN_SIZE               (2   * 1024)
#define CA_KEY_SIZE             (4   * 1024)


#define IMG_ACT_FACTORY         0
#define IMG_ACT_UPDATE1         1
#define IMG_ACT_UPDATE2         2

#define IMG_STATUS_CHECK        0
#define IMG_STATUS_READY        1


typedef struct _sBootInfo_t
{
  _u8  ActiveImg;
  _u8  Status;
  _u8  PrevImg;
  _u8  : 8;
} sBootInfo_t;



#ifdef __cplusplus
}
#endif

#endif 
