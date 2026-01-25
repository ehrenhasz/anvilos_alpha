import sys
import os
import json
sys.path.append("runtime")
from mainframe_client import MainframeClient

MAINFRAME = MainframeClient("/var/lib/anvilos/db/cortex.db")

CARDS = [
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "mkdir -p oss_sovereignty && cd oss_sovereignty && wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.6.14.tar.xz && tar -xvf linux-6.6.14.tar.xz",
            "timeout": 600
        },
        "desc": "FETCH_KERNEL"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/linux-6.6.14 && make defconfig && make kvm_guest.config",
            "timeout": 300
        },
        "desc": "CONFIG_KERNEL"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/linux-6.6.14 && make -j2 bzImage",
            "timeout": 1800
        },
        "desc": "BUILD_KERNEL"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty && wget https://busybox.net/downloads/busybox-1.36.1.tar.bz2 && tar -xvf busybox-1.36.1.tar.bz2",
            "timeout": 300
        },
        "desc": "FETCH_BUSYBOX"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/busybox-1.36.1 && make defconfig && sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config",
            "timeout": 300
        },
        "desc": "CONFIG_BUSYBOX"
    },
    {
        "op": "SYS_CMD",
        "pld": {
            "cmd": "cd oss_sovereignty/busybox-1.36.1 && make -j2",
            "timeout": 600
        },
        "desc": "BUILD_BUSYBOX"
    }
]

print("[*] Injecting Phase 2 Cards...")
for i, card in enumerate(CARDS):
    res = MAINFRAME.inject_card(card["op"], card["pld"])
    print(f"[{i+1}/{len(CARDS)}] {card['desc']}: {res}")
