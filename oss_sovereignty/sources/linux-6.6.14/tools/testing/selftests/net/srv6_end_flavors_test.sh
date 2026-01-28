readonly ksft_skip=4
readonly RDMSUFF="$(mktemp -u XXXXXXXX)"
readonly DUMMY_DEVNAME="dum0"
readonly RT2HS_DEVNAME="veth1"
readonly LOCALSID_TABLE_ID=90
readonly IPv6_RT_NETWORK=fcf0:0
readonly IPv6_HS_NETWORK=cafe
readonly IPv6_TESTS_ADDR=2001:db8::1
readonly LOCATOR_SERVICE=fcff
readonly END_FUNC=000e
readonly END_PSP_FUNC=0ef1
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
	echo "################################################################################"
	echo "TEST SECTION: $*"
	echo "################################################################################"
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
__get_srv6_rtcfg_id()
{
	local element="$1"
	echo "${element}" | cut -d':' -f1
}
__get_srv6_rtcfg_op()
{
	local element="$1"
	echo "${element}" | cut -d':' -f2 | sed 's/,/\n/g' | sort | \
		xargs | sed 's/ /,/g'
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
	ip -netns "${nsname}" link add ${DUMMY_DEVNAME} type dummy
	ip -netns "${nsname}" link set ${DUMMY_DEVNAME} up
	ip netns exec "${nsname}" sysctl -wq net.ipv6.conf.all.accept_dad=0
	ip netns exec "${nsname}" sysctl -wq net.ipv6.conf.default.accept_dad=0
	ip netns exec "${nsname}" sysctl -wq net.ipv6.conf.all.forwarding=1
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
			add "${LOCATOR_SERVICE}:${neigh}::/32" \
			table "${LOCALSID_TABLE_ID}" \
			via "${net_prefix}::${neigh}" dev "${devname}"
	done
	ip -netns "${nsname}" -6 route \
		add "${LOCATOR_SERVICE}:${rt}::${END_FUNC}" \
		table "${LOCALSID_TABLE_ID}" \
		encap seg6local action End dev "${DUMMY_DEVNAME}"
	ip -netns "${nsname}" -6 rule \
		add to "${LOCATOR_SERVICE}::/16" \
		lookup "${LOCALSID_TABLE_ID}" prio 999
	ip -netns "${nsname}" -6 route \
		add unreachable default metric 4278198272 \
		dev "${DUMMY_DEVNAME}"
}
__setup_rt_policy()
{
	local dst="$1"
	local encap_rt="$2"
	local policy_rts="$3"
	local behavior_cfg
	local in_nsname
	local rt_nsname
	local policy=''
	local function
	local fullsid
	local op_type
	local node
	local n
	in_nsname="$(get_rtname "${encap_rt}")"
	for n in ${policy_rts}; do
		node="$(__get_srv6_rtcfg_id "${n}")"
		op_type="$(__get_srv6_rtcfg_op "${n}")"
		rt_nsname="$(get_rtname "${node}")"
		case "${op_type}" in
		"noflv")
			policy="${policy}${LOCATOR_SERVICE}:${node}::${END_FUNC},"
			function="${END_FUNC}"
			behavior_cfg="End"
			;;
		"psp")
			policy="${policy}${LOCATOR_SERVICE}:${node}::${END_PSP_FUNC},"
			function="${END_PSP_FUNC}"
			behavior_cfg="End flavors psp"
			;;
		*)
			break
			;;
		esac
		fullsid="${LOCATOR_SERVICE}:${node}::${function}"
		if ! ip -netns "${rt_nsname}" -6 route get "${fullsid}" \
			&>/dev/null; then
			ip -netns "${rt_nsname}" -6 route \
				add "${fullsid}" \
				table "${LOCALSID_TABLE_ID}" \
				encap seg6local action ${behavior_cfg} \
				dev "${DUMMY_DEVNAME}"
		fi
	done
	policy="${policy%,}"
	ip -netns "${in_nsname}" -6 route \
		add "${IPv6_HS_NETWORK}::${dst}" \
		encap seg6 mode inline segs "${policy}" \
		dev "${DUMMY_DEVNAME}"
	ip -netns "${in_nsname}" -6 neigh \
		add proxy "${IPv6_HS_NETWORK}::${dst}" \
		dev "${RT2HS_DEVNAME}"
}
setup_rt_policy_ipv6()
{
	__setup_rt_policy "$1" "$2" "$3"
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
	ip -netns "${hsname}" link set veth0 up
	ip -netns "${hsname}" link set lo up
	ip -netns "${rtname}" addr \
		add "${IPv6_HS_NETWORK}::254/64" dev "${RT2HS_DEVNAME}" nodad
	ip -netns "${rtname}" link set "${RT2HS_DEVNAME}" up
	ip netns exec "${rtname}" \
		sysctl -wq net.ipv6.conf."${RT2HS_DEVNAME}".proxy_ndp=1
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
	setup_rt_policy_ipv6 2 1 "3:noflv 4:psp 2:psp"
	setup_rt_policy_ipv6 1 2 "1:psp"
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
check_and_log_hs2gw_connectivity()
{
	local hssrc="$1"
	check_hs_ipv6_connectivity "${hssrc}" 254
	log_test $? 0 "IPv6 Hosts connectivity: hs-${hssrc} -> gw"
}
check_and_log_hs_ipv6_connectivity()
{
	local hssrc="$1"
	local hsdst="$2"
	check_hs_ipv6_connectivity "${hssrc}" "${hsdst}"
	log_test $? 0 "IPv6 Hosts connectivity: hs-${hssrc} -> hs-${hsdst}"
}
check_and_log_hs_connectivity()
{
	local hssrc="$1"
	local hsdst="$2"
	check_and_log_hs_ipv6_connectivity "${hssrc}" "${hsdst}"
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
	log_section "IPv6 connectivity test among hosts and gateways"
	for hs in ${HOSTS}; do
		check_and_log_hs2gw_connectivity "${hs}"
	done
}
host_srv6_end_flv_psp_tests()
{
	log_section "SRv6 connectivity test hosts (h1 <-> h2, PSP flavor)"
	check_and_log_hs_connectivity 1 2
	check_and_log_hs_connectivity 2 1
}
test_iproute2_supp_or_ksft_skip()
{
	local flavor="$1"
	if ! ip route help 2>&1 | grep -qo "${flavor}"; then
		echo "SKIP: Missing SRv6 ${flavor} flavor support in iproute2"
		exit "${ksft_skip}"
	fi
}
test_kernel_supp_or_ksft_skip()
{
	local flavor="$1"
	local test_netns
	test_netns="kflv-$(mktemp -u XXXXXXXX)"
	if ! ip netns add "${test_netns}"; then
		echo "SKIP: Cannot set up netns to test kernel support for flavors"
		exit "${ksft_skip}"
	fi
	if ! ip -netns "${test_netns}" link \
		add "${DUMMY_DEVNAME}" type dummy; then
		echo "SKIP: Cannot set up dummy dev to test kernel support for flavors"
		ip netns del "${test_netns}"
		exit "${ksft_skip}"
	fi
	if ! ip -netns "${test_netns}" link \
		set "${DUMMY_DEVNAME}" up; then
		echo "SKIP: Cannot activate dummy dev to test kernel support for flavors"
		ip netns del "${test_netns}"
		exit "${ksft_skip}"
	fi
	if ! ip -netns "${test_netns}" -6 route \
		add "${IPv6_TESTS_ADDR}" encap seg6local \
		action End flavors "${flavor}" dev "${DUMMY_DEVNAME}"; then
		echo "SKIP: ${flavor} flavor not supported in kernel"
		ip netns del "${test_netns}"
		exit "${ksft_skip}"
	fi
	ip netns del "${test_netns}"
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
if [ "$(id -u)" -ne 0 ]; then
	echo "SKIP: Need root privileges"
	exit "${ksft_skip}"
fi
test_command_or_ksft_skip ip
test_command_or_ksft_skip ping
test_command_or_ksft_skip sysctl
test_command_or_ksft_skip grep
test_command_or_ksft_skip cut
test_command_or_ksft_skip sed
test_command_or_ksft_skip sort
test_command_or_ksft_skip xargs
test_dummy_dev_or_ksft_skip
test_iproute2_supp_or_ksft_skip psp
test_kernel_supp_or_ksft_skip psp
set -e
trap cleanup EXIT
setup
set +e
router_tests
host2gateway_tests
host_srv6_end_flv_psp_tests
print_log_test_results
