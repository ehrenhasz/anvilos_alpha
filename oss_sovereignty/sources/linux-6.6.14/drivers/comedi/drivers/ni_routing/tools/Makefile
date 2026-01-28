








ALL:
	@echo Typical targets:
	@echo "\`make csv-files\`"
	@echo "  Creates new csv-files using content of c-files of existing"
	@echo "  ni_routing/* content.  New csv files are placed in csv"
	@echo "  sub-directory."
	@echo "\`make c-files\`"
	@echo "  Creates new c-files using content of csv sub-directory.  These"
	@echo "  new c-files can be compared to the active content in the"
	@echo "  ni_routing directory."
	@echo "\`make csv-blank\`"
	@echo "  Create a new blank csv file.  This is useful for establishing a"
	@echo "  new data table for either a device family \(less likely\) or a"
	@echo "  specific board of an existing device family \(more likely\)."
	@echo "\`make clean-partial\`"
	@echo "  Remove all generated files/directories EXCEPT for csv/c files."
	@echo "\`make clean\`"
	@echo "  Remove all generated files/directories."
	@echo "\`make everything\`"
	@echo "  Build all csv-files, then all new c-files."

everything : csv-files c-files csv-blank

CPPFLAGS = -D__user=
INC_UAPI = ../../../../../include/uapi

comedi_h.py: $(INC_UAPI)/linux/comedi.h
	ctypesgen $< --include "sys/ioctl.h" --cpp 'gcc -E $(CPPFLAGS)' -o $@

convert_c_to_py: all_cfiles.c linux/comedi.h
	gcc -g -I. convert_c_to_py.c -o convert_c_to_py -std=c99




linux/comedi.h: $(INC_UAPI)/linux/comedi.h
	mkdir -p linux
	ln -snf ../$< $@

ni_values.py: convert_c_to_py
	./convert_c_to_py

csv-files : ni_values.py comedi_h.py
	./convert_py_to_csv.py

csv-blank : comedi_h.py
	./make_blank_csv.py
	@echo New blank csv signal table in csv/blank_route_table.csv

c-files : comedi_h.py
	./convert_csv_to_c.py --route_values --device_routes

ROUTE_VALUES_SRC=$(wildcard ../ni_route_values/*.c)
DEVICE_ROUTES_SRC=$(wildcard ../ni_device_routes/*.c)
all_cfiles.c : $(DEVICE_ROUTES_SRC) $(ROUTE_VALUES_SRC)
	@for i in $(DEVICE_ROUTES_SRC) $(ROUTE_VALUES_SRC); do \
		echo "
	done

clean-partial :
	$(RM) -rf comedi_h.py ni_values.py convert_c_to_py all_cfiles.c *.pyc \
		__pycache__/

clean : clean-partial
	$(RM) -rf c/ csv/ linux/













