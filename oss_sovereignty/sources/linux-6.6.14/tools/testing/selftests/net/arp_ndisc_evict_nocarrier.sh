readonly PEER_NS="ns-peer-$(mktemp -u XXXXXX)"
readonly V4_ADDR0=10.0.10.1
readonly V4_ADDR1=10.0.10.2
readonly V6_ADDR0=2001:db8:91::1
readonly V6_ADDR1=2001:db8:91::2
nsid=100
ret=0
cleanup_v6()
{
    ip netns del me
    ip netns del peer
    sysctl -w net.ipv6.conf.veth1.ndisc_evict_nocarrier=1 >/dev/null 2>&1
    sysctl -w net.ipv6.conf.all.ndisc_evict_nocarrier=1 >/dev/null 2>&1
}
create_ns()
{
    local n=${1}
    ip netns del ${n} 2>/dev/null
    ip netns add ${n}
    ip netns set ${n} $((nsid++))
    ip -netns ${n} link set lo up
}
setup_v6() {
    create_ns me
    create_ns peer
    IP="ip -netns me"
    $IP li add veth1 type veth peer name veth2
    $IP li set veth1 up
    $IP -6 addr add $V6_ADDR0/64 dev veth1 nodad
    $IP li set veth2 netns peer up
    ip -netns peer -6 addr add $V6_ADDR1/64 dev veth2 nodad
    ip netns exec me sysctl -w $1 >/dev/null 2>&1
    ip netns exec me ping -6 -c1 -Iveth1 $V6_ADDR1 >/dev/null 2>&1
    ip netns exec me ip -6 neigh get $V6_ADDR1 dev veth1 >/dev/null 2>&1
    if [ $? -ne 0 ]; then
        cleanup_v6
        echo "failed"
        exit 1
    fi
    ip netns exec peer ip link set veth2 down
}
setup_v4() {
    ip netns add "${PEER_NS}"
    ip link add name veth0 type veth peer name veth1
    ip link set dev veth0 up
    ip link set dev veth1 netns "${PEER_NS}"
    ip netns exec "${PEER_NS}" ip link set dev veth1 up
    ip addr add $V4_ADDR0/24 dev veth0
    ip netns exec "${PEER_NS}" ip addr add $V4_ADDR1/24 dev veth1
    ip netns exec ${PEER_NS} ip route add default via $V4_ADDR1 dev veth1
    ip route add default via $V4_ADDR0 dev veth0
    sysctl -w "$1" >/dev/null 2>&1
    ping -c1 -I veth0 $V4_ADDR1 -q >/dev/null 2>&1
    ip neigh get $V4_ADDR1 dev veth0 >/dev/null 2>&1
    if [ $? -ne 0 ]; then
        cleanup_v4
        echo "failed"
        exit 1
    fi
    ip netns exec "${PEER_NS}" ip link set veth1 down
}
cleanup_v4() {
    ip neigh flush dev veth0
    ip link del veth0
    local -r ns="$(ip netns list|grep $PEER_NS)"
    [ -n "$ns" ] && ip netns del $ns 2>/dev/null
    sysctl -w net.ipv4.conf.veth0.arp_evict_nocarrier=1 >/dev/null 2>&1
    sysctl -w net.ipv4.conf.all.arp_evict_nocarrier=1 >/dev/null 2>&1
}
run_arp_evict_nocarrier_enabled() {
    echo "run arp_evict_nocarrier=1 test"
    setup_v4 "net.ipv4.conf.veth0.arp_evict_nocarrier=1"
    ip neigh get $V4_ADDR1 dev veth0 >/dev/null 2>&1
    if [ $? -eq 0 ];then
        echo "failed"
        ret=1
    else
        echo "ok"
    fi
    cleanup_v4
}
run_arp_evict_nocarrier_disabled() {
    echo "run arp_evict_nocarrier=0 test"
    setup_v4 "net.ipv4.conf.veth0.arp_evict_nocarrier=0"
    ip neigh get $V4_ADDR1 dev veth0 >/dev/null 2>&1
    if [ $? -eq 0 ];then
        echo "ok"
    else
        echo "failed"
        ret=1
    fi
    cleanup_v4
}
run_arp_evict_nocarrier_disabled_all() {
    echo "run all.arp_evict_nocarrier=0 test"
    setup_v4 "net.ipv4.conf.all.arp_evict_nocarrier=0"
    ip neigh get $V4_ADDR1 dev veth0 >/dev/null 2>&1
    if [ $? -eq 0 ];then
        echo "ok"
    else
        echo "failed"
    fi
    cleanup_v4
}
run_ndisc_evict_nocarrier_enabled() {
    echo "run ndisc_evict_nocarrier=1 test"
    setup_v6 "net.ipv6.conf.veth1.ndisc_evict_nocarrier=1"
    ip netns exec me ip -6 neigh get $V6_ADDR1 dev veth1 >/dev/null 2>&1
    if [ $? -eq 0 ];then
        echo "failed"
        ret=1
    else
        echo "ok"
    fi
    cleanup_v6
}
run_ndisc_evict_nocarrier_disabled() {
    echo "run ndisc_evict_nocarrier=0 test"
    setup_v6 "net.ipv6.conf.veth1.ndisc_evict_nocarrier=0"
    ip netns exec me ip -6 neigh get $V6_ADDR1 dev veth1 >/dev/null 2>&1
    if [ $? -eq 0 ];then
        echo "ok"
    else
        echo "failed"
        ret=1
    fi
    cleanup_v6
}
run_ndisc_evict_nocarrier_disabled_all() {
    echo "run all.ndisc_evict_nocarrier=0 test"
    setup_v6 "net.ipv6.conf.all.ndisc_evict_nocarrier=0"
    ip netns exec me ip -6 neigh get $V6_ADDR1 dev veth1 >/dev/null 2>&1
    if [ $? -eq 0 ];then
        echo "ok"
    else
        echo "failed"
        ret=1
    fi
    cleanup_v6
}
run_all_tests() {
    run_arp_evict_nocarrier_enabled
    run_arp_evict_nocarrier_disabled
    run_arp_evict_nocarrier_disabled_all
    run_ndisc_evict_nocarrier_enabled
    run_ndisc_evict_nocarrier_disabled
    run_ndisc_evict_nocarrier_disabled_all
}
if [ "$(id -u)" -ne 0 ];then
	echo "SKIP: Need root privileges"
	exit $ksft_skip;
fi
run_all_tests
exit $ret
