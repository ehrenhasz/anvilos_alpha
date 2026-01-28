dist_noinst_DATA += %D%/libzfs_core %D%/README %D%/LICENSE %D%/docs
SUBSTFILES += %D%/setup.py

if PYZFS_ENABLED
ALL_LOCAL += pyzfs-all-local
pyzfs_V_1 = -v
pyzfs-all-local: %D%/setup.py
	cd %D% && $(PYTHON) setup.py -q $(pyzfs_V_$(V)) egg_info -e . build


















install-exec-local:
	cd %D% && $(PYTHON) setup.py egg_info -e . install \
	  --prefix $(prefix) \
	  --root $(DESTDIR)/ \
	  --install-lib $(pythonsitedir) \
	  --single-version-externally-managed \
	  --verbose

CLEAN_LOCAL += pyzfs-clean-local
pyzfs-clean-local:
	-$(RM) -r %D%/build/ %D%/pyzfs.egg-info/
endif
