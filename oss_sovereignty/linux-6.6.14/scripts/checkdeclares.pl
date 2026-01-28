use strict;
sub usage {
	print "Usage: checkdeclares.pl file1.h ...\n";
	print "Warns of struct declaration duplicates\n";
	exit 1;
}
if ($
	usage();
}
my $dup_counter = 0;
foreach my $file (@ARGV) {
	open(my $f, '<', $file)
	    or die "Cannot open $file: $!.\n";
	my %declaredstructs = ();
	while (<$f>) {
		if (m/^\s*struct\s*(\w*);$/o) {
			++$declaredstructs{$1};
		}
	}
	close($f);
	foreach my $structname (keys %declaredstructs) {
		if ($declaredstructs{$structname} > 1) {
			print "$file: struct $structname is declared more than once.\n";
			++$dup_counter;
		}
	}
}
if ($dup_counter == 0) {
	print "No duplicate struct declares found.\n";
}
