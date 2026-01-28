use strict;
sub usage {
	print "Usage: checkincludes.pl [-r]\n";
	print "By default we just warn of duplicates\n";
	print "To remove duplicated includes in place use -r\n";
	exit 1;
}
my $remove = 0;
if ($
	usage();
}
if ($
	if ($ARGV[0] =~ /^-/) {
		if ($ARGV[0] eq "-r") {
			$remove = 1;
			shift;
		} else {
			usage();
		}
	}
}
my $dup_counter = 0;
foreach my $file (@ARGV) {
	open(my $f, '<', $file)
	    or die "Cannot open $file: $!.\n";
	my %includedfiles = ();
	my @file_lines = ();
	while (<$f>) {
		if (m/^\s*
			++$includedfiles{$1};
		}
		push(@file_lines, $_);
	}
	close($f);
	if (!$remove) {
		foreach my $filename (keys %includedfiles) {
			if ($includedfiles{$filename} > 1) {
				print "$file: $filename is included more than once.\n";
				++$dup_counter;
			}
		}
		next;
	}
	open($f, '>', $file)
	    or die("Cannot write to $file: $!");
	my $dups = 0;
	foreach (@file_lines) {
		if (m/^\s*
			foreach my $filename (keys %includedfiles) {
				if ($1 eq $filename) {
					if ($includedfiles{$filename} > 1) {
						$includedfiles{$filename}--;
						$dups++;
						++$dup_counter;
					} else {
						print {$f} $_;
					}
				}
			}
		} else {
			print {$f} $_;
		}
	}
	if ($dups > 0) {
		print "$file: removed $dups duplicate includes\n";
	}
	close($f);
}
if ($dup_counter == 0) {
	print "No duplicate includes found.\n";
}
