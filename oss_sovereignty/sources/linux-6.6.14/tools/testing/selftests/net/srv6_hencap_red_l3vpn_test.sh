readonly ksft_skip=4
readonly RDMSUFF="$(mktemp -u XXXXXXXX)"
readonly VRF_TID=100
readonly VRF_DEVNAME="vrf-${VRF_TID}"
readonly RT2HS_DEVNAME="veth-t${VRF_TID}"
readonly LOCALSID_TABLE_ID=90
readonly IPv6_RT_NETWORK=fcf0:0
readonly IPv6_HS_NETWORK=cafe
readonly IPv4_HS_NETWORK=10.0.0
readonly VPN_LOCATOR_SERVICE=fcff
readonly END_FUNC=000e
readonly DT46_FUNC=0d46
PING_TIMEOUT_SEC=4
PAUSE_ON_FAIL=${PAUSE_ON_FAIL:=no}
ROUTERS=''
HOSTS=''
SETUP_ERR=1
ret=${ksft_skip}
nsuccess=0
nfail=0
log_test()
{
	local rc="$1"
	local expected="$2"
	local msg="$3"
	if [ "${rc}" -eq "${expected}" ]; then
		nsuccess=$((nsuccess+1))
		printf "\n    TEST: %-60s  [ OK ]\n" "${msg}"
	else
		ret=1
		nfail=$((nfail+1))
		printf "\n    TEST: %-60s  [FAIL]\n" "${msg}"
		if [ "${PAUSE_ON_FAIL}" = "yes" ]; then
			echo
			echo "hit enter to continue, 'q' to quit"
			read a
			[ "$a" = "q" ] && exit 1
		fi
	fi
}
print_log_test_results()
{
	printf "\nTests passed: %3d\n" "${nsuccess}"
	printf "Tests failed: %3d\n"   "${nfail}"
	if [ "${ret}" -ne 1 ]; then
		ret=0
	fi
}
log_section()
{
	echo
	echo "
	echo "TEST SECTION: $*"
	echo "
}
test_command_or_ksft_skip()
{
	local cmd="$1"
	if [ ! -x "$(command -v "${cmd}")" ]; then
		echo "SKIP: Could not run test without \"${cmd}\" tool";
		exit "${ksft_skip}"
	fi
}
get_nodename()
{
	local name="$1"
	echo "${name}-${RDMSUFF}"
}
get_rtname()
{
	local rtid="$1"
	get_nodename "rt-${rtid}"
}
get_hsname()
{
	local hsid="$1"
	get_nodename "hs-${hsid}"
}
__create_namespace()
{
	local name="$1"
	ip netns add "${name}"
}
create_router()
{
	local rtid="$1"
	local nsname
	nsname="$(get_rtname "${rtid}")"
	__create_namespace "${nsname}"
}
create_host()
{
	local hsid="$1"
	local nsname
	nsname="$(get_hsname "${hsid}")"
	__create_namespace "${nsname}"
}
cleanup()
{
	local nsname
	local i
	for i in ${ROUTERS}; do
		nsname="$(get_rtname "${i}")"
		ip netns del "${nsname}" &>/dev/null || true
	done
	for i in ${HOSTS}; do
		nsname="$(get_hsname "${i}")"
		ip netns del "${nsname}" &>/dev/null || true
	done
	if [ "${SETUP_ERR}" -ne 0 ]; then
		echo "SKIP: Setting up the testing environment failed"
		exit "${ksft_skip}"
	fi
	exit "${ret}"
}
add_link_rt_pairs()
{
	local rt="$1"
	local rt_neighs="$2"
	local neigh
	local nsname
	local neigh_nsname
	nsname="$(get_rtname "${rt}")"
	for neigh in ${rt_neighs}; do
		neigh_nsname="$(get_rtname "${neigh}")"
		ip link add "veth-rt-${rt}-${neigh}" netns "${nsname}" \
			type veth peer name "veth-rt-${neigh}-${rt}" \
			netns "${neigh_nsname}"
	done
}
get_network_prefix()
{
	local rt="$1"
	local neigh="$2"
	local p="${rt}"
	local q="${neigh}"
	if [ "${p}" -gt "${q}" ]; then
		p="${q}"; q="${rt}"
	fi
	echo "${IPv6_RT_NETWORK}:${p}:${q}"
}
setup_rt_networking()
{
	local rt="$1"
	local rt_neighs="$2"
	local nsname
	local net_prefix
	local devname
	local neigh
	nsname="$(get_rtname "${rt}")"
	for neigh in ${rt_neighs}; do
		devname="veth-rt-${rt}-${neigh}"
		net_prefix="$(get_network_prefix "${rt}" "${neigh}")"
		ip -netns "${nsname}" addr \
			add "${net_prefix}::${rt}/64" dev "${devname}" nodad
		ip -netns "${nsname}" link set "${devname}" up
	done
	ip -netns "${nsname}" link set lo up
	ip netns exec "${nsname}" sysctl -wq net.ipv6.conf.all.accept_dad=0
	ip netns exec "${nsname}" sysctl -wq net.ipv6.conf.default.accept_dad=0
	ip netns exec "${nsname}" sysctl -wq net.ipv6.conf.all.forwarding=1
	ip netns exec "${nsname}" sysctl -wq net.ipv4.conf.all.rp_filter=0
	ip netns exec "${nsname}" sysctl -wq net.ipv4.conf.default.rp_filter=0
	ip netns exec "${nsname}" sysctl -wq net.ipv4.ip_forward=1
}
setup_rt_local_sids()
{
	local rt="$1"
	local rt_neighs="$2"
	local net_prefix
	local devname
	local nsname
	local neigh
	nsname="$(get_rtname "${rt}")"
	for neigh in ${rt_neighs}; do
		devname="veth-rt-${rt}-${neigh}"
		net_prefix="$(get_network_prefix "${rt}" "${neigh}")"
		ip -netns "${nsname}" -6 route \
			add "${VPN_LOCATOR_SERVICE}:${neigh}::/32" \
			table "${LOCALSID_TABLE_ID}" \
			via "${net_prefix}::${neigh}" dev "${devname}"
	done
	ip -netns "${nsname}" -6 route \
		add "${VPN_LOCATOR_SERVICE}:${rt}::${END_FUNC}" \
		table "${LOCALSID_TABLE_ID}" \
		encap seg6local action End dev "${VRF_DEVNAME}"
	ip -netns "${nsname}" -6 route \
		add "${VPN_LOCATOR_SERVICE}:${rt}::${DT46_FUNC}" \
		table "${LOCALSID_TABLE_ID}" \
		encap seg6local action End.DT46 vrftable "${VRF_TID}" \
		dev "${VRF_DEVNAME}"
	ip -netns "${nsname}" -6 rule \
		add to "${VPN_LOCATOR_SERVICE}::/16" \
		lookup "${LOCALSID_TABLE_ID}" prio 999
	ip -netns "${nsname}" -6 route \
		add unreachable default metric 4278198272 \
		vrf "${VRF_DEVNAME}"
	ip -netns "${nsname}" -4 route \
		add unreachable default metric 4278198272 \
		vrf "${VRF_DEVNAME}"
}
__setup_rt_policy()
{
	local dst="$1"
	local encap_rt="$2"
	local end_rts="$3"
	local dec_rt="$4"
	local mode="$5"
	local traffic="$6"
	local nsname
	local policy=''
	local n
	nsname="$(get_rtname "${encap_rt}")"
	for n in ${end_rts}; do
		policy="${policy}${VPN_LOCATOR_SERVICE}:${n}::${END_FUNC},"
	done
	policy="${policy}${VPN_LOCATOR_SERVICE}:${dec_rt}::${DT46_FUNC}"
	if [ "${traffic}" -eq 6 ]; then
		ip -netns "${nsname}" -6 route \
			add "${IPv6_HS_NETWORK}::${dst}" vrf "${VRF_DEVNAME}" \
			encap seg6 mode "${mode}" segs "${policy}" \
			dev "${VRF_DEVNAME}"
		ip -netns "${nsname}" -6 neigh \
			add proxy "${IPv6_HS_NETWORK}::${dst}" \
			dev "${RT2HS_DEVNAME}"
	else
		ip -netns "${nsname}" -4 route \
			add "${IPv4_HS_NETWORK}.${dst}" vrf "${VRF_DEVNAME}" \
			encap seg6 mode "${mode}" segs "${policy}" \
			dev "${VRF_DEVNAME}"
	fi
}
setup_rt_policy_ipv6()
{
	__setup_rt_policy "$1" "$2" "$3" "$4" "$5" 6
}
setup_rt_policy_ipv4()
{
	__setup_rt_policy "$1" "$2" "$3" "$4" "$5" 4
}
setup_hs()
{
	local hs="$1"
	local rt="$2"
	local hsname
	local rtname
	hsname="$(get_hsname "${hs}")"
	rtname="$(get_rtname "${rt}")"
	ip netns exec "${hsname}" sysctl -wq net.ipv6.conf.all.accept_dad=0
	ip netns exec "${hsname}" sysctl -wq net.ipv6.conf.default.accept_dad=0
	ip -netns "${hsname}" link add veth0 type veth \
		peer name "${RT2HS_DEVNAME}" netns "${rtname}"
	ip -netns "${hsname}" addr \
		add "${IPv6_HS_NETWORK}::${hs}/64" dev veth0 nodad
	ip -netns "${hsname}" addr add "${IPv4_HS_NETWORK}.${hs}/24" dev veth0
	ip -netns "${hsname}" link set veth0 up
	ip -netns "${hsname}" link set lo up
	ip -netns "${rtname}" link \
		add "${VRF_DEVNAME}" type vrf table "${VRF_TID}"
	ip -netns "${rtname}" link set "${VRF_DEVNAME}" up
	ip -netns "${rtname}" link \
		set "${RT2HS_DEVNAME}" master "${VRF_DEVNAME}"
	ip -netns "${rtname}" addr \
		add "${IPv6_HS_NETWORK}::254/64" dev "${RT2HS_DEVNAME}" nodad
	ip -netns "${rtname}" addr \
		add "${IPv4_HS_NETWORK}.254/24" dev "${RT2HS_DEVNAME}"
	ip -netns "${rtname}" link set "${RT2HS_DEVNAME}" up
	ip netns exec "${rtname}" \
		sysctl -wq net.ipv6.conf."${RT2HS_DEVNAME}".proxy_ndp=1
	ip netns exec "${rtname}" \
		sysctl -wq net.ipv4.conf."${RT2HS_DEVNAME}".proxy_arp=1
	ip netns exec "${rtname}" \
		sysctl -wq net.ipv4.conf."${RT2HS_DEVNAME}".rp_filter=0
	ip netns exec "${rtname}" sh -c "echo 1 > /proc/sys/net/vrf/strict_mode"
}
setup()
{
	local i
	ROUTERS="1 2 3 4"; readonly ROUTERS
	for i in ${ROUTERS}; do
		create_router "${i}"
	done
	HOSTS="1 2 3 4"; readonly HOSTS
	for i in ${HOSTS}; do
		create_host "${i}"
	done
	add_link_rt_pairs 1 "2 3 4"
	add_link_rt_pairs 2 "3 4"
	add_link_rt_pairs 3 "4"
	setup_rt_networking 1 "2 3 4"
	setup_rt_networking 2 "1 3 4"
	setup_rt_networking 3 "1 2 4"
	setup_rt_networking 4 "1 2 3"
	setup_hs 1 1
	setup_hs 2 2
	setup_hs 3 3
	setup_hs 4 4
	setup_rt_local_sids 1 "2 3 4"
	setup_rt_local_sids 2 "1 3 4"
	setup_rt_local_sids 3 "1 2 4"
	setup_rt_local_sids 4 "1 2 3"
	setup_rt_policy_ipv6 2 1 "3 4" 2 encap.red
	setup_rt_policy_ipv6 1 2 "" 1 encap.red
	setup_rt_policy_ipv4 2 1 "" 2 encap.red
	setup_rt_policy_ipv4 1 2 "4 3" 1 encap.red
	setup_rt_policy_ipv6 4 3 "2" 4 encap.red
	setup_rt_policy_ipv6 3 4 "1" 3 encap.red
	SETUP_ERR=0
}
check_rt_connectivity()
{
	local rtsrc="$1"
	local rtdst="$2"
	local prefix
	local rtsrc_nsname
	rtsrc_nsname="$(get_rtname "${rtsrc}")"
	prefix="$(get_network_prefix "${rtsrc}" "${rtdst}")"
	ip netns exec "${rtsrc_nsname}" ping -c 1 -W "${PING_TIMEOUT_SEC}" \
		"${prefix}::${rtdst}" >/dev/null 2>&1
}
check_and_log_rt_connectivity()
{
	local rtsrc="$1"
	local rtdst="$2"
	check_rt_connectivity "${rtsrc}" "${rtdst}"
	log_test $? 0 "Routers connectivity: rt-${rtsrc} -> rt-${rtdst}"
}
check_hs_ipv6_connectivity()
{
	local hssrc="$1"
	local hsdst="$2"
	local hssrc_nsname
	hssrc_nsname="$(get_hsname "${hssrc}")"
	ip netns exec "${hssrc_nsname}" ping -c 1 -W "${PING_TIMEOUT_SEC}" \
		"${IPv6_HS_NETWORK}::${hsdst}" >/dev/null 2>&1
}
check_hs_ipv4_connectivity()
{
	local hssrc="$1"
	local hsdst="$2"
	local hssrc_nsname
	hssrc_nsname="$(get_hsname "${hssrc}")"
	ip netns exec "${hssrc_nsname}" ping -c 1 -W "${PING_TIMEOUT_SEC}" \
		"${IPv4_HS_NETWORK}.${hsdst}" >/dev/null 2>&1
}
check_and_log_hs2gw_connectivity()
{
	local hssrc="$1"
	check_hs_ipv6_connectivity "${hssrc}" 254
	log_test $? 0 "IPv6 Hosts connectivity: hs-${hssrc} -> gw"
	check_hs_ipv4_connectivity "${hssrc}" 254
	log_test $? 0 "IPv4 Hosts connectivity: hs-${hssrc} -> gw"
}
check_and_log_hs_ipv6_connectivity()
{
	local hssrc="$1"
	local hsdst="$2"
	check_hs_ipv6_connectivity "${hssrc}" "${hsdst}"
	log_test $? 0 "IPv6 Hosts connectivity: hs-${hssrc} -> hs-${hsdst}"
}
check_and_log_hs_ipv4_connectivity()
{
	local hssrc="$1"
	local hsdst="$2"
	check_hs_ipv4_connectivity "${hssrc}" "${hsdst}"
	log_test $? 0 "IPv4 Hosts connectivity: hs-${hssrc} -> hs-${hsdst}"
}
check_and_log_hs_connectivity()
{
	local hssrc="$1"
	local hsdst="$2"
	check_and_log_hs_ipv4_connectivity "${hssrc}" "${hsdst}"
	check_and_log_hs_ipv6_connectivity "${hssrc}" "${hsdst}"
}
check_and_log_hs_ipv6_isolation()
{
	local hssrc="$1"
	local hsdst="$2"
	check_hs_ipv6_connectivity "${hssrc}" "${hsdst}"
	log_test $? 1 "IPv6 Hosts isolation: hs-${hssrc} -X-> hs-${hsdst}"
}
check_and_log_hs_ipv4_isolation()
{
	local hssrc="$1"
	local hsdst="$2"
	check_hs_ipv4_connectivity "${hssrc}" "${hsdst}"
	log_test $? 1 "IPv4 Hosts isolation: hs-${hssrc} -X-> hs-${hsdst}"
}
check_and_log_hs_isolation()
{
	local hssrc="$1"
	local hsdst="$2"
	check_and_log_hs_ipv6_isolation "${hssrc}" "${hsdst}"
	check_and_log_hs_ipv4_isolation "${hssrc}" "${hsdst}"
}
router_tests()
{
	local i
	local j
	log_section "IPv6 routers connectivity test"
	for i in ${ROUTERS}; do
		for j in ${ROUTERS}; do
			if [ "${i}" -eq "${j}" ]; then
				continue
			fi
			check_and_log_rt_connectivity "${i}" "${j}"
		done
	done
}
host2gateway_tests()
{
	local hs
	log_section "IPv4/IPv6 connectivity test among hosts and gateways"
	for hs in ${HOSTS}; do
		check_and_log_hs2gw_connectivity "${hs}"
	done
}
host_vpn_tests()
{
	log_section "SRv6 VPN connectivity test hosts (h1 <-> h2, IPv4/IPv6)"
	check_and_log_hs_connectivity 1 2
	check_and_log_hs_connectivity 2 1
	log_section "SRv6 VPN connectivity test hosts (h3 <-> h4, IPv6 only)"
	check_and_log_hs_ipv6_connectivity 3 4
	check_and_log_hs_ipv6_connectivity 4 3
}
host_vpn_isolation_tests()
{
	local l1="1 2"
	local l2="3 4"
	local tmp
	local i
	local j
	local k
	log_section "SRv6 VPN isolation test among hosts"
	for k in 0 1; do
		for i in ${l1}; do
			for j in ${l2}; do
				check_and_log_hs_isolation "${i}" "${j}"
			done
		done
		tmp="${l1}"; l1="${l2}"; l2="${tmp}"
	done
	log_section "SRv6 VPN isolation test among hosts (h2 <-> h4, IPv4 only)"
	check_and_log_hs_ipv4_isolation 2 4
	check_and_log_hs_ipv4_isolation 4 2
}
test_iproute2_supp_or_ksft_skip()
{
	if ! ip route help 2>&1 | grep -qo "encap.red"; then
		echo "SKIP: Missing SRv6 encap.red support in iproute2"
		exit "${ksft_skip}"
	fi
}
test_vrf_or_ksft_skip()
{
	modprobe vrf &>/dev/null || true
	if [ ! -e /proc/sys/net/vrf/strict_mode ]; then
		echo "SKIP: vrf sysctl does not exist"
		exit "${ksft_skip}"
	fi
}
if [ "$(id -u)" -ne 0 ]; then
	echo "SKIP: Need root privileges"
	exit "${ksft_skip}"
fi
test_command_or_ksft_skip ip
test_command_or_ksft_skip ping
test_command_or_ksft_skip sysctl
test_command_or_ksft_skip grep
test_iproute2_supp_or_ksft_skip
test_vrf_or_ksft_skip
set -e
trap cleanup EXIT
setup
set +e
router_tests
host2gateway_tests
host_vpn_tests
host_vpn_isolation_tests
print_log_test_results
