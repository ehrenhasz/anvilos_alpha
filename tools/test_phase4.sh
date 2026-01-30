#!/bin/bash
echo "Starting AnvilOS v0.4.19 Test..."
echo "Use 'Alt-2' then type 'quit' to exit."
echo "If that fails, use 'Ctrl-a' then 'x' (if in nographic mode) or kill this process."
qemu-system-x86_64 -cdrom build_artifacts/anvilos_v0.4.19.iso -display curses -no-reboot