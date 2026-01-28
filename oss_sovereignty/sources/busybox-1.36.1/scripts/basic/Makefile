












hostprogs-y	:= fixdep split-include docproc
always		:= $(hostprogs-y)


$(addprefix $(obj)/,$(filter-out fixdep,$(always))): $(obj)/fixdep
