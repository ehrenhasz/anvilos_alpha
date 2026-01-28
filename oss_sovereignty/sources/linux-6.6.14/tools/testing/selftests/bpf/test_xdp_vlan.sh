readonly KSFT_SKIP=4
readonly NS1="ns1-$(mktemp -u XXXXXX)"
readonly NS2="ns2-$(mktemp -u XXXXXX)"
if [ -z "$TESTNAME" ]; then
    TESTNAME=xdp_vlan
fi
XDP_MODE=xdpgeneric
usage() {
  echo "Testing XDP + TC eBPF VLAN manipulations: $TESTNAME"
  echo ""
  echo "Usage: $0 [-vfh]"
  echo "  -v | --verbose : Verbose"
  echo "  --flush        : Flush before starting (e.g. after --interactive)"
  echo "  --interactive  : Keep netns setup running after test-run"
  echo "  --mode=XXX     : Choose XDP mode (xdp | xdpgeneric | xdpdrv)"
  echo ""
}
valid_xdp_mode()
{
	local mode=$1
	case "$mode" in
		xdpgeneric | xdpdrv | xdp)
			return 0
			;;
		*)
			return 1
	esac
}
cleanup()
{
	local status=$?
	if [ "$status" = "0" ]; then
		echo "selftests: $TESTNAME [PASS]";
	else
		echo "selftests: $TESTNAME [FAILED]";
	fi
	if [ -n "$INTERACTIVE" ]; then
		echo "Namespace setup still active explore with:"
		echo " ip netns exec ${NS1} bash"
		echo " ip netns exec ${NS2} bash"
		exit $status
	fi
	set +e
	ip link del veth1 2> /dev/null
	ip netns del ${NS1} 2> /dev/null
	ip netns del ${NS2} 2> /dev/null
}
OPTIONS=$(getopt -o hvfi: \
    --long verbose,flush,help,interactive,debug,mode: -- "$@")
if (( $? != 0 )); then
    usage
    echo "selftests: $TESTNAME [FAILED] Error calling getopt, unknown option?"
    exit 2
fi
eval set -- "$OPTIONS"
while true; do
	case "$1" in
	    -v | --verbose)
		export VERBOSE=yes
		shift
		;;
	    -i | --interactive | --debug )
		INTERACTIVE=yes
		shift
		;;
	    -f | --flush )
		cleanup
		shift
		;;
	    --mode )
		shift
		XDP_MODE=$1
		shift
		;;
	    -- )
		shift
		break
		;;
	    -h | --help )
		usage;
		echo "selftests: $TESTNAME [SKIP] usage help info requested"
		exit $KSFT_SKIP
		;;
	    * )
		shift
		break
		;;
	esac
done
if [ "$EUID" -ne 0 ]; then
	echo "selftests: $TESTNAME [FAILED] need root privileges"
	exit 1
fi
valid_xdp_mode $XDP_MODE
if [ $? -ne 0 ]; then
	echo "selftests: $TESTNAME [FAILED] unknown XDP mode ($XDP_MODE)"
	exit 1
fi
ip link set dev lo xdpgeneric off 2>/dev/null > /dev/null
if [ $? -ne 0 ]; then
	echo "selftests: $TESTNAME [SKIP] need ip xdp support"
	exit $KSFT_SKIP
fi
if [ -n "$INTERACTIVE" ]; then
	ip link del veth1 2> /dev/null
	ip netns del ${NS1} 2> /dev/null
	ip netns del ${NS2} 2> /dev/null
fi
set -e
which ip > /dev/null
which tc > /dev/null
which ethtool > /dev/null
if [ -n "$VERBOSE" ]; then
    set -v
fi
ip netns add ${NS1}
ip netns add ${NS2}
trap cleanup 0 2 3 6 9
ip link add veth1 type veth peer name veth2
ip link set veth1 netns ${NS1}
ip link set veth2 netns ${NS2}
ip netns exec ${NS1} ethtool -K veth1 rxvlan off
ip netns exec ${NS2} ethtool -K veth2 rxvlan off
ip netns exec ${NS2} ethtool -K veth2 txvlan off
ip netns exec ${NS1} ethtool -K veth1 txvlan off
export IPADDR1=100.64.41.1
export IPADDR2=100.64.41.2
ip netns exec ${NS1} ip addr add ${IPADDR1}/24 dev veth1
ip netns exec ${NS1} ip link set veth1 up
export VLAN=4011
export DEVNS2=veth2
ip netns exec ${NS2} ip link add link $DEVNS2 name $DEVNS2.$VLAN type vlan id $VLAN
ip netns exec ${NS2} ip addr add ${IPADDR2}/24 dev $DEVNS2.$VLAN
ip netns exec ${NS2} ip link set $DEVNS2 up
ip netns exec ${NS2} ip link set $DEVNS2.$VLAN up
ip netns exec ${NS1} ip link set lo up
ip netns exec ${NS2} ip link set lo up
ip netns exec ${NS2} sh -c 'ping -W 1 -c 1 100.64.41.1 || echo "Success: First ping must fail"'
export DEVNS1=veth1
export BPF_FILE=test_xdp_vlan.bpf.o
export XDP_PROG=xdp_vlan_change
ip netns exec ${NS1} ip link set $DEVNS1 $XDP_MODE object $BPF_FILE section $XDP_PROG
ip netns exec ${NS1} tc qdisc add dev $DEVNS1 clsact
ip netns exec ${NS1} tc filter add dev $DEVNS1 egress \
  prio 1 handle 1 bpf da obj $BPF_FILE sec tc_vlan_push
ip netns exec ${NS2} ping -i 0.2 -W 2 -c 2 $IPADDR1
ip netns exec ${NS1} ping -i 0.2 -W 2 -c 2 $IPADDR2
export XDP_PROG=xdp_vlan_remove_outer2
ip netns exec ${NS1} ip link set $DEVNS1 $XDP_MODE off
ip netns exec ${NS1} ip link set $DEVNS1 $XDP_MODE object $BPF_FILE section $XDP_PROG
ip netns exec ${NS2} ping -i 0.2 -W 2 -c 2 $IPADDR1
ip netns exec ${NS1} ping -i 0.2 -W 2 -c 2 $IPADDR2
