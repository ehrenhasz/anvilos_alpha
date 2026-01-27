savedcmd_/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/spl.o := ld -m elf_x86_64 -z noexecstack --no-warn-rwx-segments   -r -o /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/spl.o @/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/spl.mod  ; ./tools/objtool/objtool --hacks=jump_label --hacks=noinstr --hacks=skylake --ibt --orc --retpoline --rethunk --static-call --uaccess --prefix=16  --link  --module /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/spl.o

/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/spl.o: $(wildcard ./tools/objtool/objtool)
