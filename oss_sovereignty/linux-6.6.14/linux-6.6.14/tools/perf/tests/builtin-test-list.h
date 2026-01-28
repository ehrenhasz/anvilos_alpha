struct script_file {
	char *dir;
	char *file;
	char *desc;
};
const struct script_file *list_script_files(void);
int list_script_max_width(void);
