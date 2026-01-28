use strict;
use warnings;
use IO::Handle;
use IO::Select;
use POSIX ":sys_wait_h";
my $nm = $ENV{'NM'} || die "$0: ERROR: NM not set?";
my $objtree = $ENV{'objtree'} || '.';
my $jobs = {};		
my $results = {};	
sub get_online_processors {
	open(my $fh, "getconf _NPROCESSORS_ONLN 2>/dev/null |")
		or die "$0: ERROR: failed to execute getconf: $!";
	my $procs = <$fh>;
	close($fh);
	if (!($procs =~ /^\d+$/)) {
		return 1;
	}
	return int($procs);
}
sub write_results {
	my ($index, $initcalls) = @_;
	foreach my $counter (sort { $a <=> $b } keys(%{$initcalls})) {
		my $level = $initcalls->{$counter}->{'level'};
		my $secname = $initcalls->{$counter}->{'module'} . '__' .
			      $counter . '_' .
			      $initcalls->{$counter}->{'line'} . '_' .
			      $initcalls->{$counter}->{'function'};
		print "$index $level $secname\n";
	}
}
sub read_results{
	my ($fh) = @_;
	my $data = <$fh>;
	if (!defined($data)) {
		return 0;
	}
	chomp($data);
	my ($index, $level, $secname) = $data =~
		/^(\d+)\ ([^\ ]+)\ (.*)$/;
	if (!defined($index) ||
		!defined($level) ||
		!defined($secname)) {
		die "$0: ERROR: child process returned invalid data: $data\n";
	}
	$index = int($index);
	if (!exists($results->{$index})) {
		$results->{$index} = [];
	}
	push (@{$results->{$index}}, {
		'level'   => $level,
		'secname' => $secname
	});
	return 1;
}
sub find_initcalls {
	my ($index, $file) = @_;
	die "$0: ERROR: file $file doesn't exist?" if (! -f $file);
	open(my $fh, "\"$nm\" --defined-only \"$file\" 2>/dev/null |")
		or die "$0: ERROR: failed to execute \"$nm\": $!";
	my $initcalls = {};
	while (<$fh>) {
		chomp;
		my ($path)= $_ =~ /^(.+)\:$/;
		if (defined($path)) {
			write_results($index, $initcalls);
			$initcalls = {};
			next;
		}
		my ($module, $counter, $line, $symbol) = $_ =~
			/[a-z]\s+__initcall__(\S*)__(\d+)_(\d+)_(.*)$/;
		if (!defined($module)) {
			$module = ''
		}
		if (!defined($counter) ||
			!defined($line) ||
			!defined($symbol)) {
			next;
		}
		my ($function, $level) = $symbol =~
			/^(.*)((early|rootfs|con|[0-9])s?)$/;
		die "$0: ERROR: invalid initcall name $symbol in $file($path)"
			if (!defined($function) || !defined($level));
		$initcalls->{$counter} = {
			'module'   => $module,
			'line'     => $line,
			'function' => $function,
			'level'    => $level,
		};
	}
	close($fh);
	write_results($index, $initcalls);
}
sub wait_for_results {
	my ($select) = @_;
	my $pid = 0;
	do {
		foreach my $fh ($select->can_read(0)) {
			read_results($fh);
		}
		$pid = waitpid(-1, WNOHANG);
		if ($pid > 0) {
			if (!exists($jobs->{$pid})) {
				next;
			}
			my $fh = $jobs->{$pid};
			$select->remove($fh);
			while (read_results($fh)) {
			}
			close($fh);
			delete($jobs->{$pid});
		}
	} while ($pid > 0);
}
sub process_files {
	my $index = 0;
	my $njobs = $ENV{'PARALLELISM'} || get_online_processors();
	my $select = IO::Select->new();
	while (my $file = shift(@ARGV)) {
		my $pid = open(my $fh, '-|');
		if (!defined($pid)) {
			die "$0: ERROR: failed to fork: $!";
		} elsif ($pid) {
			$select->add($fh);
			$jobs->{$pid} = $fh;
		} else {
			STDOUT->autoflush(1);
			find_initcalls($index, "$objtree/$file");
			exit;
		}
		$index++;
		if (scalar(keys(%{$jobs})) >= $njobs) {
			wait_for_results($select);
		}
	}
	while (scalar(keys(%{$jobs})) > 0) {
		wait_for_results($select);
	}
}
sub generate_initcall_lds() {
	process_files();
	my $sections = {};	
	foreach my $index (sort { $a <=> $b } keys(%{$results})) {
		foreach my $result (@{$results->{$index}}) {
			my $level = $result->{'level'};
			if (!exists($sections->{$level})) {
				$sections->{$level} = [];
			}
			push(@{$sections->{$level}}, $result->{'secname'});
		}
	}
	die "$0: ERROR: no initcalls?" if (!keys(%{$sections}));
	print "SECTIONS {\n";
	foreach my $level (sort(keys(%{$sections}))) {
		my $section;
		if ($level eq 'con') {
			$section = '.con_initcall.init';
		} else {
			$section = ".initcall${level}.init";
		}
		print "\t${section} : {\n";
		foreach my $secname (@{$sections->{$level}}) {
			print "\t\t*(${section}..${secname}) ;\n";
		}
		print "\t}\n";
	}
	print "}\n";
}
generate_initcall_lds();
