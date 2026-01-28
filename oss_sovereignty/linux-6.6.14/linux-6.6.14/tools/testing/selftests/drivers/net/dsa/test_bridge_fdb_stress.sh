WAIT_TIME=1
NUM_NETIFS=1
REQUIRE_JQ="no"
REQUIRE_MZ="no"
NETIF_CREATE="no"
lib_dir=$(dirname "$0")
source "$lib_dir"/lib.sh
cleanup() {
	echo "Cleaning up"
	kill $pid && wait $pid &> /dev/null
	ip link del br0
	echo "Please check kernel log for errors"
}
trap 'cleanup' EXIT
eth=${NETIFS[p1]}
ip link del br0 2>&1 >/dev/null || :
ip link add br0 type bridge && ip link set $eth master br0
(while :; do
	bridge fdb add 00:01:02:03:04:05 dev $eth master static
	bridge fdb del 00:01:02:03:04:05 dev $eth master static
done) &
pid=$!
for i in $(seq 1 50); do
	bridge fdb show > /dev/null
	sleep 3
	echo "$((${i} * 2))% complete..."
done
