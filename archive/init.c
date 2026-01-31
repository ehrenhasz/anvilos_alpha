#include "kernel.h"

int main() {
    println(">> ANVIL_OS: BOOTING_SOVEREIGN_CORE");

    int32_t res = mount("proc", "/proc", "proc", 0, NULL);
    if (res == 0) {
        println(">> INIT: PROC_MOUNTED");
    }

    int32_t res2 = mount("sysfs", "/sys", "sysfs", 0, NULL);
    if (res2 == 0) {
        println(">> INIT: SYS_MOUNTED");
    }

    int32_t res3 = mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    if (res3 == 0) {
        println(">> INIT: DEV_MOUNTED");
    }

    println(">> INIT: STARTING_ANVIL_LAW");
    
    const char* path = "/bin/anvil";
    Args my_args = {
        .arg0 = "/bin/anvil",
        .arg1 = "/lib/init.mpy",
        .arg2 = NULL
    };
    
    execv(path, (char**)&my_args);

    println(">> FATAL: SOVEREIGN_ORCHESTRATOR_FAILED");
    
    while (1) {
        // The Hold
    }
    return 0;
}