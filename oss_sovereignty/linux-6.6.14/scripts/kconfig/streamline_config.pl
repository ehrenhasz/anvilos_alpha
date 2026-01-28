use warnings;
use strict;
use Getopt::Long;
my $debugprint = 0;
$debugprint = 1 if (defined($ENV{LOCALMODCONFIG_DEBUG}));
sub dprint {
    return if (!$debugprint);
    print STDERR @_;
}
my $uname = `uname -r`;
chomp $uname;
my @searchconfigs = (
	{
	    "file" => ".config",
	    "exec" => "cat",
	},
	{
	    "file" => "/proc/config.gz",
	    "exec" => "zcat",
	},
	{
	    "file" => "/boot/config-$uname",
	    "exec" => "cat",
	},
	{
	    "file" => "/boot/vmlinuz-$uname",
	    "exec" => "scripts/extract-ikconfig",
	    "test" => "scripts/extract-ikconfig",
	},
	{
	    "file" => "vmlinux",
	    "exec" => "scripts/extract-ikconfig",
	    "test" => "scripts/extract-ikconfig",
	},
	{
	    "file" => "/lib/modules/$uname/kernel/kernel/configs.ko",
	    "exec" => "scripts/extract-ikconfig",
	    "test" => "scripts/extract-ikconfig",
	},
	{
	    "file" => "kernel/configs.ko",
	    "exec" => "scripts/extract-ikconfig",
	    "test" => "scripts/extract-ikconfig",
	},
	{
	    "file" => "kernel/configs.o",
	    "exec" => "scripts/extract-ikconfig",
	    "test" => "scripts/extract-ikconfig",
	},
);
sub read_config {
    foreach my $conf (@searchconfigs) {
	my $file = $conf->{"file"};
	next if ( ! -f "$file");
	if (defined($conf->{"test"})) {
	    `$conf->{"test"} $conf->{"file"} 2>/dev/null`;
	    next if ($?);
	}
	my $exec = $conf->{"exec"};
	print STDERR "using config: '$file'\n";
	open(my $infile, '-|', "$exec $file") || die "Failed to run $exec $file";
	my @x = <$infile>;
	close $infile;
	return @x;
    }
    die "No config file found";
}
my @config_file = read_config;
my $localmodconfig = 0;
my $localyesconfig = 0;
GetOptions("localmodconfig" => \$localmodconfig,
	   "localyesconfig" => \$localyesconfig);
my $ksource = ($ARGV[0] ? $ARGV[0] : '.');
my $kconfig = $ARGV[1];
my $lsmod_file = $ENV{'LSMOD'};
my @makefiles = `find $ksource -name Makefile -or -name Kbuild 2>/dev/null`;
chomp @makefiles;
my %depends;
my %selects;
my %prompts;
my %objects;
my %config2kfile;
my $var;
my $iflevel = 0;
my @ifdeps;
my %read_kconfigs;
sub read_kconfig {
    my ($kconfig) = @_;
    my $state = "NONE";
    my $config;
    my $cont = 0;
    my $line;
    my $source = "$ksource/$kconfig";
    my $last_source = "";
    while ($source =~ /\$\((\w+)\)/ && $last_source ne $source) {
	my $env = $1;
	$last_source = $source;
	$source =~ s/\$\($env\)/$ENV{$env}/;
    }
    open(my $kinfile, '<', $source) || die "Can't open $source";
    while (<$kinfile>) {
	chomp;
	if ($cont) {
	    $_ = $line . " " . $_;
	}
	if (s/\\$//) {
	    $cont = 1;
	    $line = $_;
	    next;
	}
	$cont = 0;
	if (/^source\s+"?([^"]+)/) {
	    my $kconfig = $1;
	    if (!defined($read_kconfigs{$kconfig})) {
		$read_kconfigs{$kconfig} = 1;
		read_kconfig($kconfig);
	    }
	    next;
	}
	if (/^\s*(menu)?config\s+(\S+)\s*$/) {
	    $state = "NEW";
	    $config = $2;
	    $config2kfile{"CONFIG_$config"} = $kconfig;
	    for (my $i = 0; $i < $iflevel; $i++) {
		if ($i) {
		    $depends{$config} .= " " . $ifdeps[$i];
		} else {
		    $depends{$config} = $ifdeps[$i];
		}
		$state = "DEP";
	    }
	} elsif ($state eq "NEW" && /^\s*depends\s+on\s+(.*)$/) {
	    $state = "DEP";
	    $depends{$config} = $1;
	} elsif ($state eq "DEP" && /^\s*depends\s+on\s+(.*)$/) {
	    $depends{$config} .= " " . $1;
	} elsif ($state eq "DEP" && /^\s*def(_(bool|tristate)|ault)\s+(\S.*)$/) {
	    my $dep = $3;
	    if ($dep !~ /^\s*(y|m|n)\s*$/) {
		$dep =~ s/.*\sif\s+//;
		$depends{$config} .= " " . $dep;
		dprint "Added default depends $dep to $config\n";
	    }
	} elsif ($state ne "NONE" && /^\s*select\s+(\S+)/) {
	    my $conf = $1;
	    if (defined($selects{$conf})) {
		$selects{$conf} .= " " . $config;
	    } else {
		$selects{$conf} = $config;
	    }
	} elsif ($state ne "NONE" && /^\s*(tristate\s+\S|prompt\b)/) {
	    $prompts{$config} = 1;
	} elsif (/^if\s+(.*\S)\s*$/) {
	    my $deps = $1;
	    $deps =~ s/^[^a-zA-Z0-9_]*//;
	    $deps =~ s/[^a-zA-Z0-9_]*$//;
	    my @deps = split /[^a-zA-Z0-9_]+/, $deps;
	    $ifdeps[$iflevel++] = join ':', @deps;
	} elsif (/^endif/) {
	    $iflevel-- if ($iflevel);
	} elsif (/^\s*(---)?help(---)?\s*$/ || /^(comment|choice|menu)\b/) {
	    $state = "NONE";
	}
    }
    close($kinfile);
}
if ($kconfig) {
    read_kconfig($kconfig);
}
sub convert_vars {
    my ($line, %vars) = @_;
    my $process = "";
    while ($line =~ s/^(.*?)(\$\((.*?)\))//) {
	my $start = $1;
	my $variable = $2;
	my $var = $3;
	if (defined($vars{$var})) {
	    $process .= $start . $vars{$var};
	} else {
	    $process .= $start . $variable;
	}
    }
    $process .= $line;
    return $process;
}
foreach my $makefile (@makefiles) {
    my $line = "";
    my %make_vars;
    open(my $infile, '<', $makefile) || die "Can't open $makefile";
    while (<$infile>) {
	chomp;
	if (/^(.*)\\$/) {
	    $line .= $1;
	    next;
	}
	$line .= $_;
	$_ = $line;
	$line = "";
	my $objs;
	$_ = convert_vars($_, %make_vars);
	if (/obj-\$[({](CONFIG_[^})]*)[)}]\s*[+:]?=\s*(.*)/) {
	    $var = $1;
	    $objs = $2;
	} elsif (/^\s*(\S+)\s*[:]?=\s*(.*\S)/) {
	    $make_vars{$1} = $2;
	}
	if (defined($objs)) {
	    foreach my $obj (split /\s+/,$objs) {
		$obj =~ s/-/_/g;
		if ($obj =~ /(.*)\.o$/) {
		    my @arr;
		    if (defined($objects{$1})) {
			@arr = @{$objects{$1}};
		    }
		    $arr[$
		    $objects{$1} = \@arr;
		}
	    }
	}
    }
    close($infile);
}
my %modules;
my $linfile;
if (defined($lsmod_file)) {
    if ( ! -f $lsmod_file) {
	if ( -f $ENV{'objtree'}."/".$lsmod_file) {
	    $lsmod_file = $ENV{'objtree'}."/".$lsmod_file;
	} else {
		die "$lsmod_file not found";
	}
    }
    my $otype = ( -x $lsmod_file) ? '-|' : '<';
    open($linfile, $otype, $lsmod_file);
} else {
    my $lsmod;
    foreach my $dir ( ("/sbin", "/bin", "/usr/sbin", "/usr/bin") ) {
	if ( -x "$dir/lsmod" ) {
	    $lsmod = "$dir/lsmod";
	    last;
	}
    }
    if (!defined($lsmod)) {
	$lsmod = "lsmod";
    }
    open($linfile, '-|', $lsmod) || die "Can not call lsmod with $lsmod";
}
while (<$linfile>) {
	next if (/^Module/);  
	if (/^(\S+)/) {
		$modules{$1} = 1;
	}
}
close ($linfile);
my %configs;
foreach my $module (keys(%modules)) {
    if (defined($objects{$module})) {
	my @arr = @{$objects{$module}};
	foreach my $conf (@arr) {
	    $configs{$conf} = $module;
	    dprint "$conf added by direct ($module)\n";
	    if ($debugprint) {
		my $c=$conf;
		$c =~ s/^CONFIG_//;
		if (defined($depends{$c})) {
		    dprint " deps = $depends{$c}\n";
		} else {
		    dprint " no deps\n";
		}
	    }
	}
    } else {
	print STDERR "$module config not found!!\n";
    }
}
my %orig_configs;
my $valid = "A-Za-z_0-9";
foreach my $line (@config_file) {
    $_ = $line;
    if (/(CONFIG_[$valid]*)=(m|y)/) {
	$orig_configs{$1} = $2;
    }
}
my $repeat = 1;
my $depconfig;
sub parse_config_depends
{
    my ($p) = @_;
    while ($p =~ /[$valid]/) {
	if ($p =~ /^[^$valid]*([$valid]+)/) {
	    my $conf = "CONFIG_" . $1;
	    $p =~ s/^[^$valid]*[$valid]+//;
	    if (!defined($orig_configs{$conf}) || $orig_configs{$conf} eq "y") {
		next;
	    }
	    if (!defined($configs{$conf})) {
		$repeat = 1; 
		dprint "$conf selected by depend $depconfig\n";
		$configs{$conf} = 1;
	    }
	} else {
	    die "this should never happen";
	}
    }
}
sub parse_config_selects
{
    my ($config, $p) = @_;
    my $next_config;
    while ($p =~ /[$valid]/) {
	if ($p =~ /^[^$valid]*([$valid]+)/) {
	    my $conf = "CONFIG_" . $1;
	    $p =~ s/^[^$valid]*[$valid]+//;
	    if (!defined($orig_configs{$conf})) {
		dprint "$conf not set for $config select\n";
		next;
	    }
	    if (defined($orig_configs{$conf}) && $orig_configs{$conf} ne "m") {
		dprint "$conf (non module) selects config, we are good\n";
		return;
	    }
	    if (defined($configs{$conf})) {
		dprint "$conf selects $config so we are good\n";
		return;
	    }
	    if (!defined($next_config)) {
		$next_config = $conf;
	    }
	} else {
	    die "this should never happen";
	}
    }
    if (!defined($next_config)) {
	print STDERR "WARNING: $config is required, but nothing in the\n";
	print STDERR "  current config selects it.\n";
	return;
    }
    $repeat = 1;
    $configs{$next_config} = 1;
    dprint "$next_config selected by select $config\n";
}
my %process_selects;
sub loop_depend {
    $repeat = 1;
    while ($repeat) {
	$repeat = 0;
      forloop:
	foreach my $config (keys %configs) {
	    if (defined($orig_configs{$config}) && $orig_configs{$config} ne "m") {
		next forloop;
	    }
	    $config =~ s/^CONFIG_//;
	    $depconfig = $config;
	    if (defined($depends{$config})) {
		parse_config_depends $depends{$config};
	    }
	    if (!defined($prompts{$config}) && defined($selects{$config})) {
		$process_selects{$config} = 1;
	    }
	}
    }
}
sub loop_select {
    foreach my $config (keys %process_selects) {
	$config =~ s/^CONFIG_//;
	dprint "Process select $config\n";
	parse_config_selects $config, $selects{$config};
    }
}
while ($repeat) {
    loop_depend;
    $repeat = 0;
    loop_select;
}
my %setconfigs;
my @preserved_kconfigs;
if (defined($ENV{'LMC_KEEP'})) {
	@preserved_kconfigs = split(/:/,$ENV{LMC_KEEP});
}
sub in_preserved_kconfigs {
    my $kconfig = $config2kfile{$_[0]};
    if (!defined($kconfig)) {
	return 0;
    }
    foreach my $excl (@preserved_kconfigs) {
	if($kconfig =~ /^$excl/) {
	    return 1;
	}
    }
    return 0;
}
foreach my $line (@config_file) {
    $_ = $line;
    if (/CONFIG_IKCONFIG/) {
	if (/
	    print "CONFIG_IKCONFIG=m\n";
	    print "# CONFIG_IKCONFIG_PROC is not set\n";
	} else {
	    print;
	}
	next;
    }
    if (/CONFIG_MODULE_SIG_KEY="(.+)"/) {
	my $orig_cert = $1;
	my $default_cert = "certs/signing_key.pem";
	if (!defined($depends{"MODULE_SIG_KEY"}) ||
	    $depends{"MODULE_SIG_KEY"} !~ /"\Q$default_cert\E"/) {
	    print STDERR "WARNING: MODULE_SIG_KEY assertion failure, ",
		"update needed to ", __FILE__, " line ", __LINE__, "\n";
	    print;
	} elsif ($orig_cert ne $default_cert && ! -f $orig_cert) {
	    print STDERR "Module signature verification enabled but ",
		"module signing key \"$orig_cert\" not found. Resetting ",
		"signing key to default value.\n";
	    print "CONFIG_MODULE_SIG_KEY=\"$default_cert\"\n";
	} else {
	    print;
	}
	next;
    }
    if (/CONFIG_SYSTEM_TRUSTED_KEYS="(.+)"/) {
	my $orig_keys = $1;
	if (! -f $orig_keys) {
	    print STDERR "System keyring enabled but keys \"$orig_keys\" ",
		"not found. Resetting keys to default value.\n";
	    print "CONFIG_SYSTEM_TRUSTED_KEYS=\"\"\n";
	} else {
	    print;
	}
	next;
    }
    if (/^(CONFIG.*)=(m|y)/) {
	if (in_preserved_kconfigs($1)) {
	    dprint "Preserve config $1";
	    print;
	    next;
	}
	if (defined($configs{$1})) {
	    if ($localyesconfig) {
		$setconfigs{$1} = 'y';
		print "$1=y\n";
		next;
	    } else {
		$setconfigs{$1} = $2;
	    }
	} elsif ($2 eq "m") {
	    print "# $1 is not set\n";
	    next;
	}
    }
    print;
}
loop:
foreach my $module (keys(%modules)) {
    if (defined($objects{$module})) {
	my @arr = @{$objects{$module}};
	foreach my $conf (@arr) {
	    if (defined($setconfigs{$conf})) {
		next loop;
	    }
	}
	print STDERR "module $module did not have configs";
	foreach my $conf (@arr) {
	    print STDERR " " , $conf;
	}
	print STDERR "\n";
    }
}
