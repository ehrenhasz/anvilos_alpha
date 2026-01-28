include("manifest.py")
freeze("$(MPY_DIR)/tests/micropython", "native_misc.py")
freeze("$(MPY_DIR)/tests/micropython", "viper_misc.py")
