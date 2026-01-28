NS='stress_reuseport_listen_ns'
NR_FILES=24100
SAVED_NR_FILES=$(ulimit -n)
setup() {
	ip netns add $NS
	ip netns exec $NS sysctl -q -w net.ipv6.ip_nonlocal_bind=1
	ulimit -n $NR_FILES
}
cleanup() {
	ip netns del $NS
	ulimit -n $SAVED_NR_FILES
}
trap cleanup EXIT
setup
ip netns exec $NS ./stress_reuseport_listen 300 80
