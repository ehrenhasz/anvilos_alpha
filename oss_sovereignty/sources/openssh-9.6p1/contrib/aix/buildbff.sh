[ -z "$PERMIT_ROOT_LOGIN" ] && PERMIT_ROOT_LOGIN=no
[ -z "$X11_FORWARDING" ] && X11_FORWARDING=no
[ -z "$AIX_SRC" ] && AIX_SRC=no
umask 022
startdir=`pwd`
perl -v >/dev/null || (echo perl required; exit 1)
if  echo $0 | egrep '^/'
then
	inventory=`dirname $0`/inventory.sh		
else
	inventory=`pwd`/`dirname $0`/inventory.sh	
fi
if pwd | egrep 'contrib/aix$'
then
	echo "Changing directory to `pwd`/../.."
	echo "Please run buildbff.sh from your build directory in future."
	cd ../..
	contribaix=1
fi
if [ ! -f Makefile ]
then
	echo "Makefile not found (did you run configure?)"
	exit 1
fi
objdir=`pwd`
PKGNAME=openssh
PKGDIR=package
if [ -s ./config.local ]
then
	echo Reading local settings from config.local
	. ./config.local
fi
for confvar in prefix exec_prefix bindir sbindir libexecdir datadir mandir mansubdir sysconfdir piddir srcdir
do
	eval $confvar=`grep "^$confvar=" $objdir/Makefile | cut -d = -f 2`
done
for confvar in SSH_PRIVSEP_USER PRIVSEP_PATH
do
	eval $confvar=`awk '/
done
if [ -z "$SSH_PRIVSEP_USER" ]
then
	SSH_PRIVSEP_USER=sshd
fi
if [ -z "$PRIVSEP_PATH" ]
then
	PRIVSEP_PATH=/var/empty
fi
rm -rf $objdir/$PKGDIR
FAKE_ROOT=$objdir/$PKGDIR/root
mkdir -p $FAKE_ROOT
echo "Faking root install..."
cd $objdir
make install-nokeys DESTDIR=$FAKE_ROOT
if [ $? -gt 0 ]
then
	echo "Fake root install failed, stopping."
	exit 1
fi
cp $srcdir/LICENCE $objdir/$PKGDIR/
cp $srcdir/README* $objdir/$PKGDIR/
VERSION=`./ssh -V 2>&1 | cut -f 1 -d , | cut -f 2 -d _`
MAJOR=`echo $VERSION | cut -f 1 -d p | cut -f 1 -d .`
MINOR=`echo $VERSION | cut -f 1 -d p | cut -f 2 -d .`
PATCH=`echo $VERSION | cut -f 1 -d p | cut -f 3 -d .`
PORTABLE=`echo $VERSION | awk 'BEGIN{FS="p"}{print $2}'`
[ "$PATCH" = "" ] && PATCH=0
[ "$PORTABLE" = "" ] && PORTABLE=0
BFFVERSION=`printf "%d.%d.%d.%d" $MAJOR $MINOR $PATCH $PORTABLE`
echo "Building BFF for $PKGNAME $VERSION (package version $BFFVERSION)"
if [ "${PERMIT_ROOT_LOGIN}" = no ]
then
	perl -p -i -e "s/
		$FAKE_ROOT/${sysconfdir}/sshd_config
fi
if [ "${X11_FORWARDING}" = yes ]
then
	perl -p -i -e "s/
		$FAKE_ROOT/${sysconfdir}/sshd_config
fi
for cfgfile in ssh_config sshd_config
do
	mv $FAKE_ROOT/$sysconfdir/$cfgfile $FAKE_ROOT/$sysconfdir/$cfgfile.default
done
cd $FAKE_ROOT
echo Generating LPP control files
find . ! -name . -print >../openssh.al
$inventory >../openssh.inventory
cat <<EOD >../openssh.copyright
This software is distributed under a BSD-style license.
For the full text of the license, see /usr/lpp/openssh/LICENCE
EOD
files=`find . -type f -print`
dirs=`for file in $files; do dirname $file; done | sort -u`
for dir in $dirs
do
	du $dir
done > ../openssh.size
cat <<EOF >>../openssh.post_i
echo Creating configs from defaults if necessary.
for cfgfile in ssh_config sshd_config
do
	if [ ! -f $sysconfdir/\$cfgfile ]
	then
		echo "Creating \$cfgfile from default"
		cp $sysconfdir/\$cfgfile.default $sysconfdir/\$cfgfile
	else
		echo "\$cfgfile already exists."
	fi
done
echo
echo Checking for PrivilegeSeparation user and group.
if cut -f1 -d: /etc/group | egrep '^'$SSH_PRIVSEP_USER'\$' >/dev/null
then
	echo "PrivSep group $SSH_PRIVSEP_USER already exists."
else
	echo "Creating PrivSep group $SSH_PRIVSEP_USER."
	mkgroup -A $SSH_PRIVSEP_USER
fi
if lsuser "$SSH_PRIVSEP_USER" >/dev/null
then
	echo "PrivSep user $SSH_PRIVSEP_USER already exists."
else
	echo "Creating PrivSep user $SSH_PRIVSEP_USER."
	mkuser gecos='SSHD PrivSep User' login=false rlogin=false account_locked=true pgrp=$SSH_PRIVSEP_USER $SSH_PRIVSEP_USER
fi
if egrep '^[ \t]*UsePrivilegeSeparation[ \t]+no' $sysconfdir/sshd_config >/dev/null
then
	echo UsePrivilegeSeparation not enabled, privsep directory not required.
else
	if [ -d $PRIVSEP_PATH ]
	then
		echo "PrivSep chroot directory $PRIVSEP_PATH already exists."
	else
		echo "Creating PrivSep chroot directory $PRIVSEP_PATH."
		mkdir $PRIVSEP_PATH
		chown 0 $PRIVSEP_PATH
		chgrp 0 $PRIVSEP_PATH
		chmod 755 $PRIVSEP_PATH
	fi
fi
echo
echo Creating host keys if required.
$bindir/ssh-keygen -A
echo
if [ "$AIX_SRC" = "yes" ]
then
	echo Creating SRC sshd subsystem.
	rmssys -s sshd 2>&1 >/dev/null
	mkssys -s sshd -p "$sbindir/sshd" -a '-D' -u 0 -S -n 15 -f 9 -R -G tcpip
	startupcmd="start $sbindir/sshd \\\"\\\$src_running\\\""
	oldstartcmd="$sbindir/sshd"
else
	startupcmd="$sbindir/sshd"
	oldstartcmd="start $sbindir/sshd \\\"$src_running\\\""
fi
if egrep "^\$oldstartcmd" /etc/rc.tcpip >/dev/null
then
	if sed "s|^\$oldstartcmd|\$startupcmd|g" /etc/rc.tcpip >/etc/rc.tcpip.new
	then
		chmod 0755 /etc/rc.tcpip.new
		mv /etc/rc.tcpip /etc/rc.tcpip.old && \
		mv /etc/rc.tcpip.new /etc/rc.tcpip
	else
		echo "Updating /etc/rc.tcpip failed, please check."
	fi
else
	if grep "^\$startupcmd" /etc/rc.tcpip >/dev/null
	then
		echo "sshd found in rc.tcpip, not adding."
	else
		echo "Adding sshd to rc.tcpip"
		echo >>/etc/rc.tcpip
		echo "
		echo "\$startupcmd" >>/etc/rc.tcpip
	fi
fi
EOF
echo Creating liblpp.a
(
	cd ..
	for i in openssh.al openssh.copyright openssh.inventory openssh.post_i openssh.size LICENCE README*
	do
		ar -r liblpp.a $i
		rm $i
	done
)
echo Creating lpp_name
cat <<EOF >../lpp_name
4 R I $PKGNAME {
$PKGNAME $BFFVERSION 1 N U en_US OpenSSH $VERSION Portable for AIX
[
%
EOF
for i in $bindir $sysconfdir $libexecdir $mandir/${mansubdir}1 $mandir/${mansubdir}8 $sbindir $datadir /usr/lpp/openssh
do
	if [ -d $FAKE_ROOT/$i ]
	then
		size=`du $FAKE_ROOT/$i | awk '{print $1}'`
		echo "$i $size" >>../lpp_name
	fi
done
echo '%' >>../lpp_name
echo ']' >>../lpp_name
echo '}' >>../lpp_name
mkdir -p usr/lpp/openssh
mv ../liblpp.a usr/lpp/openssh
mv ../lpp_name .
echo Creating $PKGNAME-$VERSION.bff with backup...
rm -f $PKGNAME-$VERSION.bff
(
	echo "./lpp_name"
	find . ! -name lpp_name -a ! -name . -print
) | backup  -i -q -f ../$PKGNAME-$VERSION.bff $filelist
mv ../$PKGNAME-$VERSION.bff $startdir
cd $startdir
rm -rf $objdir/$PKGDIR
echo $0: done.
