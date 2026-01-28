readonly TARGET_IP="10.1.1.100"
readonly TARGET_NS="xdp_ns0"
readonly LOCAL_IP="10.1.1.200"
setup()
{
	ip netns add $TARGET_NS
	ip link add veth0 type veth peer name veth1
	ip link set veth0 netns $TARGET_NS
	ip netns exec $TARGET_NS ip addr add ${TARGET_IP}/24 dev veth0
	ip addr add ${LOCAL_IP}/24 dev veth1
	ip netns exec $TARGET_NS ip link set veth0 up
	ip link set veth1 up
}
cleanup()
{
	set +e
	ip netns delete $TARGET_NS 2>/dev/null
	ip link del veth1 2>/dev/null
	if [[ $server_pid -ne 0 ]]; then
		kill -TERM $server_pid
	fi
}
test()
{
	client_args="$1"
	server_args="$2"
	echo "Test client args '$client_args'; server args '$server_args'"
	server_pid=0
	if [[ -n "$server_args" ]]; then
		ip netns exec $TARGET_NS ./xdping $server_args &
		server_pid=$!
		sleep 10
	fi
	./xdping $client_args $TARGET_IP
	if [[ $server_pid -ne 0 ]]; then
		kill -TERM $server_pid
		server_pid=0
	fi
	echo "Test client args '$client_args'; server args '$server_args': PASS"
}
set -e
server_pid=0
trap cleanup EXIT
setup
for server_args in "" "-I veth0 -s -S" ; do
	client_args="-I veth1 -S"
	test "$client_args" "$server_args"
	client_args="-I veth1 -S -c 10"
	test "$client_args" "$server_args"
done
test "-I veth1 -N" "-I veth0 -s -N"
test "-I veth1 -N -c 10" "-I veth0 -s -N"
echo "OK. All tests passed"
exit 0
