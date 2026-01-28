use strict;
my (@stack, $re, $dre, $x, $xs, $funcre);
{
	my $arch = shift;
	if ($arch eq "") {
		$arch = `uname -m`;
		chomp($arch);
	}
	$x	= "[0-9a-f]";	
	$xs	= "[0-9a-f ]";	
	$funcre = qr/^$x* <(.*)>:$/;
	if ($arch eq 'aarch64') {
		$re = qr/^.*stp.*sp, \
		$dre = qr/^.*sub.*sp, sp, 
	} elsif ($arch eq 'arm') {
		$re = qr/.*sub.*sp, sp, 
	} elsif ($arch =~ /^x86(_64)?$/ || $arch =~ /^i[3456]86$/) {
		$re = qr/^.*[as][du][db]    \$(0x$x{1,8}),\%(e|r)sp$/o;
		$dre = qr/^.*[as][du][db]    (%.*),\%(e|r)sp$/o;
	} elsif ($arch eq 'blackfin') {
		$re = qr/.*[[:space:]]LINK[[:space:]]*(0x$x{1,8})/o;
	} elsif ($arch eq 'ia64') {
		$re = qr/.*adds.*r12=-(([0-9]{2}|[3-9])[0-9]{2}),r12/o;
	} elsif ($arch eq 'm68k') {
		$re = qr/.*(?:linkw %fp,|addaw )
	} elsif ($arch eq 'mips64') {
		$re = qr/.*daddiu.*sp,sp,-(([0-9]{2}|[3-9])[0-9]{2})/o;
	} elsif ($arch eq 'mips') {
		$re = qr/.*addiu.*sp,sp,-(([0-9]{2}|[3-9])[0-9]{2})/o;
	} elsif ($arch eq 'nios2') {
		$re = qr/.*addi.*sp,sp,-(([0-9]{2}|[3-9])[0-9]{2})/o;
	} elsif ($arch eq 'openrisc') {
		$re = qr/.*l\.addi.*r1,r1,-(([0-9]{2}|[3-9])[0-9]{2})/o;
	} elsif ($arch eq 'parisc' || $arch eq 'parisc64') {
		$re = qr/.*ldo ($x{1,8})\(sp\),sp/o;
	} elsif ($arch eq 'powerpc' || $arch =~ /^ppc(64)?(le)?$/ ) {
		$re = qr/.*st[dw]u.*r1,-($x{1,8})\(r1\)/o;
	} elsif ($arch =~ /^s390x?$/) {
		$re = qr/.*(?:lay|ag?hi).*\%r15,-(([0-9]{2}|[3-9])[0-9]{2})
		      (?:\(\%r15\))?$/ox;
	} elsif ($arch =~ /^sh64$/) {
		$re = qr/.*addi\.l.*r15,-(([0-9]{2}|[3-9])[0-9]{2}),r15/o;
	} elsif ($arch eq 'sparc' || $arch eq 'sparc64') {
		$re = qr/.*save.*%sp, -(([0-9]{2}|[3-9])[0-9]{2}), %sp/o;
	} else {
		print("wrong or unknown architecture \"$arch\"\n");
		exit
	}
}
my ($func, $file, $lastslash);
while (my $line = <STDIN>) {
	if ($line =~ m/$funcre/) {
		$func = $1;
	}
	elsif ($line =~ m/(.*):\s*file format/) {
		$file = $1;
		$file =~ s/\.ko//;
		$lastslash = rindex($file, "/");
		if ($lastslash != -1) {
			$file = substr($file, $lastslash + 1);
		}
	}
	elsif ($line =~ m/$re/) {
		my $size = $1;
		$size = hex($size) if ($size =~ /^0x/);
		if ($size > 0xf0000000) {
			$size = - $size;
			$size += 0x80000000;
			$size += 0x80000000;
		}
		next if ($size > 0x10000000);
		next if $line !~ m/^($xs*)/;
		my $addr = $1;
		$addr =~ s/ /0/g;
		$addr = "0x$addr";
		my $intro = "$func [$file]:";
		my $padlen = 56 - length($intro);
		while ($padlen > 0) {
			$intro .= '	';
			$padlen -= 8;
		}
		next if ($size < 100);
		push @stack, "$intro$size\n";
	}
	elsif (defined $dre && $line =~ m/$dre/) {
		my $size = "Dynamic ($1)";
		next if $line !~ m/^($xs*)/;
		my $addr = $1;
		$addr =~ s/ /0/g;
		$addr = "0x$addr";
		my $intro = "$func [$file]:";
		my $padlen = 56 - length($intro);
		while ($padlen > 0) {
			$intro .= '	';
			$padlen -= 8;
		}
		push @stack, "$intro$size\n";
	}
}
sub bysize($) {
	my ($asize, $bsize);
	($asize = $a) =~ s/.*:  *(.*)$/$1/;
	($bsize = $b) =~ s/.*:  *(.*)$/$1/;
	$bsize <=> $asize
}
print sort { ($b =~ /:\t*(\d+)$/)[0] <=> ($a =~ /:\t*(\d+)$/)[0] } @stack;
