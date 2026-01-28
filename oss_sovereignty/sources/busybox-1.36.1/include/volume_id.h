

char *get_devname_from_label(const char *spec);
char *get_devname_from_uuid(const char *spec);
void display_uuid_cache(int scan_devices);


int resolve_mount_spec(char **fsname);
int add_to_uuid_cache(const char *device);
