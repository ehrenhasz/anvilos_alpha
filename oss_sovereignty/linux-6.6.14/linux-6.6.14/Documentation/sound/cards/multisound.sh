save_IFS="${IFS}"
IFS="${IFS}:"
gettext_dir=FAILED
locale_dir=FAILED
first_param="$1"
for dir in $PATH
do
  if test "$gettext_dir" = FAILED && test -f $dir/gettext \
     && ($dir/gettext --version >/dev/null 2>&1)
  then
    set `$dir/gettext --version 2>&1`
    if test "$3" = GNU
    then
      gettext_dir=$dir
    fi
  fi
  if test "$locale_dir" = FAILED && test -f $dir/shar \
     && ($dir/shar --print-text-domain-dir >/dev/null 2>&1)
  then
    locale_dir=`$dir/shar --print-text-domain-dir`
  fi
done
IFS="$save_IFS"
if test "$locale_dir" = FAILED || test "$gettext_dir" = FAILED
then
  echo=echo
else
  TEXTDOMAINDIR=$locale_dir
  export TEXTDOMAINDIR
  TEXTDOMAIN=sharutils
  export TEXTDOMAIN
  echo="$gettext_dir/gettext -s"
fi
touch -am 1231235999 $$.touch >/dev/null 2>&1
if test ! -f 1231235999 && test -f $$.touch; then
  shar_touch=touch
else
  shar_touch=:
  echo
  $echo 'WARNING: not restoring timestamps.  Consider getting and'
  $echo "installing GNU \`touch', distributed in GNU File Utilities..."
  echo
fi
rm -f 1231235999 $$.touch
if mkdir _sh01426; then
  $echo 'x -' 'creating lock directory'
else
  $echo 'failed to create lock directory'
  exit 1
fi
if test ! -d 'MultiSound.d'; then
  $echo 'x -' 'creating directory' 'MultiSound.d'
  mkdir 'MultiSound.d'
fi
if test -f 'MultiSound.d/setdigital.c' && test "$first_param" != -c; then
  $echo 'x -' SKIPPING 'MultiSound.d/setdigital.c' '(file already exists)'
else
  $echo 'x -' extracting 'MultiSound.d/setdigital.c' '(text)'
  sed 's/^X//' << 'SHAR_EOF' > 'MultiSound.d/setdigital.c' &&
/*********************************************************************
X *
X * setdigital.c - sets the DIGITAL1 input for a mixer
X *
X * Copyright (C) 1998 Andrew Veliath
X *
X * This program is free software; you can redistribute it and/or modify
X * it under the terms of the GNU General Public License as published by
X * the Free Software Foundation; either version 2 of the License, or
X * (at your option) any later version.
X *
X * This program is distributed in the hope that it will be useful,
X * but WITHOUT ANY WARRANTY; without even the implied warranty of
X * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
X * GNU General Public License for more details.
X *
X * You should have received a copy of the GNU General Public License
X * along with this program; if not, write to the Free Software
X * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
X *
X ********************************************************************/
X
X
int main(int argc, char *argv[])
{
X	int fd;
X	unsigned long recmask, recsrc;
X
X	if (argc != 2) {
X		fprintf(stderr, "usage: setdigital <mixer device>\n");
X		exit(1);
X	}
X
X	if ((fd = open(argv[1], O_RDWR)) < 0) {
X		perror(argv[1]);
X		exit(1);
X	}
X
X	if (ioctl(fd, SOUND_MIXER_READ_RECMASK, &recmask) < 0) {
X		fprintf(stderr, "error: ioctl read recording mask failed\n");
X		perror("ioctl");
X		close(fd);
X		exit(1);
X	}
X
X	if (!(recmask & SOUND_MASK_DIGITAL1)) {
X		fprintf(stderr, "error: cannot find DIGITAL1 device in mixer\n");
X		close(fd);
X		exit(1);
X	}
X
X	if (ioctl(fd, SOUND_MIXER_READ_RECSRC, &recsrc) < 0) {
X		fprintf(stderr, "error: ioctl read recording source failed\n");
X		perror("ioctl");
X		close(fd);
X		exit(1);
X	}
X
X	recsrc |= SOUND_MASK_DIGITAL1;
X
X	if (ioctl(fd, SOUND_MIXER_WRITE_RECSRC, &recsrc) < 0) {
X		fprintf(stderr, "error: ioctl write recording source failed\n");
X		perror("ioctl");
X		close(fd);
X		exit(1);
X	}
X
X	close(fd);
X
X	return 0;
}
SHAR_EOF
  $shar_touch -am 1204092598 'MultiSound.d/setdigital.c' &&
  chmod 0664 'MultiSound.d/setdigital.c' ||
  $echo 'restore of' 'MultiSound.d/setdigital.c' 'failed'
  if ( md5sum --help 2>&1 | grep 'sage: md5sum \[' ) >/dev/null 2>&1 \
  && ( md5sum --version 2>&1 | grep -v 'textutils 1.12' ) >/dev/null; then
    md5sum -c << SHAR_EOF >/dev/null 2>&1 \
    || $echo 'MultiSound.d/setdigital.c:' 'MD5 check failed'
e87217fc3e71288102ba41fd81f71ec4  MultiSound.d/setdigital.c
SHAR_EOF
  else
    shar_count="`LC_ALL= LC_CTYPE= LANG= wc -c < 'MultiSound.d/setdigital.c'`"
    test 2064 -eq "$shar_count" ||
    $echo 'MultiSound.d/setdigital.c:' 'original size' '2064,' 'current size' "$shar_count!"
  fi
fi
if test -f 'MultiSound.d/pinnaclecfg.c' && test "$first_param" != -c; then
  $echo 'x -' SKIPPING 'MultiSound.d/pinnaclecfg.c' '(file already exists)'
else
  $echo 'x -' extracting 'MultiSound.d/pinnaclecfg.c' '(text)'
  sed 's/^X//' << 'SHAR_EOF' > 'MultiSound.d/pinnaclecfg.c' &&
/*********************************************************************
X *
X * pinnaclecfg.c - Pinnacle/Fiji Device Configuration Program
X *
X * This is for NON-PnP mode only.  For PnP mode, use isapnptools.
X *
X * This is Linux-specific, and must be run with root permissions.
X *
X * Part of the Turtle Beach MultiSound Sound Card Driver for Linux
X *
X * Copyright (C) 1998 Andrew Veliath
X *
X * This program is free software; you can redistribute it and/or modify
X * it under the terms of the GNU General Public License as published by
X * the Free Software Foundation; either version 2 of the License, or
X * (at your option) any later version.
X *
X * This program is distributed in the hope that it will be useful,
X * but WITHOUT ANY WARRANTY; without even the implied warranty of
X * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
X * GNU General Public License for more details.
X *
X * You should have received a copy of the GNU General Public License
X * along with this program; if not, write to the Free Software
X * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
X *
X ********************************************************************/
X
X
X
X
typedef __u8			BYTE;
typedef __u16			USHORT;
typedef __u16			WORD;
X
static int config_port = -1;
X
static int msnd_write_cfg(int cfg, int reg, int value)
{
X	outb(reg, cfg);
X	outb(value, cfg + 1);
X	if (value != inb(cfg + 1)) {
X		fprintf(stderr, "error: msnd_write_cfg: I/O error\n");
X		return -EIO;
X	}
X	return 0;
}
X
static int msnd_read_cfg(int cfg, int reg)
{
X	outb(reg, cfg);
X	return inb(cfg + 1);
}
X
static int msnd_write_cfg_io0(int cfg, int num, WORD io)
{
X	if (msnd_write_cfg(cfg, IREG_LOGDEVICE, num))
X		return -EIO;
X	if (msnd_write_cfg(cfg, IREG_IO0_BASEHI, HIBYTE(io)))
X		return -EIO;
X	if (msnd_write_cfg(cfg, IREG_IO0_BASELO, LOBYTE(io)))
X		return -EIO;
X	return 0;
}
X
static int msnd_read_cfg_io0(int cfg, int num, WORD *io)
{
X	if (msnd_write_cfg(cfg, IREG_LOGDEVICE, num))
X		return -EIO;
X
X	*io = MAKEWORD(msnd_read_cfg(cfg, IREG_IO0_BASELO),
X		       msnd_read_cfg(cfg, IREG_IO0_BASEHI));
X
X	return 0;
}
X
static int msnd_write_cfg_io1(int cfg, int num, WORD io)
{
X	if (msnd_write_cfg(cfg, IREG_LOGDEVICE, num))
X		return -EIO;
X	if (msnd_write_cfg(cfg, IREG_IO1_BASEHI, HIBYTE(io)))
X		return -EIO;
X	if (msnd_write_cfg(cfg, IREG_IO1_BASELO, LOBYTE(io)))
X		return -EIO;
X	return 0;
}
X
static int msnd_read_cfg_io1(int cfg, int num, WORD *io)
{
X	if (msnd_write_cfg(cfg, IREG_LOGDEVICE, num))
X		return -EIO;
X
X	*io = MAKEWORD(msnd_read_cfg(cfg, IREG_IO1_BASELO),
X		       msnd_read_cfg(cfg, IREG_IO1_BASEHI));
X
X	return 0;
}
X
static int msnd_write_cfg_irq(int cfg, int num, WORD irq)
{
X	if (msnd_write_cfg(cfg, IREG_LOGDEVICE, num))
X		return -EIO;
X	if (msnd_write_cfg(cfg, IREG_IRQ_NUMBER, LOBYTE(irq)))
X		return -EIO;
X	if (msnd_write_cfg(cfg, IREG_IRQ_TYPE, IRQTYPE_EDGE))
X		return -EIO;
X	return 0;
}
X
static int msnd_read_cfg_irq(int cfg, int num, WORD *irq)
{
X	if (msnd_write_cfg(cfg, IREG_LOGDEVICE, num))
X		return -EIO;
X
X	*irq = msnd_read_cfg(cfg, IREG_IRQ_NUMBER);
X
X	return 0;
}
X
static int msnd_write_cfg_mem(int cfg, int num, int mem)
{
X	WORD wmem;
X
X	mem >>= 8;
X	mem &= 0xfff;
X	wmem = (WORD)mem;
X	if (msnd_write_cfg(cfg, IREG_LOGDEVICE, num))
X		return -EIO;
X	if (msnd_write_cfg(cfg, IREG_MEMBASEHI, HIBYTE(wmem)))
X		return -EIO;
X	if (msnd_write_cfg(cfg, IREG_MEMBASELO, LOBYTE(wmem)))
X		return -EIO;
X	if (wmem && msnd_write_cfg(cfg, IREG_MEMCONTROL, (MEMTYPE_HIADDR | MEMTYPE_16BIT)))
X		return -EIO;
X	return 0;
}
X
static int msnd_read_cfg_mem(int cfg, int num, int *mem)
{
X	if (msnd_write_cfg(cfg, IREG_LOGDEVICE, num))
X		return -EIO;
X
X	*mem = MAKEWORD(msnd_read_cfg(cfg, IREG_MEMBASELO),
X			msnd_read_cfg(cfg, IREG_MEMBASEHI));
X	*mem <<= 8;
X
X	return 0;
}
X
static int msnd_activate_logical(int cfg, int num)
{
X	if (msnd_write_cfg(cfg, IREG_LOGDEVICE, num))
X		return -EIO;
X	if (msnd_write_cfg(cfg, IREG_ACTIVATE, LD_ACTIVATE))
X		return -EIO;
X	return 0;
}
X
static int msnd_write_cfg_logical(int cfg, int num, WORD io0, WORD io1, WORD irq, int mem)
{
X	if (msnd_write_cfg(cfg, IREG_LOGDEVICE, num))
X		return -EIO;
X	if (msnd_write_cfg_io0(cfg, num, io0))
X		return -EIO;
X	if (msnd_write_cfg_io1(cfg, num, io1))
X		return -EIO;
X	if (msnd_write_cfg_irq(cfg, num, irq))
X		return -EIO;
X	if (msnd_write_cfg_mem(cfg, num, mem))
X		return -EIO;
X	if (msnd_activate_logical(cfg, num))
X		return -EIO;
X	return 0;
}
X
static int msnd_read_cfg_logical(int cfg, int num, WORD *io0, WORD *io1, WORD *irq, int *mem)
{
X	if (msnd_write_cfg(cfg, IREG_LOGDEVICE, num))
X		return -EIO;
X	if (msnd_read_cfg_io0(cfg, num, io0))
X		return -EIO;
X	if (msnd_read_cfg_io1(cfg, num, io1))
X		return -EIO;
X	if (msnd_read_cfg_irq(cfg, num, irq))
X		return -EIO;
X	if (msnd_read_cfg_mem(cfg, num, mem))
X		return -EIO;
X	return 0;
}
X
static void usage(void)
{
X	fprintf(stderr,
X		"\n"
X		"pinnaclecfg 1.0\n"
X		"\n"
X		"usage: pinnaclecfg <config port> [device config]\n"
X		"\n"
X		"This is for use with the card in NON-PnP mode only.\n"
X		"\n"
X		"Available devices (not all available for Fiji):\n"
X		"\n"
X		"        Device                       Description\n"
X		"        -------------------------------------------------------------------\n"
X		"        reset                        Reset all devices (i.e. disable)\n"
X		"        show                         Display current device configurations\n"
X		"\n"
X		"        dsp <io> <irq> <mem>         Audio device\n"
X		"        mpu <io> <irq>               Internal Kurzweil synth\n"
X		"        ide <io0> <io1> <irq>        On-board IDE controller\n"
X		"        joystick <io>                Joystick port\n"
X		"\n");
X	exit(1);
}
X
static int cfg_reset(void)
{
X	int i;
X
X	for (i = 0; i < 4; ++i)
X		msnd_write_cfg_logical(config_port, i, 0, 0, 0, 0);
X
X	return 0;
}
X
static int cfg_show(void)
{
X	int i;
X	int count = 0;
X
X	for (i = 0; i < 4; ++i) {
X		WORD io0, io1, irq;
X		int mem;
X		msnd_read_cfg_logical(config_port, i, &io0, &io1, &irq, &mem);
X		switch (i) {
X		case 0:
X			if (io0 || irq || mem) {
X				printf("dsp 0x%x %d 0x%x\n", io0, irq, mem);
X				++count;
X			}
X			break;
X		case 1:
X			if (io0 || irq) {
X				printf("mpu 0x%x %d\n", io0, irq);
X				++count;
X			}
X			break;
X		case 2:
X			if (io0 || io1 || irq) {
X				printf("ide 0x%x 0x%x %d\n", io0, io1, irq);
X				++count;
X			}
X			break;
X		case 3:
X			if (io0) {
X				printf("joystick 0x%x\n", io0);
X				++count;
X			}
X			break;
X		}
X	}
X
X	if (count == 0)
X		fprintf(stderr, "no devices configured\n");
X
X	return 0;
}
X
static int cfg_dsp(int argc, char *argv[])
{
X	int io, irq, mem;
X
X	if (argc < 3 ||
X	    sscanf(argv[0], "0x%x", &io) != 1 ||
X	    sscanf(argv[1], "%d", &irq) != 1 ||
X	    sscanf(argv[2], "0x%x", &mem) != 1)
X		usage();
X
X	if (!(io == 0x290 ||
X	      io == 0x260 ||
X	      io == 0x250 ||
X	      io == 0x240 ||
X	      io == 0x230 ||
X	      io == 0x220 ||
X	      io == 0x210 ||
X	      io == 0x3e0)) {
X		fprintf(stderr, "error: io must be one of "
X			"210, 220, 230, 240, 250, 260, 290, or 3E0\n");
X		usage();
X	}
X
X	if (!(irq == 5 ||
X	      irq == 7 ||
X	      irq == 9 ||
X	      irq == 10 ||
X	      irq == 11 ||
X	      irq == 12)) {
X		fprintf(stderr, "error: irq must be one of "
X			"5, 7, 9, 10, 11 or 12\n");
X		usage();
X	}
X
X	if (!(mem == 0xb0000 ||
X	      mem == 0xc8000 ||
X	      mem == 0xd0000 ||
X	      mem == 0xd8000 ||
X	      mem == 0xe0000 ||
X	      mem == 0xe8000)) {
X		fprintf(stderr, "error: mem must be one of "
X			"0xb0000, 0xc8000, 0xd0000, 0xd8000, 0xe0000 or 0xe8000\n");
X		usage();
X	}
X
X	return msnd_write_cfg_logical(config_port, 0, io, 0, irq, mem);
}
X
static int cfg_mpu(int argc, char *argv[])
{
X	int io, irq;
X
X	if (argc < 2 ||
X	    sscanf(argv[0], "0x%x", &io) != 1 ||
X	    sscanf(argv[1], "%d", &irq) != 1)
X		usage();
X
X	return msnd_write_cfg_logical(config_port, 1, io, 0, irq, 0);
}
X
static int cfg_ide(int argc, char *argv[])
{
X	int io0, io1, irq;
X
X	if (argc < 3 ||
X	    sscanf(argv[0], "0x%x", &io0) != 1 ||
X	    sscanf(argv[0], "0x%x", &io1) != 1 ||
X	    sscanf(argv[1], "%d", &irq) != 1)
X		usage();
X
X	return msnd_write_cfg_logical(config_port, 2, io0, io1, irq, 0);
}
X
static int cfg_joystick(int argc, char *argv[])
{
X	int io;
X
X	if (argc < 1 ||
X	    sscanf(argv[0], "0x%x", &io) != 1)
X		usage();
X
X	return msnd_write_cfg_logical(config_port, 3, io, 0, 0, 0);
}
X
int main(int argc, char *argv[])
{
X	char *device;
X	int rv = 0;
X
X	--argc; ++argv;
X
X	if (argc < 2)
X		usage();
X
X	sscanf(argv[0], "0x%x", &config_port);
X	if (config_port != 0x250 && config_port != 0x260 && config_port != 0x270) {
X		fprintf(stderr, "error: <config port> must be 0x250, 0x260 or 0x270\n");
X		exit(1);
X	}
X	if (ioperm(config_port, 2, 1)) {
X		perror("ioperm");
X		fprintf(stderr, "note: pinnaclecfg must be run as root\n");
X		exit(1);
X	}
X	device = argv[1];
X
X	argc -= 2; argv += 2;
X
X	if (strcmp(device, "reset") == 0)
X		rv = cfg_reset();
X	else if (strcmp(device, "show") == 0)
X		rv = cfg_show();
X	else if (strcmp(device, "dsp") == 0)
X		rv = cfg_dsp(argc, argv);
X	else if (strcmp(device, "mpu") == 0)
X		rv = cfg_mpu(argc, argv);
X	else if (strcmp(device, "ide") == 0)
X		rv = cfg_ide(argc, argv);
X	else if (strcmp(device, "joystick") == 0)
X		rv = cfg_joystick(argc, argv);
X	else {
X		fprintf(stderr, "error: unknown device %s\n", device);
X		usage();
X	}
X
X	if (rv)
X		fprintf(stderr, "error: device configuration failed\n");
X
X	return 0;
}
SHAR_EOF
  $shar_touch -am 1204092598 'MultiSound.d/pinnaclecfg.c' &&
  chmod 0664 'MultiSound.d/pinnaclecfg.c' ||
  $echo 'restore of' 'MultiSound.d/pinnaclecfg.c' 'failed'
  if ( md5sum --help 2>&1 | grep 'sage: md5sum \[' ) >/dev/null 2>&1 \
  && ( md5sum --version 2>&1 | grep -v 'textutils 1.12' ) >/dev/null; then
    md5sum -c << SHAR_EOF >/dev/null 2>&1 \
    || $echo 'MultiSound.d/pinnaclecfg.c:' 'MD5 check failed'
366bdf27f0db767a3c7921d0a6db20fe  MultiSound.d/pinnaclecfg.c
SHAR_EOF
  else
    shar_count="`LC_ALL= LC_CTYPE= LANG= wc -c < 'MultiSound.d/pinnaclecfg.c'`"
    test 10224 -eq "$shar_count" ||
    $echo 'MultiSound.d/pinnaclecfg.c:' 'original size' '10224,' 'current size' "$shar_count!"
  fi
fi
if test -f 'MultiSound.d/Makefile' && test "$first_param" != -c; then
  $echo 'x -' SKIPPING 'MultiSound.d/Makefile' '(file already exists)'
else
  $echo 'x -' extracting 'MultiSound.d/Makefile' '(text)'
  sed 's/^X//' << 'SHAR_EOF' > 'MultiSound.d/Makefile' &&
CC	= gcc
CFLAGS	= -O
PROGS	= setdigital msndreset pinnaclecfg conv
X
all: $(PROGS)
X
clean:
X	rm -f $(PROGS)
SHAR_EOF
  $shar_touch -am 1204092398 'MultiSound.d/Makefile' &&
  chmod 0664 'MultiSound.d/Makefile' ||
  $echo 'restore of' 'MultiSound.d/Makefile' 'failed'
  if ( md5sum --help 2>&1 | grep 'sage: md5sum \[' ) >/dev/null 2>&1 \
  && ( md5sum --version 2>&1 | grep -v 'textutils 1.12' ) >/dev/null; then
    md5sum -c << SHAR_EOF >/dev/null 2>&1 \
    || $echo 'MultiSound.d/Makefile:' 'MD5 check failed'
76ca8bb44e3882edcf79c97df6c81845  MultiSound.d/Makefile
SHAR_EOF
  else
    shar_count="`LC_ALL= LC_CTYPE= LANG= wc -c < 'MultiSound.d/Makefile'`"
    test 106 -eq "$shar_count" ||
    $echo 'MultiSound.d/Makefile:' 'original size' '106,' 'current size' "$shar_count!"
  fi
fi
if test -f 'MultiSound.d/conv.l' && test "$first_param" != -c; then
  $echo 'x -' SKIPPING 'MultiSound.d/conv.l' '(file already exists)'
else
  $echo 'x -' extracting 'MultiSound.d/conv.l' '(text)'
  sed 's/^X//' << 'SHAR_EOF' > 'MultiSound.d/conv.l' &&
%%
[ \n\t,\r]
\;.*
DB
[0-9A-Fa-f]+H	{ int n; sscanf(yytext, "%xH", &n); printf("%c", n); }
%%
int yywrap() { return 1; }
void main() { yylex(); }
SHAR_EOF
  $shar_touch -am 0828231798 'MultiSound.d/conv.l' &&
  chmod 0664 'MultiSound.d/conv.l' ||
  $echo 'restore of' 'MultiSound.d/conv.l' 'failed'
  if ( md5sum --help 2>&1 | grep 'sage: md5sum \[' ) >/dev/null 2>&1 \
  && ( md5sum --version 2>&1 | grep -v 'textutils 1.12' ) >/dev/null; then
    md5sum -c << SHAR_EOF >/dev/null 2>&1 \
    || $echo 'MultiSound.d/conv.l:' 'MD5 check failed'
d2411fc32cd71a00dcdc1f009e858dd2  MultiSound.d/conv.l
SHAR_EOF
  else
    shar_count="`LC_ALL= LC_CTYPE= LANG= wc -c < 'MultiSound.d/conv.l'`"
    test 146 -eq "$shar_count" ||
    $echo 'MultiSound.d/conv.l:' 'original size' '146,' 'current size' "$shar_count!"
  fi
fi
if test -f 'MultiSound.d/msndreset.c' && test "$first_param" != -c; then
  $echo 'x -' SKIPPING 'MultiSound.d/msndreset.c' '(file already exists)'
else
  $echo 'x -' extracting 'MultiSound.d/msndreset.c' '(text)'
  sed 's/^X//' << 'SHAR_EOF' > 'MultiSound.d/msndreset.c' &&
/*********************************************************************
X *
X * msndreset.c - resets the MultiSound card
X *
X * Copyright (C) 1998 Andrew Veliath
X *
X * This program is free software; you can redistribute it and/or modify
X * it under the terms of the GNU General Public License as published by
X * the Free Software Foundation; either version 2 of the License, or
X * (at your option) any later version.
X *
X * This program is distributed in the hope that it will be useful,
X * but WITHOUT ANY WARRANTY; without even the implied warranty of
X * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
X * GNU General Public License for more details.
X *
X * You should have received a copy of the GNU General Public License
X * along with this program; if not, write to the Free Software
X * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
X *
X ********************************************************************/
X
X
int main(int argc, char *argv[])
{
X	int fd;
X
X	if (argc != 2) {
X		fprintf(stderr, "usage: msndreset <mixer device>\n");
X		exit(1);
X	}
X
X	if ((fd = open(argv[1], O_RDWR)) < 0) {
X		perror(argv[1]);
X		exit(1);
X	}
X
X	if (ioctl(fd, SOUND_MIXER_PRIVATE1, 0) < 0) {
X		fprintf(stderr, "error: msnd ioctl reset failed\n");
X		perror("ioctl");
X		close(fd);
X		exit(1);
X	}
X
X	close(fd);
X
X	return 0;
}
SHAR_EOF
  $shar_touch -am 1204100698 'MultiSound.d/msndreset.c' &&
  chmod 0664 'MultiSound.d/msndreset.c' ||
  $echo 'restore of' 'MultiSound.d/msndreset.c' 'failed'
  if ( md5sum --help 2>&1 | grep 'sage: md5sum \[' ) >/dev/null 2>&1 \
  && ( md5sum --version 2>&1 | grep -v 'textutils 1.12' ) >/dev/null; then
    md5sum -c << SHAR_EOF >/dev/null 2>&1 \
    || $echo 'MultiSound.d/msndreset.c:' 'MD5 check failed'
c52f876521084e8eb25e12e01dcccb8a  MultiSound.d/msndreset.c
SHAR_EOF
  else
    shar_count="`LC_ALL= LC_CTYPE= LANG= wc -c < 'MultiSound.d/msndreset.c'`"
    test 1491 -eq "$shar_count" ||
    $echo 'MultiSound.d/msndreset.c:' 'original size' '1491,' 'current size' "$shar_count!"
  fi
fi
rm -fr _sh01426
exit 0
