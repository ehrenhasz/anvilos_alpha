/
typedef int Fd_t;




typedef void (*SL_P_EVENT_HANDLER)(void);

#define P_EVENT_HANDLER SL_P_EVENT_HANDLER


Fd_t spi_Open(char *ifName, unsigned long flags);


int spi_Close(Fd_t fd);


int spi_Read(Fd_t fd, unsigned char *pBuff, int len);


int spi_Write(Fd_t fd, unsigned char *pBuff, int len);


int NwpRegisterInterruptHandler(P_EVENT_HANDLER InterruptHdl , void* pValue);



void NwpMaskInterrupt();



void NwpUnMaskInterrupt();

void NwpPowerOnPreamble(void);

void NwpPowerOff(void);

void NwpPowerOn(void);


#ifdef  __cplusplus
}
#endif 


#endif

