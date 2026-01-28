BPF_FILE="test_lwt_ip_encap.bpf.o"
if [[ $EUID -ne 0 ]]; then
	echo "This script must be run as root"
	echo "FAIL"
	exit 1
fi
readonly NS1="ns1-$(mktemp -u XXXXXX)"
readonly NS2="ns2-$(mktemp -u XXXXXX)"
readonly NS3="ns3-$(mktemp -u XXXXXX)"
readonly IPv4_1="172.16.1.100"
readonly IPv4_2="172.16.2.100"
readonly IPv4_3="172.16.3.100"
readonly IPv4_4="172.16.4.100"
readonly IPv4_5="172.16.5.100"
readonly IPv4_6="172.16.6.100"
readonly IPv4_7="172.16.7.100"
readonly IPv4_8="172.16.8.100"
readonly IPv4_GRE="172.16.16.100"
readonly IPv4_SRC=$IPv4_1
readonly IPv4_DST=$IPv4_4
readonly IPv6_1="fb01::1"
readonly IPv6_2="fb02::1"
readonly IPv6_3="fb03::1"
readonly IPv6_4="fb04::1"
readonly IPv6_5="fb05::1"
readonly IPv6_6="fb06::1"
readonly IPv6_7="fb07::1"
readonly IPv6_8="fb08::1"
readonly IPv6_GRE="fb10::1"
readonly IPv6_SRC=$IPv6_1
readonly IPv6_DST=$IPv6_4
TEST_STATUS=0
TESTS_SUCCEEDED=0
TESTS_FAILED=0
TMPFILE=""
process_test_results()
{
	if [[ "${TEST_STATUS}" -eq 0 ]] ; then
		echo "PASS"
		TESTS_SUCCEEDED=$((TESTS_SUCCEEDED+1))
	else
		echo "FAIL"
		TESTS_FAILED=$((TESTS_FAILED+1))
	fi
}
print_test_summary_and_exit()
{
	echo "passed tests: ${TESTS_SUCCEEDED}"
	echo "failed tests: ${TESTS_FAILED}"
	if [ "${TESTS_FAILED}" -eq "0" ] ; then
		exit 0
	else
		exit 1
	fi
}
setup()
{
	set -e  
	TEST_STATUS=0
	ip netns add "${NS1}"
	ip netns add "${NS2}"
	ip netns add "${NS3}"
	ip netns exec ${NS1} sysctl -wq net.ipv4.conf.all.rp_filter=0
	ip netns exec ${NS2} sysctl -wq net.ipv4.conf.all.rp_filter=0
	ip netns exec ${NS3} sysctl -wq net.ipv4.conf.all.rp_filter=0
	ip netns exec ${NS1} sysctl -wq net.ipv4.conf.default.rp_filter=0
	ip netns exec ${NS2} sysctl -wq net.ipv4.conf.default.rp_filter=0
	ip netns exec ${NS3} sysctl -wq net.ipv4.conf.default.rp_filter=0
	ip netns exec ${NS1} sysctl -wq net.ipv6.conf.all.accept_dad=0
	ip netns exec ${NS2} sysctl -wq net.ipv6.conf.all.accept_dad=0
	ip netns exec ${NS3} sysctl -wq net.ipv6.conf.all.accept_dad=0
	ip netns exec ${NS1} sysctl -wq net.ipv6.conf.default.accept_dad=0
	ip netns exec ${NS2} sysctl -wq net.ipv6.conf.default.accept_dad=0
	ip netns exec ${NS3} sysctl -wq net.ipv6.conf.default.accept_dad=0
	ip link add veth1 type veth peer name veth2
	ip link add veth3 type veth peer name veth4
	ip link add veth5 type veth peer name veth6
	ip link add veth7 type veth peer name veth8
	ip netns exec ${NS2} sysctl -wq net.ipv4.ip_forward=1
	ip netns exec ${NS2} sysctl -wq net.ipv6.conf.all.forwarding=1
	ip link set veth1 netns ${NS1}
	ip link set veth2 netns ${NS2}
	ip link set veth3 netns ${NS2}
	ip link set veth4 netns ${NS3}
	ip link set veth5 netns ${NS1}
	ip link set veth6 netns ${NS2}
	ip link set veth7 netns ${NS2}
	ip link set veth8 netns ${NS3}
	if [ ! -z "${VRF}" ] ; then
		ip -netns ${NS1} link add red type vrf table 1001
		ip -netns ${NS1} link set red up
		ip -netns ${NS1} route add table 1001 unreachable default metric 8192
		ip -netns ${NS1} -6 route add table 1001 unreachable default metric 8192
		ip -netns ${NS1} link set veth1 vrf red
		ip -netns ${NS1} link set veth5 vrf red
		ip -netns ${NS2} link add red type vrf table 1001
		ip -netns ${NS2} link set red up
		ip -netns ${NS2} route add table 1001 unreachable default metric 8192
		ip -netns ${NS2} -6 route add table 1001 unreachable default metric 8192
		ip -netns ${NS2} link set veth2 vrf red
		ip -netns ${NS2} link set veth3 vrf red
		ip -netns ${NS2} link set veth6 vrf red
		ip -netns ${NS2} link set veth7 vrf red
	fi
	ip -netns ${NS1}    addr add ${IPv4_1}/24  dev veth1
	ip -netns ${NS2}    addr add ${IPv4_2}/24  dev veth2
	ip -netns ${NS2}    addr add ${IPv4_3}/24  dev veth3
	ip -netns ${NS3}    addr add ${IPv4_4}/24  dev veth4
	ip -netns ${NS1} -6 addr add ${IPv6_1}/128 nodad dev veth1
	ip -netns ${NS2} -6 addr add ${IPv6_2}/128 nodad dev veth2
	ip -netns ${NS2} -6 addr add ${IPv6_3}/128 nodad dev veth3
	ip -netns ${NS3} -6 addr add ${IPv6_4}/128 nodad dev veth4
	ip -netns ${NS1}    addr add ${IPv4_5}/24  dev veth5
	ip -netns ${NS2}    addr add ${IPv4_6}/24  dev veth6
	ip -netns ${NS2}    addr add ${IPv4_7}/24  dev veth7
	ip -netns ${NS3}    addr add ${IPv4_8}/24  dev veth8
	ip -netns ${NS1} -6 addr add ${IPv6_5}/128 nodad dev veth5
	ip -netns ${NS2} -6 addr add ${IPv6_6}/128 nodad dev veth6
	ip -netns ${NS2} -6 addr add ${IPv6_7}/128 nodad dev veth7
	ip -netns ${NS3} -6 addr add ${IPv6_8}/128 nodad dev veth8
	ip -netns ${NS1} link set dev veth1 up
	ip -netns ${NS2} link set dev veth2 up
	ip -netns ${NS2} link set dev veth3 up
	ip -netns ${NS3} link set dev veth4 up
	ip -netns ${NS1} link set dev veth5 up
	ip -netns ${NS2} link set dev veth6 up
	ip -netns ${NS2} link set dev veth7 up
	ip -netns ${NS3} link set dev veth8 up
	ip -netns ${NS1}    route add ${IPv4_2}/32  dev veth1 ${VRF}
	ip -netns ${NS1}    route add default dev veth1 via ${IPv4_2} ${VRF}  
	ip -netns ${NS1} -6 route add ${IPv6_2}/128 dev veth1 ${VRF}
	ip -netns ${NS1} -6 route add default dev veth1 via ${IPv6_2} ${VRF}  
	ip -netns ${NS1}    route add ${IPv4_6}/32  dev veth5 ${VRF}
	ip -netns ${NS1}    route add ${IPv4_7}/32  dev veth5 via ${IPv4_6} ${VRF}
	ip -netns ${NS1}    route add ${IPv4_8}/32  dev veth5 via ${IPv4_6} ${VRF}
	ip -netns ${NS1} -6 route add ${IPv6_6}/128 dev veth5 ${VRF}
	ip -netns ${NS1} -6 route add ${IPv6_7}/128 dev veth5 via ${IPv6_6} ${VRF}
	ip -netns ${NS1} -6 route add ${IPv6_8}/128 dev veth5 via ${IPv6_6} ${VRF}
	ip -netns ${NS2}    route add ${IPv4_1}/32  dev veth2 ${VRF}
	ip -netns ${NS2}    route add ${IPv4_4}/32  dev veth3 ${VRF}
	ip -netns ${NS2} -6 route add ${IPv6_1}/128 dev veth2 ${VRF}
	ip -netns ${NS2} -6 route add ${IPv6_4}/128 dev veth3 ${VRF}
	ip -netns ${NS2}    route add ${IPv4_5}/32  dev veth6 ${VRF}
	ip -netns ${NS2}    route add ${IPv4_8}/32  dev veth7 ${VRF}
	ip -netns ${NS2} -6 route add ${IPv6_5}/128 dev veth6 ${VRF}
	ip -netns ${NS2} -6 route add ${IPv6_8}/128 dev veth7 ${VRF}
	ip -netns ${NS3}    route add ${IPv4_3}/32  dev veth4
	ip -netns ${NS3}    route add ${IPv4_1}/32  dev veth4 via ${IPv4_3}
	ip -netns ${NS3}    route add ${IPv4_2}/32  dev veth4 via ${IPv4_3}
	ip -netns ${NS3} -6 route add ${IPv6_3}/128 dev veth4
	ip -netns ${NS3} -6 route add ${IPv6_1}/128 dev veth4 via ${IPv6_3}
	ip -netns ${NS3} -6 route add ${IPv6_2}/128 dev veth4 via ${IPv6_3}
	ip -netns ${NS3}    route add ${IPv4_7}/32  dev veth8
	ip -netns ${NS3}    route add ${IPv4_5}/32  dev veth8 via ${IPv4_7}
	ip -netns ${NS3}    route add ${IPv4_6}/32  dev veth8 via ${IPv4_7}
	ip -netns ${NS3} -6 route add ${IPv6_7}/128 dev veth8
	ip -netns ${NS3} -6 route add ${IPv6_5}/128 dev veth8 via ${IPv6_7}
	ip -netns ${NS3} -6 route add ${IPv6_6}/128 dev veth8 via ${IPv6_7}
	ip -netns ${NS3} tunnel add gre_dev mode gre remote ${IPv4_1} local ${IPv4_GRE} ttl 255
	ip -netns ${NS3} link set gre_dev up
	ip -netns ${NS3} addr add ${IPv4_GRE} dev gre_dev
	ip -netns ${NS1} route add ${IPv4_GRE}/32 dev veth5 via ${IPv4_6} ${VRF}
	ip -netns ${NS2} route add ${IPv4_GRE}/32 dev veth7 via ${IPv4_8} ${VRF}
	ip -netns ${NS3} -6 tunnel add name gre6_dev mode ip6gre remote ${IPv6_1} local ${IPv6_GRE} ttl 255
	ip -netns ${NS3} link set gre6_dev up
	ip -netns ${NS3} -6 addr add ${IPv6_GRE} nodad dev gre6_dev
	ip -netns ${NS1} -6 route add ${IPv6_GRE}/128 dev veth5 via ${IPv6_6} ${VRF}
	ip -netns ${NS2} -6 route add ${IPv6_GRE}/128 dev veth7 via ${IPv6_8} ${VRF}
	TMPFILE=$(mktemp /tmp/test_lwt_ip_encap.XXXXXX)
	sleep 1  
	set +e
}
cleanup()
{
	if [ -f ${TMPFILE} ] ; then
		rm ${TMPFILE}
	fi
	ip netns del ${NS1} 2> /dev/null
	ip netns del ${NS2} 2> /dev/null
	ip netns del ${NS3} 2> /dev/null
}
trap cleanup EXIT
remove_routes_to_gredev()
{
	ip -netns ${NS1} route del ${IPv4_GRE} dev veth5 ${VRF}
	ip -netns ${NS2} route del ${IPv4_GRE} dev veth7 ${VRF}
	ip -netns ${NS1} -6 route del ${IPv6_GRE}/128 dev veth5 ${VRF}
	ip -netns ${NS2} -6 route del ${IPv6_GRE}/128 dev veth7 ${VRF}
}
add_unreachable_routes_to_gredev()
{
	ip -netns ${NS1} route add unreachable ${IPv4_GRE}/32 ${VRF}
	ip -netns ${NS2} route add unreachable ${IPv4_GRE}/32 ${VRF}
	ip -netns ${NS1} -6 route add unreachable ${IPv6_GRE}/128 ${VRF}
	ip -netns ${NS2} -6 route add unreachable ${IPv6_GRE}/128 ${VRF}
}
test_ping()
{
	local readonly PROTO=$1
	local readonly EXPECTED=$2
	local RET=0
	if [ "${PROTO}" == "IPv4" ] ; then
		ip netns exec ${NS1} ping  -c 1 -W 1 -I veth1 ${IPv4_DST} 2>&1 > /dev/null
		RET=$?
	elif [ "${PROTO}" == "IPv6" ] ; then
		ip netns exec ${NS1} ping6 -c 1 -W 1 -I veth1 ${IPv6_DST} 2>&1 > /dev/null
		RET=$?
	else
		echo "    test_ping: unknown PROTO: ${PROTO}"
		TEST_STATUS=1
	fi
	if [ "0" != "${RET}" ]; then
		RET=1
	fi
	if [ "${EXPECTED}" != "${RET}" ] ; then
		echo "    test_ping failed: expected: ${EXPECTED}; got ${RET}"
		TEST_STATUS=1
	fi
}
test_gso()
{
	local readonly PROTO=$1
	local readonly PKT_SZ=5000
	local IP_DST=""
	: > ${TMPFILE}  
	command -v nc >/dev/null 2>&1 || \
		{ echo >&2 "nc is not available: skipping TSO tests"; return; }
	if [ "${PROTO}" == "IPv4" ] ; then
		IP_DST=${IPv4_DST}
		ip netns exec ${NS3} bash -c \
			"nc -4 -l -p 9000 > ${TMPFILE} &"
	elif [ "${PROTO}" == "IPv6" ] ; then
		IP_DST=${IPv6_DST}
		ip netns exec ${NS3} bash -c \
			"nc -6 -l -p 9000 > ${TMPFILE} &"
		RET=$?
	else
		echo "    test_gso: unknown PROTO: ${PROTO}"
		TEST_STATUS=1
	fi
	sleep 1  
	ip netns exec ${NS1} bash -c \
		"dd if=/dev/zero bs=$PKT_SZ count=1 > /dev/tcp/${IP_DST}/9000 2>/dev/null"
	sleep 2 
	SZ=$(stat -c %s ${TMPFILE})
	if [ "$SZ" != "$PKT_SZ" ] ; then
		echo "    test_gso failed: ${PROTO}"
		TEST_STATUS=1
	fi
}
test_egress()
{
	local readonly ENCAP=$1
	echo "starting egress ${ENCAP} encap test ${VRF}"
	setup
	test_ping IPv4 0
	test_ping IPv6 0
	ip -netns ${NS2}    route del ${IPv4_DST}/32  dev veth3 ${VRF}
	ip -netns ${NS2} -6 route del ${IPv6_DST}/128 dev veth3 ${VRF}
	test_ping IPv4 1
	test_ping IPv6 1
	if [ "${ENCAP}" == "IPv4" ] ; then
		ip -netns ${NS1} route add ${IPv4_DST} encap bpf xmit obj \
			${BPF_FILE} sec encap_gre dev veth1 ${VRF}
		ip -netns ${NS1} -6 route add ${IPv6_DST} encap bpf xmit obj \
			${BPF_FILE} sec encap_gre dev veth1 ${VRF}
	elif [ "${ENCAP}" == "IPv6" ] ; then
		ip -netns ${NS1} route add ${IPv4_DST} encap bpf xmit obj \
			${BPF_FILE} sec encap_gre6 dev veth1 ${VRF}
		ip -netns ${NS1} -6 route add ${IPv6_DST} encap bpf xmit obj \
			${BPF_FILE} sec encap_gre6 dev veth1 ${VRF}
	else
		echo "    unknown encap ${ENCAP}"
		TEST_STATUS=1
	fi
	test_ping IPv4 0
	test_ping IPv6 0
	if [ -z "${VRF}" ] ; then
		test_gso IPv4
		test_gso IPv6
	fi
	remove_routes_to_gredev
	test_ping IPv4 1
	test_ping IPv6 1
	add_unreachable_routes_to_gredev
	test_ping IPv4 1
	test_ping IPv6 1
	cleanup
	process_test_results
}
test_ingress()
{
	local readonly ENCAP=$1
	echo "starting ingress ${ENCAP} encap test ${VRF}"
	setup
	test_ping IPv4 0
	test_ping IPv6 0
	ip -netns ${NS2}    route del ${IPv4_DST}/32  dev veth3 ${VRF}
	ip -netns ${NS2} -6 route del ${IPv6_DST}/128 dev veth3 ${VRF}
	test_ping IPv4 1
	test_ping IPv6 1
	if [ "${ENCAP}" == "IPv4" ] ; then
		ip -netns ${NS2} route add ${IPv4_DST} encap bpf in obj \
			${BPF_FILE} sec encap_gre dev veth2 ${VRF}
		ip -netns ${NS2} -6 route add ${IPv6_DST} encap bpf in obj \
			${BPF_FILE} sec encap_gre dev veth2 ${VRF}
	elif [ "${ENCAP}" == "IPv6" ] ; then
		ip -netns ${NS2} route add ${IPv4_DST} encap bpf in obj \
			${BPF_FILE} sec encap_gre6 dev veth2 ${VRF}
		ip -netns ${NS2} -6 route add ${IPv6_DST} encap bpf in obj \
			${BPF_FILE} sec encap_gre6 dev veth2 ${VRF}
	else
		echo "FAIL: unknown encap ${ENCAP}"
		TEST_STATUS=1
	fi
	test_ping IPv4 0
	test_ping IPv6 0
	remove_routes_to_gredev
	test_ping IPv4 1
	test_ping IPv6 1
	add_unreachable_routes_to_gredev
	test_ping IPv4 1
	test_ping IPv6 1
	cleanup
	process_test_results
}
VRF=""
test_egress IPv4
test_egress IPv6
test_ingress IPv4
test_ingress IPv6
VRF="vrf red"
test_egress IPv4
test_egress IPv6
test_ingress IPv4
test_ingress IPv6
print_test_summary_and_exit
