#include "kernel.h"

int main() {
    println(">> ANVIL_OS: GENERATING_SYSTEM_AUTH");

    const char* path = "build_artifacts/rootfs_stage/etc/passwd";
    
    // Ensure dir exists
    mkdir("build_artifacts/rootfs_stage/etc", 0755);

    FILE* f = fopen(path, "w");
    if (f) {
        fprintf(f, "root:x:0:0:root:/root:/bin/bash\n");
        fclose(f);
        println(">> INIT: PASSWD_GENERATED");
    } else {
        println(">> INIT: PASSWD_FAILED");
    }
    return 0;
}