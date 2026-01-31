#ifndef ANVIL_KERNEL_H
#define ANVIL_KERNEL_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#define copy_nonoverlapping(src, dst, count) __builtin_memcpy(dst, src, (count) * sizeof(*(src)))

typedef struct Args Args;
struct Args {
    const int8_t* arg0;
    const int8_t* arg1;
    const int8_t* arg2;
};

int main();

// Helper for Anvil's println
static inline void println(const char* msg) {
    printf("%s\n", msg);
}

#endif
