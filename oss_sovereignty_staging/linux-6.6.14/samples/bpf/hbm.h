 
struct hbm_vqueue {
	struct bpf_spin_lock lock;
	 
	unsigned long long lasttime;	 
	int credit;			 
	unsigned int rate;		 
};

struct hbm_queue_stats {
	unsigned long rate;		 
	unsigned long stats:1,		 
		loopback:1,		 
		no_cn:1;		 
	unsigned long long pkts_marked;
	unsigned long long bytes_marked;
	unsigned long long pkts_dropped;
	unsigned long long bytes_dropped;
	unsigned long long pkts_total;
	unsigned long long bytes_total;
	unsigned long long firstPacketTime;
	unsigned long long lastPacketTime;
	unsigned long long pkts_ecn_ce;
	unsigned long long returnValCount[4];
	unsigned long long sum_cwnd;
	unsigned long long sum_rtt;
	unsigned long long sum_cwnd_cnt;
	long long sum_credit;
};
