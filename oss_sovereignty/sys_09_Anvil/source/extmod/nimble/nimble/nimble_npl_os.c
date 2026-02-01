 

#include <stdio.h>
#include "py/mphal.h"
#include "py/runtime.h"
#include "nimble/ble.h"
#include "nimble/nimble_npl.h"
#include "extmod/nimble/hal/hal_uart.h"

#include "extmod/modbluetooth.h"
#include "extmod/nimble/modbluetooth_nimble.h"

#define DEBUG_OS_printf(...) 
#define DEBUG_MALLOC_printf(...) 
#define DEBUG_EVENT_printf(...) 
#define DEBUG_MUTEX_printf(...) 
#define DEBUG_SEM_printf(...) 
#define DEBUG_CALLOUT_printf(...) 
#define DEBUG_TIME_printf(...) 
#define DEBUG_CRIT_printf(...) 

bool ble_npl_os_started(void) {
    DEBUG_OS_printf("ble_npl_os_started\n");
    return true;
}

void *ble_npl_get_current_task_id(void) {
    DEBUG_OS_printf("ble_npl_get_current_task_id\n");
    return NULL;
}

 





typedef struct _mp_bluetooth_nimble_malloc_t {
    struct _mp_bluetooth_nimble_malloc_t *prev;
    struct _mp_bluetooth_nimble_malloc_t *next;
    size_t size;
    uint8_t data[];
} mp_bluetooth_nimble_malloc_t;


static void *m_malloc_bluetooth(size_t size) {
    size += sizeof(mp_bluetooth_nimble_malloc_t);
    mp_bluetooth_nimble_malloc_t *alloc = m_malloc0(size);
    alloc->size = size;
    alloc->next = MP_STATE_PORT(bluetooth_nimble_memory);
    if (alloc->next) {
        alloc->next->prev = alloc;
    }
    MP_STATE_PORT(bluetooth_nimble_memory) = alloc;
    return alloc->data;
}

static mp_bluetooth_nimble_malloc_t* get_nimble_malloc(void *ptr) {
    return (mp_bluetooth_nimble_malloc_t*)((uintptr_t)ptr - sizeof(mp_bluetooth_nimble_malloc_t));
}

static void m_free_bluetooth(void *ptr) {
    mp_bluetooth_nimble_malloc_t *alloc = get_nimble_malloc(ptr);
    if (alloc->next) {
        alloc->next->prev = alloc->prev;
    }
    if (alloc->prev) {
        alloc->prev->next = alloc->next;
    } else {
        MP_STATE_PORT(bluetooth_nimble_memory) = NULL;
    }
    m_free(alloc
    #if MICROPY_MALLOC_USES_ALLOCATED_SIZE
           , alloc->size
    #endif
    );
}



static bool is_valid_nimble_malloc(void *ptr) {
    DEBUG_MALLOC_printf("NIMBLE is_valid_nimble_malloc(%p)\n", ptr);
    mp_bluetooth_nimble_malloc_t *alloc = MP_STATE_PORT(bluetooth_nimble_memory);
    while (alloc) {
        DEBUG_MALLOC_printf("NIMBLE   checking: %p\n", alloc->data);
        if (alloc->data == ptr) {
            return true;
        }
        alloc = alloc->next;
    }
    return false;
}

void *nimble_malloc(size_t size) {
    DEBUG_MALLOC_printf("NIMBLE malloc(%u)\n", (uint)size);
    void* ptr = m_malloc_bluetooth(size);
    DEBUG_MALLOC_printf("  --> %p\n", ptr);
    return ptr;
}


void nimble_free(void *ptr) {
    DEBUG_MALLOC_printf("NIMBLE free(%p)\n", ptr);

    if (ptr) {
        
        
        
        
        
        
        if (is_valid_nimble_malloc(ptr)) {
            m_free_bluetooth(ptr);
        }
    }
}


void *nimble_realloc(void *ptr, size_t new_size) {
    DEBUG_MALLOC_printf("NIMBLE realloc(%p, %u)\n", ptr, (uint)new_size);

    if (!ptr) {
        return nimble_malloc(new_size);
    }

    assert(is_valid_nimble_malloc(ptr));

    
    mp_bluetooth_nimble_malloc_t *alloc = get_nimble_malloc(ptr);
    size_t old_size = alloc->size - sizeof(mp_bluetooth_nimble_malloc_t);
    if (old_size >= new_size) {
        return ptr;
    }

    
    void *ptr2 = m_malloc_bluetooth(new_size);

    
    memcpy(ptr2, ptr, old_size);
    m_free_bluetooth(ptr);

    DEBUG_MALLOC_printf("  --> %p\n", ptr2);

    return ptr2;
}


int nimble_sprintf(char *str, const char *fmt, ...) {
    str[0] = 0;
    return 0;
}

 


struct ble_npl_eventq *global_eventq = NULL;


void mp_bluetooth_nimble_os_eventq_run_all(void) {
    if (mp_bluetooth_nimble_ble_state == MP_BLUETOOTH_NIMBLE_BLE_STATE_OFF) {
        return;
    }

    
    while (true) {
        struct ble_npl_event *ev = NULL;

        os_sr_t sr;
        OS_ENTER_CRITICAL(sr);
        
        for (struct ble_npl_eventq *evq = global_eventq; evq != NULL; evq = evq->nextq) {
            ev = evq->head;
            if (ev) {
                
                evq->head = ev->next;
                if (ev->next) {
                    ev->next->prev = NULL;
                    ev->next = NULL;
                }
                ev->prev = NULL;

                ev->pending = false;

                
                break;
            }
        }
        OS_EXIT_CRITICAL(sr);

        if (!ev) {
            break;
        }

        
        DEBUG_EVENT_printf("event_run(%p)\n", ev);
        ev->fn(ev);
        DEBUG_EVENT_printf("event_run(%p) done\n", ev);

        if (ev->pending) {
            
            
            
            break;
        }
    }
}

void ble_npl_eventq_init(struct ble_npl_eventq *evq) {
    DEBUG_EVENT_printf("ble_npl_eventq_init(%p)\n", evq);
    os_sr_t sr;
    OS_ENTER_CRITICAL(sr);
    evq->head = NULL;
    struct ble_npl_eventq **evq2;
    for (evq2 = &global_eventq; *evq2 != NULL; evq2 = &(*evq2)->nextq) {
    }
    *evq2 = evq;
    evq->nextq = NULL;
    OS_EXIT_CRITICAL(sr);
}

void ble_npl_eventq_put(struct ble_npl_eventq *evq, struct ble_npl_event *ev) {
    DEBUG_EVENT_printf("ble_npl_eventq_put(%p, %p (%p, %p))\n", evq, ev, ev->fn, ev->arg);
    os_sr_t sr;
    OS_ENTER_CRITICAL(sr);
    ev->next = NULL;
    ev->pending = true;
    if (evq->head == NULL) {
        
        evq->head = ev;
        ev->prev = NULL;
    } else {
        
        struct ble_npl_event *tail = evq->head;
        while (true) {
            if (tail == ev) {
                DEBUG_EVENT_printf("  --> already in queue\n");
                
                
                break;
            }
            if (tail->next == NULL) {
                
                tail->next = ev;
                ev->prev = tail;
                break;
            }
            DEBUG_EVENT_printf("  --> %p\n", tail->next);
            tail = tail->next;
        }
    }
    OS_EXIT_CRITICAL(sr);
}

void ble_npl_event_init(struct ble_npl_event *ev, ble_npl_event_fn *fn, void *arg) {
    DEBUG_EVENT_printf("ble_npl_event_init(%p, %p, %p)\n", ev, fn, arg);
    ev->fn = fn;
    ev->arg = arg;
    ev->next = NULL;
    ev->pending = false;
}

void *ble_npl_event_get_arg(struct ble_npl_event *ev) {
    DEBUG_EVENT_printf("ble_npl_event_get_arg(%p) -> %p\n", ev, ev->arg);
    return ev->arg;
}

void ble_npl_event_set_arg(struct ble_npl_event *ev, void *arg) {
    DEBUG_EVENT_printf("ble_npl_event_set_arg(%p, %p)\n", ev, arg);
    ev->arg = arg;
}

 


ble_npl_error_t ble_npl_mutex_init(struct ble_npl_mutex *mu) {
    DEBUG_MUTEX_printf("ble_npl_mutex_init(%p)\n", mu);
    mu->locked = 0;
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_mutex_pend(struct ble_npl_mutex *mu, ble_npl_time_t timeout) {
    DEBUG_MUTEX_printf("ble_npl_mutex_pend(%p, %u) locked=%u\n", mu, (uint)timeout, (uint)mu->locked);

    
    

    ++mu->locked;

    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_mutex_release(struct ble_npl_mutex *mu) {
    DEBUG_MUTEX_printf("ble_npl_mutex_release(%p) locked=%u\n", mu, (uint)mu->locked);
    assert(mu->locked > 0);

    --mu->locked;

    return BLE_NPL_OK;
}

 


ble_npl_error_t ble_npl_sem_init(struct ble_npl_sem *sem, uint16_t tokens) {
    DEBUG_SEM_printf("ble_npl_sem_init(%p, %u)\n", sem, (uint)tokens);
    sem->count = tokens;
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_sem_pend(struct ble_npl_sem *sem, ble_npl_time_t timeout) {
    DEBUG_SEM_printf("ble_npl_sem_pend(%p, %u) count=%u\n", sem, (uint)timeout, (uint)sem->count);

    
    
    
    
    

    if (sem->count == 0) {
        uint32_t t0 = mp_hal_ticks_ms();
        while (sem->count == 0 && mp_hal_ticks_ms() - t0 < timeout) {
            if (sem->count != 0) {
                break;
            }

            mp_bluetooth_nimble_hci_uart_wfi();
        }

        if (sem->count == 0) {
            DEBUG_SEM_printf("ble_npl_sem_pend: semaphore timeout\n");
            return BLE_NPL_TIMEOUT;
        }

        DEBUG_SEM_printf("ble_npl_sem_pend: acquired in %u ms\n", (int)(mp_hal_ticks_ms() - t0));
    }
    sem->count -= 1;
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_sem_release(struct ble_npl_sem *sem) {
    DEBUG_SEM_printf("ble_npl_sem_release(%p)\n", sem);
    sem->count += 1;
    return BLE_NPL_OK;
}

uint16_t ble_npl_sem_get_count(struct ble_npl_sem *sem) {
    DEBUG_SEM_printf("ble_npl_sem_get_count(%p)\n", sem);
    return sem->count;
}

 


static struct ble_npl_callout *global_callout = NULL;

void mp_bluetooth_nimble_os_callout_process(void) {
    os_sr_t sr;
    OS_ENTER_CRITICAL(sr);
    uint32_t tnow = mp_hal_ticks_ms();
    for (struct ble_npl_callout *c = global_callout; c != NULL; c = c->nextc) {
        if (!c->active) {
            continue;
        }
        if ((int32_t)(tnow - c->ticks) >= 0) {
            DEBUG_CALLOUT_printf("callout_run(%p) tnow=%u ticks=%u evq=%p\n", c, (uint)tnow, (uint)c->ticks, c->evq);
            c->active = false;
            if (c->evq) {
                
                ble_npl_eventq_put(c->evq, &c->ev);
            } else {
                
                OS_EXIT_CRITICAL(sr);
                c->ev.fn(&c->ev);
                OS_ENTER_CRITICAL(sr);
            }
            DEBUG_CALLOUT_printf("callout_run(%p) done\n", c);
        }
    }
    OS_EXIT_CRITICAL(sr);
}

void ble_npl_callout_init(struct ble_npl_callout *c, struct ble_npl_eventq *evq, ble_npl_event_fn *ev_cb, void *ev_arg) {
    DEBUG_CALLOUT_printf("ble_npl_callout_init(%p, %p, %p, %p)\n", c, evq, ev_cb, ev_arg);
    os_sr_t sr;
    OS_ENTER_CRITICAL(sr);
    c->active = false;
    c->ticks = 0;
    c->evq = evq;
    ble_npl_event_init(&c->ev, ev_cb, ev_arg);

    struct ble_npl_callout **c2;
    for (c2 = &global_callout; *c2 != NULL; c2 = &(*c2)->nextc) {
        if (c == *c2) {
            
            OS_EXIT_CRITICAL(sr);
            return;
        }
    }
    *c2 = c;
    c->nextc = NULL;
    OS_EXIT_CRITICAL(sr);
}

ble_npl_error_t ble_npl_callout_reset(struct ble_npl_callout *c, ble_npl_time_t ticks) {
    DEBUG_CALLOUT_printf("ble_npl_callout_reset(%p, %u) tnow=%u\n", c, (uint)ticks, (uint)mp_hal_ticks_ms());
    os_sr_t sr;
    OS_ENTER_CRITICAL(sr);
    c->active = true;
    c->ticks = ble_npl_time_get() + ticks;
    OS_EXIT_CRITICAL(sr);
    return BLE_NPL_OK;
}

void ble_npl_callout_stop(struct ble_npl_callout *c) {
    DEBUG_CALLOUT_printf("ble_npl_callout_stop(%p)\n", c);
    c->active = false;
}

bool ble_npl_callout_is_active(struct ble_npl_callout *c) {
    DEBUG_CALLOUT_printf("ble_npl_callout_is_active(%p)\n", c);
    return c->active;
}

ble_npl_time_t ble_npl_callout_get_ticks(struct ble_npl_callout *c) {
    DEBUG_CALLOUT_printf("ble_npl_callout_get_ticks(%p)\n", c);
    return c->ticks;
}

ble_npl_time_t ble_npl_callout_remaining_ticks(struct ble_npl_callout *c, ble_npl_time_t now) {
    DEBUG_CALLOUT_printf("ble_npl_callout_remaining_ticks(%p, %u)\n", c, (uint)now);
    if (c->ticks > now) {
        return c->ticks - now;
    } else {
        return 0;
    }
}

void *ble_npl_callout_get_arg(struct ble_npl_callout *c) {
    DEBUG_CALLOUT_printf("ble_npl_callout_get_arg(%p)\n", c);
    return ble_npl_event_get_arg(&c->ev);
}

void ble_npl_callout_set_arg(struct ble_npl_callout *c, void *arg) {
    DEBUG_CALLOUT_printf("ble_npl_callout_set_arg(%p, %p)\n", c, arg);
    ble_npl_event_set_arg(&c->ev, arg);
}

 


uint32_t ble_npl_time_get(void) {
    DEBUG_TIME_printf("ble_npl_time_get -> %u\n", (uint)mp_hal_ticks_ms());
    return mp_hal_ticks_ms();
}

ble_npl_error_t ble_npl_time_ms_to_ticks(uint32_t ms, ble_npl_time_t *out_ticks) {
    DEBUG_TIME_printf("ble_npl_time_ms_to_ticks(%u)\n", (uint)ms);
    *out_ticks = ms;
    return BLE_NPL_OK;
}

ble_npl_time_t ble_npl_time_ms_to_ticks32(uint32_t ms) {
    DEBUG_TIME_printf("ble_npl_time_ms_to_ticks32(%u)\n", (uint)ms);
    return ms;
}

uint32_t ble_npl_time_ticks_to_ms32(ble_npl_time_t ticks) {
    DEBUG_TIME_printf("ble_npl_time_ticks_to_ms32(%u)\n", (uint)ticks);
    return ticks;
}

void ble_npl_time_delay(ble_npl_time_t ticks) {
    mp_hal_delay_ms(ticks + 1);
}

 










uint32_t ble_npl_hw_enter_critical(void) {
    DEBUG_CRIT_printf("ble_npl_hw_enter_critical()\n");
    MICROPY_PY_BLUETOOTH_ENTER
    return atomic_state;
}

void ble_npl_hw_exit_critical(uint32_t atomic_state) {
    MICROPY_PY_BLUETOOTH_EXIT
    DEBUG_CRIT_printf("ble_npl_hw_exit_critical(%u)\n", (uint)atomic_state);
}
