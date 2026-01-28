source bond_topo_2d1c.sh
setup_prepare()
{
	gateway_create
	server_create
	client_create
	local i=2
	ip -n ${s_ns} link add eth${i} type veth peer name s${i} netns ${g_ns}
	ip -n ${g_ns} link set s${i} up
	ip -n ${g_ns} link set s${i} master br0
	ip -n ${s_ns} link set eth${i} master bond0
	tc -n ${g_ns} qdisc add dev s${i} clsact
}
