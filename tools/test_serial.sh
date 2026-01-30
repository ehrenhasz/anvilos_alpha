#!/bin/bash
echo "Starting AnvilOS v0.4.8 Test (SERIAL MODE)..."
echo "Output redirected to this terminal."
echo "Use 'Ctrl-a' then 'x' to exit QEMU."
qemu-system-x86_64 -cdrom build_artifacts/anvilos_v0.4.8.iso -nographic -no-reboot
