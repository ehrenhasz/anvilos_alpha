source "$relative_path/mirror_topo_lib.sh"
mirror_gre_topo_h3_create()
{
	mirror_topo_h3_create
	tunnel_create h3-gt4 gretap 192.0.2.130 192.0.2.129
	ip link set h3-gt4 vrf v$h3
	matchall_sink_create h3-gt4
	tunnel_create h3-gt6 ip6gretap 2001:db8:2::2 2001:db8:2::1
	ip link set h3-gt6 vrf v$h3
	matchall_sink_create h3-gt6
}
mirror_gre_topo_h3_destroy()
{
	tunnel_destroy h3-gt6
	tunnel_destroy h3-gt4
	mirror_topo_h3_destroy
}
mirror_gre_topo_switch_create()
{
	mirror_topo_switch_create
	tunnel_create gt4 gretap 192.0.2.129 192.0.2.130 \
		      ttl 100 tos inherit
	tunnel_create gt6 ip6gretap 2001:db8:2::1 2001:db8:2::2 \
		      ttl 100 tos inherit allow-localremote
}
mirror_gre_topo_switch_destroy()
{
	tunnel_destroy gt6
	tunnel_destroy gt4
	mirror_topo_switch_destroy
}
mirror_gre_topo_create()
{
	mirror_topo_h1_create
	mirror_topo_h2_create
	mirror_gre_topo_h3_create
	mirror_gre_topo_switch_create
}
mirror_gre_topo_destroy()
{
	mirror_gre_topo_switch_destroy
	mirror_gre_topo_h3_destroy
	mirror_topo_h2_destroy
	mirror_topo_h1_destroy
}
