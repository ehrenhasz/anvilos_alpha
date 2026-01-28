cat /dev/urandom \
| while true; do read junk; echo "junk $RANDOM $junk"; done \
| ../busybox gzip \
| ../busybox gunzip -c >/dev/null
