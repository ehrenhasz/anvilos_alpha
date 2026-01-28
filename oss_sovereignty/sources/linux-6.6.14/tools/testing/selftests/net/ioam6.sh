ksft_skip=4
ALPHA=(
	1					# ID
	11111111				# Wide ID
	0xffff					# Ingress ID
	0xffffffff				# Ingress Wide ID
	101					# Egress ID
	101101					# Egress Wide ID
	0xdeadbee0				# Namespace Data
	0xcafec0caf00dc0de			# Namespace Wide Data
	777					# Schema ID (0xffffff = None)
	"something that will be 4n-aligned"	# Schema Data
)
BETA=(
	2
	22222222
	201
	201201
	202
	202202
	0xdeadbee1
	0xcafec0caf11dc0de
	666
	"Hello there -Obi"
)
GAMMA=(
	3
	33333333
	301
	301301
	0xffff
	0xffffffff
	0xdeadbee2
	0xcafec0caf22dc0de
	0xffffff
	""
)
TESTS_OUTPUT="
	out_undef_ns
	out_no_room
	out_bits
	out_full_supp_trace
"
TESTS_INPUT="
	in_undef_ns
	in_no_room
	in_oflag
	in_bits
	in_full_supp_trace
"
TESTS_GLOBAL="
	fwd_full_supp_trace
"
check_kernel_compatibility()
{
  ip netns add ioam-tmp-node
  ip link add name veth0 netns ioam-tmp-node type veth \
         peer name veth1 netns ioam-tmp-node
  ip -netns ioam-tmp-node link set veth0 up
  ip -netns ioam-tmp-node link set veth1 up
  ip -netns ioam-tmp-node ioam namespace add 0
  ns_ad=$?
  ip -netns ioam-tmp-node ioam namespace show | grep -q "namespace 0"
  ns_sh=$?
  if [[ $ns_ad != 0 || $ns_sh != 0 ]]
  then
    echo "SKIP: kernel version probably too old, missing ioam support"
    ip link del veth0 2>/dev/null || true
    ip netns del ioam-tmp-node || true
    exit $ksft_skip
  fi
  ip -netns ioam-tmp-node route add db02::/64 encap ioam6 mode inline \
         trace prealloc type 0x800000 ns 0 size 4 dev veth0
  tr_ad=$?
  ip -netns ioam-tmp-node -6 route | grep -q "encap ioam6"
  tr_sh=$?
  if [[ $tr_ad != 0 || $tr_sh != 0 ]]
  then
    echo "SKIP: cannot attach an ioam trace to a route, did you compile" \
         "without CONFIG_IPV6_IOAM6_LWTUNNEL?"
    ip link del veth0 2>/dev/null || true
    ip netns del ioam-tmp-node || true
    exit $ksft_skip
  fi
  ip link del veth0 2>/dev/null || true
  ip netns del ioam-tmp-node || true
  lsmod | grep -q "ip6_tunnel"
  ip6tnl_loaded=$?
  if [ $ip6tnl_loaded = 0 ]
  then
    encap_tests=0
  else
    modprobe ip6_tunnel &>/dev/null
    lsmod | grep -q "ip6_tunnel"
    encap_tests=$?
    if [ $encap_tests != 0 ]
    then
      ip a | grep -q "ip6tnl0"
      encap_tests=$?
      if [ $encap_tests != 0 ]
      then
        echo "Note: ip6_tunnel not found neither as a module nor inside the" \
             "kernel, tests that require it (encap mode) will be omitted"
      fi
    fi
  fi
}
cleanup()
{
  ip link del ioam-veth-alpha 2>/dev/null || true
  ip link del ioam-veth-gamma 2>/dev/null || true
  ip netns del ioam-node-alpha || true
  ip netns del ioam-node-beta || true
  ip netns del ioam-node-gamma || true
  if [ $ip6tnl_loaded != 0 ]
  then
    modprobe -r ip6_tunnel 2>/dev/null || true
  fi
}
setup()
{
  ip netns add ioam-node-alpha
  ip netns add ioam-node-beta
  ip netns add ioam-node-gamma
  ip link add name ioam-veth-alpha netns ioam-node-alpha type veth \
         peer name ioam-veth-betaL netns ioam-node-beta
  ip link add name ioam-veth-betaR netns ioam-node-beta type veth \
         peer name ioam-veth-gamma netns ioam-node-gamma
  ip -netns ioam-node-alpha link set ioam-veth-alpha name veth0
  ip -netns ioam-node-beta link set ioam-veth-betaL name veth0
  ip -netns ioam-node-beta link set ioam-veth-betaR name veth1
  ip -netns ioam-node-gamma link set ioam-veth-gamma name veth0
  ip -netns ioam-node-alpha addr add db01::2/64 dev veth0
  ip -netns ioam-node-alpha link set veth0 up
  ip -netns ioam-node-alpha link set lo up
  ip -netns ioam-node-alpha route add db02::/64 via db01::1 dev veth0
  ip -netns ioam-node-alpha route del db01::/64
  ip -netns ioam-node-alpha route add db01::/64 dev veth0
  ip -netns ioam-node-beta addr add db01::1/64 dev veth0
  ip -netns ioam-node-beta addr add db02::1/64 dev veth1
  ip -netns ioam-node-beta link set veth0 up
  ip -netns ioam-node-beta link set veth1 up
  ip -netns ioam-node-beta link set lo up
  ip -netns ioam-node-gamma addr add db02::2/64 dev veth0
  ip -netns ioam-node-gamma link set veth0 up
  ip -netns ioam-node-gamma link set lo up
  ip -netns ioam-node-gamma route add db01::/64 via db02::1 dev veth0
  ip netns exec ioam-node-alpha sysctl -wq net.ipv6.ioam6_id=${ALPHA[0]}
  ip netns exec ioam-node-alpha sysctl -wq net.ipv6.ioam6_id_wide=${ALPHA[1]}
  ip netns exec ioam-node-alpha sysctl -wq net.ipv6.conf.veth0.ioam6_id=${ALPHA[4]}
  ip netns exec ioam-node-alpha sysctl -wq net.ipv6.conf.veth0.ioam6_id_wide=${ALPHA[5]}
  ip -netns ioam-node-alpha ioam namespace add 123 data ${ALPHA[6]} wide ${ALPHA[7]}
  ip -netns ioam-node-alpha ioam schema add ${ALPHA[8]} "${ALPHA[9]}"
  ip -netns ioam-node-alpha ioam namespace set 123 schema ${ALPHA[8]}
  ip netns exec ioam-node-beta sysctl -wq net.ipv6.conf.all.forwarding=1
  ip netns exec ioam-node-beta sysctl -wq net.ipv6.ioam6_id=${BETA[0]}
  ip netns exec ioam-node-beta sysctl -wq net.ipv6.ioam6_id_wide=${BETA[1]}
  ip netns exec ioam-node-beta sysctl -wq net.ipv6.conf.veth0.ioam6_enabled=1
  ip netns exec ioam-node-beta sysctl -wq net.ipv6.conf.veth0.ioam6_id=${BETA[2]}
  ip netns exec ioam-node-beta sysctl -wq net.ipv6.conf.veth0.ioam6_id_wide=${BETA[3]}
  ip netns exec ioam-node-beta sysctl -wq net.ipv6.conf.veth1.ioam6_id=${BETA[4]}
  ip netns exec ioam-node-beta sysctl -wq net.ipv6.conf.veth1.ioam6_id_wide=${BETA[5]}
  ip -netns ioam-node-beta ioam namespace add 123 data ${BETA[6]} wide ${BETA[7]}
  ip -netns ioam-node-beta ioam schema add ${BETA[8]} "${BETA[9]}"
  ip -netns ioam-node-beta ioam namespace set 123 schema ${BETA[8]}
  ip netns exec ioam-node-gamma sysctl -wq net.ipv6.ioam6_id=${GAMMA[0]}
  ip netns exec ioam-node-gamma sysctl -wq net.ipv6.ioam6_id_wide=${GAMMA[1]}
  ip netns exec ioam-node-gamma sysctl -wq net.ipv6.conf.veth0.ioam6_enabled=1
  ip netns exec ioam-node-gamma sysctl -wq net.ipv6.conf.veth0.ioam6_id=${GAMMA[2]}
  ip netns exec ioam-node-gamma sysctl -wq net.ipv6.conf.veth0.ioam6_id_wide=${GAMMA[3]}
  ip -netns ioam-node-gamma ioam namespace add 123 data ${GAMMA[6]} wide ${GAMMA[7]}
  sleep 1
  ip netns exec ioam-node-alpha ping6 -c 5 -W 1 db02::2 &>/dev/null
  if [ $? != 0 ]
  then
    echo "Setup FAILED"
    cleanup &>/dev/null
    exit 0
  fi
}
log_test_passed()
{
  local desc=$1
  printf "TEST: %-60s  [ OK ]\n" "${desc}"
}
log_test_failed()
{
  local desc=$1
  printf "TEST: %-60s  [FAIL]\n" "${desc}"
}
log_results()
{
  echo "- Tests passed: ${npassed}"
  echo "- Tests failed: ${nfailed}"
}
run_test()
{
  local name=$1
  local desc=$2
  local node_src=$3
  local node_dst=$4
  local ip6_src=$5
  local ip6_dst=$6
  local if_dst=$7
  local trace_type=$8
  local ioam_ns=$9
  ip netns exec $node_dst ./ioam6_parser $if_dst $name $ip6_src $ip6_dst \
         $trace_type $ioam_ns &
  local spid=$!
  sleep 0.1
  ip netns exec $node_src ping6 -t 64 -c 1 -W 1 $ip6_dst &>/dev/null
  if [ $? != 0 ]
  then
    nfailed=$((nfailed+1))
    log_test_failed "${desc}"
    kill -2 $spid &>/dev/null
  else
    wait $spid
    if [ $? = 0 ]
    then
      npassed=$((npassed+1))
      log_test_passed "${desc}"
    else
      nfailed=$((nfailed+1))
      log_test_failed "${desc}"
    fi
  fi
}
run()
{
  echo
  printf "%0.s-" {1..74}
  echo
  echo "OUTPUT tests"
  printf "%0.s-" {1..74}
  echo
  ip netns exec ioam-node-beta sysctl -wq net.ipv6.conf.veth0.ioam6_enabled=0
  for t in $TESTS_OUTPUT
  do
    $t "inline"
    [ $encap_tests = 0 ] && $t "encap"
  done
  ip netns exec ioam-node-beta sysctl -wq net.ipv6.conf.veth0.ioam6_enabled=1
  ip -netns ioam-node-alpha route change db01::/64 dev veth0
  echo
  printf "%0.s-" {1..74}
  echo
  echo "INPUT tests"
  printf "%0.s-" {1..74}
  echo
  ip -netns ioam-node-alpha ioam namespace del 123
  for t in $TESTS_INPUT
  do
    $t "inline"
    [ $encap_tests = 0 ] && $t "encap"
  done
  ip -netns ioam-node-alpha ioam namespace add 123 \
         data ${ALPHA[6]} wide ${ALPHA[7]}
  ip -netns ioam-node-alpha ioam namespace set 123 schema ${ALPHA[8]}
  ip -netns ioam-node-alpha route change db01::/64 dev veth0
  echo
  printf "%0.s-" {1..74}
  echo
  echo "GLOBAL tests"
  printf "%0.s-" {1..74}
  echo
  for t in $TESTS_GLOBAL
  do
    $t "inline"
    [ $encap_tests = 0 ] && $t "encap"
  done
  echo
  log_results
}
bit2type=(
  0x800000 0x400000 0x200000 0x100000 0x080000 0x040000 0x020000 0x010000
  0x008000 0x004000 0x002000 0x001000 0x000800 0x000400 0x000200 0x000100
  0x000080 0x000040 0x000020 0x000010 0x000008 0x000004 0x000002
)
bit2size=( 4 4 4 4 4 4 4 4 8 8 8 4 4 4 4 4 4 4 4 4 4 4 4 )
out_undef_ns()
{
  local desc="Unknown IOAM namespace"
  [ "$1" = "encap" ] && mode="$1 tundst db01::1" || mode="$1"
  [ "$1" = "encap" ] && ip -netns ioam-node-beta link set ip6tnl0 up
  ip -netns ioam-node-alpha route change db01::/64 encap ioam6 mode $mode \
         trace prealloc type 0x800000 ns 0 size 4 dev veth0
  run_test ${FUNCNAME[0]} "${desc} ($1 mode)" ioam-node-alpha ioam-node-beta \
         db01::2 db01::1 veth0 0x800000 0
  [ "$1" = "encap" ] && ip -netns ioam-node-beta link set ip6tnl0 down
}
out_no_room()
{
  local desc="Missing trace room"
  [ "$1" = "encap" ] && mode="$1 tundst db01::1" || mode="$1"
  [ "$1" = "encap" ] && ip -netns ioam-node-beta link set ip6tnl0 up
  ip -netns ioam-node-alpha route change db01::/64 encap ioam6 mode $mode \
         trace prealloc type 0xc00000 ns 123 size 4 dev veth0
  run_test ${FUNCNAME[0]} "${desc} ($1 mode)" ioam-node-alpha ioam-node-beta \
         db01::2 db01::1 veth0 0xc00000 123
  [ "$1" = "encap" ] && ip -netns ioam-node-beta link set ip6tnl0 down
}
out_bits()
{
  local desc="Trace type with bit <n> only"
  local tmp=${bit2size[22]}
  bit2size[22]=$(( $tmp + ${#ALPHA[9]} + ((4 - (${#ALPHA[9]} % 4)) % 4) ))
  [ "$1" = "encap" ] && mode="$1 tundst db01::1" || mode="$1"
  [ "$1" = "encap" ] && ip -netns ioam-node-beta link set ip6tnl0 up
  for i in {0..22}
  do
    ip -netns ioam-node-alpha route change db01::/64 encap ioam6 mode $mode \
           trace prealloc type ${bit2type[$i]} ns 123 size ${bit2size[$i]} \
           dev veth0 &>/dev/null
    local cmd_res=$?
    local descr="${desc/<n>/$i}"
    if [[ $i -ge 12 && $i -le 21 ]]
    then
      if [ $cmd_res != 0 ]
      then
        npassed=$((npassed+1))
        log_test_passed "$descr"
      else
        nfailed=$((nfailed+1))
        log_test_failed "$descr"
      fi
    else
	run_test "out_bit$i" "$descr ($1 mode)" ioam-node-alpha \
           ioam-node-beta db01::2 db01::1 veth0 ${bit2type[$i]} 123
    fi
  done
  [ "$1" = "encap" ] && ip -netns ioam-node-beta link set ip6tnl0 down
  bit2size[22]=$tmp
}
out_full_supp_trace()
{
  local desc="Full supported trace"
  [ "$1" = "encap" ] && mode="$1 tundst db01::1" || mode="$1"
  [ "$1" = "encap" ] && ip -netns ioam-node-beta link set ip6tnl0 up
  ip -netns ioam-node-alpha route change db01::/64 encap ioam6 mode $mode \
         trace prealloc type 0xfff002 ns 123 size 100 dev veth0
  run_test ${FUNCNAME[0]} "${desc} ($1 mode)" ioam-node-alpha ioam-node-beta \
         db01::2 db01::1 veth0 0xfff002 123
  [ "$1" = "encap" ] && ip -netns ioam-node-beta link set ip6tnl0 down
}
in_undef_ns()
{
  local desc="Unknown IOAM namespace"
  [ "$1" = "encap" ] && mode="$1 tundst db01::1" || mode="$1"
  [ "$1" = "encap" ] && ip -netns ioam-node-beta link set ip6tnl0 up
  ip -netns ioam-node-alpha route change db01::/64 encap ioam6 mode $mode \
         trace prealloc type 0x800000 ns 0 size 4 dev veth0
  run_test ${FUNCNAME[0]} "${desc} ($1 mode)" ioam-node-alpha ioam-node-beta \
         db01::2 db01::1 veth0 0x800000 0
  [ "$1" = "encap" ] && ip -netns ioam-node-beta link set ip6tnl0 down
}
in_no_room()
{
  local desc="Missing trace room"
  [ "$1" = "encap" ] && mode="$1 tundst db01::1" || mode="$1"
  [ "$1" = "encap" ] && ip -netns ioam-node-beta link set ip6tnl0 up
  ip -netns ioam-node-alpha route change db01::/64 encap ioam6 mode $mode \
         trace prealloc type 0xc00000 ns 123 size 4 dev veth0
  run_test ${FUNCNAME[0]} "${desc} ($1 mode)" ioam-node-alpha ioam-node-beta \
         db01::2 db01::1 veth0 0xc00000 123
  [ "$1" = "encap" ] && ip -netns ioam-node-beta link set ip6tnl0 down
}
in_bits()
{
  local desc="Trace type with bit <n> only"
  local tmp=${bit2size[22]}
  bit2size[22]=$(( $tmp + ${#BETA[9]} + ((4 - (${#BETA[9]} % 4)) % 4) ))
  [ "$1" = "encap" ] && mode="$1 tundst db01::1" || mode="$1"
  [ "$1" = "encap" ] && ip -netns ioam-node-beta link set ip6tnl0 up
  for i in {0..11} {22..22}
  do
    ip -netns ioam-node-alpha route change db01::/64 encap ioam6 mode $mode \
           trace prealloc type ${bit2type[$i]} ns 123 size ${bit2size[$i]} \
           dev veth0
    run_test "in_bit$i" "${desc/<n>/$i} ($1 mode)" ioam-node-alpha \
           ioam-node-beta db01::2 db01::1 veth0 ${bit2type[$i]} 123
  done
  [ "$1" = "encap" ] && ip -netns ioam-node-beta link set ip6tnl0 down
  bit2size[22]=$tmp
}
in_oflag()
{
  local desc="Overflow flag is set"
  ip -netns ioam-node-alpha ioam namespace add 123
  [ "$1" = "encap" ] && mode="$1 tundst db01::1" || mode="$1"
  [ "$1" = "encap" ] && ip -netns ioam-node-beta link set ip6tnl0 up
  ip -netns ioam-node-alpha route change db01::/64 encap ioam6 mode $mode \
         trace prealloc type 0xc00000 ns 123 size 4 dev veth0
  run_test ${FUNCNAME[0]} "${desc} ($1 mode)" ioam-node-alpha ioam-node-beta \
         db01::2 db01::1 veth0 0xc00000 123
  [ "$1" = "encap" ] && ip -netns ioam-node-beta link set ip6tnl0 down
  ip -netns ioam-node-alpha ioam namespace del 123
}
in_full_supp_trace()
{
  local desc="Full supported trace"
  [ "$1" = "encap" ] && mode="$1 tundst db01::1" || mode="$1"
  [ "$1" = "encap" ] && ip -netns ioam-node-beta link set ip6tnl0 up
  ip -netns ioam-node-alpha route change db01::/64 encap ioam6 mode $mode \
         trace prealloc type 0xfff002 ns 123 size 80 dev veth0
  run_test ${FUNCNAME[0]} "${desc} ($1 mode)" ioam-node-alpha ioam-node-beta \
         db01::2 db01::1 veth0 0xfff002 123
  [ "$1" = "encap" ] && ip -netns ioam-node-beta link set ip6tnl0 down
}
fwd_full_supp_trace()
{
  local desc="Forward - Full supported trace"
  [ "$1" = "encap" ] && mode="$1 tundst db02::2" || mode="$1"
  [ "$1" = "encap" ] && ip -netns ioam-node-gamma link set ip6tnl0 up
  ip -netns ioam-node-alpha route change db02::/64 encap ioam6 mode $mode \
         trace prealloc type 0xfff002 ns 123 size 244 via db01::1 dev veth0
  run_test ${FUNCNAME[0]} "${desc} ($1 mode)" ioam-node-alpha ioam-node-gamma \
         db01::2 db02::2 veth0 0xfff002 123
  [ "$1" = "encap" ] && ip -netns ioam-node-gamma link set ip6tnl0 down
}
npassed=0
nfailed=0
if [ "$(id -u)" -ne 0 ]
then
  echo "SKIP: Need root privileges"
  exit $ksft_skip
fi
if [ ! -x "$(command -v ip)" ]
then
  echo "SKIP: Could not run test without ip tool"
  exit $ksft_skip
fi
ip ioam &>/dev/null
if [ $? = 1 ]
then
  echo "SKIP: iproute2 too old, missing ioam command"
  exit $ksft_skip
fi
check_kernel_compatibility
cleanup &>/dev/null
setup
run
cleanup &>/dev/null
