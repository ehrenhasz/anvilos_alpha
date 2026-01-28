lsmod | grep chacha | cut -f1 -d' ' | xargs rmmod
modprobe chacha_generic
modprobe chacha_s390
insmod test_cipher.ko size=63
insmod test_cipher.ko size=64
insmod test_cipher.ko size=65
insmod test_cipher.ko size=127
insmod test_cipher.ko size=128
insmod test_cipher.ko size=129
insmod test_cipher.ko size=511
insmod test_cipher.ko size=512
insmod test_cipher.ko size=513
insmod test_cipher.ko size=4096
insmod test_cipher.ko size=65611
insmod test_cipher.ko size=6291456
insmod test_cipher.ko size=62914560
dmesg | tail -170
