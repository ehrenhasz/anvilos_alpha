ksft_skip=4
if ! which nettest >/dev/null; then
	PATH=$PWD:$PATH
	if ! which nettest >/dev/null; then
		echo "'nettest' command not found; skipping tests"
		exit $ksft_skip
	fi
fi
result=0
hide_output(){ exec 3>&1 4>&2 >/dev/null 2>/dev/null; }
show_output(){ exec >&3 2>&4; }
show_result(){
	if [ $1 -eq 0 ]; then
		printf "TEST: %-60s  [ OK ]\n" "${2}"
	else
		printf "TEST: %-60s  [FAIL]\n" "${2}"
		result=1
	fi
}
_do_segmenttest(){
	ip -n foo-ns address add $1/$3 dev foo || return 1
	ip -n foo-ns link set foo up || return 1
	ip -n bar-ns address add $2/$3 dev bar || return 1
	ip -n bar-ns link set bar up || return 1
	ip netns exec foo-ns timeout 2 ping -c 1 $2 || return 1
	ip netns exec bar-ns timeout 2 ping -c 1 $1 || return 1
	nettest -B -N bar-ns -O foo-ns -r $1 || return 1
	nettest -B -N foo-ns -O bar-ns -r $2 || return 1
	return 0
}
_do_route_test(){
	ip -n foo-ns address add $1/$5 dev foo || return 1
	ip -n foo-ns link set foo up || return 1
	ip -n foo-ns route add default via $2 || return 1
	ip -n bar-ns address add $4/$5 dev bar || return 1
	ip -n bar-ns link set bar up || return 1
	ip -n bar-ns route add default via $3 || return 1
	ip -n router-ns address add $2/$5 dev foo1 || return 1
	ip -n router-ns link set foo1 up || return 1
	ip -n router-ns address add $3/$5 dev bar1 || return 1
	ip -n router-ns link set bar1 up || return 1
	echo 1 | ip netns exec router-ns tee /proc/sys/net/ipv4/ip_forward
	ip netns exec foo-ns timeout 2 ping -c 1 $2 || return 1
	ip netns exec foo-ns timeout 2 ping -c 1 $4 || return 1
	ip netns exec bar-ns timeout 2 ping -c 1 $3 || return 1
	ip netns exec bar-ns timeout 2 ping -c 1 $1 || return 1
	nettest -B -N bar-ns -O foo-ns -r $1 || return 1
	nettest -B -N foo-ns -O bar-ns -r $4 || return 1
	return 0
}
segmenttest(){
	hide_output
	ip netns add foo-ns
	ip netns add bar-ns
	ip link add foo netns foo-ns type veth peer name bar netns bar-ns
	test_result=0
	_do_segmenttest "$@" || test_result=1
	ip netns pids foo-ns | xargs -r kill -9
	ip netns pids bar-ns | xargs -r kill -9
	ip netns del foo-ns
	ip netns del bar-ns
	show_output
	[ -n "$expect_failure" ] && test_result=`expr 1 - $test_result`
	show_result $test_result "$4"
}
route_test(){
	hide_output
	ip netns add foo-ns
	ip netns add bar-ns
	ip netns add router-ns
	ip link add foo netns foo-ns type veth peer name foo1 netns router-ns
	ip link add bar netns bar-ns type veth peer name bar1 netns router-ns
	test_result=0
	_do_route_test "$@" || test_result=1
	ip netns pids foo-ns | xargs -r kill -9
	ip netns pids bar-ns | xargs -r kill -9
	ip netns pids router-ns | xargs -r kill -9
	ip netns del foo-ns
	ip netns del bar-ns
	ip netns del router-ns
	show_output
	[ -n "$expect_failure" ] && test_result=`expr 1 - $test_result`
	show_result $test_result "$6"
}
echo "
echo "Unicast address extensions tests (behavior of reserved IPv4 addresses)"
echo "
segmenttest 240.1.2.1   240.1.2.4    24 "assign and ping within 240/4 (1 of 2) (is allowed)"
segmenttest 250.100.2.1 250.100.30.4 16 "assign and ping within 240/4 (2 of 2) (is allowed)"
segmenttest 0.1.2.17    0.1.2.23  24 "assign and ping within 0/8 (1 of 2) (is allowed)"
segmenttest 0.77.240.17 0.77.2.23 16 "assign and ping within 0/8 (2 of 2) (is allowed)"
segmenttest 255.255.3.1 255.255.50.77 16 "assign and ping inside 255.255/16 (is allowed)"
segmenttest 255.255.255.1 255.255.255.254 24 "assign and ping inside 255.255.255/24 (is allowed)"
route_test 240.5.6.7 240.5.6.1  255.1.2.1    255.1.2.3      24 "route between 240.5.6/24 and 255.1.2/24 (is allowed)"
route_test 0.200.6.7 0.200.38.1 245.99.101.1 245.99.200.111 16 "route between 0.200/16 and 245.99/16 (is allowed)"
segmenttest 5.10.15.20 5.10.15.0 24 "assign and ping lowest address (/24)"
segmenttest 192.168.101.192 192.168.101.193 26 "assign and ping lowest address (/26)"
route_test 192.168.42.1 192.168.42.0 9.8.7.6 9.8.7.0 24 "routing using lowest address"
expect_failure=true
segmenttest 0.0.1.5       0.0.0.0         16 "assigning 0.0.0.0 (is forbidden)"
segmenttest 255.255.255.1 255.255.255.255 16 "assigning 255.255.255.255 (is forbidden)"
segmenttest 127.99.4.5 127.99.4.6 16 "assign and ping inside 127/8 (is forbidden)"
segmenttest 225.1.2.3 225.1.2.200 24 "assign and ping class D address (is forbidden)"
route_test 225.1.42.1 225.1.42.2 9.8.7.6 9.8.7.1 24 "routing using class D (is forbidden)"
route_test 127.99.2.3 127.99.2.4 200.1.2.3 200.1.2.4 24 "routing using 127/8 (is forbidden)"
unset expect_failure
exit ${result}
