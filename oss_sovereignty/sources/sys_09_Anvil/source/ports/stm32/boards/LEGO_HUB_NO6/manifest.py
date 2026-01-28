include("$(PORT_DIR)/boards/manifest.py")
module("fwupdate.py", base_path="$(PORT_DIR)/mboot", opt=3)
module("appupdate.py", opt=3)
