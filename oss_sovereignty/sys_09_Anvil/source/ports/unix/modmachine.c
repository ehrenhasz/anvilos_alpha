 




#if MICROPY_PLAT_DEV_MEM
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#define MICROPY_PAGE_SIZE 4096
#define MICROPY_PAGE_MASK (MICROPY_PAGE_SIZE - 1)
#endif


int pyexec_system_exit = 0;

uintptr_t mod_machine_mem_get_addr(mp_obj_t addr_o, uint align) {
    uintptr_t addr = mp_obj_get_int_truncated(addr_o);
    if ((addr & (align - 1)) != 0) {
        mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("address %08x is not aligned to %d bytes"), addr, align);
    }
    #if MICROPY_PLAT_DEV_MEM
    {
        
        static int fd;
        static uintptr_t last_base = (uintptr_t)-1;
        static uintptr_t map_page;
        if (!fd) {
            int _fd = open("/dev/mem", O_RDWR | O_SYNC);
            if (_fd == -1) {
                mp_raise_OSError(errno);
            }
            fd = _fd;
        }

        uintptr_t cur_base = addr & ~MICROPY_PAGE_MASK;
        if (cur_base != last_base) {
            map_page = (uintptr_t)mmap(NULL, MICROPY_PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, cur_base);
            last_base = cur_base;
        }
        addr = map_page + (addr & MICROPY_PAGE_MASK);
    }
    #endif

    return addr;
}

static void mp_machine_idle(void) {
    #ifdef MICROPY_UNIX_MACHINE_IDLE
    MICROPY_UNIX_MACHINE_IDLE
    #else
    
    #endif
}
