ksft_skip=4
readonly LOCALSID_TABLE_ID=90
readonly IPv6_RT_NETWORK=fd00
readonly IPv4_HS_NETWORK=10.0.0
readonly VPN_LOCATOR_SERVICE=fc00
PING_TIMEOUT_SEC=4
ret=0
PAUSE_ON_FAIL=${PAUSE_ON_FAIL:=no}
log_test()
{
	local rc=$1
	local expected=$2
	local msg="$3"
	if [ ${rc} -eq ${expected} ]; then
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
	if [ "$TESTS" != "none" ]; then
		printf "\nTests passed: %3d\n" ${nsuccess}
		printf "Tests failed: %3d\n"   ${nfail}
	fi
}
log_section()
{
	echo
	echo "################################################################################"
	echo "TEST SECTION: $*"
	echo "################################################################################"
}
cleanup()
{
	ip link del veth-rt-1 2>/dev/null || true
	ip link del veth-rt-2 2>/dev/null || true
	for ns in $(ip netns show | grep -E 'rt-*|hs-*'); do
		ip netns del ${ns} || true
	done
}
setup_rt_networking()
{
	local rt=$1
	local nsname=rt-${rt}
	ip netns add ${nsname}
	ip netns exec ${nsname} sysctl -wq net.ipv6.conf.all.accept_dad=0
	ip netns exec ${nsname} sysctl -wq net.ipv6.conf.default.accept_dad=0
	ip link set veth-rt-${rt} netns ${nsname}
	ip -netns ${nsname} link set veth-rt-${rt} name veth0
	ip -netns ${nsname} addr add ${IPv6_RT_NETWORK}::${rt}/64 dev veth0 nodad
	ip -netns ${nsname} link set veth0 up
	ip -netns ${nsname} link set lo up
	ip netns exec ${nsname} sysctl -wq net.ipv4.ip_forward=1
	ip netns exec ${nsname} sysctl -wq net.ipv6.conf.all.forwarding=1
}
setup_hs()
{
	local hs=$1
	local rt=$2
	local tid=$3
	local hsname=hs-t${tid}-${hs}
	local rtname=rt-${rt}
	local rtveth=veth-t${tid}
	ip netns add ${hsname}
	ip netns exec ${rtname} sysctl -wq net.ipv4.conf.all.rp_filter=0
	ip netns exec ${rtname} sysctl -wq net.ipv4.conf.default.rp_filter=0
	ip -netns ${hsname} link add veth0 type veth peer name ${rtveth}
	ip -netns ${hsname} link set ${rtveth} netns ${rtname}
	ip -netns ${hsname} addr add ${IPv4_HS_NETWORK}.${hs}/24 dev veth0
	ip -netns ${hsname} link set veth0 up
	ip -netns ${hsname} link set lo up
	ip -netns ${rtname} link add vrf-${tid} type vrf table ${tid}
	ip -netns ${rtname} link set vrf-${tid} up
	ip -netns ${rtname} link set ${rtveth} master vrf-${tid}
	ip -netns ${rtname} addr add ${IPv4_HS_NETWORK}.254/24 dev ${rtveth}
	ip -netns ${rtname} link set ${rtveth} up
	ip netns exec ${rtname} sysctl -wq net.ipv4.conf.${rtveth}.proxy_arp=1
	ip netns exec ${rtname} sh -c "echo 1 > /proc/sys/net/vrf/strict_mode"
}
setup_vpn_config()
{
	local hssrc=$1
	local rtsrc=$2
	local hsdst=$3
	local rtdst=$4
	local tid=$5
	local hssrc_name=hs-t${tid}-${hssrc}
	local hsdst_name=hs-t${tid}-${hsdst}
	local rtsrc_name=rt-${rtsrc}
	local rtdst_name=rt-${rtdst}
	local vpn_sid=${VPN_LOCATOR_SERVICE}:${hssrc}${hsdst}:${tid}::6004
	ip -netns ${rtsrc_name} -4 route add ${IPv4_HS_NETWORK}.${hsdst}/32 vrf vrf-${tid} \
		encap seg6 mode encap segs ${vpn_sid} dev veth0
	ip -netns ${rtsrc_name} -6 route add ${vpn_sid}/128 vrf vrf-${tid} \
		via fd00::${rtdst} dev veth0
	ip -netns ${rtdst_name} -6 route add ${vpn_sid}/128 table ${LOCALSID_TABLE_ID} \
		encap seg6local action End.DT4 vrftable ${tid} dev vrf-${tid}
	if [ -z "$(ip -netns ${rtdst_name} -6 rule show | \
	    grep "to ${VPN_LOCATOR_SERVICE}::/16 lookup ${LOCALSID_TABLE_ID}")" ]; then
		ip -netns ${rtdst_name} -6 rule add \
			to ${VPN_LOCATOR_SERVICE}::/16 \
			lookup ${LOCALSID_TABLE_ID} prio 999
	fi
}
setup()
{
	ip link add veth-rt-1 type veth peer name veth-rt-2
	setup_rt_networking 1
	setup_rt_networking 2
	setup_hs 1 1 100  
	setup_hs 2 2 100
	setup_hs 3 1 200
	setup_hs 4 2 200
	setup_vpn_config 1 1 2 2 100  
	setup_vpn_config 2 2 1 1 100
	setup_vpn_config 3 1 4 2 200
	setup_vpn_config 4 2 3 1 200
}
check_rt_connectivity()
{
	local rtsrc=$1
	local rtdst=$2
	ip netns exec rt-${rtsrc} ping -c 1 -W 1 ${IPv6_RT_NETWORK}::${rtdst} \
		>/dev/null 2>&1
}
check_and_log_rt_connectivity()
{
	local rtsrc=$1
	local rtdst=$2
	check_rt_connectivity ${rtsrc} ${rtdst}
	log_test $? 0 "Routers connectivity: rt-${rtsrc} -> rt-${rtdst}"
}
check_hs_connectivity()
{
	local hssrc=$1
	local hsdst=$2
	local tid=$3
	ip netns exec hs-t${tid}-${hssrc} ping -c 1 -W ${PING_TIMEOUT_SEC} \
		${IPv4_HS_NETWORK}.${hsdst} >/dev/null 2>&1
}
check_and_log_hs_connectivity()
{
	local hssrc=$1
	local hsdst=$2
	local tid=$3
	check_hs_connectivity ${hssrc} ${hsdst} ${tid}
	log_test $? 0 "Hosts connectivity: hs-t${tid}-${hssrc} -> hs-t${tid}-${hsdst} (tenant ${tid})"
}
check_and_log_hs_isolation()
{
	local hssrc=$1
	local tidsrc=$2
	local hsdst=$3
	local tiddst=$4
	check_hs_connectivity ${hssrc} ${hsdst} ${tidsrc}
	log_test $? 1 "Hosts isolation: hs-t${tidsrc}-${hssrc} -X-> hs-t${tiddst}-${hsdst}"
}
check_and_log_hs2gw_connectivity()
{
	local hssrc=$1
	local tid=$2
	check_hs_connectivity ${hssrc} 254 ${tid}
	log_test $? 0 "Hosts connectivity: hs-t${tid}-${hssrc} -> gw (tenant ${tid})"
}
router_tests()
{
	log_section "IPv6 routers connectivity test"
	check_and_log_rt_connectivity 1 2
	check_and_log_rt_connectivity 2 1
}
host2gateway_tests()
{
	log_section "IPv4 connectivity test among hosts and gateway"
	check_and_log_hs2gw_connectivity 1 100
	check_and_log_hs2gw_connectivity 2 100
	check_and_log_hs2gw_connectivity 3 200
	check_and_log_hs2gw_connectivity 4 200
}
host_vpn_tests()
{
	log_section "SRv6 VPN connectivity test among hosts in the same tenant"
	check_and_log_hs_connectivity 1 2 100
	check_and_log_hs_connectivity 2 1 100
	check_and_log_hs_connectivity 3 4 200
	check_and_log_hs_connectivity 4 3 200
}
host_vpn_isolation_tests()
{
	local i
	local j
	local k
	local tmp
	local l1="1 2"
	local l2="3 4"
	local t1=100
	local t2=200
	log_section "SRv6 VPN isolation test among hosts in different tentants"
	for k in 0 1; do
		for i in ${l1}; do
			for j in ${l2}; do
				check_and_log_hs_isolation ${i} ${t1} ${j} ${t2}
			done
		done
		tmp="${l1}"; l1="${l2}"; l2="${tmp}"
		tmp=${t1}; t1=${t2}; t2=${tmp}
	done
}
if [ "$(id -u)" -ne 0 ];then
	echo "SKIP: Need root privileges"
	exit $ksft_skip
fi
if [ ! -x "$(command -v ip)" ]; then
	echo "SKIP: Could not run test without ip tool"
	exit $ksft_skip
fi
modprobe vrf &>/dev/null
if [ ! -e /proc/sys/net/vrf/strict_mode ]; then
        echo "SKIP: vrf sysctl does not exist"
        exit $ksft_skip
fi
cleanup &>/dev/null
setup
router_tests
host2gateway_tests
host_vpn_tests
host_vpn_isolation_tests
print_log_test_results
cleanup &>/dev/null
exit ${ret}
