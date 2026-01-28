my $usage = <<EOT;
usage: config-enum enum [file ...]
Returns the elements from an enum declaration.
"Best effort": we're not building an entire C interpreter here!
EOT
use warnings;
use strict;
use Getopt::Std;
my %opts;
if (!getopts("", \%opts) || @ARGV < 1) {
	print $usage;
	exit 2;
}
my $enum = shift;
my $in_enum = 0;
while (<>) {
	s/\/\*.*\*\///;
	if (m/\/\*/) {
		while ($_ .= <>) {
			last if s/\/\*.*\*\///s;
		}
	}
	next if /^
	$in_enum = 1 if s/^\s*enum\s+${enum}(?:\s|$)//;
	next unless $in_enum;
	s/\s*=[^,]+,/,/g;
	while (m/\b([a-z_][a-z0-9_]*)\b/ig) {
		print $1, "\n";
	}
	$in_enum = 0 if m/}\s*;/;
}
exit 0;
