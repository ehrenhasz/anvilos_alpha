echo "Warning: '${INSTALLKERNEL}' command not available - additional " \
     "bootloader config required" >&2
if [ -f "$4/vmlinuz-$1" ]; then mv -- "$4/vmlinuz-$1" "$4/vmlinuz-$1.old"; fi
if [ -f "$4/System.map-$1" ]; then mv -- "$4/System.map-$1" "$4/System.map-$1.old"; fi
cat -- "$2" > "$4/vmlinuz-$1"
cp -- "$3" "$4/System.map-$1"
