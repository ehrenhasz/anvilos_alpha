BPF_FILE="bpf_flow.bpf.o"
export TESTNAME=test_flow_dissector
unmount=0
ksft_skip=4
msg="skip all tests:"
if [ $UID != 0 ]; then
	echo $msg please run this as root >&2
	exit $ksft_skip
fi
if [[ -z $(ip netns identify $$) ]]; then
	err=0
	if bpftool="$(which bpftool)"; then
		echo "Testing global flow dissector..."
		$bpftool prog loadall $BPF_FILE /sys/fs/bpf/flow \
			type flow_dissector
		if ! unshare --net $bpftool prog attach pinned \
			/sys/fs/bpf/flow/_dissect flow_dissector; then
			echo "Unexpected unsuccessful attach in namespace" >&2
			err=1
		fi
		$bpftool prog attach pinned /sys/fs/bpf/flow/_dissect \
			flow_dissector
		if unshare --net $bpftool prog attach pinned \
			/sys/fs/bpf/flow/_dissect flow_dissector; then
			echo "Unexpected successful attach in namespace" >&2
			err=1
		fi
		if ! $bpftool prog detach pinned \
			/sys/fs/bpf/flow/_dissect flow_dissector; then
			echo "Failed to detach flow dissector" >&2
			err=1
		fi
		rm -rf /sys/fs/bpf/flow
	else
		echo "Skipping root flow dissector test, bpftool not found" >&2
	fi
	../net/in_netns.sh "$0" "$@"
	err=$(( $err + $? ))
	if (( $err == 0 )); then
		echo "selftests: $TESTNAME [PASS]";
	else
		echo "selftests: $TESTNAME [FAILED]";
	fi
	exit $err
fi
exit_handler()
{
	set +e
	tc filter del dev lo ingress pref 1337 2> /dev/null
	tc qdisc del dev lo ingress 2> /dev/null
	./flow_dissector_load -d 2> /dev/null
	if [ $unmount -ne 0 ]; then
		umount bpffs 2> /dev/null
	fi
}
set -e
trap exit_handler 0 2 3 6 9
if /bin/mount | grep /sys/fs/bpf > /dev/null; then
	echo "bpffs already mounted"
else
	echo "bpffs not mounted. Mounting..."
	unmount=1
	/bin/mount bpffs /sys/fs/bpf -t bpf
fi
./flow_dissector_load -p $BPF_FILE -s _dissect
tc qdisc add dev lo ingress
echo 0 > /proc/sys/net/ipv4/conf/default/rp_filter
echo 0 > /proc/sys/net/ipv4/conf/all/rp_filter
echo 0 > /proc/sys/net/ipv4/conf/lo/rp_filter
echo "Testing IPv4..."
tc filter add dev lo parent ffff: protocol ip pref 1337 flower ip_proto \
	udp src_port 9 action drop
./test_flow_dissector -i 4 -f 8
./test_flow_dissector -i 4 -f 9 -F
./test_flow_dissector -i 4 -f 10
echo "Testing IPv4 from 127.0.0.127 (fallback to generic dissector)..."
./test_flow_dissector -i 4 -S 127.0.0.127 -f 8
./test_flow_dissector -i 4 -S 127.0.0.127 -f 9 -F
./test_flow_dissector -i 4 -S 127.0.0.127 -f 10
echo "Testing IPIP..."
./with_addr.sh ./with_tunnels.sh ./test_flow_dissector -o 4 -e bare -i 4 \
	-D 192.168.0.1 -S 1.1.1.1 -f 8
./with_addr.sh ./with_tunnels.sh ./test_flow_dissector -o 4 -e bare -i 4 \
	-D 192.168.0.1 -S 1.1.1.1 -f 9 -F
./with_addr.sh ./with_tunnels.sh ./test_flow_dissector -o 4 -e bare -i 4 \
	-D 192.168.0.1 -S 1.1.1.1 -f 10
echo "Testing IPv4 + GRE..."
./with_addr.sh ./with_tunnels.sh ./test_flow_dissector -o 4 -e gre -i 4 \
	-D 192.168.0.1 -S 1.1.1.1 -f 8
./with_addr.sh ./with_tunnels.sh ./test_flow_dissector -o 4 -e gre -i 4 \
	-D 192.168.0.1 -S 1.1.1.1 -f 9 -F
./with_addr.sh ./with_tunnels.sh ./test_flow_dissector -o 4 -e gre -i 4 \
	-D 192.168.0.1 -S 1.1.1.1 -f 10
tc filter del dev lo ingress pref 1337
echo "Testing port range..."
tc filter add dev lo parent ffff: protocol ip pref 1337 flower ip_proto \
	udp src_port 8-10 action drop
./test_flow_dissector -i 4 -f 7
./test_flow_dissector -i 4 -f 9 -F
./test_flow_dissector -i 4 -f 11
tc filter del dev lo ingress pref 1337
echo "Testing IPv6..."
tc filter add dev lo parent ffff: protocol ipv6 pref 1337 flower ip_proto \
	udp src_port 9 action drop
./test_flow_dissector -i 6 -f 8
./test_flow_dissector -i 6 -f 9 -F
./test_flow_dissector -i 6 -f 10
exit 0
