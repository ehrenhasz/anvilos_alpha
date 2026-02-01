 

 

 

#ifndef FIFO_DEFS_H
#define FIFO_DEFS_H 1

#define head	sp->_fifohead
#define tail	sp->_fifotail
 
#define peek	sp->_fifopeek

#define h_inc() { \
	    (head >= FIFO_SIZE-1) \
		? head = 0 \
		: head++; \
	    if (head == tail) \
		head = -1, tail = 0; \
	}
#define h_dec() { \
	    (head <= 0) \
		? head = FIFO_SIZE-1 \
		: head--; \
	    if (head == tail) \
		tail = -1; \
	}
#define t_inc() { \
	    (tail >= FIFO_SIZE-1) \
		? tail = 0 \
		: tail++; \
	    if (tail == head) \
		tail = -1; \
	    }
#define t_dec() { \
	    (tail <= 0) \
		? tail = FIFO_SIZE-1 \
		: tail--; \
	    if (head == tail) \
		fifo_clear(sp); \
	    }
#define p_inc() { \
	    (peek >= FIFO_SIZE-1) \
		? peek = 0 \
		: peek++; \
	    }

#define cooked_key_in_fifo()	((head >= 0) && (peek != head))
#define raw_key_in_fifo()	((head >= 0) && (peek != tail))

#endif  
