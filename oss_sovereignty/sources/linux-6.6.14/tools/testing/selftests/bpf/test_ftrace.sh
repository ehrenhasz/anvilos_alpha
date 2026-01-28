if [[ -e /sys/kernel/tracing/trace ]]; then
    TR=/sys/kernel/tracing/
else
    TR=/sys/kernel/debug/tracing/
fi
clear_trace() { 
    echo > $TR/trace
}
disable_tracing() { 
    echo 0 > $TR/tracing_on
}
enable_tracing() { 
    echo 1 > $TR/tracing_on
}
reset_tracer() { 
    echo nop > $TR/current_tracer
}
disable_tracing
clear_trace
echo "" > $TR/set_ftrace_filter
echo '*printk* *console* *wake* *serial* *lock*' > $TR/set_ftrace_notrace
echo "bpf_prog_test*" > $TR/set_graph_function
echo "" > $TR/set_graph_notrace
echo function_graph > $TR/current_tracer
enable_tracing
./test_progs -t fentry
./test_progs -t fexit
disable_tracing
clear_trace
reset_tracer
exit 0
