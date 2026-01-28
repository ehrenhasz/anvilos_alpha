ERR=4 
PING6="ping"
PAUSE_ON_FAIL="no"
readonly NS0=$(mktemp -u ns0-XXXXXXXX)
readonly NS1=$(mktemp -u ns1-XXXXXXXX)
readonly NS2=$(mktemp -u ns2-XXXXXXXX)
readonly NS3=$(mktemp -u ns3-XXXXXXXX)
exit_cleanup()
{
	for ns in "$@"; do
		ip netns delete "${ns}" 2>/dev/null || true
	done
	if [ "${ERR}" -eq 4 ]; then
		echo "Error: Setting up the testing environment failed." >&2
	fi
	exit "${ERR}"
}
create_namespaces()
{
	ip netns add "${NS0}" || exit_cleanup
	ip netns add "${NS1}" || exit_cleanup "${NS0}"
	ip netns add "${NS2}" || exit_cleanup "${NS0}" "${NS1}"
	ip netns add "${NS3}" || exit_cleanup "${NS0}" "${NS1}" "${NS2}"
}
exit_cleanup_all()
{
	exit_cleanup "${NS0}" "${NS1}" "${NS2}" "${NS3}"
}
iface_config()
{
	local NS="${1}"; readonly NS
	local DEV="${2}"; readonly DEV
	local LOCAL_IP4="${3}"; readonly LOCAL_IP4
	local PEER_IP4="${4}"; readonly PEER_IP4
	local LOCAL_IP6="${5}"; readonly LOCAL_IP6
	local PEER_IP6="${6}"; readonly PEER_IP6
	ip -netns "${NS}" link set dev "${DEV}" up
	ip -netns "${NS}" address add dev "${DEV}" "${LOCAL_IP4}" peer "${PEER_IP4}"
	ip -netns "${NS}" address add dev "${DEV}" "${LOCAL_IP6}" peer "${PEER_IP6}" nodad
}
setup_underlay()
{
	for ns in "${NS0}" "${NS1}" "${NS2}" "${NS3}"; do
		ip -netns "${ns}" link set dev lo up
	done;
	ip link add name veth01 netns "${NS0}" type veth peer name veth10 netns "${NS1}"
	ip link add name veth12 netns "${NS1}" type veth peer name veth21 netns "${NS2}"
	ip link add name veth23 netns "${NS2}" type veth peer name veth32 netns "${NS3}"
	iface_config "${NS0}" veth01 192.0.2.10 192.0.2.11/32 2001:db8::10 2001:db8::11/128
	iface_config "${NS1}" veth10 192.0.2.11 192.0.2.10/32 2001:db8::11 2001:db8::10/128
	iface_config "${NS1}" veth12 192.0.2.21 192.0.2.22/32 2001:db8::21 2001:db8::22/128
	iface_config "${NS2}" veth21 192.0.2.22 192.0.2.21/32 2001:db8::22 2001:db8::21/128
	iface_config "${NS2}" veth23 192.0.2.32 192.0.2.33/32 2001:db8::32 2001:db8::33/128
	iface_config "${NS3}" veth32 192.0.2.33 192.0.2.32/32 2001:db8::33 2001:db8::32/128
	tc -netns "${NS1}" qdisc add dev veth10 ingress
	tc -netns "${NS2}" qdisc add dev veth23 ingress
}
setup_overlay_ipv4()
{
	ip -netns "${NS0}" address add 192.0.2.100/32 dev lo
	ip -netns "${NS3}" address add 192.0.2.103/32 dev lo
	ip -netns "${NS0}" route add 192.0.2.103/32 src 192.0.2.100 via 192.0.2.11
	ip -netns "${NS3}" route add 192.0.2.100/32 src 192.0.2.103 via 192.0.2.32
	ip netns exec "${NS1}" sysctl -qw net.ipv4.ip_forward=1
	ip netns exec "${NS2}" sysctl -qw net.ipv4.ip_forward=1
	ip -netns "${NS1}" route add 192.0.2.100/32 via 192.0.2.10
	ip -netns "${NS2}" route add 192.0.2.103/32 via 192.0.2.33
	ip netns exec "${NS1}" sysctl -qw net.ipv4.conf.all.rp_filter=0
	ip netns exec "${NS2}" sysctl -qw net.ipv4.conf.all.rp_filter=0
	ip netns exec "${NS1}" sysctl -qw net.ipv4.conf.default.rp_filter=0
	ip netns exec "${NS2}" sysctl -qw net.ipv4.conf.default.rp_filter=0
}
setup_overlay_ipv6()
{
	ip -netns "${NS0}" address add 2001:db8::100/128 dev lo
	ip -netns "${NS3}" address add 2001:db8::103/128 dev lo
	ip -netns "${NS0}" route add 2001:db8::103/128 src 2001:db8::100 via 2001:db8::11
	ip -netns "${NS3}" route add 2001:db8::100/128 src 2001:db8::103 via 2001:db8::32
	ip netns exec "${NS1}" sysctl -qw net.ipv6.conf.all.forwarding=1
	ip netns exec "${NS2}" sysctl -qw net.ipv6.conf.all.forwarding=1
	ip -netns "${NS1}" route add 2001:db8::100/128 via 2001:db8::10
	ip -netns "${NS2}" route add 2001:db8::103/128 via 2001:db8::33
}
setup_overlay_mpls()
{
	ip -netns "${NS0}" address add 2001:db8::200/128 dev lo
	ip -netns "${NS3}" address add 2001:db8::203/128 dev lo
	ip -netns "${NS0}" route add 2001:db8::203/128 src 2001:db8::200 encap mpls 203 via 2001:db8::11
	ip -netns "${NS3}" route add 2001:db8::200/128 src 2001:db8::203 encap mpls 200 via 2001:db8::32
	ip netns exec "${NS1}" sysctl -qw net.mpls.platform_labels=256
	ip netns exec "${NS2}" sysctl -qw net.mpls.platform_labels=256
	ip -netns "${NS1}" -family mpls route add 200 via inet6 2001:db8::10
	ip -netns "${NS2}" -family mpls route add 203 via inet6 2001:db8::33
}
ping_test_one()
{
	local PING="$1"; readonly PING
	local IP="$2"; readonly IP
	local MSG="$3"; readonly MSG
	local RET
	printf "TEST: %-60s  " "${MSG}"
	set +e
	ip netns exec "${NS0}" "${PING}" -w 5 -c 1 "${IP}" > /dev/null 2>&1
	RET=$?
	set -e
	if [ "${RET}" -eq 0 ]; then
		printf "[ OK ]\n"
	else
		ERR=1
		printf "[FAIL]\n"
		if [ "${PAUSE_ON_FAIL}" = "yes" ]; then
			printf "\nHit enter to continue, 'q' to quit\n"
			read a
			if [ "$a" = "q" ]; then
				exit 1
			fi
		fi
	fi
}
ping_test()
{
	local UNDERLAY="$1"; readonly UNDERLAY
	local MODE
	local MSG
	if [ "${MULTIPROTO}" = "multiproto" ]; then
		MODE=" (multiproto mode)"
	else
		MODE=""
	fi
	if [ $IPV4 ]; then
		ping_test_one "ping" "192.0.2.103" "IPv4 packets over ${UNDERLAY}${MODE}"
	fi
	if [ $IPV6 ]; then
		ping_test_one "${PING6}" "2001:db8::103" "IPv6 packets over ${UNDERLAY}${MODE}"
	fi
	if [ $MPLS_UC ]; then
		ping_test_one "${PING6}" "2001:db8::203" "Unicast MPLS packets over ${UNDERLAY}${MODE}"
	fi
}
test_overlay()
{
	local ETHERTYPE="$1"; readonly ETHERTYPE
	local MULTIPROTO="$2"; readonly MULTIPROTO
	local IPV4
	local IPV6
	local MPLS_UC
	case "${ETHERTYPE}" in
		"ipv4")
			IPV4="ipv4"
			if [ "${MULTIPROTO}" = "multiproto" ]; then
				IPV6="ipv6"
			else
				IPV6=""
			fi
			MPLS_UC=""
			;;
		"ipv6")
			IPV6="ipv6"
			IPV4=""
			MPLS_UC=""
			;;
		"mpls_uc")
			MPLS_UC="mpls_uc"
			IPV4=""
			IPV6=""
			;;
		*)
			exit 1
			;;
	esac
	readonly IPV4
	readonly IPV6
	readonly MPLS_UC
	ip -netns "${NS1}" link add name bareudp_ns1 up type bareudp dstport 6635 ethertype "${ETHERTYPE}" "${MULTIPROTO}"
	ip -netns "${NS2}" link add name bareudp_ns2 up type bareudp dstport 6635 ethertype "${ETHERTYPE}" "${MULTIPROTO}"
	if [ $IPV4 ]; then
		tc -netns "${NS1}" filter add dev veth10 ingress protocol ipv4         \
			flower dst_ip 192.0.2.103/32                                   \
			action tunnel_key set src_ip 192.0.2.21 dst_ip 192.0.2.22 id 0 \
			action mirred egress redirect dev bareudp_ns1
		tc -netns "${NS2}" filter add dev veth23 ingress protocol ipv4         \
			flower dst_ip 192.0.2.100/32                                   \
			action tunnel_key set src_ip 192.0.2.22 dst_ip 192.0.2.21 id 0 \
			action mirred egress redirect dev bareudp_ns2
	fi
	if [ $IPV6 ]; then
		tc -netns "${NS1}" filter add dev veth10 ingress protocol ipv6         \
			flower dst_ip 2001:db8::103/128                                \
			action tunnel_key set src_ip 192.0.2.21 dst_ip 192.0.2.22 id 0 \
			action mirred egress redirect dev bareudp_ns1
		tc -netns "${NS2}" filter add dev veth23 ingress protocol ipv6         \
			flower dst_ip 2001:db8::100/128                                \
			action tunnel_key set src_ip 192.0.2.22 dst_ip 192.0.2.21 id 0 \
			action mirred egress redirect dev bareudp_ns2
	fi
	if [ $MPLS_UC ]; then
		ip netns exec "${NS1}" sysctl -qw net.mpls.conf.bareudp_ns1.input=1
		ip netns exec "${NS2}" sysctl -qw net.mpls.conf.bareudp_ns2.input=1
		tc -netns "${NS1}" filter add dev veth10 ingress protocol mpls_uc      \
			flower mpls_label 203                                          \
			action tunnel_key set src_ip 192.0.2.21 dst_ip 192.0.2.22 id 0 \
			action mirred egress redirect dev bareudp_ns1
		tc -netns "${NS2}" filter add dev veth23 ingress protocol mpls_uc      \
			flower mpls_label 200                                          \
			action tunnel_key set src_ip 192.0.2.22 dst_ip 192.0.2.21 id 0 \
			action mirred egress redirect dev bareudp_ns2
	fi
	ping_test "UDPv4"
	tc -netns "${NS1}" filter delete dev veth10 ingress
	tc -netns "${NS2}" filter delete dev veth23 ingress
	if [ $IPV4 ]; then
		tc -netns "${NS1}" filter add dev veth10 ingress protocol ipv4             \
			flower dst_ip 192.0.2.103/32                                       \
			action tunnel_key set src_ip 2001:db8::21 dst_ip 2001:db8::22 id 0 \
			action mirred egress redirect dev bareudp_ns1
		tc -netns "${NS2}" filter add dev veth23 ingress protocol ipv4             \
			flower dst_ip 192.0.2.100/32                                       \
			action tunnel_key set src_ip 2001:db8::22 dst_ip 2001:db8::21 id 0 \
			action mirred egress redirect dev bareudp_ns2
	fi
	if [ $IPV6 ]; then
		tc -netns "${NS1}" filter add dev veth10 ingress protocol ipv6             \
			flower dst_ip 2001:db8::103/128                                    \
			action tunnel_key set src_ip 2001:db8::21 dst_ip 2001:db8::22 id 0 \
			action mirred egress redirect dev bareudp_ns1
		tc -netns "${NS2}" filter add dev veth23 ingress protocol ipv6             \
			flower dst_ip 2001:db8::100/128                                    \
			action tunnel_key set src_ip 2001:db8::22 dst_ip 2001:db8::21 id 0 \
			action mirred egress redirect dev bareudp_ns2
	fi
	if [ $MPLS_UC ]; then
		tc -netns "${NS1}" filter add dev veth10 ingress protocol mpls_uc          \
			flower mpls_label 203                                              \
			action tunnel_key set src_ip 2001:db8::21 dst_ip 2001:db8::22 id 0 \
			action mirred egress redirect dev bareudp_ns1
		tc -netns "${NS2}" filter add dev veth23 ingress protocol mpls_uc          \
			flower mpls_label 200                                              \
			action tunnel_key set src_ip 2001:db8::22 dst_ip 2001:db8::21 id 0 \
			action mirred egress redirect dev bareudp_ns2
	fi
	ping_test "UDPv6"
	tc -netns "${NS1}" filter delete dev veth10 ingress
	tc -netns "${NS2}" filter delete dev veth23 ingress
	ip -netns "${NS1}" link delete bareudp_ns1
	ip -netns "${NS2}" link delete bareudp_ns2
}
check_features()
{
	ip link help 2>&1 | grep -q bareudp
	if [ $? -ne 0 ]; then
		echo "Missing bareudp support in iproute2" >&2
		exit_cleanup
	fi
	ping -w 1 -c 1 ::1 > /dev/null 2>&1 || PING6="ping6"
}
usage()
{
	echo "Usage: $0 [-p]"
	exit 1
}
while getopts :p o
do
	case $o in
		p) PAUSE_ON_FAIL="yes";;
		*) usage;;
	esac
done
check_features
create_namespaces
set -e
trap exit_cleanup_all EXIT
setup_underlay
setup_overlay_ipv4
setup_overlay_ipv6
setup_overlay_mpls
test_overlay ipv4 nomultiproto
test_overlay ipv6 nomultiproto
test_overlay ipv4 multiproto
test_overlay mpls_uc nomultiproto
if [ "${ERR}" -eq 1 ]; then
	echo "Some tests failed." >&2
else
	ERR=0
fi
