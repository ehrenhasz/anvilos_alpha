use strict;
$| = 1;
my $debugging;
foreach my $file (@ARGV) {
    next if $file =~ "include/generated/uapi/linux/version\.h";
    next if $file =~ "usr/include/linux/version\.h";
    open( my $f, '<', $file )
      or die "Can't open $file: $!\n";
    my ($fInComment, $fInString, $fUseVersion);
    my $iLinuxVersion = 0;
    while (<$f>) {
	$fInComment && (s+^.*?\*/+ +o ? ($fInComment = 0) : next);
	m+/\*+o && (s+/\*.*?\*/+ +go, (s+/\*.*$+ +o && ($fInComment = 1)));
	if ( m/^\s*
	    $iLinuxVersion      = $. if m/^\s*
	}
	$fInString && (s+^.*?"+ +o ? ($fInString = 0) : next);
	m+"+o && (s+".*?"+ +go, (s+".*$+ +o && ($fInString = 1)));
	if ( m/^\s*
	    $iLinuxVersion      = $. if m/^\s*
	}
	if (($_ =~ /LINUX_VERSION_CODE/) || ($_ =~ /\WKERNEL_VERSION/) ||
	    ($_ =~ /LINUX_VERSION_MAJOR/) || ($_ =~ /LINUX_VERSION_PATCHLEVEL/) ||
	    ($_ =~ /LINUX_VERSION_SUBLEVEL/)) {
	    $fUseVersion = 1;
            last if $iLinuxVersion;
        }
    }
    if ($fUseVersion && ! $iLinuxVersion) {
	print "$file: $.: need linux/version.h\n";
    }
    if ($iLinuxVersion && ! $fUseVersion) {
	print "$file: $iLinuxVersion linux/version.h not needed.\n";
    }
    if ($debugging) {
        if ($iLinuxVersion && $fUseVersion) {
	    print "$file: version use is OK ($iLinuxVersion)\n";
        }
        if (! $iLinuxVersion && ! $fUseVersion) {
	    print "$file: version use is OK (none)\n";
        }
    }
    close($f);
}
