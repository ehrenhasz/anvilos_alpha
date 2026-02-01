
 
	int cur_pid = bpf_get_current_pid_tgid() >> 32;

	if (cur_pid == inPid)
		bpf_map_update_elem(&hashmap1, &inKey, &inValue, BPF_NOEXIST);

	return 0;
}

char _license[] SEC("license") = "GPL";
