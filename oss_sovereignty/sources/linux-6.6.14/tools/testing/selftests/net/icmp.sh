NS1=ns1
NS2=ns2
H1_IP=172.16.0.1/32
H1_IP6=2001:db8:1::1
RT1=172.16.1.0/24
PINGADDR=172.16.1.1
RT2=172.16.0.0/24
H2_IP6=2001:db8:1::2
TMPFILE=$(mktemp)
cleanup()
{
    rm -f "$TMPFILE"
    ip netns del $NS1
    ip netns del $NS2
}
trap cleanup EXIT
ip netns add $NS1
ip netns add $NS2
ip -netns $NS1 link add veth0 type veth peer name veth0 netns $NS2
ip -netns $NS1 link set dev veth0 up
ip -netns $NS2 link set dev veth0 up
ip -netns $NS1 addr add $H1_IP dev veth0
ip -netns $NS1 addr add $H1_IP6/64 dev veth0 nodad
ip -netns $NS2 addr add $H2_IP6/64 dev veth0 nodad
ip -netns $NS1 route add $RT1 via inet6 $H2_IP6
ip -netns $NS2 route add $RT2 via inet6 $H1_IP6
ip netns exec $NS2 sysctl -qw net.ipv4.icmp_ratelimit=0 net.ipv4.ip_forward=1
ip netns exec $NS1 ping -w 3 -i 0.5 $PINGADDR >/dev/null &
ip netns exec $NS1 timeout 10 tcpdump -tpni veth0 -c 1 'icmp and icmp[icmptype] != icmp-echo' > $TMPFILE 2>/dev/null
RESP_IP=$(awk '{print $2}' < $TMPFILE)
if [[ "$RESP_IP" != "192.0.0.8" ]]; then
    echo "FAIL - got ICMP response from $RESP_IP, should be 192.0.0.8"
    exit 1
else
    echo "OK"
    exit 0
fi
