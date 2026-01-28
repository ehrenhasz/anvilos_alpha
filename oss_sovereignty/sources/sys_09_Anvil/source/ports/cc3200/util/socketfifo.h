
#ifndef MICROPY_INCLUDED_CC3200_UTIL_SOCKETFIFO_H
#define MICROPY_INCLUDED_CC3200_UTIL_SOCKETFIFO_H







typedef struct {
    void                    *data;
    signed short            *sd;
    unsigned short          datasize;
    unsigned char           closesockets;
    bool                    freedata;

}SocketFifoElement_t;


extern void SOCKETFIFO_Init (FIFO_t *fifo, void *elements, uint32_t maxcount);
extern bool SOCKETFIFO_Push (const void * const element);
extern bool SOCKETFIFO_Pop (void * const element);
extern bool SOCKETFIFO_Peek (void * const element);
extern bool SOCKETFIFO_IsEmpty (void);
extern bool SOCKETFIFO_IsFull (void);
extern void SOCKETFIFO_Flush (void);
extern unsigned int SOCKETFIFO_Count (void);

#endif 
