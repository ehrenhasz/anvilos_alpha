gzip="gzip"
gunzip="../busybox gunzip"
c=0
i=$PID
while true; do
    c=$((c+1))
    len1=$(( (((RANDOM*RANDOM)^i) & 0x7ffffff) % 100003 ))
    i=$((i * 1664525 + 1013904223))
    len2=$(( (((RANDOM*RANDOM)^i) & 0x7ffffff) % 100003 ))
    cat /dev/urandom | while true; do read junk; echo "junk $c $i $junk"; done \
    | dd bs=$len1 count=1 >z1 2>/dev/null
    cat /dev/urandom | while true; do read junk; echo "junk $c $i $junk"; done \
    | dd bs=$len2 count=1 >z2 2>/dev/null
    $gzip <z1 >zz.gz
    $gzip <z2 >>zz.gz
    $gunzip -c zz.gz >z9 || {
	echo "Exitcode $?"
	exit
    }
    sum=`cat z1 z2 | md5sum`
    sum9=`md5sum <z9`
    test "$sum" == "$sum9" || {
	echo "md5sums don't match"
	exit
    }
    echo "Test $c ok: len1=$len1 len2=$len2 sum=$sum"
    sum=`cat z1 z2 z1 z2 | md5sum`
    rm z1.gz z2.gz 2>/dev/null
    $gzip z1
    $gzip z2
    cat z1.gz z2.gz z1.gz z2.gz >zz.gz
    $gunzip -c zz.gz >z9 || {
	echo "Exitcode $? (2)"
	exit
    }
    sum9=`md5sum <z9`
    test "$sum" == "$sum9" || {
	echo "md5sums don't match (1)"
	exit
    }
    echo "Test $c ok: len1=$len1 len2=$len2 sum=$sum (2)"
done
