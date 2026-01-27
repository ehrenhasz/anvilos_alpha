#include "kernel.h"

void main() {

    __auto_type welcome = "ANVIL SOVEREIGN INIT (LAW) ACTIVE\n";
    
    // 1. Write Welcome
    {
        __asm__ volatile("syscall" :  : "a"(1), "D"(1), "S"(welcome), "d"(34) : );;
    }

    // 2. Simple spin - In a real kernel we would mount here
    // For Phase 2, we just prove the Law is running.
    while(1){
        // The hold is eternal.
    }

}
