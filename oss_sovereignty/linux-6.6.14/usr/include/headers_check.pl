use warnings;
use strict;
use File::Basename;
my ($dir, $arch, @files) = @ARGV;
my $ret = 0;
my $line;
my $lineno = 0;
my $filename;
foreach my $file (@files) {
	$filename = $file;
	open(my $fh, '<', $filename)
		or die "$filename: $!\n";
	$lineno = 0;
	while ($line = <$fh>) {
		$lineno++;
		&check_include();
		&check_asm_types();
		&check_sizetypes();
		&check_declarations();
	}
	close $fh;
}
exit $ret;
sub check_include
{
	if ($line =~ m/^\s*
		my $inc = $1;
		my $found;
		$found = stat($dir . "/" . $inc);
		if (!$found) {
			$inc =~ s
			$found = stat($dir . "/" . $inc);
		}
		if (!$found) {
			printf STDERR "$filename:$lineno: included file '$inc' is not exported\n";
			$ret = 1;
		}
	}
}
sub check_declarations
{
	if ($line =~ m/^void seqbuf_dump\(void\);/) {
		return;
	}
	if ($line =~ m/^extern "C"/) {
		return;
	}
	if ($line =~ m/^(\s*extern|unsigned|char|short|int|long|void)\b/) {
		printf STDERR "$filename:$lineno: " .
			      "userspace cannot reference function or " .
			      "variable defined in the kernel\n";
	}
}
sub check_config
{
	if ($line =~ m/[^a-zA-Z0-9_]+CONFIG_([a-zA-Z0-9_]+)[^a-zA-Z0-9_]/) {
		printf STDERR "$filename:$lineno: leaks CONFIG_$1 to userspace where it is not valid\n";
	}
}
my $linux_asm_types;
sub check_asm_types
{
	if ($filename =~ /types.h|int-l64.h|int-ll64.h/o) {
		return;
	}
	if ($lineno == 1) {
		$linux_asm_types = 0;
	} elsif ($linux_asm_types >= 1) {
		return;
	}
	if ($line =~ m/^\s*
		$linux_asm_types = 1;
		printf STDERR "$filename:$lineno: " .
		"include of <linux/types.h> is preferred over <asm/types.h>\n"
	}
}
my $linux_types;
my %import_stack = ();
sub check_include_typesh
{
	my $path = $_[0];
	my $import_path;
	my $fh;
	my @file_paths = ($path, $dir . "/" .  $path, dirname($filename) . "/" . $path);
	for my $possible ( @file_paths ) {
	    if (not $import_stack{$possible} and open($fh, '<', $possible)) {
		$import_path = $possible;
		$import_stack{$import_path} = 1;
		last;
	    }
	}
	if (eof $fh) {
	    return;
	}
	my $line;
	while ($line = <$fh>) {
		if ($line =~ m/^\s*
			$linux_types = 1;
			last;
		}
		if (my $included = ($line =~ /^\s*
			check_include_typesh($included);
		}
	}
	close $fh;
	delete $import_stack{$import_path};
}
sub check_sizetypes
{
	if ($filename =~ /types.h|int-l64.h|int-ll64.h/o) {
		return;
	}
	if ($lineno == 1) {
		$linux_types = 0;
	} elsif ($linux_types >= 1) {
		return;
	}
	if ($line =~ m/^\s*
		$linux_types = 1;
		return;
	}
	if (my $included = ($line =~ /^\s*
		check_include_typesh($included);
	}
	if ($line =~ m/__[us](8|16|32|64)\b/) {
		printf STDERR "$filename:$lineno: " .
		              "found __[us]{8,16,32,64} type " .
		              "without #include <linux/types.h>\n";
		$linux_types = 2;
	}
}
