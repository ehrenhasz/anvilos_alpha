use strict;
use IO::Handle;
binmode STDIN;
binmode STDOUT;
STDOUT->autoflush(1);
my $needed = 1;
while (1) {
	my $char;
	my $bytes = sysread(STDIN, $char, 1);
	exit 0 if ($bytes == 0);
	if ($needed) {
		print "# ";
		$needed = 0;
	}
	print $char;
	$needed = 1 if ($char eq "\n");
}
