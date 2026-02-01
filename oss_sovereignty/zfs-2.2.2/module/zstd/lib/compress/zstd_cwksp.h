 

#ifndef ZSTD_CWKSP_H
#define ZSTD_CWKSP_H

 
#include "../common/zstd_internal.h"

#if defined (__cplusplus)
extern "C" {
#endif

 

 
#ifndef ZSTD_CWKSP_ASAN_REDZONE_SIZE
#define ZSTD_CWKSP_ASAN_REDZONE_SIZE 128
#endif

 
typedef enum {
    ZSTD_cwksp_alloc_objects,
    ZSTD_cwksp_alloc_buffers,
    ZSTD_cwksp_alloc_aligned
} ZSTD_cwksp_alloc_phase_e;

 
typedef struct {
    void* workspace;
    void* workspaceEnd;

    void* objectEnd;
    void* tableEnd;
    void* tableValidEnd;
    void* allocStart;

    int allocFailed;
    int workspaceOversizedDuration;
    ZSTD_cwksp_alloc_phase_e phase;
} ZSTD_cwksp;

 

MEM_STATIC size_t ZSTD_cwksp_available_space(ZSTD_cwksp* ws);

MEM_STATIC void ZSTD_cwksp_assert_internal_consistency(ZSTD_cwksp* ws) {
    (void)ws;
    assert(ws->workspace <= ws->objectEnd);
    assert(ws->objectEnd <= ws->tableEnd);
    assert(ws->objectEnd <= ws->tableValidEnd);
    assert(ws->tableEnd <= ws->allocStart);
    assert(ws->tableValidEnd <= ws->allocStart);
    assert(ws->allocStart <= ws->workspaceEnd);
}

 
MEM_STATIC size_t ZSTD_cwksp_align(size_t size, size_t const align) {
    size_t const mask = align - 1;
    assert((align & mask) == 0);
    return (size + mask) & ~mask;
}

 
MEM_STATIC size_t ZSTD_cwksp_alloc_size(size_t size) {
#if defined (ADDRESS_SANITIZER) && !defined (ZSTD_ASAN_DONT_POISON_WORKSPACE)
    return size + 2 * ZSTD_CWKSP_ASAN_REDZONE_SIZE;
#else
    return size;
#endif
}

MEM_STATIC void ZSTD_cwksp_internal_advance_phase(
        ZSTD_cwksp* ws, ZSTD_cwksp_alloc_phase_e phase) {
    assert(phase >= ws->phase);
    if (phase > ws->phase) {
        if (ws->phase < ZSTD_cwksp_alloc_buffers &&
                phase >= ZSTD_cwksp_alloc_buffers) {
            ws->tableValidEnd = ws->objectEnd;
        }
        if (ws->phase < ZSTD_cwksp_alloc_aligned &&
                phase >= ZSTD_cwksp_alloc_aligned) {
             
             
            ws->allocStart = (BYTE*)ws->allocStart - ((size_t)ws->allocStart & (sizeof(U32)-1));
            if (ws->allocStart < ws->tableValidEnd) {
                ws->tableValidEnd = ws->allocStart;
            }
        }
        ws->phase = phase;
    }
}

 
MEM_STATIC int ZSTD_cwksp_owns_buffer(const ZSTD_cwksp* ws, const void* ptr) {
    return (ptr != NULL) && (ws->workspace <= ptr) && (ptr <= ws->workspaceEnd);
}

 
MEM_STATIC void* ZSTD_cwksp_reserve_internal(
        ZSTD_cwksp* ws, size_t bytes, ZSTD_cwksp_alloc_phase_e phase) {
    void* alloc;
    void* bottom = ws->tableEnd;
    ZSTD_cwksp_internal_advance_phase(ws, phase);
    alloc = (BYTE *)ws->allocStart - bytes;

#if defined (ADDRESS_SANITIZER) && !defined (ZSTD_ASAN_DONT_POISON_WORKSPACE)
     
    alloc = (BYTE *)alloc - 2 * ZSTD_CWKSP_ASAN_REDZONE_SIZE;
#endif

    DEBUGLOG(5, "cwksp: reserving %p %zd bytes, %zd bytes remaining",
        alloc, bytes, ZSTD_cwksp_available_space(ws) - bytes);
    ZSTD_cwksp_assert_internal_consistency(ws);
    assert(alloc >= bottom);
    if (alloc < bottom) {
        DEBUGLOG(4, "cwksp: alloc failed!");
        ws->allocFailed = 1;
        return NULL;
    }
    if (alloc < ws->tableValidEnd) {
        ws->tableValidEnd = alloc;
    }
    ws->allocStart = alloc;

#if defined (ADDRESS_SANITIZER) && !defined (ZSTD_ASAN_DONT_POISON_WORKSPACE)
     
    alloc = (BYTE *)alloc + ZSTD_CWKSP_ASAN_REDZONE_SIZE;
    __asan_unpoison_memory_region(alloc, bytes);
#endif

    return alloc;
}

 
MEM_STATIC BYTE* ZSTD_cwksp_reserve_buffer(ZSTD_cwksp* ws, size_t bytes) {
    return (BYTE*)ZSTD_cwksp_reserve_internal(ws, bytes, ZSTD_cwksp_alloc_buffers);
}

 
MEM_STATIC void* ZSTD_cwksp_reserve_aligned(ZSTD_cwksp* ws, size_t bytes) {
    assert((bytes & (sizeof(U32)-1)) == 0);
    return ZSTD_cwksp_reserve_internal(ws, ZSTD_cwksp_align(bytes, sizeof(U32)), ZSTD_cwksp_alloc_aligned);
}

 
MEM_STATIC void* ZSTD_cwksp_reserve_table(ZSTD_cwksp* ws, size_t bytes) {
    const ZSTD_cwksp_alloc_phase_e phase = ZSTD_cwksp_alloc_aligned;
    void* alloc = ws->tableEnd;
    void* end = (BYTE *)alloc + bytes;
    void* top = ws->allocStart;

    DEBUGLOG(5, "cwksp: reserving %p table %zd bytes, %zd bytes remaining",
        alloc, bytes, ZSTD_cwksp_available_space(ws) - bytes);
    assert((bytes & (sizeof(U32)-1)) == 0);
    ZSTD_cwksp_internal_advance_phase(ws, phase);
    ZSTD_cwksp_assert_internal_consistency(ws);
    assert(end <= top);
    if (end > top) {
        DEBUGLOG(4, "cwksp: table alloc failed!");
        ws->allocFailed = 1;
        return NULL;
    }
    ws->tableEnd = end;

#if defined (ADDRESS_SANITIZER) && !defined (ZSTD_ASAN_DONT_POISON_WORKSPACE)
    __asan_unpoison_memory_region(alloc, bytes);
#endif

    return alloc;
}

 
MEM_STATIC void* ZSTD_cwksp_reserve_object(ZSTD_cwksp* ws, size_t bytes) {
    size_t roundedBytes = ZSTD_cwksp_align(bytes, sizeof(void*));
    void* alloc = ws->objectEnd;
    void* end = (BYTE*)alloc + roundedBytes;

#if defined (ADDRESS_SANITIZER) && !defined (ZSTD_ASAN_DONT_POISON_WORKSPACE)
     
    end = (BYTE *)end + 2 * ZSTD_CWKSP_ASAN_REDZONE_SIZE;
#endif

    DEBUGLOG(5,
        "cwksp: reserving %p object %zd bytes (rounded to %zd), %zd bytes remaining",
        alloc, bytes, roundedBytes, ZSTD_cwksp_available_space(ws) - roundedBytes);
    assert(((size_t)alloc & (sizeof(void*)-1)) == 0);
    assert((bytes & (sizeof(void*)-1)) == 0);
    ZSTD_cwksp_assert_internal_consistency(ws);
     
    if (ws->phase != ZSTD_cwksp_alloc_objects || end > ws->workspaceEnd) {
        DEBUGLOG(4, "cwksp: object alloc failed!");
        ws->allocFailed = 1;
        return NULL;
    }
    ws->objectEnd = end;
    ws->tableEnd = end;
    ws->tableValidEnd = end;

#if defined (ADDRESS_SANITIZER) && !defined (ZSTD_ASAN_DONT_POISON_WORKSPACE)
     
    alloc = (BYTE *)alloc + ZSTD_CWKSP_ASAN_REDZONE_SIZE;
    __asan_unpoison_memory_region(alloc, bytes);
#endif

    return alloc;
}

MEM_STATIC void ZSTD_cwksp_mark_tables_dirty(ZSTD_cwksp* ws) {
    DEBUGLOG(4, "cwksp: ZSTD_cwksp_mark_tables_dirty");

#if defined (MEMORY_SANITIZER) && !defined (ZSTD_MSAN_DONT_POISON_WORKSPACE)
     
    {
        size_t size = (BYTE*)ws->tableValidEnd - (BYTE*)ws->objectEnd;
        assert(__msan_test_shadow(ws->objectEnd, size) == -1);
        __msan_poison(ws->objectEnd, size);
    }
#endif

    assert(ws->tableValidEnd >= ws->objectEnd);
    assert(ws->tableValidEnd <= ws->allocStart);
    ws->tableValidEnd = ws->objectEnd;
    ZSTD_cwksp_assert_internal_consistency(ws);
}

MEM_STATIC void ZSTD_cwksp_mark_tables_clean(ZSTD_cwksp* ws) {
    DEBUGLOG(4, "cwksp: ZSTD_cwksp_mark_tables_clean");
    assert(ws->tableValidEnd >= ws->objectEnd);
    assert(ws->tableValidEnd <= ws->allocStart);
    if (ws->tableValidEnd < ws->tableEnd) {
        ws->tableValidEnd = ws->tableEnd;
    }
    ZSTD_cwksp_assert_internal_consistency(ws);
}

 
MEM_STATIC void ZSTD_cwksp_clean_tables(ZSTD_cwksp* ws) {
    DEBUGLOG(4, "cwksp: ZSTD_cwksp_clean_tables");
    assert(ws->tableValidEnd >= ws->objectEnd);
    assert(ws->tableValidEnd <= ws->allocStart);
    if (ws->tableValidEnd < ws->tableEnd) {
        memset(ws->tableValidEnd, 0, (BYTE*)ws->tableEnd - (BYTE*)ws->tableValidEnd);
    }
    ZSTD_cwksp_mark_tables_clean(ws);
}

 
MEM_STATIC void ZSTD_cwksp_clear_tables(ZSTD_cwksp* ws) {
    DEBUGLOG(4, "cwksp: clearing tables!");

#if defined (ADDRESS_SANITIZER) && !defined (ZSTD_ASAN_DONT_POISON_WORKSPACE)
    {
        size_t size = (BYTE*)ws->tableValidEnd - (BYTE*)ws->objectEnd;
        __asan_poison_memory_region(ws->objectEnd, size);
    }
#endif

    ws->tableEnd = ws->objectEnd;
    ZSTD_cwksp_assert_internal_consistency(ws);
}

 
MEM_STATIC void ZSTD_cwksp_clear(ZSTD_cwksp* ws) {
    DEBUGLOG(4, "cwksp: clearing!");

#if defined (MEMORY_SANITIZER) && !defined (ZSTD_MSAN_DONT_POISON_WORKSPACE)
     
    {
        size_t size = (BYTE*)ws->workspaceEnd - (BYTE*)ws->tableValidEnd;
        __msan_poison(ws->tableValidEnd, size);
    }
#endif

#if defined (ADDRESS_SANITIZER) && !defined (ZSTD_ASAN_DONT_POISON_WORKSPACE)
    {
        size_t size = (BYTE*)ws->workspaceEnd - (BYTE*)ws->objectEnd;
        __asan_poison_memory_region(ws->objectEnd, size);
    }
#endif

    ws->tableEnd = ws->objectEnd;
    ws->allocStart = ws->workspaceEnd;
    ws->allocFailed = 0;
    if (ws->phase > ZSTD_cwksp_alloc_buffers) {
        ws->phase = ZSTD_cwksp_alloc_buffers;
    }
    ZSTD_cwksp_assert_internal_consistency(ws);
}

 
MEM_STATIC void ZSTD_cwksp_init(ZSTD_cwksp* ws, void* start, size_t size) {
    DEBUGLOG(4, "cwksp: init'ing workspace with %zd bytes", size);
    assert(((size_t)start & (sizeof(void*)-1)) == 0);  
    ws->workspace = start;
    ws->workspaceEnd = (BYTE*)start + size;
    ws->objectEnd = ws->workspace;
    ws->tableValidEnd = ws->objectEnd;
    ws->phase = ZSTD_cwksp_alloc_objects;
    ZSTD_cwksp_clear(ws);
    ws->workspaceOversizedDuration = 0;
    ZSTD_cwksp_assert_internal_consistency(ws);
}

MEM_STATIC size_t ZSTD_cwksp_create(ZSTD_cwksp* ws, size_t size, ZSTD_customMem customMem) {
    void* workspace = ZSTD_malloc(size, customMem);
    DEBUGLOG(4, "cwksp: creating new workspace with %zd bytes", size);
    RETURN_ERROR_IF(workspace == NULL, memory_allocation, "NULL pointer!");
    ZSTD_cwksp_init(ws, workspace, size);
    return 0;
}

MEM_STATIC void ZSTD_cwksp_free(ZSTD_cwksp* ws, ZSTD_customMem customMem) {
    void *ptr = ws->workspace;
    DEBUGLOG(4, "cwksp: freeing workspace");
    memset(ws, 0, sizeof(ZSTD_cwksp));
    ZSTD_free(ptr, customMem);
}

 
MEM_STATIC void ZSTD_cwksp_move(ZSTD_cwksp* dst, ZSTD_cwksp* src) {
    *dst = *src;
    memset(src, 0, sizeof(ZSTD_cwksp));
}

MEM_STATIC size_t ZSTD_cwksp_sizeof(const ZSTD_cwksp* ws) {
    return (size_t)((BYTE*)ws->workspaceEnd - (BYTE*)ws->workspace);
}

MEM_STATIC int ZSTD_cwksp_reserve_failed(const ZSTD_cwksp* ws) {
    return ws->allocFailed;
}

 

MEM_STATIC size_t ZSTD_cwksp_available_space(ZSTD_cwksp* ws) {
    return (size_t)((BYTE*)ws->allocStart - (BYTE*)ws->tableEnd);
}

MEM_STATIC int ZSTD_cwksp_check_available(ZSTD_cwksp* ws, size_t additionalNeededSpace) {
    return ZSTD_cwksp_available_space(ws) >= additionalNeededSpace;
}

MEM_STATIC int ZSTD_cwksp_check_too_large(ZSTD_cwksp* ws, size_t additionalNeededSpace) {
    return ZSTD_cwksp_check_available(
        ws, additionalNeededSpace * ZSTD_WORKSPACETOOLARGE_FACTOR);
}

MEM_STATIC int ZSTD_cwksp_check_wasteful(ZSTD_cwksp* ws, size_t additionalNeededSpace) {
    return ZSTD_cwksp_check_too_large(ws, additionalNeededSpace)
        && ws->workspaceOversizedDuration > ZSTD_WORKSPACETOOLARGE_MAXDURATION;
}

MEM_STATIC void ZSTD_cwksp_bump_oversized_duration(
        ZSTD_cwksp* ws, size_t additionalNeededSpace) {
    if (ZSTD_cwksp_check_too_large(ws, additionalNeededSpace)) {
        ws->workspaceOversizedDuration++;
    } else {
        ws->workspaceOversizedDuration = 0;
    }
}

#if defined (__cplusplus)
}
#endif

#endif  
