. xsk_prereqs.sh
ETH=""
while getopts "vi:d" flag
do
	case "${flag}" in
		v) verbose=1;;
		d) debug=1;;
		i) ETH=${OPTARG};;
	esac
done
TEST_NAME="PREREQUISITES"
URANDOM=/dev/urandom
[ ! -e "${URANDOM}" ] && { echo "${URANDOM} not found. Skipping tests."; test_exit $ksft_fail; }
VETH0_POSTFIX=$(cat ${URANDOM} | tr -dc '0-9' | fold -w 256 | head -n 1 | head --bytes 4)
VETH0=ve${VETH0_POSTFIX}
VETH1_POSTFIX=$(cat ${URANDOM} | tr -dc '0-9' | fold -w 256 | head -n 1 | head --bytes 4)
VETH1=ve${VETH1_POSTFIX}
MTU=1500
trap ctrl_c INT
function ctrl_c() {
        cleanup_exit ${VETH0} ${VETH1}
	exit 1
}
setup_vethPairs() {
	if [[ $verbose -eq 1 ]]; then
	        echo "setting up ${VETH0}"
	fi
	ip link add ${VETH0} numtxqueues 4 numrxqueues 4 type veth peer name ${VETH1} numtxqueues 4 numrxqueues 4
	if [ -f /proc/net/if_inet6 ]; then
		echo 1 > /proc/sys/net/ipv6/conf/${VETH0}/disable_ipv6
		echo 1 > /proc/sys/net/ipv6/conf/${VETH1}/disable_ipv6
	fi
	if [[ $verbose -eq 1 ]]; then
	        echo "setting up ${VETH1}"
	fi
	if [[ $busy_poll -eq 1 ]]; then
	        echo 2 > /sys/class/net/${VETH0}/napi_defer_hard_irqs
		echo 200000 > /sys/class/net/${VETH0}/gro_flush_timeout
		echo 2 > /sys/class/net/${VETH1}/napi_defer_hard_irqs
		echo 200000 > /sys/class/net/${VETH1}/gro_flush_timeout
	fi
	ip link set ${VETH1} mtu ${MTU}
	ip link set ${VETH0} mtu ${MTU}
	ip link set ${VETH1} up
	ip link set ${VETH0} up
}
if [ ! -z $ETH ]; then
	VETH0=${ETH}
	VETH1=${ETH}
else
	validate_root_exec
	validate_veth_support ${VETH0}
	validate_ip_utility
	setup_vethPairs
	retval=$?
	if [ $retval -ne 0 ]; then
		test_status $retval "${TEST_NAME}"
		cleanup_exit ${VETH0} ${VETH1}
		exit $retval
	fi
fi
if [[ $verbose -eq 1 ]]; then
	ARGS+="-v "
fi
retval=$?
test_status $retval "${TEST_NAME}"
statusList=()
TEST_NAME="XSK_SELFTESTS_${VETH0}_SOFTIRQ"
if [[ $debug -eq 1 ]]; then
    echo "-i" ${VETH0} "-i" ${VETH1}
    exit
fi
exec_xskxceiver
if [ -z $ETH ]; then
	cleanup_exit ${VETH0} ${VETH1}
else
	cleanup_iface ${ETH} ${MTU}
fi
TEST_NAME="XSK_SELFTESTS_${VETH0}_BUSY_POLL"
busy_poll=1
if [ -z $ETH ]; then
	setup_vethPairs
fi
exec_xskxceiver
if [ -z $ETH ]; then
	cleanup_exit ${VETH0} ${VETH1}
else
	cleanup_iface ${ETH} ${MTU}
fi
failures=0
echo -e "\nSummary:"
for i in "${!statusList[@]}"
do
	if [ ${statusList[$i]} -ne 0 ]; then
	        test_status ${statusList[$i]} ${nameList[$i]}
		failures=1
	fi
done
if [ $failures -eq 0 ]; then
        echo "All tests successful!"
fi
