original_stack_limit=$(ulimit -s)
./mmap_default
ulimit -s unlimited
./mmap_bottomup
ulimit -s $original_stack_limit
