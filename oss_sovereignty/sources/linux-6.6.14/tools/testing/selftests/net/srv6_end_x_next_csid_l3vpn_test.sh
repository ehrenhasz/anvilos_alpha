readonly ksft_skip=4
readonly RDMSUFF="$(mktemp -u XXXXXXXX)"
readonly DUMMY_DEVNAME="dum0"
readonly VRF_TID=100
readonly VRF_DEVNAME="vrf-${VRF_TID}"
readonly RT2HS_DEVNAME="veth-t${VRF_TID}"
readonly LOCALSID_TABLE_ID=90
readonly IPv6_RT_NETWORK=fcf0:0
readonly IPv6_HS_NETWORK=cafe
readonly IPv4_HS_NETWORK=10.0.0
readonly VPN_LOCATOR_SERVICE=fcff
readonly DT46_FUNC=0d46
readonly HEADEND_ENCAP="encap.red"
readonly LCBLOCK_ADDR=fcbb0000
readonly LCBLOCK_BLEN=32
readonly LCNODEFUNC_FMT="0%d00"
readonly LCNODEFUNC_BLEN=16
readonly LCBLOCK_NODEFUNC_BLEN=$((LCBLOCK_BLEN + LCNODEFUNC_BLEN))
readonly CSID_CNTR_PREFIX="dead:beaf::/32"
readonly CSID_CNTR_RT_ID_TEST=1
readonly CSID_CNTR_RT_TABLE=91
declare -ra CSID_CONTAINER_CFGS=(
	"d,d,y"
	"d,16,y"
	"16,d,y"
	"16,32,y"
	"32,16,y"
	"48,8,y"
	"8,48,y"
	"d,0,n"
	"0,d,n"
	"32,0,n"
	"0,32,n"
	"17,d,n"
	"d,17,n"
	"120,16,n"
	"16,120,n"
	"0,128,n"
	"128,0,n"
	"130,0,n"
	"0,130,n"
	"0,0,n"
)
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
	ip netns exec "${nsname}" sysctl -wq net.ipv6.conf.all.accept_dad=0
	ip netns exec "${nsname}" sysctl -wq net.ipv6.conf.default.accept_dad=0
	ip netns exec "${nsname}" sysctl -wq net.ipv6.conf.all.forwarding=1
	ip netns exec "${nsname}" sysctl -wq net.ipv4.conf.all.rp_filter=0
	ip netns exec "${nsname}" sysctl -wq net.ipv4.conf.default.rp_filter=0
	ip netns exec "${nsname}" sysctl -wq net.ipv4.ip_forward=1
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
        ip -netns "${nsname}" link add "${DUMMY_DEVNAME}" type dummy
        ip -netns "${nsname}" link set "${DUMMY_DEVNAME}" up
	ip -netns "${nsname}" link set lo up
}
build_ipv6_addr()
{
	local addr="$1"
	local out=""
	local strlen="${
	local padn
	local i
	for (( i = 0; i < strlen; i++ )); do
		if (( i > 0 && i < 32 && (i % 4) == 0 )); then
			out="${out}:"
		fi
		out="${out}${addr:$i:1}"
	done
	padn=$((32 - strlen))
	for (( i = padn; i > 0; i-- )); do
		if (( i > 0 && i < 32 && (i % 4) == 0 )); then
			out="${out}:"
		fi
		out="${out}0"
	done
	printf "${out}"
}
build_csid()
{
	local nodeid="$1"
	printf "${LCNODEFUNC_FMT}" "${nodeid}"
}
build_lcnode_func_prefix()
{
	local nodeid="$1"
	local lcnodefunc
	local prefix
	local out
	lcnodefunc="$(build_csid "${nodeid}")"
	prefix="$(build_ipv6_addr "${LCBLOCK_ADDR}${lcnodefunc}")"
	out="${prefix}/${LCBLOCK_NODEFUNC_BLEN}"
	echo "${out}"
}
set_end_x_nextcsid()
{
	local rt="$1"
	local adj="$2"
	nsname="$(get_rtname "${rt}")"
	net_prefix="$(get_network_prefix "${rt}" "${adj}")"
	lcnode_func_prefix="$(build_lcnode_func_prefix "${rt}")"
	ip -netns "${nsname}" -6 route \
		replace "${lcnode_func_prefix}" \
		table "${LOCALSID_TABLE_ID}" \
		encap seg6local action End.X nh6 "${net_prefix}::${adj}" \
		flavors next-csid lblen "${LCBLOCK_BLEN}" \
		nflen "${LCNODEFUNC_BLEN}" dev "${DUMMY_DEVNAME}"
}
set_underlay_sids_reachability()
{
	local rt="$1"
	local rt_neighs="$2"
	nsname="$(get_rtname "${rt}")"
	for neigh in ${rt_neighs}; do
		devname="veth-rt-${rt}-${neigh}"
		net_prefix="$(get_network_prefix "${rt}" "${neigh}")"
		ip -netns "${nsname}" -6 route \
			replace "${VPN_LOCATOR_SERVICE}:${neigh}::/32" \
			table "${LOCALSID_TABLE_ID}" \
			via "${net_prefix}::${neigh}" dev "${devname}"
		lcnode_func_prefix="$(build_lcnode_func_prefix "${neigh}")"
		ip -netns "${nsname}" -6 route \
			replace "${lcnode_func_prefix}" \
			table "${LOCALSID_TABLE_ID}" \
			via "${net_prefix}::${neigh}" dev "${devname}"
	done
}
setup_rt_local_sids()
{
	local rt="$1"
	local rt_neighs="$2"
	local net_prefix
	local devname
	local nsname
	local neigh
	local lcnode_func_prefix
	local lcblock_prefix
	nsname="$(get_rtname "${rt}")"
        set_underlay_sids_reachability "${rt}" "${rt_neighs}"
	ip -netns "${nsname}" -6 rule \
		add to "${VPN_LOCATOR_SERVICE}::/16" \
		lookup "${LOCALSID_TABLE_ID}" prio 999
	lcblock_prefix="$(build_ipv6_addr "${LCBLOCK_ADDR}")"
	ip -netns "${nsname}" -6 rule \
		add to "${lcblock_prefix}/${LCBLOCK_BLEN}" \
		lookup "${LOCALSID_TABLE_ID}" prio 999
}
__setup_l3vpn()
{
	local src="$1"
	local dst="$2"
	local end_rts="$3"
	local mode="$4"
	local traffic="$5"
	local nsname
	local policy
	local container
	local decapsid
	local lcnfunc
	local dt
	local n
	local rtsrc_nsname
	local rtdst_nsname
	rtsrc_nsname="$(get_rtname "${src}")"
	rtdst_nsname="$(get_rtname "${dst}")"
	container="${LCBLOCK_ADDR}"
	for n in ${end_rts}; do
		lcnfunc="$(build_csid "${n}")"
		container="${container}${lcnfunc}"
	done
	if [ "${mode}" -eq 1 ]; then
		dt="$(build_csid "${dst}")${DT46_FUNC}"
		container="${container}${dt}"
		policy="$(build_ipv6_addr "${container}")"
		container="${LCBLOCK_ADDR}${dt}"
		decapsid="$(build_ipv6_addr "${container}")"
	else
		decapsid="${VPN_LOCATOR_SERVICE}:${dst}::${DT46_FUNC}"
		policy="$(build_ipv6_addr "${container}"),${decapsid}"
	fi
	if [ "${traffic}" -eq 6 ]; then
		ip -netns "${rtsrc_nsname}" -6 route \
			add "${IPv6_HS_NETWORK}::${dst}" vrf "${VRF_DEVNAME}" \
			encap seg6 mode "${HEADEND_ENCAP}" segs "${policy}" \
			dev "${VRF_DEVNAME}"
		ip -netns "${rtsrc_nsname}" -6 neigh \
			add proxy "${IPv6_HS_NETWORK}::${dst}" \
			dev "${RT2HS_DEVNAME}"
	else
		ip -netns "${rtsrc_nsname}" -4 route \
			add "${IPv4_HS_NETWORK}.${dst}" vrf "${VRF_DEVNAME}" \
			encap seg6 mode "${HEADEND_ENCAP}" segs "${policy}" \
			dev "${VRF_DEVNAME}"
	fi
	ip -netns "${rtdst_nsname}" -6 route \
		add "${decapsid}" \
		table "${LOCALSID_TABLE_ID}" \
		encap seg6local action End.DT46 vrftable "${VRF_TID}" \
		dev "${VRF_DEVNAME}"
}
setup_ipv4_vpn_2sids()
{
	__setup_l3vpn "$1" "$2" "$3" 2 4
}
setup_ipv6_vpn_1sid()
{
	__setup_l3vpn "$1" "$2" "$3" 1 6
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
	ip -netns "${rtname}" -6 route \
		add unreachable default metric 4278198272 \
		vrf "${VRF_DEVNAME}"
	ip -netns "${rtname}" -4 route \
		add unreachable default metric 4278198272 \
		vrf "${VRF_DEVNAME}"
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
	HOSTS="1 2"; readonly HOSTS
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
	setup_rt_local_sids 1 "2 3 4"
	setup_rt_local_sids 2 "1 3 4"
	setup_rt_local_sids 3 "1 2 4"
	setup_rt_local_sids 4 "1 2 3"
	setup_ipv6_vpn_1sid 1 2 "3"
	setup_ipv6_vpn_1sid 2 1 "4"
	setup_ipv4_vpn_2sids 1 2 "3"
	setup_ipv4_vpn_2sids 2 1 "3"
        set_end_x_nextcsid 3 4
        set_end_x_nextcsid 4 1
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
	log_section "SRv6 VPN connectivity test hosts (h1 <-> h2, IPv6)"
	check_and_log_hs_ipv6_connectivity 1 2
	check_and_log_hs_ipv6_connectivity 2 1
	log_section "SRv6 VPN connectivity test hosts (h1 <-> h2, IPv4)"
	check_and_log_hs_ipv4_connectivity 1 2
	check_and_log_hs_ipv4_connectivity 2 1
}
__nextcsid_end_x_behavior_test()
{
	local nsname="$1"
	local cmd="$2"
	local blen="$3"
	local flen="$4"
	local layout=""
	if [ "${blen}" != "d" ]; then
		layout="${layout} lblen ${blen}"
	fi
	if [ "${flen}" != "d" ]; then
		layout="${layout} nflen ${flen}"
	fi
	ip -netns "${nsname}" -6 route \
		"${cmd}" "${CSID_CNTR_PREFIX}" \
		table "${CSID_CNTR_RT_TABLE}" \
		encap seg6local action End.X nh6 :: \
		flavors next-csid ${layout} \
		dev "${DUMMY_DEVNAME}" &>/dev/null
	return "$?"
}
rt_x_nextcsid_end_x_behavior_test()
{
	local rt="$1"
	local blen="$2"
	local flen="$3"
	local nsname
	local ret
	nsname="$(get_rtname "${rt}")"
	__nextcsid_end_x_behavior_test "${nsname}" "add" "${blen}" "${flen}"
	ret="$?"
	__nextcsid_end_x_behavior_test "${nsname}" "del" "${blen}" "${flen}"
	return "${ret}"
}
__parse_csid_container_cfg()
{
	local cfg="$1"
	local index="$2"
	local out
	echo "${cfg}" | cut -d',' -f"${index}"
}
csid_container_cfg_tests()
{
	local valid
	local blen
	local flen
	local cfg
	local ret
	log_section "C-SID Container config tests (legend: d='kernel default')"
	for cfg in "${CSID_CONTAINER_CFGS[@]}"; do
		blen="$(__parse_csid_container_cfg "${cfg}" 1)"
		flen="$(__parse_csid_container_cfg "${cfg}" 2)"
		valid="$(__parse_csid_container_cfg "${cfg}" 3)"
		rt_x_nextcsid_end_x_behavior_test \
			"${CSID_CNTR_RT_ID_TEST}" \
			"${blen}" \
			"${flen}"
		ret="$?"
		if [ "${valid}" == "y" ]; then
			log_test "${ret}" 0 \
				"Accept valid C-SID container cfg (lblen=${blen}, nflen=${flen})"
		else
			log_test "${ret}" 2 \
				"Reject invalid C-SID container cfg (lblen=${blen}, nflen=${flen})"
		fi
	done
}
test_iproute2_supp_or_ksft_skip()
{
	if ! ip route help 2>&1 | grep -qo "next-csid"; then
		echo "SKIP: Missing SRv6 NEXT-C-SID flavor support in iproute2"
		exit "${ksft_skip}"
	fi
}
test_dummy_dev_or_ksft_skip()
{
        local test_netns
        test_netns="dummy-$(mktemp -u XXXXXXXX)"
        if ! ip netns add "${test_netns}"; then
                echo "SKIP: Cannot set up netns for testing dummy dev support"
                exit "${ksft_skip}"
        fi
        modprobe dummy &>/dev/null || true
        if ! ip -netns "${test_netns}" link \
                add "${DUMMY_DEVNAME}" type dummy; then
                echo "SKIP: dummy dev not supported"
                ip netns del "${test_netns}"
                exit "${ksft_skip}"
        fi
        ip netns del "${test_netns}"
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
test_command_or_ksft_skip cut
test_iproute2_supp_or_ksft_skip
test_dummy_dev_or_ksft_skip
test_vrf_or_ksft_skip
set -e
trap cleanup EXIT
setup
set +e
csid_container_cfg_tests
router_tests
host2gateway_tests
host_vpn_tests
print_log_test_results
