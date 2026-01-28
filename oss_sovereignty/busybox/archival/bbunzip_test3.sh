i=$PID
c=0
while true; do
    c=$((c + 1))
    echo "Block# $c" >&2
    i=$((i * 1664525 + 1013904223))
    len=$(( (((RANDOM*RANDOM)^i) & 0x7ffffff) % 100003 ))
    cat /dev/urandom \
    | while true; do read junk; echo "junk $c $i $junk"; done \
    | dd bs=$len count=1 2>/dev/null \
    | gzip >xxx.gz
    cat xxx.gz xxx.gz xxx.gz xxx.gz xxx.gz xxx.gz xxx.gz xxx.gz
done | ../busybox gunzip -c >/dev/null
