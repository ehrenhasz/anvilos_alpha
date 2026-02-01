

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <test_progs.h>

__u32 get_map_id(struct bpf_object *obj, const char *name)
{
	struct bpf_map_info map_info = {};
	__u32 map_info_len, duration = 0;
	struct bpf_map *map;
	int err;

	map_info_len = sizeof(map_info);

	map = bpf_object__find_map_by_name(obj, name);
	if (CHECK(!map, "find map", "NULL map"))
		return 0;

	err = bpf_map_get_info_by_fd(bpf_map__fd(map),
				     &map_info, &map_info_len);
	CHECK(err, "get map info", "err %d errno %d", err, errno);
	return map_info.id;
}

void test_pinning(void)
{
	const char *file_invalid = "./test_pinning_invalid.bpf.o";
	const char *custpinpath = "/sys/fs/bpf/custom/pinmap";
	const char *nopinpath = "/sys/fs/bpf/nopinmap";
	const char *nopinpath2 = "/sys/fs/bpf/nopinmap2";
	const char *custpath = "/sys/fs/bpf/custom";
	const char *pinpath = "/sys/fs/bpf/pinmap";
	const char *file = "./test_pinning.bpf.o";
	__u32 map_id, map_id2, duration = 0;
	struct stat statbuf = {};
	struct bpf_object *obj;
	struct bpf_map *map;
	int err, map_fd;
	DECLARE_LIBBPF_OPTS(bpf_object_open_opts, opts,
		.pin_root_path = custpath,
	);

	 
	obj = bpf_object__open_file(file_invalid, NULL);
	err = libbpf_get_error(obj);
	if (CHECK(err != -EINVAL, "invalid open", "err %d errno %d\n", err, errno)) {
		obj = NULL;
		goto out;
	}

	 
	obj = bpf_object__open_file(file, NULL);
	err = libbpf_get_error(obj);
	if (CHECK(err, "default open", "err %d errno %d\n", err, errno)) {
		obj = NULL;
		goto out;
	}

	err = bpf_object__load(obj);
	if (CHECK(err, "default load", "err %d errno %d\n", err, errno))
		goto out;

	 
	err = stat(pinpath, &statbuf);
	if (CHECK(err, "stat pinpath", "err %d errno %d\n", err, errno))
		goto out;

	 
	err = stat(nopinpath, &statbuf);
	if (CHECK(!err || errno != ENOENT, "stat nopinpath",
		  "err %d errno %d\n", err, errno))
		goto out;

	 
	err = stat(nopinpath2, &statbuf);
	if (CHECK(!err || errno != ENOENT, "stat nopinpath2",
		  "err %d errno %d\n", err, errno))
		goto out;

	map_id = get_map_id(obj, "pinmap");
	if (!map_id)
		goto out;

	bpf_object__close(obj);

	obj = bpf_object__open_file(file, NULL);
	if (CHECK_FAIL(libbpf_get_error(obj))) {
		obj = NULL;
		goto out;
	}

	err = bpf_object__load(obj);
	if (CHECK(err, "default load", "err %d errno %d\n", err, errno))
		goto out;

	 
	map_id2 = get_map_id(obj, "pinmap");
	if (CHECK(map_id != map_id2, "check reuse",
		  "err %d errno %d id %d id2 %d\n", err, errno, map_id, map_id2))
		goto out;

	 
	map = bpf_object__find_map_by_name(obj, "pinmap");
	if (CHECK(!map, "find map", "NULL map"))
		goto out;

	err = bpf_map__pin(map, NULL);
	if (CHECK(err, "re-pin map", "err %d errno %d\n", err, errno))
		goto out;

	 
	err = bpf_map__pin(map, "/sys/fs/bpf/other");
	if (CHECK(!err, "pin map different", "err %d errno %d\n", err, errno))
		goto out;

	 
	err = bpf_object__unpin_maps(obj, NULL);
	if (CHECK(err, "unpin maps", "err %d errno %d\n", err, errno))
		goto out;

	 
	err = bpf_object__pin_maps(obj, NULL);
	if (CHECK(err, "pin maps", "err %d errno %d\n", err, errno))
		goto out;

	 
	if (!ASSERT_STREQ(bpf_map__pin_path(map), pinpath, "get pin path"))
		goto out;

	 
	map = bpf_object__find_map_by_name(obj, "nopinmap");
	if (CHECK(!map, "find map", "NULL map"))
		goto out;

	err = bpf_map__set_pin_path(map, custpinpath);
	if (CHECK(err, "set pin path", "err %d errno %d\n", err, errno))
		goto out;

	 
	if (!ASSERT_STREQ(bpf_map__pin_path(map), custpinpath,
			  "get pin path after set"))
		goto out;

	 
	err = bpf_object__pin_maps(obj, NULL);
	if (CHECK(err, "pin maps", "err %d errno %d\n", err, errno))
		goto out;

	 
	err = stat(custpinpath, &statbuf);
	if (CHECK(err, "stat custpinpath", "err %d errno %d\n", err, errno))
		goto out;

	 
	err = unlink(custpinpath);
	if (CHECK(err, "unlink custpinpath", "err %d errno %d\n", err, errno))
		goto out;

	err = rmdir(custpath);
	if (CHECK(err, "rmdir custpindir", "err %d errno %d\n", err, errno))
		goto out;

	bpf_object__close(obj);

	 
	obj = bpf_object__open_file(file, NULL);
	err = libbpf_get_error(obj);
	if (CHECK(err, "default open", "err %d errno %d\n", err, errno)) {
		obj = NULL;
		goto out;
	}

	 
	bpf_object__for_each_map(map, obj) {
		if (!strcmp(bpf_map__name(map), "nopinmap"))
			err = bpf_map__set_pin_path(map, nopinpath2);
		else if (!strcmp(bpf_map__name(map), "nopinmap2"))
			err = bpf_map__set_pin_path(map, pinpath);
		else
			continue;

		if (CHECK(err, "set pin path", "err %d errno %d\n", err, errno))
			goto out;
	}

	 
	err = bpf_object__load(obj);
	if (CHECK(err != -EINVAL, "param mismatch load", "err %d errno %d\n", err, errno))
		goto out;

	 
	err = stat(nopinpath2, &statbuf);
	if (CHECK(!err || errno != ENOENT, "stat nopinpath2",
		  "err %d errno %d\n", err, errno))
		goto out;

	 
	err = stat(pinpath, &statbuf);
	if (CHECK(err, "stat pinpath", "err %d errno %d\n", err, errno))
		goto out;

	bpf_object__close(obj);

	 
	obj = bpf_object__open_file(file, &opts);
	if (CHECK_FAIL(libbpf_get_error(obj))) {
		obj = NULL;
		goto out;
	}

	err = bpf_object__load(obj);
	if (CHECK(err, "custom load", "err %d errno %d\n", err, errno))
		goto out;

	 
	err = stat(custpinpath, &statbuf);
	if (CHECK(err, "stat custpinpath", "err %d errno %d\n", err, errno))
		goto out;

	 
	err = unlink(custpinpath);
	if (CHECK(err, "unlink custpinpath", "err %d errno %d\n", err, errno))
		goto out;

	err = rmdir(custpath);
	if (CHECK(err, "rmdir custpindir", "err %d errno %d\n", err, errno))
		goto out;

	bpf_object__close(obj);

	 
	obj = bpf_object__open_file(file, NULL);
	err = libbpf_get_error(obj);
	if (CHECK(err, "default open", "err %d errno %d\n", err, errno)) {
		obj = NULL;
		goto out;
	}

	map_fd = bpf_map_create(BPF_MAP_TYPE_ARRAY, NULL, sizeof(__u32),
				sizeof(__u64), 1, NULL);
	if (CHECK(map_fd < 0, "create pinmap manually", "fd %d\n", map_fd))
		goto out;

	map = bpf_object__find_map_by_name(obj, "pinmap");
	if (CHECK(!map, "find map", "NULL map"))
		goto close_map_fd;

	err = bpf_map__reuse_fd(map, map_fd);
	if (CHECK(err, "reuse pinmap fd", "err %d errno %d\n", err, errno))
		goto close_map_fd;

	err = bpf_map__set_pin_path(map, custpinpath);
	if (CHECK(err, "set pin path", "err %d errno %d\n", err, errno))
		goto close_map_fd;

	err = bpf_object__load(obj);
	if (CHECK(err, "custom load", "err %d errno %d\n", err, errno))
		goto close_map_fd;

	 
	err = stat(custpinpath, &statbuf);
	if (CHECK(err, "stat custpinpath", "err %d errno %d\n", err, errno))
		goto close_map_fd;

close_map_fd:
	close(map_fd);
out:
	unlink(pinpath);
	unlink(nopinpath);
	unlink(nopinpath2);
	unlink(custpinpath);
	rmdir(custpath);
	if (obj)
		bpf_object__close(obj);
}
