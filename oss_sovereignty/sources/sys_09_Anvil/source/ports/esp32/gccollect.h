

extern uint32_t _text_start;
extern uint32_t _text_end;
extern uint32_t _irom0_text_start;
extern uint32_t _irom0_text_end;
extern uint32_t _data_start;
extern uint32_t _data_end;
extern uint32_t _rodata_start;
extern uint32_t _rodata_end;
extern uint32_t _bss_start;
extern uint32_t _bss_end;
extern uint32_t _heap_start;
extern uint32_t _heap_end;

void gc_collect(void);
