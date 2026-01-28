


#ifndef INCLUDE__PERF_HISI_PTT_H__
#define INCLUDE__PERF_HISI_PTT_H__

#define HISI_PTT_PMU_NAME		"hisi_ptt"
#define HISI_PTT_AUXTRACE_PRIV_SIZE	sizeof(u64)

struct auxtrace_record *hisi_ptt_recording_init(int *err,
						struct perf_pmu *hisi_ptt_pmu);

int hisi_ptt_process_auxtrace_info(union perf_event *event,
				   struct perf_session *session);

#endif
