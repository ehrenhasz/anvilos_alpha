
SHELL = /bin/bash








include Filelist
$(eval all_patterns := $(shell \
	make -f Filelist --question --print-data-base --no-builtin-rules \
		--no-builtin-variables 2>/dev/null \
	| sed -nre \
		'/^
			n; \
			s/ = .*//; \
			T; \
			s/.*/$$(\0)/; \
			p; \
		}'))



all_patterns := $(all_patterns) \
	$(foreach readme, $(IN_README_DIR), READMEdir/$(readme))




listed_files := $(wildcard $(all_patterns))


.PHONY: check
check:
	@
	@
	$(file > Filelist-listed-files)
	$(foreach filename, $(listed_files),\
		$(file >> Filelist-listed-files,$(filename)))
	@
	@
	diff -u0 --label files-in-git <(git ls-files | sort) \
		--label Filelist <(sort --unique Filelist-listed-files); \
	RV=$$?; \
	rm Filelist-listed-files; \
	(($$RV != 0)) && echo "Add files to the right variable in Filelist."; \
	exit $$RV
