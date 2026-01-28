$convertme = 'convertme.pl';
$convert = 0;
%converted = ();
die "usage: $0 file.c | file.h\n" if scalar(@ARGV) == 0;
open(CM, ">$convertme") or die "convertme.pl $!";
print CM "#!/usr/bin/perl -p -i\n\n";
while (<>) {
	next if /getopt/;
	while (/([a-zA-Z_][a-zA-Z0-9_]*)/g) {
		$var = $1;
		next if ($var =~ /BusyBox/);
		$convert++ if ($var =~ /^[a-z]+[A-Z][a-z]+/);
		$convert++ if ($var =~ /^[A-Z][a-z]+[A-Z][a-z]+/);
		if ($convert) {
			$convert = 0;
			next if ($converted{$var});
			$converted{$var} = 1;
			print CM "s/\\b$var\\b/"; 
			$var = lcfirst($var);
			$var =~ s/([A-Z])/_$1/g;
			$var = lc($var);
			print CM "$var/g;\n";
		}
	}
}
close(CM);
chmod 0755, $convertme;
print "Done. Scheduled name changes are in $convertme.\n";
print "Please review/modify it and then type ./$convertme to do the search & replace.\n";
