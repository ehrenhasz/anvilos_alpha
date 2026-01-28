use strict;
use warnings;
use POSIX;
use File::Basename;
use Cwd 'abs_path';
use Term::ANSIColor qw(:constants);
use Encode qw(decode encode);
my $P = $0;
my $D = dirname(abs_path($P));
my $V = '0.32';
use Getopt::Long qw(:config no_auto_abbrev);
my $quiet = 0;
my $verbose = 0;
my %verbose_messages = ();
my %verbose_emitted = ();
my $tree = 1;
my $chk_signoff = 1;
my $chk_patch = 1;
my $tst_only;
my $emacs = 0;
my $terse = 0;
my $showfile = 0;
my $file = 0;
my $git = 0;
my %git_commits = ();
my $check = 0;
my $check_orig = 0;
my $summary = 1;
my $mailback = 0;
my $summary_file = 0;
my $show_types = 0;
my $list_types = 0;
my $fix = 0;
my $fix_inplace = 0;
my $root;
my $gitroot = $ENV{'GIT_DIR'};
$gitroot = ".git" if !defined($gitroot);
my %debug;
my %camelcase = ();
my %use_type = ();
my @use = ();
my %ignore_type = ();
my @ignore = ();
my $help = 0;
my $configuration_file = ".checkpatch.conf";
my $max_line_length = 100;
my $ignore_perl_version = 0;
my $minimum_perl_version = 5.10.0;
my $min_conf_desc_length = 4;
my $spelling_file = "$D/spelling.txt";
my $codespell = 0;
my $codespellfile = "/usr/share/codespell/dictionary.txt";
my $user_codespellfile = "";
my $conststructsfile = "$D/const_structs.checkpatch";
my $docsfile = "$D/../Documentation/dev-tools/checkpatch.rst";
my $typedefsfile;
my $color = "auto";
my $allow_c99_comments = 1; 
my $git_command ='export LANGUAGE=en_US.UTF-8; git';
my $tabsize = 8;
my ${CONFIG_} = "CONFIG_";
my %maybe_linker_symbol; 
sub help {
	my ($exitcode) = @_;
	print << "EOM";
Usage: $P [OPTION]... [FILE]...
Version: $V
Options:
  -q, --quiet                quiet
  -v, --verbose              verbose mode
  --no-tree                  run without a kernel tree
  --no-signoff               do not check for 'Signed-off-by' line
  --patch                    treat FILE as patchfile (default)
  --emacs                    emacs compile window format
  --terse                    one line per report
  --showfile                 emit diffed file position, not input file position
  -g, --git                  treat FILE as a single commit or git revision range
                             single git commit with:
                               <rev>
                               <rev>^
                               <rev>~n
                             multiple git commits with:
                               <rev1>..<rev2>
                               <rev1>...<rev2>
                               <rev>-<count>
                             git merges are ignored
  -f, --file                 treat FILE as regular source file
  --subjective, --strict     enable more subjective tests
  --list-types               list the possible message types
  --types TYPE(,TYPE2...)    show only these comma separated message types
  --ignore TYPE(,TYPE2...)   ignore various comma separated message types
  --show-types               show the specific message type in the output
  --max-line-length=n        set the maximum line length, (default $max_line_length)
                             if exceeded, warn on patches
                             requires --strict for use with --file
  --min-conf-desc-length=n   set the min description length, if shorter, warn
  --tab-size=n               set the number of spaces for tab (default $tabsize)
  --root=PATH                PATH to the kernel tree root
  --no-summary               suppress the per-file summary
  --mailback                 only produce a report in case of warnings/errors
  --summary-file             include the filename in summary
  --debug KEY=[0|1]          turn on/off debugging of KEY, where KEY is one of
                             'values', 'possible', 'type', and 'attr' (default
                             is all off)
  --test-only=WORD           report only warnings/errors containing WORD
                             literally
  --fix                      EXPERIMENTAL - may create horrible results
                             If correctable single-line errors exist, create
                             "<inputfile>.EXPERIMENTAL-checkpatch-fixes"
                             with potential errors corrected to the preferred
                             checkpatch style
  --fix-inplace              EXPERIMENTAL - may create horrible results
                             Is the same as --fix, but overwrites the input
                             file.  It's your fault if there's no backup or git
  --ignore-perl-version      override checking of perl version.  expect
                             runtime errors.
  --codespell                Use the codespell dictionary for spelling/typos
                             (default:$codespellfile)
  --codespellfile            Use this codespell dictionary
  --typedefsfile             Read additional types from this file
  --color[=WHEN]             Use colors 'always', 'never', or only when output
                             is a terminal ('auto'). Default is 'auto'.
  --kconfig-prefix=WORD      use WORD as a prefix for Kconfig symbols (default
                             ${CONFIG_})
  -h, --help, --version      display this help and exit
When FILE is - read standard input.
EOM
	exit($exitcode);
}
sub uniq {
	my %seen;
	return grep { !$seen{$_}++ } @_;
}
sub list_types {
	my ($exitcode) = @_;
	my $count = 0;
	local $/ = undef;
	open(my $script, '<', abs_path($P)) or
	    die "$P: Can't read '$P' $!\n";
	my $text = <$script>;
	close($script);
	my %types = ();
	while ($text =~ /(?:(\bCHK|\bWARN|\bERROR|&\{\$msg_level})\s*\(|\$msg_type\s*=)\s*"([^"]+)"/g) {
		if (defined($1)) {
			if (exists($types{$2})) {
				$types{$2} .= ",$1" if ($types{$2} ne $1);
			} else {
				$types{$2} = $1;
			}
		} else {
			$types{$2} = "UNDETERMINED";
		}
	}
	print("#\tMessage type\n\n");
	if ($color) {
		print(" ( Color coding: ");
		print(RED . "ERROR" . RESET);
		print(" | ");
		print(YELLOW . "WARNING" . RESET);
		print(" | ");
		print(GREEN . "CHECK" . RESET);
		print(" | ");
		print("Multiple levels / Undetermined");
		print(" )\n\n");
	}
	foreach my $type (sort keys %types) {
		my $orig_type = $type;
		if ($color) {
			my $level = $types{$type};
			if ($level eq "ERROR") {
				$type = RED . $type . RESET;
			} elsif ($level eq "WARN") {
				$type = YELLOW . $type . RESET;
			} elsif ($level eq "CHK") {
				$type = GREEN . $type . RESET;
			}
		}
		print(++$count . "\t" . $type . "\n");
		if ($verbose && exists($verbose_messages{$orig_type})) {
			my $message = $verbose_messages{$orig_type};
			$message =~ s/\n/\n\t/g;
			print("\t" . $message . "\n\n");
		}
	}
	exit($exitcode);
}
my $conf = which_conf($configuration_file);
if (-f $conf) {
	my @conf_args;
	open(my $conffile, '<', "$conf")
	    or warn "$P: Can't find a readable $configuration_file file $!\n";
	while (<$conffile>) {
		my $line = $_;
		$line =~ s/\s*\n?$//g;
		$line =~ s/^\s*//g;
		$line =~ s/\s+/ /g;
		next if ($line =~ m/^\s*
		next if ($line =~ m/^\s*$/);
		my @words = split(" ", $line);
		foreach my $word (@words) {
			last if ($word =~ m/^
			push (@conf_args, $word);
		}
	}
	close($conffile);
	unshift(@ARGV, @conf_args) if @conf_args;
}
sub load_docs {
	open(my $docs, '<', "$docsfile")
	    or warn "$P: Can't read the documentation file $docsfile $!\n";
	my $type = '';
	my $desc = '';
	my $in_desc = 0;
	while (<$docs>) {
		chomp;
		my $line = $_;
		$line =~ s/\s+$//;
		if ($line =~ /^\s*\*\*(.+)\*\*$/) {
			if ($desc ne '') {
				$verbose_messages{$type} = trim($desc);
			}
			$type = $1;
			$desc = '';
			$in_desc = 1;
		} elsif ($in_desc) {
			if ($line =~ /^(?:\s{4,}|$)/) {
				$line =~ s/^\s{4}//;
				$desc .= $line;
				$desc .= "\n";
			} else {
				$verbose_messages{$type} = trim($desc);
				$type = '';
				$desc = '';
				$in_desc = 0;
			}
		}
	}
	if ($desc ne '') {
		$verbose_messages{$type} = trim($desc);
	}
	close($docs);
}
foreach (@ARGV) {
	if ($_ eq "--color" || $_ eq "-color") {
		$_ = "--color=$color";
	}
}
GetOptions(
	'q|quiet+'	=> \$quiet,
	'v|verbose!'	=> \$verbose,
	'tree!'		=> \$tree,
	'signoff!'	=> \$chk_signoff,
	'patch!'	=> \$chk_patch,
	'emacs!'	=> \$emacs,
	'terse!'	=> \$terse,
	'showfile!'	=> \$showfile,
	'f|file!'	=> \$file,
	'g|git!'	=> \$git,
	'subjective!'	=> \$check,
	'strict!'	=> \$check,
	'ignore=s'	=> \@ignore,
	'types=s'	=> \@use,
	'show-types!'	=> \$show_types,
	'list-types!'	=> \$list_types,
	'max-line-length=i' => \$max_line_length,
	'min-conf-desc-length=i' => \$min_conf_desc_length,
	'tab-size=i'	=> \$tabsize,
	'root=s'	=> \$root,
	'summary!'	=> \$summary,
	'mailback!'	=> \$mailback,
	'summary-file!'	=> \$summary_file,
	'fix!'		=> \$fix,
	'fix-inplace!'	=> \$fix_inplace,
	'ignore-perl-version!' => \$ignore_perl_version,
	'debug=s'	=> \%debug,
	'test-only=s'	=> \$tst_only,
	'codespell!'	=> \$codespell,
	'codespellfile=s'	=> \$user_codespellfile,
	'typedefsfile=s'	=> \$typedefsfile,
	'color=s'	=> \$color,
	'no-color'	=> \$color,	
	'nocolor'	=> \$color,	
	'kconfig-prefix=s'	=> \${CONFIG_},
	'h|help'	=> \$help,
	'version'	=> \$help
) or $help = 2;
if ($user_codespellfile) {
	$codespellfile = $user_codespellfile;
} elsif (!(-f $codespellfile)) {
	if (($codespell || $help) && which("python3") ne "") {
		my $python_codespell_dict = << "EOF";
import os.path as op
import codespell_lib
codespell_dir = op.dirname(codespell_lib.__file__)
codespell_file = op.join(codespell_dir, 'data', 'dictionary.txt')
print(codespell_file, end='')
EOF
		my $codespell_dict = `python3 -c "$python_codespell_dict" 2> /dev/null`;
		$codespellfile = $codespell_dict if (-f $codespell_dict);
	}
}
help($help - 1) if ($help);
die "$P: --git cannot be used with --file or --fix\n" if ($git && ($file || $fix));
die "$P: --verbose cannot be used with --terse\n" if ($verbose && $terse);
if ($color =~ /^[01]$/) {
	$color = !$color;
} elsif ($color =~ /^always$/i) {
	$color = 1;
} elsif ($color =~ /^never$/i) {
	$color = 0;
} elsif ($color =~ /^auto$/i) {
	$color = (-t STDOUT);
} else {
	die "$P: Invalid color mode: $color\n";
}
load_docs() if ($verbose);
list_types(0) if ($list_types);
$fix = 1 if ($fix_inplace);
$check_orig = $check;
my $exit = 0;
my $perl_version_ok = 1;
if ($^V && $^V lt $minimum_perl_version) {
	$perl_version_ok = 0;
	printf "$P: requires at least perl version %vd\n", $minimum_perl_version;
	exit(1) if (!$ignore_perl_version);
}
if ($
	push(@ARGV, '-');
}
die "$P: Invalid TAB size: $tabsize\n" if ($tabsize < 2);
sub hash_save_array_words {
	my ($hashRef, $arrayRef) = @_;
	my @array = split(/,/, join(',', @$arrayRef));
	foreach my $word (@array) {
		$word =~ s/\s*\n?$//g;
		$word =~ s/^\s*//g;
		$word =~ s/\s+/ /g;
		$word =~ tr/[a-z]/[A-Z]/;
		next if ($word =~ m/^\s*
		next if ($word =~ m/^\s*$/);
		$hashRef->{$word}++;
	}
}
sub hash_show_words {
	my ($hashRef, $prefix) = @_;
	if (keys %$hashRef) {
		print "\nNOTE: $prefix message types:";
		foreach my $word (sort keys %$hashRef) {
			print " $word";
		}
		print "\n";
	}
}
hash_save_array_words(\%ignore_type, \@ignore);
hash_save_array_words(\%use_type, \@use);
my $dbg_values = 0;
my $dbg_possible = 0;
my $dbg_type = 0;
my $dbg_attr = 0;
for my $key (keys %debug) {
	eval "\${dbg_$key} = '$debug{$key}';";
	die "$@" if ($@);
}
my $rpt_cleaners = 0;
if ($terse) {
	$emacs = 1;
	$quiet++;
}
if ($tree) {
	if (defined $root) {
		if (!top_of_kernel_tree($root)) {
			die "$P: $root: --root does not point at a valid tree\n";
		}
	} else {
		if (top_of_kernel_tree('.')) {
			$root = '.';
		} elsif ($0 =~ m@(.*)/scripts/[^/]*$@ &&
						top_of_kernel_tree($1)) {
			$root = $1;
		}
	}
	if (!defined $root) {
		print "Must be run from the top-level dir. of a kernel tree\n";
		exit(2);
	}
}
my $emitted_corrupt = 0;
our $Ident	= qr{
			[A-Za-z_][A-Za-z\d_]*
			(?:\s*\
		}x;
our $Storage	= qr{extern|static|asmlinkage};
our $Sparse	= qr{
			__user|
			__kernel|
			__force|
			__iomem|
			__must_check|
			__kprobes|
			__ref|
			__refconst|
			__refdata|
			__rcu|
			__private
		}x;
our $InitAttributePrefix = qr{__(?:mem|cpu|dev|net_|)};
our $InitAttributeData = qr{$InitAttributePrefix(?:initdata\b)};
our $InitAttributeConst = qr{$InitAttributePrefix(?:initconst\b)};
our $InitAttributeInit = qr{$InitAttributePrefix(?:init\b)};
our $InitAttribute = qr{$InitAttributeData|$InitAttributeConst|$InitAttributeInit};
our $Attribute	= qr{
			const|
			volatile|
			__percpu|
			__nocast|
			__safe|
			__bitwise|
			__packed__|
			__packed2__|
			__naked|
			__maybe_unused|
			__always_unused|
			__noreturn|
			__used|
			__cold|
			__pure|
			__noclone|
			__deprecated|
			__read_mostly|
			__ro_after_init|
			__kprobes|
			$InitAttribute|
			____cacheline_aligned|
			____cacheline_aligned_in_smp|
			____cacheline_internodealigned_in_smp|
			__weak|
			__alloc_size\s*\(\s*\d+\s*(?:,\s*\d+\s*)?\)
		  }x;
our $Modifier;
our $Inline	= qr{inline|__always_inline|noinline|__inline|__inline__};
our $Member	= qr{->$Ident|\.$Ident|\[[^]]*\]};
our $Lval	= qr{$Ident(?:$Member)*};
our $Int_type	= qr{(?i)llu|ull|ll|lu|ul|l|u};
our $Binary	= qr{(?i)0b[01]+$Int_type?};
our $Hex	= qr{(?i)0x[0-9a-f]+$Int_type?};
our $Int	= qr{[0-9]+$Int_type?};
our $Octal	= qr{0[0-7]+$Int_type?};
our $String	= qr{(?:\b[Lu])?"[X\t]*"};
our $Float_hex	= qr{(?i)0x[0-9a-f]+p-?[0-9]+[fl]?};
our $Float_dec	= qr{(?i)(?:[0-9]+\.[0-9]*|[0-9]*\.[0-9]+)(?:e-?[0-9]+)?[fl]?};
our $Float_int	= qr{(?i)[0-9]+e-?[0-9]+[fl]?};
our $Float	= qr{$Float_hex|$Float_dec|$Float_int};
our $Constant	= qr{$Float|$Binary|$Octal|$Hex|$Int};
our $Assignment	= qr{\*\=|/=|%=|\+=|-=|<<=|>>=|&=|\^=|\|=|=};
our $Compare    = qr{<=|>=|==|!=|<|(?<!-)>};
our $Arithmetic = qr{\+|-|\*|\/|%};
our $Operators	= qr{
			<=|>=|==|!=|
			=>|->|<<|>>|<|>|!|~|
			&&|\|\||,|\^|\+\+|--|&|\||$Arithmetic
		  }x;
our $c90_Keywords = qr{do|for|while|if|else|return|goto|continue|switch|default|case|break}x;
our $BasicType;
our $NonptrType;
our $NonptrTypeMisordered;
our $NonptrTypeWithAttr;
our $Type;
our $TypeMisordered;
our $Declare;
our $DeclareMisordered;
our $NON_ASCII_UTF8	= qr{
	[\xC2-\xDF][\x80-\xBF]               
	|  \xE0[\xA0-\xBF][\x80-\xBF]        
	| [\xE1-\xEC\xEE\xEF][\x80-\xBF]{2}  
	|  \xED[\x80-\x9F][\x80-\xBF]        
	|  \xF0[\x90-\xBF][\x80-\xBF]{2}     
	| [\xF1-\xF3][\x80-\xBF]{3}          
	|  \xF4[\x80-\x8F][\x80-\xBF]{2}     
}x;
our $UTF8	= qr{
	[\x09\x0A\x0D\x20-\x7E]              
	| $NON_ASCII_UTF8
}x;
our $typeC99Typedefs = qr{(?:__)?(?:[us]_?)?int_?(?:8|16|32|64)_t};
our $typeOtherOSTypedefs = qr{(?x:
	u_(?:char|short|int|long) |          
	u(?:nchar|short|int|long)            
)};
our $typeKernelTypedefs = qr{(?x:
	(?:__)?(?:u|s|be|le)(?:8|16|32|64)|
	atomic_t
)};
our $typeStdioTypedefs = qr{(?x:
	FILE
)};
our $typeTypedefs = qr{(?x:
	$typeC99Typedefs\b|
	$typeOtherOSTypedefs\b|
	$typeKernelTypedefs\b|
	$typeStdioTypedefs\b
)};
our $zero_initializer = qr{(?:(?:0[xX])?0+$Int_type?|NULL|false)\b};
our $logFunctions = qr{(?x:
	printk(?:_ratelimited|_once|_deferred_once|_deferred|)|
	(?:[a-z0-9]+_){1,2}(?:printk|emerg|alert|crit|err|warning|warn|notice|info|debug|dbg|vdbg|devel|cont|WARN)(?:_ratelimited|_once|)|
	TP_printk|
	WARN(?:_RATELIMIT|_ONCE|)|
	panic|
	MODULE_[A-Z_]+|
	seq_vprintf|seq_printf|seq_puts
)};
our $allocFunctions = qr{(?x:
	(?:(?:devm_)?
		(?:kv|k|v)[czm]alloc(?:_array)?(?:_node)? |
		kstrdup(?:_const)? |
		kmemdup(?:_nul)?) |
	(?:\w+)?alloc_skb(?:_ip_align)? |
	dma_alloc_coherent
)};
our $signature_tags = qr{(?xi:
	Signed-off-by:|
	Co-developed-by:|
	Acked-by:|
	Tested-by:|
	Reviewed-by:|
	Reported-by:|
	Suggested-by:|
	To:|
	Cc:
)};
our @link_tags = qw(Link Closes);
our $link_tags_search = "";
our $link_tags_print = "";
foreach my $entry (@link_tags) {
	if ($link_tags_search ne "") {
		$link_tags_search .= '|';
		$link_tags_print .= ' or ';
	}
	$entry .= ':';
	$link_tags_search .= $entry;
	$link_tags_print .= "'$entry'";
}
$link_tags_search = "(?:${link_tags_search})";
our $tracing_logging_tags = qr{(?xi:
	[=-]*> |
	<[=-]* |
	\[ |
	\] |
	start |
	called |
	entered |
	entry |
	enter |
	in |
	inside |
	here |
	begin |
	exit |
	end |
	done |
	leave |
	completed |
	out |
	return |
	[\.\!:\s]*
)};
sub edit_distance_min {
	my (@arr) = @_;
	my $len = scalar @arr;
	if ((scalar @arr) < 1) {
		return;
	}
	my $min = $arr[0];
	for my $i (0 .. ($len-1)) {
		if ($arr[$i] < $min) {
			$min = $arr[$i];
		}
	}
	return $min;
}
sub get_edit_distance {
	my ($str1, $str2) = @_;
	$str1 = lc($str1);
	$str2 = lc($str2);
	$str1 =~ s/-//g;
	$str2 =~ s/-//g;
	my $len1 = length($str1);
	my $len2 = length($str2);
	my @distance;
	for my $i (0 .. $len1) {
		for my $j (0 .. $len2) {
			if ($i == 0) {
				$distance[$i][$j] = $j;
			} elsif ($j == 0) {
				$distance[$i][$j] = $i;
			} elsif (substr($str1, $i-1, 1) eq substr($str2, $j-1, 1)) {
				$distance[$i][$j] = $distance[$i - 1][$j - 1];
			} else {
				my $dist1 = $distance[$i][$j - 1]; 
				my $dist2 = $distance[$i - 1][$j]; 
				my $dist3 = $distance[$i - 1][$j - 1]; 
				$distance[$i][$j] = 1 + edit_distance_min($dist1, $dist2, $dist3);
			}
		}
	}
	return $distance[$len1][$len2];
}
sub find_standard_signature {
	my ($sign_off) = @_;
	my @standard_signature_tags = (
		'Signed-off-by:', 'Co-developed-by:', 'Acked-by:', 'Tested-by:',
		'Reviewed-by:', 'Reported-by:', 'Suggested-by:'
	);
	foreach my $signature (@standard_signature_tags) {
		return $signature if (get_edit_distance($sign_off, $signature) <= 2);
	}
	return "";
}
our $obsolete_archives = qr{(?xi:
	\Qfreedesktop.org/archives/dri-devel\E |
	\Qlists.infradead.org\E |
	\Qlkml.org\E |
	\Qmail-archive.com\E |
	\Qmailman.alsa-project.org/pipermail\E |
	\Qmarc.info\E |
	\Qozlabs.org/pipermail\E |
	\Qspinics.net\E
)};
our @typeListMisordered = (
	qr{char\s+(?:un)?signed},
	qr{int\s+(?:(?:un)?signed\s+)?short\s},
	qr{int\s+short(?:\s+(?:un)?signed)},
	qr{short\s+int(?:\s+(?:un)?signed)},
	qr{(?:un)?signed\s+int\s+short},
	qr{short\s+(?:un)?signed},
	qr{long\s+int\s+(?:un)?signed},
	qr{int\s+long\s+(?:un)?signed},
	qr{long\s+(?:un)?signed\s+int},
	qr{int\s+(?:un)?signed\s+long},
	qr{int\s+(?:un)?signed},
	qr{int\s+long\s+long\s+(?:un)?signed},
	qr{long\s+long\s+int\s+(?:un)?signed},
	qr{long\s+long\s+(?:un)?signed\s+int},
	qr{long\s+long\s+(?:un)?signed},
	qr{long\s+(?:un)?signed},
);
our @typeList = (
	qr{void},
	qr{(?:(?:un)?signed\s+)?char},
	qr{(?:(?:un)?signed\s+)?short\s+int},
	qr{(?:(?:un)?signed\s+)?short},
	qr{(?:(?:un)?signed\s+)?int},
	qr{(?:(?:un)?signed\s+)?long\s+int},
	qr{(?:(?:un)?signed\s+)?long\s+long\s+int},
	qr{(?:(?:un)?signed\s+)?long\s+long},
	qr{(?:(?:un)?signed\s+)?long},
	qr{(?:un)?signed},
	qr{float},
	qr{double},
	qr{bool},
	qr{struct\s+$Ident},
	qr{union\s+$Ident},
	qr{enum\s+$Ident},
	qr{${Ident}_t},
	qr{${Ident}_handler},
	qr{${Ident}_handler_fn},
	@typeListMisordered,
);
our $C90_int_types = qr{(?x:
	long\s+long\s+int\s+(?:un)?signed|
	long\s+long\s+(?:un)?signed\s+int|
	long\s+long\s+(?:un)?signed|
	(?:(?:un)?signed\s+)?long\s+long\s+int|
	(?:(?:un)?signed\s+)?long\s+long|
	int\s+long\s+long\s+(?:un)?signed|
	int\s+(?:(?:un)?signed\s+)?long\s+long|
	long\s+int\s+(?:un)?signed|
	long\s+(?:un)?signed\s+int|
	long\s+(?:un)?signed|
	(?:(?:un)?signed\s+)?long\s+int|
	(?:(?:un)?signed\s+)?long|
	int\s+long\s+(?:un)?signed|
	int\s+(?:(?:un)?signed\s+)?long|
	int\s+(?:un)?signed|
	(?:(?:un)?signed\s+)?int
)};
our @typeListFile = ();
our @typeListWithAttr = (
	@typeList,
	qr{struct\s+$InitAttribute\s+$Ident},
	qr{union\s+$InitAttribute\s+$Ident},
);
our @modifierList = (
	qr{fastcall},
);
our @modifierListFile = ();
our @mode_permission_funcs = (
	["module_param", 3],
	["module_param_(?:array|named|string)", 4],
	["module_param_array_named", 5],
	["debugfs_create_(?:file|u8|u16|u32|u64|x8|x16|x32|x64|size_t|atomic_t|bool|blob|regset32|u32_array)", 2],
	["proc_create(?:_data|)", 2],
	["(?:CLASS|DEVICE|SENSOR|SENSOR_DEVICE|IIO_DEVICE)_ATTR", 2],
	["IIO_DEV_ATTR_[A-Z_]+", 1],
	["SENSOR_(?:DEVICE_|)ATTR_2", 2],
	["SENSOR_TEMPLATE(?:_2|)", 3],
	["__ATTR", 2],
);
my $word_pattern = '\b[A-Z]?[a-z]{2,}\b';
our $mode_perms_search = "";
foreach my $entry (@mode_permission_funcs) {
	$mode_perms_search .= '|' if ($mode_perms_search ne "");
	$mode_perms_search .= $entry->[0];
}
$mode_perms_search = "(?:${mode_perms_search})";
our %deprecated_apis = (
	"synchronize_rcu_bh"			=> "synchronize_rcu",
	"synchronize_rcu_bh_expedited"		=> "synchronize_rcu_expedited",
	"call_rcu_bh"				=> "call_rcu",
	"rcu_barrier_bh"			=> "rcu_barrier",
	"synchronize_sched"			=> "synchronize_rcu",
	"synchronize_sched_expedited"		=> "synchronize_rcu_expedited",
	"call_rcu_sched"			=> "call_rcu",
	"rcu_barrier_sched"			=> "rcu_barrier",
	"get_state_synchronize_sched"		=> "get_state_synchronize_rcu",
	"cond_synchronize_sched"		=> "cond_synchronize_rcu",
	"kmap"					=> "kmap_local_page",
	"kunmap"				=> "kunmap_local",
	"kmap_atomic"				=> "kmap_local_page",
	"kunmap_atomic"				=> "kunmap_local",
);
our $deprecated_apis_search = "";
foreach my $entry (keys %deprecated_apis) {
	$deprecated_apis_search .= '|' if ($deprecated_apis_search ne "");
	$deprecated_apis_search .= $entry;
}
$deprecated_apis_search = "(?:${deprecated_apis_search})";
our $mode_perms_world_writable = qr{
	S_IWUGO		|
	S_IWOTH		|
	S_IRWXUGO	|
	S_IALLUGO	|
	0[0-7][0-7][2367]
}x;
our %mode_permission_string_types = (
	"S_IRWXU" => 0700,
	"S_IRUSR" => 0400,
	"S_IWUSR" => 0200,
	"S_IXUSR" => 0100,
	"S_IRWXG" => 0070,
	"S_IRGRP" => 0040,
	"S_IWGRP" => 0020,
	"S_IXGRP" => 0010,
	"S_IRWXO" => 0007,
	"S_IROTH" => 0004,
	"S_IWOTH" => 0002,
	"S_IXOTH" => 0001,
	"S_IRWXUGO" => 0777,
	"S_IRUGO" => 0444,
	"S_IWUGO" => 0222,
	"S_IXUGO" => 0111,
);
our $mode_perms_string_search = "";
foreach my $entry (keys %mode_permission_string_types) {
	$mode_perms_string_search .= '|' if ($mode_perms_string_search ne "");
	$mode_perms_string_search .= $entry;
}
our $single_mode_perms_string_search = "(?:${mode_perms_string_search})";
our $multi_mode_perms_string_search = qr{
	${single_mode_perms_string_search}
	(?:\s*\|\s*${single_mode_perms_string_search})*
}x;
sub perms_to_octal {
	my ($string) = @_;
	return trim($string) if ($string =~ /^\s*0[0-7]{3,3}\s*$/);
	my $val = "";
	my $oval = "";
	my $to = 0;
	my $curpos = 0;
	my $lastpos = 0;
	while ($string =~ /\b(($single_mode_perms_string_search)\b(?:\s*\|\s*)?\s*)/g) {
		$curpos = pos($string);
		my $match = $2;
		my $omatch = $1;
		last if ($lastpos > 0 && ($curpos - length($omatch) != $lastpos));
		$lastpos = $curpos;
		$to |= $mode_permission_string_types{$match};
		$val .= '\s*\|\s*' if ($val ne "");
		$val .= $match;
		$oval .= $omatch;
	}
	$oval =~ s/^\s*\|\s*//;
	$oval =~ s/\s*\|\s*$//;
	return sprintf("%04o", $to);
}
our $allowed_asm_includes = qr{(?x:
	irq|
	memory|
	time|
	reboot
)};
my $misspellings;
my %spelling_fix;
if (open(my $spelling, '<', $spelling_file)) {
	while (<$spelling>) {
		my $line = $_;
		$line =~ s/\s*\n?$//g;
		$line =~ s/^\s*//g;
		next if ($line =~ m/^\s*
		next if ($line =~ m/^\s*$/);
		my ($suspect, $fix) = split(/\|\|/, $line);
		$spelling_fix{$suspect} = $fix;
	}
	close($spelling);
} else {
	warn "No typos will be found - file '$spelling_file': $!\n";
}
if ($codespell) {
	if (open(my $spelling, '<', $codespellfile)) {
		while (<$spelling>) {
			my $line = $_;
			$line =~ s/\s*\n?$//g;
			$line =~ s/^\s*//g;
			next if ($line =~ m/^\s*
			next if ($line =~ m/^\s*$/);
			next if ($line =~ m/, disabled/i);
			$line =~ s/,.*$//;
			my ($suspect, $fix) = split(/->/, $line);
			$spelling_fix{$suspect} = $fix;
		}
		close($spelling);
	} else {
		warn "No codespell typos will be found - file '$codespellfile': $!\n";
	}
}
$misspellings = join("|", sort keys %spelling_fix) if keys %spelling_fix;
sub read_words {
	my ($wordsRef, $file) = @_;
	if (open(my $words, '<', $file)) {
		while (<$words>) {
			my $line = $_;
			$line =~ s/\s*\n?$//g;
			$line =~ s/^\s*//g;
			next if ($line =~ m/^\s*
			next if ($line =~ m/^\s*$/);
			if ($line =~ /\s/) {
				print("$file: '$line' invalid - ignored\n");
				next;
			}
			$$wordsRef .= '|' if (defined $$wordsRef);
			$$wordsRef .= $line;
		}
		close($file);
		return 1;
	}
	return 0;
}
my $const_structs;
if (show_type("CONST_STRUCT")) {
	read_words(\$const_structs, $conststructsfile)
	    or warn "No structs that should be const will be found - file '$conststructsfile': $!\n";
}
if (defined($typedefsfile)) {
	my $typeOtherTypedefs;
	read_words(\$typeOtherTypedefs, $typedefsfile)
	    or warn "No additional types will be considered - file '$typedefsfile': $!\n";
	$typeTypedefs .= '|' . $typeOtherTypedefs if (defined $typeOtherTypedefs);
}
sub build_types {
	my $mods = "(?x:  \n" . join("|\n  ", (@modifierList, @modifierListFile)) . "\n)";
	my $all = "(?x:  \n" . join("|\n  ", (@typeList, @typeListFile)) . "\n)";
	my $Misordered = "(?x:  \n" . join("|\n  ", @typeListMisordered) . "\n)";
	my $allWithAttr = "(?x:  \n" . join("|\n  ", @typeListWithAttr) . "\n)";
	$Modifier	= qr{(?:$Attribute|$Sparse|$mods)};
	$BasicType	= qr{
				(?:$typeTypedefs\b)|
				(?:${all}\b)
		}x;
	$NonptrType	= qr{
			(?:$Modifier\s+|const\s+)*
			(?:
				(?:typeof|__typeof__)\s*\([^\)]*\)|
				(?:$typeTypedefs\b)|
				(?:${all}\b)
			)
			(?:\s+$Modifier|\s+const)*
		  }x;
	$NonptrTypeMisordered	= qr{
			(?:$Modifier\s+|const\s+)*
			(?:
				(?:${Misordered}\b)
			)
			(?:\s+$Modifier|\s+const)*
		  }x;
	$NonptrTypeWithAttr	= qr{
			(?:$Modifier\s+|const\s+)*
			(?:
				(?:typeof|__typeof__)\s*\([^\)]*\)|
				(?:$typeTypedefs\b)|
				(?:${allWithAttr}\b)
			)
			(?:\s+$Modifier|\s+const)*
		  }x;
	$Type	= qr{
			$NonptrType
			(?:(?:\s|\*|\[\])+\s*const|(?:\s|\*\s*(?:const\s*)?|\[\])+|(?:\s*\[\s*\])+){0,4}
			(?:\s+$Inline|\s+$Modifier)*
		  }x;
	$TypeMisordered	= qr{
			$NonptrTypeMisordered
			(?:(?:\s|\*|\[\])+\s*const|(?:\s|\*\s*(?:const\s*)?|\[\])+|(?:\s*\[\s*\])+){0,4}
			(?:\s+$Inline|\s+$Modifier)*
		  }x;
	$Declare	= qr{(?:$Storage\s+(?:$Inline\s+)?)?$Type};
	$DeclareMisordered	= qr{(?:$Storage\s+(?:$Inline\s+)?)?$TypeMisordered};
}
build_types();
our $Typecast	= qr{\s*(\(\s*$NonptrType\s*\)){0,1}\s*};
our $balanced_parens = qr/(\((?:[^\(\)]++|(?-1))*\))/;
our $LvalOrFunc	= qr{((?:[\&\*]\s*)?$Lval)\s*($balanced_parens{0,1})\s*};
our $FuncArg = qr{$Typecast{0,1}($LvalOrFunc|$Constant|$String)};
our $declaration_macros = qr{(?x:
	(?:$Storage\s+)?(?:[A-Z_][A-Z0-9]*_){0,2}(?:DEFINE|DECLARE)(?:_[A-Z0-9]+){1,6}\s*\(|
	(?:$Storage\s+)?[HLP]?LIST_HEAD\s*\(|
	(?:SKCIPHER_REQUEST|SHASH_DESC|AHASH_REQUEST)_ON_STACK\s*\(|
	(?:$Storage\s+)?(?:XA_STATE|XA_STATE_ORDER)\s*\(
)};
our %allow_repeated_words = (
	add => '',
	added => '',
	bad => '',
	be => '',
);
sub deparenthesize {
	my ($string) = @_;
	return "" if (!defined($string));
	while ($string =~ /^\s*\(.*\)\s*$/) {
		$string =~ s@^\s*\(\s*@@;
		$string =~ s@\s*\)\s*$@@;
	}
	$string =~ s@\s+@ @g;
	return $string;
}
sub seed_camelcase_file {
	my ($file) = @_;
	return if (!(-f $file));
	local $/;
	open(my $include_file, '<', "$file")
	    or warn "$P: Can't read '$file' $!\n";
	my $text = <$include_file>;
	close($include_file);
	my @lines = split('\n', $text);
	foreach my $line (@lines) {
		next if ($line !~ /(?:[A-Z][a-z]|[a-z][A-Z])/);
		if ($line =~ /^[ \t]*(?:
			$camelcase{$1} = 1;
		} elsif ($line =~ /^\s*$Declare\s+(\w*(?:[A-Z][a-z]|[a-z][A-Z])\w*)\s*[\(\[,;]/) {
			$camelcase{$1} = 1;
		} elsif ($line =~ /^\s*(?:union|struct|enum)\s+(\w*(?:[A-Z][a-z]|[a-z][A-Z])\w*)\s*[;\{]/) {
			$camelcase{$1} = 1;
		}
	}
}
our %maintained_status = ();
sub is_maintained_obsolete {
	my ($filename) = @_;
	return 0 if (!$tree || !(-e "$root/scripts/get_maintainer.pl"));
	if (!exists($maintained_status{$filename})) {
		$maintained_status{$filename} = `perl $root/scripts/get_maintainer.pl --status --nom --nol --nogit --nogit-fallback -f $filename 2>&1`;
	}
	return $maintained_status{$filename} =~ /obsolete/i;
}
sub is_SPDX_License_valid {
	my ($license) = @_;
	return 1 if (!$tree || which("python3") eq "" || !(-x "$root/scripts/spdxcheck.py") || !(-e "$gitroot"));
	my $root_path = abs_path($root);
	my $status = `cd "$root_path"; echo "$license" | scripts/spdxcheck.py -`;
	return 0 if ($status ne "");
	return 1;
}
my $camelcase_seeded = 0;
sub seed_camelcase_includes {
	return if ($camelcase_seeded);
	my $files;
	my $camelcase_cache = "";
	my @include_files = ();
	$camelcase_seeded = 1;
	if (-e "$gitroot") {
		my $git_last_include_commit = `${git_command} log --no-merges --pretty=format:"%h%n" -1 -- include`;
		chomp $git_last_include_commit;
		$camelcase_cache = ".checkpatch-camelcase.git.$git_last_include_commit";
	} else {
		my $last_mod_date = 0;
		$files = `find $root/include -name "*.h"`;
		@include_files = split('\n', $files);
		foreach my $file (@include_files) {
			my $date = POSIX::strftime("%Y%m%d%H%M",
						   localtime((stat $file)[9]));
			$last_mod_date = $date if ($last_mod_date < $date);
		}
		$camelcase_cache = ".checkpatch-camelcase.date.$last_mod_date";
	}
	if ($camelcase_cache ne "" && -f $camelcase_cache) {
		open(my $camelcase_file, '<', "$camelcase_cache")
		    or warn "$P: Can't read '$camelcase_cache' $!\n";
		while (<$camelcase_file>) {
			chomp;
			$camelcase{$_} = 1;
		}
		close($camelcase_file);
		return;
	}
	if (-e "$gitroot") {
		$files = `${git_command} ls-files "include/*.h"`;
		@include_files = split('\n', $files);
	}
	foreach my $file (@include_files) {
		seed_camelcase_file($file);
	}
	if ($camelcase_cache ne "") {
		unlink glob ".checkpatch-camelcase.*";
		open(my $camelcase_file, '>', "$camelcase_cache")
		    or warn "$P: Can't write '$camelcase_cache' $!\n";
		foreach (sort { lc($a) cmp lc($b) } keys(%camelcase)) {
			print $camelcase_file ("$_\n");
		}
		close($camelcase_file);
	}
}
sub git_is_single_file {
	my ($filename) = @_;
	return 0 if ((which("git") eq "") || !(-e "$gitroot"));
	my $output = `${git_command} ls-files -- $filename 2>/dev/null`;
	my $count = $output =~ tr/\n//;
	return $count eq 1 && $output =~ m{^${filename}$};
}
sub git_commit_info {
	my ($commit, $id, $desc) = @_;
	return ($id, $desc) if ((which("git") eq "") || !(-e "$gitroot"));
	my $output = `${git_command} log --no-color --format='%H %s' -1 $commit 2>&1`;
	$output =~ s/^\s*//gm;
	my @lines = split("\n", $output);
	return ($id, $desc) if ($
	if ($lines[0] =~ /^error: short SHA1 $commit is ambiguous/) {
	} elsif ($lines[0] =~ /^fatal: ambiguous argument '$commit': unknown revision or path not in the working tree\./ ||
		 $lines[0] =~ /^fatal: bad object $commit/) {
		$id = undef;
	} else {
		$id = substr($lines[0], 0, 12);
		$desc = substr($lines[0], 41);
	}
	return ($id, $desc);
}
$chk_signoff = 0 if ($file);
my @rawlines = ();
my @lines = ();
my @fixed = ();
my @fixed_inserted = ();
my @fixed_deleted = ();
my $fixlinenr = -1;
die "$P: No git repository found\n" if ($git && !-e "$gitroot");
if ($git) {
	my @commits = ();
	foreach my $commit_expr (@ARGV) {
		my $git_range;
		if ($commit_expr =~ m/^(.*)-(\d+)$/) {
			$git_range = "-$2 $1";
		} elsif ($commit_expr =~ m/\.\./) {
			$git_range = "$commit_expr";
		} else {
			$git_range = "-1 $commit_expr";
		}
		my $lines = `${git_command} log --no-color --no-merges --pretty=format:'%H %s' $git_range`;
		foreach my $line (split(/\n/, $lines)) {
			$line =~ /^([0-9a-fA-F]{40,40}) (.*)$/;
			next if (!defined($1) || !defined($2));
			my $sha1 = $1;
			my $subject = $2;
			unshift(@commits, $sha1);
			$git_commits{$sha1} = $subject;
		}
	}
	die "$P: no git commits after extraction!\n" if (@commits == 0);
	@ARGV = @commits;
}
my $vname;
$allow_c99_comments = !defined $ignore_type{"C99_COMMENT_TOLERANCE"};
for my $filename (@ARGV) {
	my $FILE;
	my $is_git_file = git_is_single_file($filename);
	my $oldfile = $file;
	$file = 1 if ($is_git_file);
	if ($git) {
		open($FILE, '-|', "git format-patch -M --stdout -1 $filename") ||
			die "$P: $filename: git format-patch failed - $!\n";
	} elsif ($file) {
		open($FILE, '-|', "diff -u /dev/null $filename") ||
			die "$P: $filename: diff failed - $!\n";
	} elsif ($filename eq '-') {
		open($FILE, '<&STDIN');
	} else {
		open($FILE, '<', "$filename") ||
			die "$P: $filename: open failed - $!\n";
	}
	if ($filename eq '-') {
		$vname = 'Your patch';
	} elsif ($git) {
		$vname = "Commit " . substr($filename, 0, 12) . ' ("' . $git_commits{$filename} . '")';
	} else {
		$vname = $filename;
	}
	while (<$FILE>) {
		chomp;
		push(@rawlines, $_);
		$vname = qq("$1") if ($filename eq '-' && $_ =~ m/^Subject:\s+(.+)/i);
	}
	close($FILE);
	if ($
		print '-' x length($vname) . "\n";
		print "$vname\n";
		print '-' x length($vname) . "\n";
	}
	if (!process($filename)) {
		$exit = 1;
	}
	@rawlines = ();
	@lines = ();
	@fixed = ();
	@fixed_inserted = ();
	@fixed_deleted = ();
	$fixlinenr = -1;
	@modifierListFile = ();
	@typeListFile = ();
	build_types();
	$file = $oldfile if ($is_git_file);
}
if (!$quiet) {
	hash_show_words(\%use_type, "Used");
	hash_show_words(\%ignore_type, "Ignored");
	if (!$perl_version_ok) {
		print << "EOM"
NOTE: perl $^V is not modern enough to detect all possible issues.
      An upgrade to at least perl $minimum_perl_version is suggested.
EOM
	}
	if ($exit) {
		print << "EOM"
NOTE: If any of the errors are false positives, please report
      them to the maintainer, see CHECKPATCH in MAINTAINERS.
EOM
	}
}
exit($exit);
sub top_of_kernel_tree {
	my ($root) = @_;
	my @tree_check = (
		"COPYING", "CREDITS", "Kbuild", "MAINTAINERS", "Makefile",
		"README", "Documentation", "arch", "include", "drivers",
		"fs", "init", "ipc", "kernel", "lib", "scripts",
	);
	foreach my $check (@tree_check) {
		if (! -e $root . '/' . $check) {
			return 0;
		}
	}
	return 1;
}
sub parse_email {
	my ($formatted_email) = @_;
	my $name = "";
	my $quoted = "";
	my $name_comment = "";
	my $address = "";
	my $comment = "";
	if ($formatted_email =~ /^(.*)<(\S+\@\S+)>(.*)$/) {
		$name = $1;
		$address = $2;
		$comment = $3 if defined $3;
	} elsif ($formatted_email =~ /^\s*<(\S+\@\S+)>(.*)$/) {
		$address = $1;
		$comment = $2 if defined $2;
	} elsif ($formatted_email =~ /(\S+\@\S+)(.*)$/) {
		$address = $1;
		$comment = $2 if defined $2;
		$formatted_email =~ s/\Q$address\E.*$//;
		$name = $formatted_email;
		$name = trim($name);
		$name =~ s/^\"|\"$//g;
		if ($name ne "" && $address !~ /^<[^>]+>$/) {
			$name = "";
			$address = "";
			$comment = "";
		}
	}
	if ($name =~ s/\"(.+)\"//) {
		$quoted = $1;
	}
	while ($name =~ s/\s*($balanced_parens)\s*/ /) {
		$name_comment .= trim($1);
	}
	$name =~ s/^[ \"]+|[ \"]+$//g;
	$name = trim("$quoted $name");
	$address = trim($address);
	$address =~ s/^\<|\>$//g;
	$comment = trim($comment);
	if ($name =~ /[^\w \-]/i) { 
		$name =~ s/(?<!\\)"/\\"/g; 
		$name = "\"$name\"";
	}
	return ($name, $name_comment, $address, $comment);
}
sub format_email {
	my ($name, $name_comment, $address, $comment) = @_;
	my $formatted_email;
	$name =~ s/^[ \"]+|[ \"]+$//g;
	$address = trim($address);
	$address =~ s/(?:\.|\,|\")+$//; 
	if ($name =~ /[^\w \-]/i) { 
		$name =~ s/(?<!\\)"/\\"/g; 
		$name = "\"$name\"";
	}
	$name_comment = trim($name_comment);
	$name_comment = " $name_comment" if ($name_comment ne "");
	$comment = trim($comment);
	$comment = " $comment" if ($comment ne "");
	if ("$name" eq "") {
		$formatted_email = "$address";
	} else {
		$formatted_email = "$name$name_comment <$address>";
	}
	$formatted_email .= "$comment";
	return $formatted_email;
}
sub reformat_email {
	my ($email) = @_;
	my ($email_name, $name_comment, $email_address, $comment) = parse_email($email);
	return format_email($email_name, $name_comment, $email_address, $comment);
}
sub same_email_addresses {
	my ($email1, $email2) = @_;
	my ($email1_name, $name1_comment, $email1_address, $comment1) = parse_email($email1);
	my ($email2_name, $name2_comment, $email2_address, $comment2) = parse_email($email2);
	return $email1_name eq $email2_name &&
	       $email1_address eq $email2_address &&
	       $name1_comment eq $name2_comment &&
	       $comment1 eq $comment2;
}
sub which {
	my ($bin) = @_;
	foreach my $path (split(/:/, $ENV{PATH})) {
		if (-e "$path/$bin") {
			return "$path/$bin";
		}
	}
	return "";
}
sub which_conf {
	my ($conf) = @_;
	foreach my $path (split(/:/, ".:$ENV{HOME}:.scripts")) {
		if (-e "$path/$conf") {
			return "$path/$conf";
		}
	}
	return "";
}
sub expand_tabs {
	my ($str) = @_;
	my $res = '';
	my $n = 0;
	for my $c (split(//, $str)) {
		if ($c eq "\t") {
			$res .= ' ';
			$n++;
			for (; ($n % $tabsize) != 0; $n++) {
				$res .= ' ';
			}
			next;
		}
		$res .= $c;
		$n++;
	}
	return $res;
}
sub copy_spacing {
	(my $res = shift) =~ tr/\t/ /c;
	return $res;
}
sub line_stats {
	my ($line) = @_;
	$line =~ s/^.//;
	$line = expand_tabs($line);
	my ($white) = ($line =~ /^(\s*)/);
	return (length($line), length($white));
}
my $sanitise_quote = '';
sub sanitise_line_reset {
	my ($in_comment) = @_;
	if ($in_comment) {
		$sanitise_quote = '*/';
	} else {
		$sanitise_quote = '';
	}
}
sub sanitise_line {
	my ($line) = @_;
	my $res = '';
	my $l = '';
	my $qlen = 0;
	my $off = 0;
	my $c;
	$res = substr($line, 0, 1);
	for ($off = 1; $off < length($line); $off++) {
		$c = substr($line, $off, 1);
		if ($sanitise_quote eq '' && substr($line, $off, 2) eq '/*') {
			$sanitise_quote = '*/';
			substr($res, $off, 2, "$;$;");
			$off++;
			next;
		}
		if ($sanitise_quote eq '*/' && substr($line, $off, 2) eq '*/') {
			$sanitise_quote = '';
			substr($res, $off, 2, "$;$;");
			$off++;
			next;
		}
		if ($sanitise_quote eq '' && substr($line, $off, 2) eq '//') {
			$sanitise_quote = '//';
			substr($res, $off, 2, $sanitise_quote);
			$off++;
			next;
		}
		if (($sanitise_quote eq "'" || $sanitise_quote eq '"') &&
		    $c eq "\\") {
			substr($res, $off, 2, 'XX');
			$off++;
			next;
		}
		if ($c eq "'" || $c eq '"') {
			if ($sanitise_quote eq '') {
				$sanitise_quote = $c;
				substr($res, $off, 1, $c);
				next;
			} elsif ($sanitise_quote eq $c) {
				$sanitise_quote = '';
			}
		}
		if ($off != 0 && $sanitise_quote eq '*/' && $c ne "\t") {
			substr($res, $off, 1, $;);
		} elsif ($off != 0 && $sanitise_quote eq '//' && $c ne "\t") {
			substr($res, $off, 1, $;);
		} elsif ($off != 0 && $sanitise_quote && $c ne "\t") {
			substr($res, $off, 1, 'X');
		} else {
			substr($res, $off, 1, $c);
		}
	}
	if ($sanitise_quote eq '//') {
		$sanitise_quote = '';
	}
	if ($res =~ /^.\s*\
		my $clean = 'X' x length($1);
		$res =~ s@\<.*\>@<$clean>@;
	} elsif ($res =~ /^.\s*\
		my $clean = 'X' x length($1);
		$res =~ s@(\
	}
	if ($allow_c99_comments && $res =~ m@(//.*$)@) {
		my $match = $1;
		$res =~ s/\Q$match\E/"$;" x length($match)/e;
	}
	return $res;
}
sub get_quoted_string {
	my ($line, $rawline) = @_;
	return "" if (!defined($line) || !defined($rawline));
	return "" if ($line !~ m/($String)/g);
	return substr($rawline, $-[0], $+[0] - $-[0]);
}
sub ctx_statement_block {
	my ($linenr, $remain, $off) = @_;
	my $line = $linenr - 1;
	my $blk = '';
	my $soff = $off;
	my $coff = $off - 1;
	my $coff_set = 0;
	my $loff = 0;
	my $type = '';
	my $level = 0;
	my @stack = ();
	my $p;
	my $c;
	my $len = 0;
	my $remainder;
	while (1) {
		@stack = (['', 0]) if ($
		if ($off >= $len) {
			for (; $remain > 0; $line++) {
				last if (!defined $lines[$line]);
				next if ($lines[$line] =~ /^-/);
				$remain--;
				$loff = $len;
				$blk .= $lines[$line] . "\n";
				$len = length($blk);
				$line++;
				last;
			}
			if ($off >= $len) {
				last;
			}
			if ($level == 0 && substr($blk, $off) =~ /^.\s*
				$level++;
				$type = '
			}
		}
		$p = $c;
		$c = substr($blk, $off, 1);
		$remainder = substr($blk, $off);
		if ($remainder =~ /^
			push(@stack, [ $type, $level ]);
		} elsif ($remainder =~ /^
			($type, $level) = @{$stack[$
		} elsif ($remainder =~ /^
			($type, $level) = @{pop(@stack)};
		}
		if ($level == 0 && $c eq ';') {
			last;
		}
		if ($level == 0 && $coff_set == 0 &&
				(!defined($p) || $p =~ /(?:\s|\}|\+)/) &&
				$remainder =~ /^(else)(?:\s|{)/ &&
				$remainder !~ /^else\s+if\b/) {
			$coff = $off + length($1) - 1;
			$coff_set = 1;
		}
		if (($type eq '' || $type eq '(') && $c eq '(') {
			$level++;
			$type = '(';
		}
		if ($type eq '(' && $c eq ')') {
			$level--;
			$type = ($level != 0)? '(' : '';
			if ($level == 0 && $coff < $soff) {
				$coff = $off;
				$coff_set = 1;
			}
		}
		if (($type eq '' || $type eq '{') && $c eq '{') {
			$level++;
			$type = '{';
		}
		if ($type eq '{' && $c eq '}') {
			$level--;
			$type = ($level != 0)? '{' : '';
			if ($level == 0) {
				if (substr($blk, $off + 1, 1) eq ';') {
					$off++;
				}
				last;
			}
		}
		if ($type eq '
			$level--;
			$type = '';
			$off++;
			last;
		}
		$off++;
	}
	if ($off == $len) {
		$loff = $len + 1;
		$line++;
		$remain--;
	}
	my $statement = substr($blk, $soff, $off - $soff + 1);
	my $condition = substr($blk, $soff, $coff - $soff + 1);
	return ($statement, $condition,
			$line, $remain + 1, $off - $loff + 1, $level);
}
sub statement_lines {
	my ($stmt) = @_;
	$stmt =~ s/(^|\n)./$1/g;
	$stmt =~ s/^\s*//;
	$stmt =~ s/\s*$//;
	my @stmt_lines = ($stmt =~ /\n/g);
	return $
}
sub statement_rawlines {
	my ($stmt) = @_;
	my @stmt_lines = ($stmt =~ /\n/g);
	return $
}
sub statement_block_size {
	my ($stmt) = @_;
	$stmt =~ s/(^|\n)./$1/g;
	$stmt =~ s/^\s*{//;
	$stmt =~ s/}\s*$//;
	$stmt =~ s/^\s*//;
	$stmt =~ s/\s*$//;
	my @stmt_lines = ($stmt =~ /\n/g);
	my @stmt_statements = ($stmt =~ /;/g);
	my $stmt_lines = $
	my $stmt_statements = $
	if ($stmt_lines > $stmt_statements) {
		return $stmt_lines;
	} else {
		return $stmt_statements;
	}
}
sub ctx_statement_full {
	my ($linenr, $remain, $off) = @_;
	my ($statement, $condition, $level);
	my (@chunks);
	($statement, $condition, $linenr, $remain, $off, $level) =
				ctx_statement_block($linenr, $remain, $off);
	push(@chunks, [ $condition, $statement ]);
	if (!($remain > 0 && $condition =~ /^\s*(?:\n[+-])?\s*(?:if|else|do)\b/s)) {
		return ($level, $linenr, @chunks);
	}
	for (;;) {
		($statement, $condition, $linenr, $remain, $off, $level) =
				ctx_statement_block($linenr, $remain, $off);
		last if (!($remain > 0 && $condition =~ /^(?:\s*\n[+-])*\s*(?:else|do)\b/s));
		push(@chunks, [ $condition, $statement ]);
	}
	return ($level, $linenr, @chunks);
}
sub ctx_block_get {
	my ($linenr, $remain, $outer, $open, $close, $off) = @_;
	my $line;
	my $start = $linenr - 1;
	my $blk = '';
	my @o;
	my @c;
	my @res = ();
	my $level = 0;
	my @stack = ($level);
	for ($line = $start; $remain > 0; $line++) {
		next if ($rawlines[$line] =~ /^-/);
		$remain--;
		$blk .= $rawlines[$line];
		if ($lines[$line] =~ /^.\s*
			push(@stack, $level);
		} elsif ($lines[$line] =~ /^.\s*
			$level = $stack[$
		} elsif ($lines[$line] =~ /^.\s*
			$level = pop(@stack);
		}
		foreach my $c (split(//, $lines[$line])) {
			if ($off > 0) {
				$off--;
				next;
			}
			if ($c eq $close && $level > 0) {
				$level--;
				last if ($level == 0);
			} elsif ($c eq $open) {
				$level++;
			}
		}
		if (!$outer || $level <= 1) {
			push(@res, $rawlines[$line]);
		}
		last if ($level == 0);
	}
	return ($level, @res);
}
sub ctx_block_outer {
	my ($linenr, $remain) = @_;
	my ($level, @r) = ctx_block_get($linenr, $remain, 1, '{', '}', 0);
	return @r;
}
sub ctx_block {
	my ($linenr, $remain) = @_;
	my ($level, @r) = ctx_block_get($linenr, $remain, 0, '{', '}', 0);
	return @r;
}
sub ctx_statement {
	my ($linenr, $remain, $off) = @_;
	my ($level, @r) = ctx_block_get($linenr, $remain, 0, '(', ')', $off);
	return @r;
}
sub ctx_block_level {
	my ($linenr, $remain) = @_;
	return ctx_block_get($linenr, $remain, 0, '{', '}', 0);
}
sub ctx_statement_level {
	my ($linenr, $remain, $off) = @_;
	return ctx_block_get($linenr, $remain, 0, '(', ')', $off);
}
sub ctx_locate_comment {
	my ($first_line, $end_line) = @_;
	my ($current_comment) = ($rawlines[$end_line - 1] =~ m@^\+.*(//.*$)@);
	return $current_comment if (defined $current_comment);
	($current_comment) = ($rawlines[$end_line - 2] =~ m@^[\+ ].*(//.*$)@);
	return $current_comment if (defined $current_comment);
	($current_comment) = ($rawlines[$end_line] =~ m@^[\+ ].*(//.*$)@);
	return $current_comment if (defined $current_comment);
	($current_comment) = ($rawlines[$end_line - 1] =~ m@.*(/\*.*\*/)\s*(?:\\\s*)?$@);
	return $current_comment if (defined $current_comment);
	my $in_comment = 0;
	$current_comment = '';
	for (my $linenr = $first_line; $linenr < $end_line; $linenr++) {
		my $line = $rawlines[$linenr - 1];
		if ($linenr == $first_line and $line =~ m@^.\s*\*@) {
			$in_comment = 1;
		}
		if ($line =~ m@/\*@) {
			$in_comment = 1;
		}
		if (!$in_comment && $current_comment ne '') {
			$current_comment = '';
		}
		$current_comment .= $line . "\n" if ($in_comment);
		if ($line =~ m@\*/@) {
			$in_comment = 0;
		}
	}
	chomp($current_comment);
	return($current_comment);
}
sub ctx_has_comment {
	my ($first_line, $end_line) = @_;
	my $cmt = ctx_locate_comment($first_line, $end_line);
	return ($cmt ne '');
}
sub raw_line {
	my ($linenr, $cnt) = @_;
	my $offset = $linenr - 1;
	$cnt++;
	my $line;
	while ($cnt) {
		$line = $rawlines[$offset++];
		next if (defined($line) && $line =~ /^-/);
		$cnt--;
	}
	return $line;
}
sub get_stat_real {
	my ($linenr, $lc) = @_;
	my $stat_real = raw_line($linenr, 0);
	for (my $count = $linenr + 1; $count <= $lc; $count++) {
		$stat_real = $stat_real . "\n" . raw_line($count, 0);
	}
	return $stat_real;
}
sub get_stat_here {
	my ($linenr, $cnt, $here) = @_;
	my $herectx = $here . "\n";
	for (my $n = 0; $n < $cnt; $n++) {
		$herectx .= raw_line($linenr, $n) . "\n";
	}
	return $herectx;
}
sub cat_vet {
	my ($vet) = @_;
	my ($res, $coded);
	$res = '';
	while ($vet =~ /([^[:cntrl:]]*)([[:cntrl:]]|$)/g) {
		$res .= $1;
		if ($2 ne '') {
			$coded = sprintf("^%c", unpack('C', $2) + 64);
			$res .= $coded;
		}
	}
	$res =~ s/$/\$/;
	return $res;
}
my $av_preprocessor = 0;
my $av_pending;
my @av_paren_type;
my $av_pend_colon;
sub annotate_reset {
	$av_preprocessor = 0;
	$av_pending = '_';
	@av_paren_type = ('E');
	$av_pend_colon = 'O';
}
sub annotate_values {
	my ($stream, $type) = @_;
	my $res;
	my $var = '_' x length($stream);
	my $cur = $stream;
	print "$stream\n" if ($dbg_values > 1);
	while (length($cur)) {
		@av_paren_type = ('E') if ($
		print " <" . join('', @av_paren_type) .
				"> <$type> <$av_pending>" if ($dbg_values > 1);
		if ($cur =~ /^(\s+)/o) {
			print "WS($1)\n" if ($dbg_values > 1);
			if ($1 =~ /\n/ && $av_preprocessor) {
				$type = pop(@av_paren_type);
				$av_preprocessor = 0;
			}
		} elsif ($cur =~ /^(\(\s*$Type\s*)\)/ && $av_pending eq '_') {
			print "CAST($1)\n" if ($dbg_values > 1);
			push(@av_paren_type, $type);
			$type = 'c';
		} elsif ($cur =~ /^($Type)\s*(?:$Ident|,|\)|\(|\s*$)/) {
			print "DECLARE($1)\n" if ($dbg_values > 1);
			$type = 'T';
		} elsif ($cur =~ /^($Modifier)\s*/) {
			print "MODIFIER($1)\n" if ($dbg_values > 1);
			$type = 'T';
		} elsif ($cur =~ /^(\
			print "DEFINE($1,$2)\n" if ($dbg_values > 1);
			$av_preprocessor = 1;
			push(@av_paren_type, $type);
			if ($2 ne '') {
				$av_pending = 'N';
			}
			$type = 'E';
		} elsif ($cur =~ /^(\
			print "UNDEF($1)\n" if ($dbg_values > 1);
			$av_preprocessor = 1;
			push(@av_paren_type, $type);
		} elsif ($cur =~ /^(\
			print "PRE_START($1)\n" if ($dbg_values > 1);
			$av_preprocessor = 1;
			push(@av_paren_type, $type);
			push(@av_paren_type, $type);
			$type = 'E';
		} elsif ($cur =~ /^(\
			print "PRE_RESTART($1)\n" if ($dbg_values > 1);
			$av_preprocessor = 1;
			push(@av_paren_type, $av_paren_type[$
			$type = 'E';
		} elsif ($cur =~ /^(\
			print "PRE_END($1)\n" if ($dbg_values > 1);
			$av_preprocessor = 1;
			pop(@av_paren_type);
			push(@av_paren_type, $type);
			$type = 'E';
		} elsif ($cur =~ /^(\\\n)/o) {
			print "PRECONT($1)\n" if ($dbg_values > 1);
		} elsif ($cur =~ /^(__attribute__)\s*\(?/o) {
			print "ATTR($1)\n" if ($dbg_values > 1);
			$av_pending = $type;
			$type = 'N';
		} elsif ($cur =~ /^(sizeof)\s*(\()?/o) {
			print "SIZEOF($1)\n" if ($dbg_values > 1);
			if (defined $2) {
				$av_pending = 'V';
			}
			$type = 'N';
		} elsif ($cur =~ /^(if|while|for)\b/o) {
			print "COND($1)\n" if ($dbg_values > 1);
			$av_pending = 'E';
			$type = 'N';
		} elsif ($cur =~/^(case)/o) {
			print "CASE($1)\n" if ($dbg_values > 1);
			$av_pend_colon = 'C';
			$type = 'N';
		} elsif ($cur =~/^(return|else|goto|typeof|__typeof__)\b/o) {
			print "KEYWORD($1)\n" if ($dbg_values > 1);
			$type = 'N';
		} elsif ($cur =~ /^(\()/o) {
			print "PAREN('$1')\n" if ($dbg_values > 1);
			push(@av_paren_type, $av_pending);
			$av_pending = '_';
			$type = 'N';
		} elsif ($cur =~ /^(\))/o) {
			my $new_type = pop(@av_paren_type);
			if ($new_type ne '_') {
				$type = $new_type;
				print "PAREN('$1') -> $type\n"
							if ($dbg_values > 1);
			} else {
				print "PAREN('$1')\n" if ($dbg_values > 1);
			}
		} elsif ($cur =~ /^($Ident)\s*\(/o) {
			print "FUNC($1)\n" if ($dbg_values > 1);
			$type = 'V';
			$av_pending = 'V';
		} elsif ($cur =~ /^($Ident\s*):(?:\s*\d+\s*(,|=|;))?/) {
			if (defined $2 && $type eq 'C' || $type eq 'T') {
				$av_pend_colon = 'B';
			} elsif ($type eq 'E') {
				$av_pend_colon = 'L';
			}
			print "IDENT_COLON($1,$type>$av_pend_colon)\n" if ($dbg_values > 1);
			$type = 'V';
		} elsif ($cur =~ /^($Ident|$Constant)/o) {
			print "IDENT($1)\n" if ($dbg_values > 1);
			$type = 'V';
		} elsif ($cur =~ /^($Assignment)/o) {
			print "ASSIGN($1)\n" if ($dbg_values > 1);
			$type = 'N';
		} elsif ($cur =~/^(;|{|})/) {
			print "END($1)\n" if ($dbg_values > 1);
			$type = 'E';
			$av_pend_colon = 'O';
		} elsif ($cur =~/^(,)/) {
			print "COMMA($1)\n" if ($dbg_values > 1);
			$type = 'C';
		} elsif ($cur =~ /^(\?)/o) {
			print "QUESTION($1)\n" if ($dbg_values > 1);
			$type = 'N';
		} elsif ($cur =~ /^(:)/o) {
			print "COLON($1,$av_pend_colon)\n" if ($dbg_values > 1);
			substr($var, length($res), 1, $av_pend_colon);
			if ($av_pend_colon eq 'C' || $av_pend_colon eq 'L') {
				$type = 'E';
			} else {
				$type = 'N';
			}
			$av_pend_colon = 'O';
		} elsif ($cur =~ /^(\[)/o) {
			print "CLOSE($1)\n" if ($dbg_values > 1);
			$type = 'N';
		} elsif ($cur =~ /^(-(?![->])|\+(?!\+)|\*|\&\&|\&)/o) {
			my $variant;
			print "OPV($1)\n" if ($dbg_values > 1);
			if ($type eq 'V') {
				$variant = 'B';
			} else {
				$variant = 'U';
			}
			substr($var, length($res), 1, $variant);
			$type = 'N';
		} elsif ($cur =~ /^($Operators)/o) {
			print "OP($1)\n" if ($dbg_values > 1);
			if ($1 ne '++' && $1 ne '--') {
				$type = 'N';
			}
		} elsif ($cur =~ /(^.)/o) {
			print "C($1)\n" if ($dbg_values > 1);
		}
		if (defined $1) {
			$cur = substr($cur, length($1));
			$res .= $type x length($1);
		}
	}
	return ($res, $var);
}
sub possible {
	my ($possible, $line) = @_;
	my $notPermitted = qr{(?:
		^(?:
			$Modifier|
			$Storage|
			$Type|
			DEFINE_\S+
		)$|
		^(?:
			goto|
			return|
			case|
			else|
			asm|__asm__|
			do|
			\
			\
		)(?:\s|$)|
		^(?:typedef|struct|enum)\b
	    )}x;
	warn "CHECK<$possible> ($line)\n" if ($dbg_possible > 2);
	if ($possible !~ $notPermitted) {
		$possible =~ s/\s*$Storage\s*//g;
		$possible =~ s/\s*$Sparse\s*//g;
		if ($possible =~ /^\s*$/) {
		} elsif ($possible =~ /\s/) {
			$possible =~ s/\s*$Type\s*//g;
			for my $modifier (split(' ', $possible)) {
				if ($modifier !~ $notPermitted) {
					warn "MODIFIER: $modifier ($possible) ($line)\n" if ($dbg_possible);
					push(@modifierListFile, $modifier);
				}
			}
		} else {
			warn "POSSIBLE: $possible ($line)\n" if ($dbg_possible);
			push(@typeListFile, $possible);
		}
		build_types();
	} else {
		warn "NOTPOSS: $possible ($line)\n" if ($dbg_possible > 1);
	}
}
my $prefix = '';
sub show_type {
	my ($type) = @_;
	$type =~ tr/[a-z]/[A-Z]/;
	return defined $use_type{$type} if (scalar keys %use_type > 0);
	return !defined $ignore_type{$type};
}
sub report {
	my ($level, $type, $msg) = @_;
	if (!show_type($type) ||
	    (defined $tst_only && $msg !~ /\Q$tst_only\E/)) {
		return 0;
	}
	my $output = '';
	if ($color) {
		if ($level eq 'ERROR') {
			$output .= RED;
		} elsif ($level eq 'WARNING') {
			$output .= YELLOW;
		} else {
			$output .= GREEN;
		}
	}
	$output .= $prefix . $level . ':';
	if ($show_types) {
		$output .= BLUE if ($color);
		$output .= "$type:";
	}
	$output .= RESET if ($color);
	$output .= ' ' . $msg . "\n";
	if ($showfile) {
		my @lines = split("\n", $output, -1);
		splice(@lines, 1, 1);
		$output = join("\n", @lines);
	}
	if ($terse) {
		$output = (split('\n', $output))[0] . "\n";
	}
	if ($verbose && exists($verbose_messages{$type}) &&
	    !exists($verbose_emitted{$type})) {
		$output .= $verbose_messages{$type} . "\n\n";
		$verbose_emitted{$type} = 1;
	}
	push(our @report, $output);
	return 1;
}
sub report_dump {
	our @report;
}
sub fixup_current_range {
	my ($lineRef, $offset, $length) = @_;
	if ($$lineRef =~ /^\@\@ -\d+,\d+ \+(\d+),(\d+) \@\@/) {
		my $o = $1;
		my $l = $2;
		my $no = $o + $offset;
		my $nl = $l + $length;
		$$lineRef =~ s/\+$o,$l \@\@/\+$no,$nl \@\@/;
	}
}
sub fix_inserted_deleted_lines {
	my ($linesRef, $insertedRef, $deletedRef) = @_;
	my $range_last_linenr = 0;
	my $delta_offset = 0;
	my $old_linenr = 0;
	my $new_linenr = 0;
	my $next_insert = 0;
	my $next_delete = 0;
	my @lines = ();
	my $inserted = @{$insertedRef}[$next_insert++];
	my $deleted = @{$deletedRef}[$next_delete++];
	foreach my $old_line (@{$linesRef}) {
		my $save_line = 1;
		my $line = $old_line;	
		if ($line =~ /^(?:\+\+\+|\-\-\-)\s+\S+/) {	
			$delta_offset = 0;
		} elsif ($line =~ /^\@\@ -\d+,\d+ \+\d+,\d+ \@\@/) {	
			$range_last_linenr = $new_linenr;
			fixup_current_range(\$line, $delta_offset, 0);
		}
		while (defined($deleted) && ${$deleted}{'LINENR'} == $old_linenr) {
			$deleted = @{$deletedRef}[$next_delete++];
			$save_line = 0;
			fixup_current_range(\$lines[$range_last_linenr], $delta_offset--, -1);
		}
		while (defined($inserted) && ${$inserted}{'LINENR'} == $old_linenr) {
			push(@lines, ${$inserted}{'LINE'});
			$inserted = @{$insertedRef}[$next_insert++];
			$new_linenr++;
			fixup_current_range(\$lines[$range_last_linenr], $delta_offset++, 1);
		}
		if ($save_line) {
			push(@lines, $line);
			$new_linenr++;
		}
		$old_linenr++;
	}
	return @lines;
}
sub fix_insert_line {
	my ($linenr, $line) = @_;
	my $inserted = {
		LINENR => $linenr,
		LINE => $line,
	};
	push(@fixed_inserted, $inserted);
}
sub fix_delete_line {
	my ($linenr, $line) = @_;
	my $deleted = {
		LINENR => $linenr,
		LINE => $line,
	};
	push(@fixed_deleted, $deleted);
}
sub ERROR {
	my ($type, $msg) = @_;
	if (report("ERROR", $type, $msg)) {
		our $clean = 0;
		our $cnt_error++;
		return 1;
	}
	return 0;
}
sub WARN {
	my ($type, $msg) = @_;
	if (report("WARNING", $type, $msg)) {
		our $clean = 0;
		our $cnt_warn++;
		return 1;
	}
	return 0;
}
sub CHK {
	my ($type, $msg) = @_;
	if ($check && report("CHECK", $type, $msg)) {
		our $clean = 0;
		our $cnt_chk++;
		return 1;
	}
	return 0;
}
sub check_absolute_file {
	my ($absolute, $herecurr) = @_;
	my $file = $absolute;
	while ($file =~ s@^[^/]*/@@) {
		if (-f "$root/$file") {
			last;
		}
	}
	if (! -f _)  {
		return 0;
	}
	my $prefix = $absolute;
	substr($prefix, -length($file)) = '';
	if ($prefix ne ".../") {
		WARN("USE_RELATIVE_PATH",
		     "use relative pathname instead of absolute in changelog text\n" . $herecurr);
	}
}
sub trim {
	my ($string) = @_;
	$string =~ s/^\s+|\s+$//g;
	return $string;
}
sub ltrim {
	my ($string) = @_;
	$string =~ s/^\s+//;
	return $string;
}
sub rtrim {
	my ($string) = @_;
	$string =~ s/\s+$//;
	return $string;
}
sub string_find_replace {
	my ($string, $find, $replace) = @_;
	$string =~ s/$find/$replace/g;
	return $string;
}
sub tabify {
	my ($leading) = @_;
	my $source_indent = $tabsize;
	my $max_spaces_before_tab = $source_indent - 1;
	my $spaces_to_tab = " " x $source_indent;
	1 while $leading =~ s@^([\t]*)$spaces_to_tab@$1\t@g;
	1 while $leading =~ s@^([\t]*)( {1,$max_spaces_before_tab})\t@$1\t@g;
	return "$leading";
}
sub pos_last_openparen {
	my ($line) = @_;
	my $pos = 0;
	my $opens = $line =~ tr/\(/\(/;
	my $closes = $line =~ tr/\)/\)/;
	my $last_openparen = 0;
	if (($opens == 0) || ($closes >= $opens)) {
		return -1;
	}
	my $len = length($line);
	for ($pos = 0; $pos < $len; $pos++) {
		my $string = substr($line, $pos);
		if ($string =~ /^($FuncArg|$balanced_parens)/) {
			$pos += length($1) - 1;
		} elsif (substr($line, $pos, 1) eq '(') {
			$last_openparen = $pos;
		} elsif (index($string, '(') == -1) {
			last;
		}
	}
	return length(expand_tabs(substr($line, 0, $last_openparen))) + 1;
}
sub get_raw_comment {
	my ($line, $rawline) = @_;
	my $comment = '';
	for my $i (0 .. (length($line) - 1)) {
		if (substr($line, $i, 1) eq "$;") {
			$comment .= substr($rawline, $i, 1);
		}
	}
	return $comment;
}
sub exclude_global_initialisers {
	my ($realfile) = @_;
	return $realfile =~ m@^tools/testing/selftests/bpf/progs/.*\.c$@ ||
		$realfile =~ m@^samples/bpf/.*_kern\.c$@ ||
		$realfile =~ m@/bpf/.*\.bpf\.c$@;
}
sub process {
	my $filename = shift;
	my $linenr=0;
	my $prevline="";
	my $prevrawline="";
	my $stashline="";
	my $stashrawline="";
	my $length;
	my $indent;
	my $previndent=0;
	my $stashindent=0;
	our $clean = 1;
	my $signoff = 0;
	my $author = '';
	my $authorsignoff = 0;
	my $author_sob = '';
	my $is_patch = 0;
	my $is_binding_patch = -1;
	my $in_header_lines = $file ? 0 : 1;
	my $in_commit_log = 0;		
	my $has_patch_separator = 0;	
	my $has_commit_log = 0;		
	my $commit_log_lines = 0;	
	my $commit_log_possible_stack_dump = 0;
	my $commit_log_long_line = 0;
	my $commit_log_has_diff = 0;
	my $reported_maintainer_file = 0;
	my $non_utf8_charset = 0;
	my $last_git_commit_id_linenr = -1;
	my $last_blank_line = 0;
	my $last_coalesced_string_linenr = -1;
	our @report = ();
	our $cnt_lines = 0;
	our $cnt_error = 0;
	our $cnt_warn = 0;
	our $cnt_chk = 0;
	my $realfile = '';
	my $realline = 0;
	my $realcnt = 0;
	my $here = '';
	my $context_function;		
	my $in_comment = 0;
	my $comment_edge = 0;
	my $first_line = 0;
	my $p1_prefix = '';
	my $prev_values = 'E';
	my %suppress_ifbraces;
	my %suppress_whiletrailers;
	my %suppress_export;
	my $suppress_statement = 0;
	my %signatures = ();
	my @setup_docs = ();
	my $setup_docs = 0;
	my $camelcase_file_seeded = 0;
	my $checklicenseline = 1;
	sanitise_line_reset();
	my $line;
	foreach my $rawline (@rawlines) {
		$linenr++;
		$line = $rawline;
		push(@fixed, $rawline) if ($fix);
		if ($rawline=~/^\+\+\+\s+(\S+)/) {
			$setup_docs = 0;
			if ($1 =~ m@Documentation/admin-guide/kernel-parameters.txt$@) {
				$setup_docs = 1;
			}
		}
		if ($rawline =~ /^\@\@ -\d+(?:,\d+)? \+(\d+)(,(\d+))? \@\@/) {
			$realline=$1-1;
			if (defined $2) {
				$realcnt=$3+1;
			} else {
				$realcnt=1+1;
			}
			$in_comment = 0;
			my $edge;
			my $cnt = $realcnt;
			for (my $ln = $linenr + 1; $cnt > 0; $ln++) {
				next if (defined $rawlines[$ln - 1] &&
					 $rawlines[$ln - 1] =~ /^-/);
				$cnt--;
				last if (!defined $rawlines[$ln - 1]);
				if ($rawlines[$ln - 1] =~ m@(/\*|\*/)@ &&
				    $rawlines[$ln - 1] !~ m@"[^"]*(?:/\*|\*/)[^"]*"@) {
					($edge) = $1;
					last;
				}
			}
			if (defined $edge && $edge eq '*/') {
				$in_comment = 1;
			}
			if (!defined $edge &&
			    $rawlines[$linenr] =~ m@^.\s*(?:\*\*+| \*)(?:\s|$)@)
			{
				$in_comment = 1;
			}
			sanitise_line_reset($in_comment);
		} elsif ($realcnt && $rawline =~ /^(?:\+| |$)/) {
			$line = sanitise_line($rawline);
		}
		push(@lines, $line);
		if ($realcnt > 1) {
			$realcnt-- if ($line =~ /^(?:\+| |$)/);
		} else {
			$realcnt = 0;
		}
		if ($setup_docs && $line =~ /^\+/) {
			push(@setup_docs, $line);
		}
	}
	$prefix = '';
	$realcnt = 0;
	$linenr = 0;
	$fixlinenr = -1;
	foreach my $line (@lines) {
		$linenr++;
		$fixlinenr++;
		my $sline = $line;	
		$sline =~ s/$;/ /g;	
		my $rawline = $rawlines[$linenr - 1];
		my $raw_comment = get_raw_comment($line, $rawline);
		if (!$in_commit_log &&
		    ($line =~ /^ mode change [0-7]+ => [0-7]+ \S+\s*$/ ||
		    ($line =~ /^rename (?:from|to) \S+\s*$/ ||
		     $line =~ /^diff --git a\/[\w\/\.\_\-]+ b\/\S+\s*$/))) {
			$is_patch = 1;
		}
		if (!$in_commit_log &&
		    $line =~ /^\@\@ -\d+(?:,\d+)? \+(\d+)(,(\d+))? \@\@(.*)/) {
			my $context = $4;
			$is_patch = 1;
			$first_line = $linenr + 1;
			$realline=$1-1;
			if (defined $2) {
				$realcnt=$3+1;
			} else {
				$realcnt=1+1;
			}
			annotate_reset();
			$prev_values = 'E';
			%suppress_ifbraces = ();
			%suppress_whiletrailers = ();
			%suppress_export = ();
			$suppress_statement = 0;
			if ($context =~ /\b(\w+)\s*\(/) {
				$context_function = $1;
			} else {
				undef $context_function;
			}
			next;
		} elsif ($line =~ /^( |\+|$)/) {
			$realline++;
			$realcnt-- if ($realcnt != 0);
			($length, $indent) = line_stats($rawline);
			($prevline, $stashline) = ($stashline, $line);
			($previndent, $stashindent) = ($stashindent, $indent);
			($prevrawline, $stashrawline) = ($stashrawline, $rawline);
		} elsif ($realcnt == 1) {
			$realcnt--;
		}
		my $hunk_line = ($realcnt != 0);
		$here = "#$linenr: " if (!$file);
		$here = "#$realline: " if ($file);
		my $found_file = 0;
		if ($line =~ /^diff --git.*?(\S+)$/) {
			$realfile = $1;
			$realfile =~ s@^([^/]*)/@@ if (!$file);
			$in_commit_log = 0;
			$found_file = 1;
		} elsif ($line =~ /^\+\+\+\s+(\S+)/) {
			$realfile = $1;
			$realfile =~ s@^([^/]*)/@@ if (!$file);
			$in_commit_log = 0;
			$p1_prefix = $1;
			if (!$file && $tree && $p1_prefix ne '' &&
			    -e "$root/$p1_prefix") {
				WARN("PATCH_PREFIX",
				     "patch prefix '$p1_prefix' exists, appears to be a -p0 patch\n");
			}
			if ($realfile =~ m@^include/asm/@) {
				ERROR("MODIFIED_INCLUDE_ASM",
				      "do not modify files in include/asm, change architecture specific files in include/asm-<architecture>\n" . "$here$rawline\n");
			}
			$found_file = 1;
		}
		if ($showfile) {
			$prefix = "$realfile:$realline: "
		} elsif ($emacs) {
			if ($file) {
				$prefix = "$filename:$realline: ";
			} else {
				$prefix = "$filename:$linenr: ";
			}
		}
		if ($found_file) {
			if (is_maintained_obsolete($realfile)) {
				WARN("OBSOLETE",
				     "$realfile is marked as 'obsolete' in the MAINTAINERS hierarchy.  No unnecessary modifications please.\n");
			}
			if ($realfile =~ m@^(?:drivers/net/|net/|drivers/staging/)@) {
				$check = 1;
			} else {
				$check = $check_orig;
			}
			$checklicenseline = 1;
			if ($realfile !~ /^MAINTAINERS/) {
				my $last_binding_patch = $is_binding_patch;
				$is_binding_patch = () = $realfile =~ m@^(?:Documentation/devicetree/|include/dt-bindings/)@;
				if (($last_binding_patch != -1) &&
				    ($last_binding_patch ^ $is_binding_patch)) {
					WARN("DT_SPLIT_BINDING_PATCH",
					     "DT binding docs and includes should be a separate patch. See: Documentation/devicetree/bindings/submitting-patches.rst\n");
				}
			}
			next;
		}
		$here .= "FILE: $realfile:$realline:" if ($realcnt != 0);
		my $hereline = "$here\n$rawline\n";
		my $herecurr = "$here\n$rawline\n";
		my $hereprev = "$here\n$prevrawline\n$rawline\n";
		$cnt_lines++ if ($realcnt != 0);
		if ($in_commit_log) {
			if ($line !~ /^\s*$/) {
				$commit_log_lines++;	
			}
		} elsif ($has_commit_log && $commit_log_lines < 2) {
			WARN("COMMIT_MESSAGE",
			     "Missing commit description - Add an appropriate one\n");
			$commit_log_lines = 2;	
		}
		if ($in_commit_log && !$commit_log_has_diff &&
		    (($line =~ m@^\s+diff\b.*a/([\w/]+)@ &&
		      $line =~ m@^\s+diff\b.*a/[\w/]+\s+b/$1\b@) ||
		     $line =~ m@^\s*(?:\-\-\-\s+a/|\+\+\+\s+b/)@ ||
		     $line =~ m/^\s*\@\@ \-\d+,\d+ \+\d+,\d+ \@\@/)) {
			ERROR("DIFF_IN_COMMIT_MSG",
			      "Avoid using diff content in the commit message - patch(1) might not work\n" . $herecurr);
			$commit_log_has_diff = 1;
		}
		if ($line =~ /^new (file )?mode.*[7531]\d{0,2}$/) {
			my $permhere = $here . "FILE: $realfile\n";
			if ($realfile !~ m@scripts/@ &&
			    $realfile !~ /\.(py|pl|awk|sh)$/) {
				ERROR("EXECUTE_PERMISSIONS",
				      "do not set execute permissions for source files\n" . $permhere);
			}
		}
		if (decode("MIME-Header", $line) =~ /^From:\s*(.*)/) {
			$author = $1;
			my $curline = $linenr;
			while(defined($rawlines[$curline]) && ($rawlines[$curline++] =~ /^[ \t]\s*(.*)/)) {
				$author .= $1;
			}
			$author = encode("utf8", $author) if ($line =~ /=\?utf-8\?/i);
			$author =~ s/"//g;
			$author = reformat_email($author);
		}
		if ($line =~ /^\s*signed-off-by:\s*(.*)/i) {
			$signoff++;
			$in_commit_log = 0;
			if ($author ne ''  && $authorsignoff != 1) {
				if (same_email_addresses($1, $author)) {
					$authorsignoff = 1;
				} else {
					my $ctx = $1;
					my ($email_name, $email_comment, $email_address, $comment1) = parse_email($ctx);
					my ($author_name, $author_comment, $author_address, $comment2) = parse_email($author);
					if (lc $email_address eq lc $author_address && $email_name eq $author_name) {
						$author_sob = $ctx;
						$authorsignoff = 2;
					} elsif (lc $email_address eq lc $author_address) {
						$author_sob = $ctx;
						$authorsignoff = 3;
					} elsif ($email_name eq $author_name) {
						$author_sob = $ctx;
						$authorsignoff = 4;
						my $address1 = $email_address;
						my $address2 = $author_address;
						if ($address1 =~ /(\S+)\+\S+(\@.*)/) {
							$address1 = "$1$2";
						}
						if ($address2 =~ /(\S+)\+\S+(\@.*)/) {
							$address2 = "$1$2";
						}
						if ($address1 eq $address2) {
							$authorsignoff = 5;
						}
					}
				}
			}
		}
		if ($line =~ /^---$/) {
			$has_patch_separator = 1;
			$in_commit_log = 0;
		}
		if ($line =~ /^\s*MAINTAINERS\s*\|/) {
			$reported_maintainer_file = 1;
		}
		if (!$in_header_lines &&
		    $line =~ /^(\s*)([a-z0-9_-]+by:|$signature_tags)(\s*)(.*)/i) {
			my $space_before = $1;
			my $sign_off = $2;
			my $space_after = $3;
			my $email = $4;
			my $ucfirst_sign_off = ucfirst(lc($sign_off));
			if ($sign_off !~ /$signature_tags/) {
				my $suggested_signature = find_standard_signature($sign_off);
				if ($suggested_signature eq "") {
					WARN("BAD_SIGN_OFF",
					     "Non-standard signature: $sign_off\n" . $herecurr);
				} else {
					if (WARN("BAD_SIGN_OFF",
						 "Non-standard signature: '$sign_off' - perhaps '$suggested_signature'?\n" . $herecurr) &&
					    $fix) {
						$fixed[$fixlinenr] =~ s/$sign_off/$suggested_signature/;
					}
				}
			}
			if (defined $space_before && $space_before ne "") {
				if (WARN("BAD_SIGN_OFF",
					 "Do not use whitespace before $ucfirst_sign_off\n" . $herecurr) &&
				    $fix) {
					$fixed[$fixlinenr] =
					    "$ucfirst_sign_off $email";
				}
			}
			if ($sign_off =~ /-by:$/i && $sign_off ne $ucfirst_sign_off) {
				if (WARN("BAD_SIGN_OFF",
					 "'$ucfirst_sign_off' is the preferred signature form\n" . $herecurr) &&
				    $fix) {
					$fixed[$fixlinenr] =
					    "$ucfirst_sign_off $email";
				}
			}
			if (!defined $space_after || $space_after ne " ") {
				if (WARN("BAD_SIGN_OFF",
					 "Use a single space after $ucfirst_sign_off\n" . $herecurr) &&
				    $fix) {
					$fixed[$fixlinenr] =
					    "$ucfirst_sign_off $email";
				}
			}
			my ($email_name, $name_comment, $email_address, $comment) = parse_email($email);
			my $suggested_email = format_email(($email_name, $name_comment, $email_address, $comment));
			if ($suggested_email eq "") {
				ERROR("BAD_SIGN_OFF",
				      "Unrecognized email address: '$email'\n" . $herecurr);
			} else {
				my $dequoted = $suggested_email;
				$dequoted =~ s/^"//;
				$dequoted =~ s/" </ </;
				if (!same_email_addresses($email, $suggested_email)) {
					if (WARN("BAD_SIGN_OFF",
						 "email address '$email' might be better as '$suggested_email'\n" . $herecurr) &&
					    $fix) {
						$fixed[$fixlinenr] =~ s/\Q$email\E/$suggested_email/;
					}
				}
				my $stripped_address = $email_address;
				$stripped_address =~ s/\([^\(\)]*\)//g;
				if ($email_address ne $stripped_address) {
					if (WARN("BAD_SIGN_OFF",
						 "address part of email should not have comments: '$email_address'\n" . $herecurr) &&
					    $fix) {
						$fixed[$fixlinenr] =~ s/\Q$email_address\E/$stripped_address/;
					}
				}
				my $comment_count = () = $name_comment =~ /\([^\)]+\)/g;
				if ($comment_count > 1) {
					WARN("BAD_SIGN_OFF",
					     "Use a single name comment in email: '$email'\n" . $herecurr);
				}
				if ($email =~ /^.*stable\@(?:vger\.)?kernel\.org/i) {
					if (($comment ne "" && $comment !~ /^
					    ($email_name ne "")) {
						my $cur_name = $email_name;
						my $new_comment = $comment;
						$cur_name =~ s/[a-zA-Z\s\-\"]+//g;
						$new_comment =~ s/^\((.*)\)$/$1/;
						$new_comment =~ s/^\[(.*)\]$/$1/;
						$new_comment =~ s/^[\s\
						$new_comment = trim("$new_comment $cur_name") if ($cur_name ne $new_comment);
						$new_comment = " # $new_comment" if ($new_comment ne "");
						my $new_email = "$email_address$new_comment";
						if (WARN("BAD_STABLE_ADDRESS_STYLE",
							 "Invalid email format for stable: '$email', prefer '$new_email'\n" . $herecurr) &&
						    $fix) {
							$fixed[$fixlinenr] =~ s/\Q$email\E/$new_email/;
						}
					}
				} elsif ($comment ne "" && $comment !~ /^(?:
					my $new_comment = $comment;
					$new_comment =~ s/^\[(.*)\]$/$1/;
					$new_comment =~ s/^\/\*(.*)\*\/$/$1/;
					$new_comment = trim($new_comment);
					$new_comment =~ s/^[^\w]$//; 
					$new_comment = "($new_comment)" if ($new_comment ne "");
					my $new_email = format_email($email_name, $name_comment, $email_address, $new_comment);
					if (WARN("BAD_SIGN_OFF",
						 "Unexpected content after email: '$email', should be: '$new_email'\n" . $herecurr) &&
					    $fix) {
						$fixed[$fixlinenr] =~ s/\Q$email\E/$new_email/;
					}
				}
			}
			my $sig_nospace = $line;
			$sig_nospace =~ s/\s//g;
			$sig_nospace = lc($sig_nospace);
			if (defined $signatures{$sig_nospace}) {
				WARN("BAD_SIGN_OFF",
				     "Duplicate signature\n" . $herecurr);
			} else {
				$signatures{$sig_nospace} = 1;
			}
			if ($sign_off =~ /^co-developed-by:$/i) {
				if ($email eq $author) {
					WARN("BAD_SIGN_OFF",
					      "Co-developed-by: should not be used to attribute nominal patch author '$author'\n" . $herecurr);
				}
				if (!defined $lines[$linenr]) {
					WARN("BAD_SIGN_OFF",
					     "Co-developed-by: must be immediately followed by Signed-off-by:\n" . $herecurr);
				} elsif ($rawlines[$linenr] !~ /^signed-off-by:\s*(.*)/i) {
					WARN("BAD_SIGN_OFF",
					     "Co-developed-by: must be immediately followed by Signed-off-by:\n" . $herecurr . $rawlines[$linenr] . "\n");
				} elsif ($1 ne $email) {
					WARN("BAD_SIGN_OFF",
					     "Co-developed-by and Signed-off-by: name/email do not match\n" . $herecurr . $rawlines[$linenr] . "\n");
				}
			}
			if ($sign_off =~ /^reported(?:|-and-tested)-by:$/i) {
				if (!defined $lines[$linenr]) {
					WARN("BAD_REPORTED_BY_LINK",
					     "Reported-by: should be immediately followed by Closes: with a URL to the report\n" . $herecurr . "\n");
				} elsif ($rawlines[$linenr] !~ /^closes:\s*/i) {
					WARN("BAD_REPORTED_BY_LINK",
					     "Reported-by: should be immediately followed by Closes: with a URL to the report\n" . $herecurr . $rawlines[$linenr] . "\n");
				}
			}
		}
		if (!$in_header_lines &&
		    $line =~ /^\s*fixes:?\s*(?:commit\s*)?[0-9a-f]{5,}\b/i) {
			my $orig_commit = "";
			my $id = "0123456789ab";
			my $title = "commit title";
			my $tag_case = 1;
			my $tag_space = 1;
			my $id_length = 1;
			my $id_case = 1;
			my $title_has_quotes = 0;
			if ($line =~ /(\s*fixes:?)\s+([0-9a-f]{5,})\s+($balanced_parens)/i) {
				my $tag = $1;
				$orig_commit = $2;
				$title = $3;
				$tag_case = 0 if $tag eq "Fixes:";
				$tag_space = 0 if ($line =~ /^fixes:? [0-9a-f]{5,} ($balanced_parens)/i);
				$id_length = 0 if ($orig_commit =~ /^[0-9a-f]{12}$/i);
				$id_case = 0 if ($orig_commit !~ /[A-F]/);
				$title = substr($title, 1, -1);
				if ($title =~ /^".*"$/) {
					$title = substr($title, 1, -1);
					$title_has_quotes = 1;
				}
			}
			my ($cid, $ctitle) = git_commit_info($orig_commit, $id,
							     $title);
			if ($ctitle ne $title || $tag_case || $tag_space ||
			    $id_length || $id_case || !$title_has_quotes) {
				if (WARN("BAD_FIXES_TAG",
				     "Please use correct Fixes: style 'Fixes: <12 chars of sha1> (\"<title line>\")' - ie: 'Fixes: $cid (\"$ctitle\")'\n" . $herecurr) &&
				    $fix) {
					$fixed[$fixlinenr] = "Fixes: $cid (\"$ctitle\")";
				}
			}
		}
		if ($in_header_lines &&
		    $line =~ /^Subject:.*\b(?:checkpatch|sparse|smatch)\b[^:]/i) {
			WARN("EMAIL_SUBJECT",
			     "A patch subject line should describe the change not the tool that found it\n" . $herecurr);
		}
		if ($realfile eq '' && !$has_patch_separator && $line =~ /^\s*change-id:/i) {
			if (ERROR("GERRIT_CHANGE_ID",
			          "Remove Gerrit Change-Id's before submitting upstream\n" . $herecurr) &&
			    $fix) {
				fix_delete_line($fixlinenr, $rawline);
			}
		}
		if ($in_commit_log && !$commit_log_possible_stack_dump &&
		    ($line =~ /^\s*(?:WARNING:|BUG:)/ ||
		     $line =~ /^\s*\[\s*\d+\.\d{6,6}\s*\]/ ||
		     $line =~ /^\s*\[\<[0-9a-fA-F]{8,}\>\]/) ||
		     $line =~ /^(?:\s+\w+:\s+[0-9a-fA-F]+){3,3}/ ||
		     $line =~ /^\s*\
			$commit_log_possible_stack_dump = 1;
		}
		if ($in_commit_log && !$commit_log_long_line &&
		    length($line) > 75 &&
		    !($line =~ /^\s*[a-zA-Z0-9_\/\.]+\s+\|\s+\d+/ ||
		      $line =~ /^\s*(?:[\w\.\-\+]*\/)++[\w\.\-\+]+:/ ||
		      $line =~ /^\s*(?:Fixes:|$link_tags_search|$signature_tags)/i ||
		      $commit_log_possible_stack_dump)) {
			WARN("COMMIT_LOG_LONG_LINE",
			     "Prefer a maximum 75 chars per line (possible unwrapped commit description?)\n" . $herecurr);
			$commit_log_long_line = 1;
		}
		if ($in_commit_log && $commit_log_possible_stack_dump &&
		    $line =~ /^\s*$/) {
			$commit_log_possible_stack_dump = 0;
		}
		if ($in_commit_log &&
		    $line =~ /^\s*(\w+:)\s*http/ && $1 !~ /^$link_tags_search$/) {
			if ($1 =~ /^v(?:ersion)?\d+/i) {
				WARN("COMMIT_LOG_VERSIONING",
				     "Patch version information should be after the --- line\n" . $herecurr);
			} else {
				WARN("COMMIT_LOG_USE_LINK",
				     "Unknown link reference '$1', use $link_tags_print instead\n" . $herecurr);
			}
		}
		if ($in_commit_log &&
		    $line =~ /^\s*(\w+:)\s*(\S+)/) {
			my $tag = $1;
			my $value = $2;
			if ($tag =~ /^$link_tags_search$/ && $value !~ m{^https?://}) {
				WARN("COMMIT_LOG_WRONG_LINK",
				     "'$tag' should be followed by a public http(s) link\n" . $herecurr);
			}
		}
		if ($in_commit_log && $line =~ /^
			if (WARN("COMMIT_COMMENT_SYMBOL",
				 "Commit log lines starting with '#' are dropped by git as comments\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/^/ /;
			}
		}
		if ($perl_version_ok &&
		    $in_commit_log && !$commit_log_possible_stack_dump &&
		    $line !~ /^\s*(?:Link|Patchwork|http|https|BugLink|base-commit):/i &&
		    $line !~ /^This reverts commit [0-9a-f]{7,40}/ &&
		    (($line =~ /\bcommit\s+[0-9a-f]{5,}\b/i ||
		      ($line =~ /\bcommit\s*$/i && defined($rawlines[$linenr]) && $rawlines[$linenr] =~ /^\s*[0-9a-f]{5,}\b/i)) ||
		     ($line =~ /(?:\s|^)[0-9a-f]{12,40}(?:[\s"'\(\[]|$)/i &&
		      $line !~ /[\<\[][0-9a-f]{12,40}[\>\]]/i &&
		      $line !~ /\bfixes:\s*[0-9a-f]{12,40}/i))) {
			my $init_char = "c";
			my $orig_commit = "";
			my $short = 1;
			my $long = 0;
			my $case = 1;
			my $space = 1;
			my $id = '0123456789ab';
			my $orig_desc = "commit description";
			my $description = "";
			my $herectx = $herecurr;
			my $has_parens = 0;
			my $has_quotes = 0;
			my $input = $line;
			if ($line =~ /(?:\bcommit\s+[0-9a-f]{5,}|\bcommit\s*$)/i) {
				for (my $n = 0; $n < 2; $n++) {
					if ($input =~ /\bcommit\s+[0-9a-f]{5,}\s*($balanced_parens)/i) {
						$orig_desc = $1;
						$has_parens = 1;
						$orig_desc = substr($orig_desc, 1, -1);
						if ($orig_desc =~ /^".*"$/) {
							$orig_desc = substr($orig_desc, 1, -1);
							$has_quotes = 1;
						}
						last;
					}
					last if ($
					$input .= " " . trim($rawlines[$linenr + $n]);
					$herectx .= "$rawlines[$linenr + $n]\n";
				}
				$herectx = $herecurr if (!$has_parens);
			}
			if ($input =~ /\b(c)ommit\s+([0-9a-f]{5,})\b/i) {
				$init_char = $1;
				$orig_commit = lc($2);
				$short = 0 if ($input =~ /\bcommit\s+[0-9a-f]{12,40}/i);
				$long = 1 if ($input =~ /\bcommit\s+[0-9a-f]{41,}/i);
				$space = 0 if ($input =~ /\bcommit [0-9a-f]/i);
				$case = 0 if ($input =~ /\b[Cc]ommit\s+[0-9a-f]{5,40}[^A-F]/);
			} elsif ($input =~ /\b([0-9a-f]{12,40})\b/i) {
				$orig_commit = lc($1);
			}
			($id, $description) = git_commit_info($orig_commit,
							      $id, $orig_desc);
			if (defined($id) &&
			    ($short || $long || $space || $case || ($orig_desc ne $description) || !$has_quotes) &&
			    $last_git_commit_id_linenr != $linenr - 1) {
				ERROR("GIT_COMMIT_ID",
				      "Please use git commit description style 'commit <12+ chars of sha1> (\"<title line>\")' - ie: '${init_char}ommit $id (\"$description\")'\n" . $herectx);
			}
			$last_git_commit_id_linenr = $linenr if ($line =~ /\bcommit\s*$/i);
		}
		if ($rawline =~ m{http.*\b$obsolete_archives}) {
			WARN("PREFER_LORE_ARCHIVE",
			     "Use lore.kernel.org archive links when possible - see https://lore.kernel.org/lists.html\n" . $herecurr);
		}
		if (!$reported_maintainer_file && !$in_commit_log &&
		    ($line =~ /^(?:new|deleted) file mode\s*\d+\s*$/ ||
		     $line =~ /^rename (?:from|to) [\w\/\.\-]+\s*$/ ||
		     ($line =~ /\{\s*([\w\/\.\-]*)\s*\=\>\s*([\w\/\.\-]*)\s*\}/ &&
		      (defined($1) || defined($2))))) {
			$is_patch = 1;
			$reported_maintainer_file = 1;
			WARN("FILE_PATH_CHANGES",
			     "added, moved or deleted file(s), does MAINTAINERS need updating?\n" . $herecurr);
		}
		if (!$in_commit_log &&
		    ($line =~ /^new file mode\s*\d+\s*$/) &&
		    ($realfile =~ m@^Documentation/devicetree/bindings/.*\.txt$@)) {
			WARN("DT_SCHEMA_BINDING_PATCH",
			     "DT bindings should be in DT schema format. See: Documentation/devicetree/bindings/writing-schema.rst\n");
		}
		if ($realcnt != 0 && $line !~ m{^(?:\+|-| |\\ No newline|$)}) {
			ERROR("CORRUPTED_PATCH",
			      "patch seems to be corrupt (line wrapped?)\n" .
				$herecurr) if (!$emitted_corrupt++);
		}
		if (($realfile =~ /^$/ || $line =~ /^\+/) &&
		    $rawline !~ m/^$UTF8*$/) {
			my ($utf8_prefix) = ($rawline =~ /^($UTF8*)/);
			my $blank = copy_spacing($rawline);
			my $ptr = substr($blank, 0, length($utf8_prefix)) . "^";
			my $hereptr = "$hereline$ptr\n";
			CHK("INVALID_UTF8",
			    "Invalid UTF-8, patch and commit message should be encoded in UTF-8\n" . $hereptr);
		}
		if ($in_header_lines && $realfile =~ /^$/ &&
		    !($rawline =~ /^\s+(?:\S|$)/ ||
		      $rawline =~ /^(?:commit\b|from\b|[\w-]+:)/i)) {
			$in_header_lines = 0;
			$in_commit_log = 1;
			$has_commit_log = 1;
		}
		if ($in_header_lines &&
		    $rawline =~ /^Content-Type:.+charset="(.+)".*$/ &&
		    $1 !~ /utf-8/i) {
			$non_utf8_charset = 1;
		}
		if ($in_commit_log && $non_utf8_charset && $realfile =~ /^$/ &&
		    $rawline =~ /$NON_ASCII_UTF8/) {
			WARN("UTF8_BEFORE_PATCH",
			    "8-bit UTF-8 used in possible commit log\n" . $herecurr);
		}
		if ($tree && $in_commit_log) {
			while ($line =~ m{(?:^|\s)(/\S*)}g) {
				my $file = $1;
				if ($file =~ m{^(.*?)(?::\d+)+:?$} &&
				    check_absolute_file($1, $herecurr)) {
				} else {
					check_absolute_file($file, $herecurr);
				}
			}
		}
		if (defined($misspellings) &&
		    ($in_commit_log || $line =~ /^(?:\+|Subject:)/i)) {
			while ($rawline =~ /(?:^|[^\w\-'`])($misspellings)(?:[^\w\-'`]|$)/gi) {
				my $typo = $1;
				my $blank = copy_spacing($rawline);
				my $ptr = substr($blank, 0, $-[1]) . "^" x length($typo);
				my $hereptr = "$hereline$ptr\n";
				my $typo_fix = $spelling_fix{lc($typo)};
				$typo_fix = ucfirst($typo_fix) if ($typo =~ /^[A-Z]/);
				$typo_fix = uc($typo_fix) if ($typo =~ /^[A-Z]+$/);
				my $msg_level = \&WARN;
				$msg_level = \&CHK if ($file);
				if (&{$msg_level}("TYPO_SPELLING",
						  "'$typo' may be misspelled - perhaps '$typo_fix'?\n" . $hereptr) &&
				    $fix) {
					$fixed[$fixlinenr] =~ s/(^|[^A-Za-z@])($typo)($|[^A-Za-z@])/$1$typo_fix$3/;
				}
			}
		}
		if ($in_commit_log && $line =~ /(^fixes:|\bcommit)\s+([0-9a-f]{6,40})\b/i) {
			my $id;
			my $description;
			($id, $description) = git_commit_info($2, undef, undef);
			if (!defined($id)) {
				WARN("UNKNOWN_COMMIT_ID",
				     "Unknown commit id '$2', maybe rebased or not pulled?\n" . $herecurr);
			}
		}
		if (($rawline =~ /^\+/ || $in_commit_log) &&
		    $rawline !~ /[bcCdDlMnpPs\?-][rwxsStT-]{9}/) {
			pos($rawline) = 1 if (!$in_commit_log);
			while ($rawline =~ /\b($word_pattern) (?=($word_pattern))/g) {
				my $first = $1;
				my $second = $2;
				my $start_pos = $-[1];
				my $end_pos = $+[2];
				if ($first =~ /(?:struct|union|enum)/) {
					pos($rawline) += length($first) + length($second) + 1;
					next;
				}
				next if (lc($first) ne lc($second));
				next if ($first eq 'long');
				my $start_char = '';
				my $end_char = '';
				$start_char = substr($rawline, $start_pos - 1, 1) if ($start_pos > ($in_commit_log ? 0 : 1));
				$end_char = substr($rawline, $end_pos, 1) if ($end_pos < length($rawline));
				next if ($start_char =~ /^\S$/);
				next if (index(" \t.,;?!", $end_char) == -1);
				if ($first =~ /\b[0-9a-f]{2,}\b/i) {
					next if (!exists($allow_repeated_words{lc($first)}));
				}
				if (WARN("REPEATED_WORD",
					 "Possible repeated word: '$first'\n" . $herecurr) &&
				    $fix) {
					$fixed[$fixlinenr] =~ s/\b$first $second\b/$first/;
				}
			}
			if ($prevline =~ /$;+\s*$/ &&
			    $prevrawline =~ /($word_pattern)\s*$/) {
				my $last_word = $1;
				if ($rawline =~ /^\+\s*\*\s*$last_word /) {
					if (WARN("REPEATED_WORD",
						 "Possible repeated word: '$last_word'\n" . $hereprev) &&
					    $fix) {
						$fixed[$fixlinenr] =~ s/(\+\s*\*\s*)$last_word /$1/;
					}
				}
			}
		}
		next if (!$hunk_line || $line =~ /^-/);
		if ($line =~ /^\+.*\015/) {
			my $herevet = "$here\n" . cat_vet($rawline) . "\n";
			if (ERROR("DOS_LINE_ENDINGS",
				  "DOS line endings\n" . $herevet) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/[\s\015]+$//;
			}
		} elsif ($rawline =~ /^\+.*\S\s+$/ || $rawline =~ /^\+\s+$/) {
			my $herevet = "$here\n" . cat_vet($rawline) . "\n";
			if (ERROR("TRAILING_WHITESPACE",
				  "trailing whitespace\n" . $herevet) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/\s+$//;
			}
			$rpt_cleaners = 1;
		}
		if ($rawline =~ /\bwrite to the Free/i ||
		    $rawline =~ /\b675\s+Mass\s+Ave/i ||
		    $rawline =~ /\b59\s+Temple\s+Pl/i ||
		    $rawline =~ /\b51\s+Franklin\s+St/i) {
			my $herevet = "$here\n" . cat_vet($rawline) . "\n";
			my $msg_level = \&ERROR;
			$msg_level = \&CHK if ($file);
			&{$msg_level}("FSF_MAILING_ADDRESS",
				      "Do not include the paragraph about writing to the Free Software Foundation's mailing address from the sample GPL notice. The FSF has changed addresses in the past, and may do so again. Linux already includes a copy of the GPL.\n" . $herevet)
		}
		if ($realfile =~ /Kconfig/ &&
		    $line =~ /^\+\s*(?:config|menuconfig|choice)\b/) {
			my $ln = $linenr;
			my $needs_help = 0;
			my $has_help = 0;
			my $help_length = 0;
			while (defined $lines[$ln]) {
				my $f = $lines[$ln++];
				next if ($f =~ /^-/);
				last if ($f !~ /^[\+ ]/);	
				if ($f =~ /^\+\s*(?:bool|tristate|prompt)\s*["']/) {
					$needs_help = 1;
					next;
				}
				if ($f =~ /^\+\s*help\s*$/) {
					$has_help = 1;
					next;
				}
				$f =~ s/^.//;	
				$f =~ s/
				$f =~ s/^\s+//;	
				next if ($f =~ /^$/);	
				if ($f =~ /^(?:config|menuconfig|choice|endchoice|
					       if|endif|menu|endmenu|source)\b/x) {
					last;
				}
				$help_length++ if ($has_help);
			}
			if ($needs_help &&
			    $help_length < $min_conf_desc_length) {
				my $stat_real = get_stat_real($linenr, $ln - 1);
				WARN("CONFIG_DESCRIPTION",
				     "please write a help paragraph that fully describes the config symbol\n" . "$here\n$stat_real\n");
			}
		}
		if ($realfile =~ /^MAINTAINERS$/) {
			if ($rawline =~ /^\+[A-Z]:/ &&
			    $rawline !~ /^\+[A-Z]:\t\S/) {
				if (WARN("MAINTAINERS_STYLE",
					 "MAINTAINERS entries use one tab after TYPE:\n" . $herecurr) &&
				    $fix) {
					$fixed[$fixlinenr] =~ s/^(\+[A-Z]):\s*/$1:\t/;
				}
			}
			my $preferred_order = 'MRLSWQBCPTFXNK';
			if ($rawline =~ /^\+[A-Z]:/ &&
			    $prevrawline =~ /^[\+ ][A-Z]:/) {
				$rawline =~ /^\+([A-Z]):\s*(.*)/;
				my $cur = $1;
				my $curval = $2;
				$prevrawline =~ /^[\+ ]([A-Z]):\s*(.*)/;
				my $prev = $1;
				my $prevval = $2;
				my $curindex = index($preferred_order, $cur);
				my $previndex = index($preferred_order, $prev);
				if ($curindex < 0) {
					WARN("MAINTAINERS_STYLE",
					     "Unknown MAINTAINERS entry type: '$cur'\n" . $herecurr);
				} else {
					if ($previndex >= 0 && $curindex < $previndex) {
						WARN("MAINTAINERS_STYLE",
						     "Misordered MAINTAINERS entry - list '$cur:' before '$prev:'\n" . $hereprev);
					} elsif ((($prev eq 'F' && $cur eq 'F') ||
						  ($prev eq 'X' && $cur eq 'X')) &&
						 ($prevval cmp $curval) > 0) {
						WARN("MAINTAINERS_STYLE",
						     "Misordered MAINTAINERS entry - list file patterns in alphabetic order\n" . $hereprev);
					}
				}
			}
		}
		if (($realfile =~ /Makefile.*/ || $realfile =~ /Kbuild.*/) &&
		    ($line =~ /\+(EXTRA_[A-Z]+FLAGS).*/)) {
			my $flag = $1;
			my $replacement = {
				'EXTRA_AFLAGS' =>   'asflags-y',
				'EXTRA_CFLAGS' =>   'ccflags-y',
				'EXTRA_CPPFLAGS' => 'cppflags-y',
				'EXTRA_LDFLAGS' =>  'ldflags-y',
			};
			WARN("DEPRECATED_VARIABLE",
			     "Use of $flag is deprecated, please use \`$replacement->{$flag} instead.\n" . $herecurr) if ($replacement->{$flag});
		}
		if (defined $root &&
			(($realfile =~ /\.dtsi?$/ && $line =~ /^\+\s*compatible\s*=\s*\"/) ||
			 ($realfile =~ /\.[ch]$/ && $line =~ /^\+.*\.compatible\s*=\s*\"/))) {
			my @compats = $rawline =~ /\"([a-zA-Z0-9\-\,\.\+_]+)\"/g;
			my $dt_path = $root . "/Documentation/devicetree/bindings/";
			my $vp_file = $dt_path . "vendor-prefixes.yaml";
			foreach my $compat (@compats) {
				my $compat2 = $compat;
				$compat2 =~ s/\,[a-zA-Z0-9]*\-/\,<\.\*>\-/;
				my $compat3 = $compat;
				$compat3 =~ s/\,([a-z]*)[0-9]*\-/\,$1<\.\*>\-/;
				`grep -Erq "$compat|$compat2|$compat3" $dt_path`;
				if ( $? >> 8 ) {
					WARN("UNDOCUMENTED_DT_STRING",
					     "DT compatible string \"$compat\" appears un-documented -- check $dt_path\n" . $herecurr);
				}
				next if $compat !~ /^([a-zA-Z0-9\-]+)\,/;
				my $vendor = $1;
				`grep -Eq "\\"\\^\Q$vendor\E,\\.\\*\\":" $vp_file`;
				if ( $? >> 8 ) {
					WARN("UNDOCUMENTED_DT_STRING",
					     "DT compatible string vendor \"$vendor\" appears un-documented -- check $vp_file\n" . $herecurr);
				}
			}
		}
		if ($realline == $checklicenseline) {
			if ($rawline =~ /^[ \+]\s*\
				$checklicenseline = 2;
			} elsif ($rawline =~ /^\+/) {
				my $comment = "";
				if ($realfile =~ /\.(h|s|S)$/) {
					$comment = '/*';
				} elsif ($realfile =~ /\.(c|rs|dts|dtsi)$/) {
					$comment = '//';
				} elsif (($checklicenseline == 2) || $realfile =~ /\.(sh|pl|py|awk|tc|yaml)$/) {
					$comment = '
				} elsif ($realfile =~ /\.rst$/) {
					$comment = '..';
				}
				if ($realfile =~ /\.[chsS]$/ &&
				    $rawline =~ /SPDX-License-Identifier:/ &&
				    $rawline !~ m@^\+\s*\Q$comment\E\s*@) {
					WARN("SPDX_LICENSE_TAG",
					     "Improper SPDX comment style for '$realfile', please use '$comment' instead\n" . $herecurr);
				}
				if ($comment !~ /^$/ &&
				    $rawline !~ m@^\+\Q$comment\E SPDX-License-Identifier: @) {
					WARN("SPDX_LICENSE_TAG",
					     "Missing or malformed SPDX-License-Identifier tag in line $checklicenseline\n" . $herecurr);
				} elsif ($rawline =~ /(SPDX-License-Identifier: .*)/) {
					my $spdx_license = $1;
					if (!is_SPDX_License_valid($spdx_license)) {
						WARN("SPDX_LICENSE_TAG",
						     "'$spdx_license' is not supported in LICENSES/...\n" . $herecurr);
					}
					if ($realfile =~ m@^Documentation/devicetree/bindings/@ &&
					    $spdx_license !~ /GPL-2\.0(?:-only)? OR BSD-2-Clause/) {
						my $msg_level = \&WARN;
						$msg_level = \&CHK if ($file);
						if (&{$msg_level}("SPDX_LICENSE_TAG",
								  "DT binding documents should be licensed (GPL-2.0-only OR BSD-2-Clause)\n" . $herecurr) &&
						    $fix) {
							$fixed[$fixlinenr] =~ s/SPDX-License-Identifier: .*/SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)/;
						}
					}
					if ($realfile =~ m@^include/dt-bindings/@ &&
					    $spdx_license !~ /GPL-2\.0(?:-only)? OR \S+/) {
						WARN("SPDX_LICENSE_TAG",
						     "DT binding headers should be licensed (GPL-2.0-only OR .*)\n" . $herecurr);
					}
				}
			}
		}
		if ($rawline =~ /^\+.*\b\Q$realfile\E\b/) {
			WARN("EMBEDDED_FILENAME",
			     "It's generally not useful to have the filename in the file\n" . $herecurr);
		}
		next if ($realfile !~ /\.(h|c|rs|s|S|sh|dtsi|dts)$/);
		if ($realline != $checklicenseline &&
		    $rawline =~ /\bSPDX-License-Identifier:/ &&
		    substr($line, @-, @+ - @-) eq "$;" x (@+ - @-)) {
			WARN("SPDX_LICENSE_TAG",
			     "Misplaced SPDX-License-Identifier tag - use line $checklicenseline instead\n" . $herecurr);
		}
		if ($line =~ /^\+/ && $length > $max_line_length) {
			my $msg_type = "LONG_LINE";
			if ($line =~ /^\+\s*$logFunctions\s*\(\s*(?:(?:KERN_\S+\s*|[^"]*))?($String\s*(?:|,|\)\s*;)\s*)$/ &&
			    length(expand_tabs(substr($line, 1, length($line) - length($1) - 1))) <= $max_line_length) {
				$msg_type = "";
			} elsif ($line =~ /^\+\s*$String\s*(?:\s*|,|\)\s*;)\s*$/ ||
				 $line =~ /^\+\s*
				$msg_type = "";
			} elsif ($line =~ /^\+.*\bEFI_GUID\s*\(/ ||
				 $line =~ /^\+\s*(?:\w+)?\s*DEFINE_PER_CPU/) {
				$msg_type = "";
			} elsif ($rawline =~ /^\+.*\b[a-z][\w\.\+\-]*:\/\/\S+/i) {
				$msg_type = "";
			} elsif ($line =~ /($;[\s$;]*)$/ &&
				 length(expand_tabs(substr($line, 1, length($line) - length($1) - 1))) <= $max_line_length) {
				$msg_type = "LONG_LINE_COMMENT"
			} elsif ($sline =~ /\s*($String(?:\s*(?:\\|,\s*|\)\s*;\s*))?)$/ &&
				 length(expand_tabs(substr($line, 1, length($line) - length($1) - 1))) <= $max_line_length) {
				$msg_type = "LONG_LINE_STRING"
			}
			if ($msg_type ne "" &&
			    (show_type("LONG_LINE") || show_type($msg_type))) {
				my $msg_level = \&WARN;
				$msg_level = \&CHK if ($file);
				&{$msg_level}($msg_type,
					      "line length of $length exceeds $max_line_length columns\n" . $herecurr);
			}
		}
		if ($line =~ /^\+/ && defined $lines[$linenr] && $lines[$linenr] =~ /^\\ No newline at end of file/) {
			if (WARN("MISSING_EOF_NEWLINE",
			         "adding a line without newline at end of file\n" . $herecurr) &&
			    $fix) {
				fix_delete_line($fixlinenr+1, "No newline at end of file");
			}
		}
		if ($realfile =~ /\.S$/ &&
		    $line =~ /^\+\s*(?:[A-Z]+_)?SYM_[A-Z]+_(?:START|END)(?:_[A-Z_]+)?\s*\(\s*\.L/) {
			WARN("AVOID_L_PREFIX",
			     "Avoid using '.L' prefixed local symbol names for denoting a range of code via 'SYM_*_START/END' annotations; see Documentation/core-api/asm-annotations.rst\n" . $herecurr);
		}
		next if ($realfile !~ /\.(h|c|pl|dtsi|dts)$/);
		if ($rawline =~ /^\+\s* \t\s*\S/ ||
		    $rawline =~ /^\+\s*        \s*/) {
			my $herevet = "$here\n" . cat_vet($rawline) . "\n";
			$rpt_cleaners = 1;
			if (ERROR("CODE_INDENT",
				  "code indent should use tabs where possible\n" . $herevet) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/^\+([ \t]+)/"\+" . tabify($1)/e;
			}
		}
		if ($rawline =~ /^\+/ && $rawline =~ / \t/) {
			my $herevet = "$here\n" . cat_vet($rawline) . "\n";
			if (WARN("SPACE_BEFORE_TAB",
				"please, no space before tabs\n" . $herevet) &&
			    $fix) {
				while ($fixed[$fixlinenr] =~
					   s/(^\+.*) {$tabsize,$tabsize}\t/$1\t\t/) {}
				while ($fixed[$fixlinenr] =~
					   s/(^\+.*) +\t/$1\t/) {}
			}
		}
		if ($sline =~ /^\+\s+($Assignment)[^=]/) {
			my $operator = $1;
			if (CHK("ASSIGNMENT_CONTINUATIONS",
				"Assignment operator '$1' should be on the previous line\n" . $hereprev) &&
			    $fix && $prevrawline =~ /^\+/) {
				$fixed[$fixlinenr - 1] .= " $operator";
				$fixed[$fixlinenr] =~ s/\Q$operator\E\s*//;
			}
		}
		if ($rawline =~ /^\+\s*(&&|\|\|)/) {
			my $operator = $1;
			if (CHK("LOGICAL_CONTINUATIONS",
				"Logical continuations should be on the previous line\n" . $hereprev) &&
			    $fix && $prevrawline =~ /^\+/) {
				$prevline =~ /[\s$;]*$/;
				my $line_end = substr($prevrawline, $-[0]);
				$fixed[$fixlinenr - 1] =~ s/\Q$line_end\E$/ $operator$line_end/;
				$fixed[$fixlinenr] =~ s/\Q$operator\E\s*//;
			}
		}
		if ($perl_version_ok &&
		    $sline =~ /^\+\t+( +)(?:$c90_Keywords\b|\{\s*$|\}\s*(?:else\b|while\b|\s*$)|$Declare\s*$Ident\s*[;=])/) {
			my $indent = length($1);
			if ($indent % $tabsize) {
				if (WARN("TABSTOP",
					 "Statements should start on a tabstop\n" . $herecurr) &&
				    $fix) {
					$fixed[$fixlinenr] =~ s@(^\+\t+) +@$1 . "\t" x ($indent/$tabsize)@e;
				}
			}
		}
		if ($perl_version_ok &&
		    $prevline =~ /^\+([ \t]*)((?:$c90_Keywords(?:\s+if)\s*)|(?:$Declare\s*)?(?:$Ident|\(\s*\*\s*$Ident\s*\))\s*|(?:\*\s*)*$Lval\s*=\s*$Ident\s*)\(.*(\&\&|\|\||,)\s*$/) {
			$prevline =~ /^\+(\t*)(.*)$/;
			my $oldindent = $1;
			my $rest = $2;
			my $pos = pos_last_openparen($rest);
			if ($pos >= 0) {
				$line =~ /^(\+| )([ \t]*)/;
				my $newindent = $2;
				my $goodtabindent = $oldindent .
					"\t" x ($pos / $tabsize) .
					" "  x ($pos % $tabsize);
				my $goodspaceindent = $oldindent . " "  x $pos;
				if ($newindent ne $goodtabindent &&
				    $newindent ne $goodspaceindent) {
					if (CHK("PARENTHESIS_ALIGNMENT",
						"Alignment should match open parenthesis\n" . $hereprev) &&
					    $fix && $line =~ /^\+/) {
						$fixed[$fixlinenr] =~
						    s/^\+[ \t]*/\+$goodtabindent/;
					}
				}
			}
		}
		if ($line =~ /^\+(.*)\(\s*$Type\s*\)([ \t]++)((?![={]|\\$|$Attribute|__attribute__))/ &&
		    (!defined($1) || $1 !~ /\b(?:sizeof|__alignof__)\s*$/)) {
			if (CHK("SPACING",
				"No space is necessary after a cast\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~
				    s/(\(\s*$Type\s*\))[ \t]+/$1/;
			}
		}
		if ($realfile =~ m@^(drivers/net/|net/)@ &&
		    $prevrawline =~ /^\+[ \t]*\/\*[ \t]*$/ &&
		    $rawline =~ /^\+[ \t]*\*/ &&
		    $realline > 3) { 
			WARN("NETWORKING_BLOCK_COMMENT_STYLE",
			     "networking block comments don't use an empty /* line, use /* Comment...\n" . $hereprev);
		}
		if ($prevline =~ /$;[ \t]*$/ &&			
		    $prevrawline =~ /^\+.*?\/\*/ &&		
		    $prevrawline !~ /\*\/[ \t]*$/ &&		
		    $rawline =~ /^\+/ &&			
		    $rawline !~ /^\+[ \t]*\*/) {		
			WARN("BLOCK_COMMENT_STYLE",
			     "Block comments use * on subsequent lines\n" . $hereprev);
		}
		if ($rawline !~ m@^\+[ \t]*\*/[ \t]*$@ &&	
		    $rawline !~ m@^\+.*/\*.*\*/[ \t]*$@ &&	
		    $rawline !~ m@^\+.*\*{2,}/[ \t]*$@ &&	
		    $rawline =~ m@^\+[ \t]*.+\*\/[ \t]*$@) {	
			WARN("BLOCK_COMMENT_STYLE",
			     "Block comments use a trailing */ on a separate line\n" . $herecurr);
		}
		if ($prevline =~ /$;[ \t]*$/ &&			
		    $line =~ /^\+[ \t]*$;/ &&			
		    $rawline =~ /^\+[ \t]*\*/ &&		
		    (($prevrawline =~ /^\+.*?\/\*/ &&		
		      $prevrawline !~ /\*\/[ \t]*$/) ||		
		     $prevrawline =~ /^\+[ \t]*\*/)) {		
			my $oldindent;
			$prevrawline =~ m@^\+([ \t]*/?)\*@;
			if (defined($1)) {
				$oldindent = expand_tabs($1);
			} else {
				$prevrawline =~ m@^\+(.*/?)\*@;
				$oldindent = expand_tabs($1);
			}
			$rawline =~ m@^\+([ \t]*)\*@;
			my $newindent = $1;
			$newindent = expand_tabs($newindent);
			if (length($oldindent) ne length($newindent)) {
				WARN("BLOCK_COMMENT_STYLE",
				     "Block comments should align the * on each line\n" . $hereprev);
			}
		}
		if ($prevline =~ /^[\+ ]};?\s*$/ &&
		    $line =~ /^\+/ &&
		    !($line =~ /^\+\s*$/ ||
		      $line =~ /^\+\s*(?:EXPORT_SYMBOL|early_param)/ ||
		      $line =~ /^\+\s*MODULE_/i ||
		      $line =~ /^\+\s*\
		      $line =~ /^\+[a-z_]*init/ ||
		      $line =~ /^\+\s*(?:static\s+)?[A-Z_]*ATTR/ ||
		      $line =~ /^\+\s*DECLARE/ ||
		      $line =~ /^\+\s*builtin_[\w_]*driver/ ||
		      $line =~ /^\+\s*__setup/)) {
			if (CHK("LINE_SPACING",
				"Please use a blank line after function/struct/union/enum declarations\n" . $hereprev) &&
			    $fix) {
				fix_insert_line($fixlinenr, "\+");
			}
		}
		if ($prevline =~ /^[\+ ]\s*$/ &&
		    $line =~ /^\+\s*$/ &&
		    $last_blank_line != ($linenr - 1)) {
			if (CHK("LINE_SPACING",
				"Please don't use multiple blank lines\n" . $hereprev) &&
			    $fix) {
				fix_delete_line($fixlinenr, $rawline);
			}
			$last_blank_line = $linenr;
		}
		if (($prevline =~ /\+(\s+)\S/) && $sline =~ /^\+$1\S/) {
			my $sl = $sline;
			my $pl = $prevline;
			$sl =~ s/\b(?:$Attribute|$Sparse)\b//g;
			$pl =~ s/\b(?:$Attribute|$Sparse)\b//g;
			if (($pl =~ /^\+\s+$Declare\s*$Ident\s*[=,;:\[]/ ||
			     $pl =~ /^\+\s+$Declare\s*\(\s*\*\s*$Ident\s*\)\s*[=,;:\[\(]/ ||
			     $pl =~ /^\+\s+$Ident(?:\s+|\s*\*\s*)$Ident\s*[=,;\[]/ ||
			     $pl =~ /^\+\s+$declaration_macros/) &&
			    !($pl =~ /^\+\s+$c90_Keywords\b/ ||
			      $pl =~ /(?:$Compare|$Assignment|$Operators)\s*$/ ||
			      $pl =~ /(?:\{\s*|\\)$/) &&
			    !($sl =~ /^\+\s+$Declare\s*$Ident\s*[=,;:\[]/ ||
			      $sl =~ /^\+\s+$Declare\s*\(\s*\*\s*$Ident\s*\)\s*[=,;:\[\(]/ ||
			      $sl =~ /^\+\s+$Ident(?:\s+|\s*\*\s*)$Ident\s*[=,;\[]/ ||
			      $sl =~ /^\+\s+$declaration_macros/ ||
			      $sl =~ /^\+\s+(?:static\s+)?(?:const\s+)?(?:union|struct|enum|typedef)\b/ ||
			      $sl =~ /^\+\s+(?:$|[\{\}\.\
			      $sl =~ /^\+\s+$Ident\s*:\s*\d+\s*[,;]/ ||
			      $sl =~ /^\+\s+\(?\s*(?:$Compare|$Assignment|$Operators)/)) {
				if (WARN("LINE_SPACING",
					 "Missing a blank line after declarations\n" . $hereprev) &&
				    $fix) {
					fix_insert_line($fixlinenr, "\+");
				}
			}
		}
		if ($rawline =~ /^\+ / && $line !~ /^\+ *(?:$;|
			my $herevet = "$here\n" . cat_vet($rawline) . "\n";
			if (WARN("LEADING_SPACE",
				 "please, no spaces at the start of a line\n" . $herevet) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/^\+([ \t]+)/"\+" . tabify($1)/e;
			}
		}
		next if ($realfile !~ /\.(h|c)$/);
		if ($line =~ /^\+.*([\[\(])\s*$/) {
			CHK("OPEN_ENDED_LINE",
			    "Lines should not end with a '$1'\n" . $herecurr);
		}
		if ($sline =~ /^\+\{\s*$/ &&
		    $prevline =~ /^\+(?:(?:(?:$Storage|$Inline)\s*)*\s*$Type\s*)?($Ident)\(/) {
			$context_function = $1;
		}
		if ($sline =~ /^\+\}\s*$/) {
			undef $context_function;
		}
		if ($sline =~ /^\+([\t]+)(?:}[ \t]*)?else(?:[ \t]*{)?\s*$/) {
			my $tabs = length($1) + 1;
			if ($prevline =~ /^\+\t{$tabs,$tabs}break\b/ ||
			    ($prevline =~ /^\+\t{$tabs,$tabs}return\b/ &&
			     defined $lines[$linenr] &&
			     $lines[$linenr] !~ /^[ \+]\t{$tabs,$tabs}return/)) {
				WARN("UNNECESSARY_ELSE",
				     "else is not generally useful after a break or return\n" . $hereprev);
			}
		}
		if ($sline =~ /^\+([\t]+)break\s*;\s*$/) {
			my $tabs = $1;
			if ($prevline =~ /^\+$tabs(goto|return|break)\b/) {
				if (WARN("UNNECESSARY_BREAK",
					 "break is not useful after a $1\n" . $hereprev) &&
				    $fix) {
					fix_delete_line($fixlinenr, $rawline);
				}
			}
		}
		if ($rawline =~ /^\+.*\$(Revision|Log|Id)(?:\$|)/) {
			WARN("CVS_KEYWORD",
			     "CVS style keyword markers, these will _not_ be updated\n". $herecurr);
		}
		if ($line =~ /\b(__dev(init|exit)(data|const|))\b/) {
			WARN("HOTPLUG_SECTION",
			     "Using $1 is unnecessary\n" . $herecurr);
		}
		my ($stat, $cond, $line_nr_next, $remain_next, $off_next,
		    $realline_next);
		if ($linenr > $suppress_statement &&
		    $realcnt && $sline =~ /.\s*\S/) {
			($stat, $cond, $line_nr_next, $remain_next, $off_next) =
				ctx_statement_block($linenr, $realcnt, 0);
			$stat =~ s/\n./\n /g;
			$cond =~ s/\n./\n /g;
			my $frag = $stat; $frag =~ s/;+\s*$//;
			if ($frag !~ /(?:{|;)/) {
				$suppress_statement = $line_nr_next;
			}
			$realline_next = $line_nr_next;
			if (defined $realline_next &&
			    (!defined $lines[$realline_next - 1] ||
			     substr($lines[$realline_next - 1], $off_next) =~ /^\s*$/)) {
				$realline_next++;
			}
			my $s = $stat;
			$s =~ s/{.*$//s;
			if ($s =~ /$Ident:\*$/s) {
			} elsif ($s =~ /^.\s*$Ident\s*\(/s) {
			} elsif ($s =~ /^.\s*else\b/s) {
			} elsif ($prev_values eq 'E' && $s =~ /^.\s*(?:$Storage\s+)?(?:$Inline\s+)?(?:const\s+)?((?:\s*$Ident)+?)\b(?:\s+$Sparse)?\s*\**\s*(?:$Ident|\(\*[^\)]*\))(?:\s*$Modifier)?\s*(?:;|=|,|\()/s) {
				my $type = $1;
				$type =~ s/\s+/ /g;
				possible($type, "A:" . $s);
			} elsif ($s =~ /^.(?:$Storage\s+)?(?:$Inline\s+)?(?:const\s+)?($Ident)\b\s*(?!:)/s) {
				possible($1, "B:" . $s);
			}
			while ($s =~ /\(($Ident)(?:\s+$Sparse)*[\s\*]+\s*\)/sg) {
				possible($1, "C:" . $s);
			}
			if ($prev_values eq 'E' && $s =~ /^(.(?:typedef\s*)?(?:(?:$Storage|$Inline)\s*)*\s*$Type\s*(?:\b$Ident|\(\*\s*$Ident\))\s*)\(/s) {
				my ($name_len) = length($1);
				my $ctx = $s;
				substr($ctx, 0, $name_len + 1, '');
				$ctx =~ s/\)[^\)]*$//;
				for my $arg (split(/\s*,\s*/, $ctx)) {
					if ($arg =~ /^(?:const\s+)?($Ident)(?:\s+$Sparse)*\s*\**\s*(:?\b$Ident)?$/s || $arg =~ /^($Ident)$/s) {
						possible($1, "D:" . $s);
					}
				}
			}
		}
		if ($line=~/\bswitch\s*\(.*\)/) {
			my $err = '';
			my $sep = '';
			my @ctx = ctx_block_outer($linenr, $realcnt);
			shift(@ctx);
			for my $ctx (@ctx) {
				my ($clen, $cindent) = line_stats($ctx);
				if ($ctx =~ /^\+\s*(case\s+|default:)/ &&
							$indent != $cindent) {
					$err .= "$sep$ctx\n";
					$sep = '';
				} else {
					$sep = "[...]\n";
				}
			}
			if ($err ne '') {
				ERROR("SWITCH_CASE_INDENT_LEVEL",
				      "switch and case should be at the same indent\n$hereline$err");
			}
		}
		if ($line =~ /(.*)\b((?:if|while|for|switch|(?:[a-z_]+|)for_each[a-z_]+)\s*\(|do\b|else\b)/ && $line !~ /^.\s*\
			my $pre_ctx = "$1$2";
			my ($level, @ctx) = ctx_statement_level($linenr, $realcnt, 0);
			if ($line =~ /^\+\t{6,}/) {
				WARN("DEEP_INDENTATION",
				     "Too many leading tabs - consider code refactoring\n" . $herecurr);
			}
			my $ctx_cnt = $realcnt - $
			my $ctx = join("\n", @ctx);
			my $ctx_ln = $linenr;
			my $ctx_skip = $realcnt;
			while ($ctx_skip > $ctx_cnt || ($ctx_skip == $ctx_cnt &&
					defined $lines[$ctx_ln - 1] &&
					$lines[$ctx_ln - 1] =~ /^-/)) {
				$ctx_skip-- if (!defined $lines[$ctx_ln - 1] || $lines[$ctx_ln - 1] !~ /^-/);
				$ctx_ln++;
			}
			if ($ctx !~ /{\s*/ && defined($lines[$ctx_ln - 1]) && $lines[$ctx_ln - 1] =~ /^\+\s*{/) {
				ERROR("OPEN_BRACE",
				      "that open brace { should be on the previous line\n" .
					"$here\n$ctx\n$rawlines[$ctx_ln - 1]\n");
			}
			if ($level == 0 && $pre_ctx !~ /}\s*while\s*\($/ &&
			    $ctx =~ /\)\s*\;\s*$/ &&
			    defined $lines[$ctx_ln - 1])
			{
				my ($nlength, $nindent) = line_stats($lines[$ctx_ln - 1]);
				if ($nindent > $indent) {
					WARN("TRAILING_SEMICOLON",
					     "trailing semicolon indicates no statements, indent implies otherwise\n" .
						"$here\n$ctx\n$rawlines[$ctx_ln - 1]\n");
				}
			}
		}
		if ($line =~ /\b(?:(?:if|while|for|(?:[a-z_]+|)for_each[a-z_]+)\s*\(|(?:do|else)\b)/ && $line !~ /^.\s*
			($stat, $cond, $line_nr_next, $remain_next, $off_next) =
				ctx_statement_block($linenr, $realcnt, 0)
					if (!defined $stat);
			my ($s, $c) = ($stat, $cond);
			substr($s, 0, length($c), '');
			$s =~ s/$;/ /g;
			$c =~ s/$;/ /g;
			my @newlines = ($c =~ /\n/gs);
			my $cond_lines = 1 + $
			$s =~ s/\n./\n/gs;
			while ($s =~ /\n\s+\\\n/) {
				$cond_lines += $s =~ s/\n\s+\\\n/\n/g;
			}
			my $continuation = 0;
			my $check = 0;
			$s =~ s/^.*\bdo\b//;
			$s =~ s/^\s*{//;
			if ($s =~ s/^\s*\\//) {
				$continuation = 1;
			}
			if ($s =~ s/^\s*?\n//) {
				$check = 1;
				$cond_lines++;
			}
			if (($prevline =~ /^.\s*
			    $prevline =~ /\\\s*$/) && $continuation == 0) {
				$check = 0;
			}
			my $cond_ptr = -1;
			$continuation = 0;
			while ($cond_ptr != $cond_lines) {
				$cond_ptr = $cond_lines;
				if ($s =~ /^\s*\
					$check = 0;
				}
				if ($continuation ||
				    $s =~ /^\s*?\n/ ||
				    $s =~ /^\s*
				    $s =~ /^\s*$Ident\s*:/) {
					$continuation = ($s =~ /^.*?\\\n/) ? 1 : 0;
					if ($s =~ s/^.*?\n//) {
						$cond_lines++;
					}
				}
			}
			my (undef, $sindent) = line_stats("+" . $s);
			my $stat_real = raw_line($linenr, $cond_lines);
			if (!defined($stat_real) ||
			    $stat !~ /^\+/ && $stat_real !~ /^\+/) {
				$check = 0;
			}
			if (defined($stat_real) && $cond_lines > 1) {
				$stat_real = "[...]\n$stat_real";
			}
			if ($check && $s ne '' &&
			    (($sindent % $tabsize) != 0 ||
			     ($sindent < $indent) ||
			     ($sindent == $indent &&
			      ($s !~ /^\s*(?:\}|\{|else\b)/)) ||
			     ($sindent > $indent + $tabsize))) {
				WARN("SUSPECT_CODE_INDENT",
				     "suspect code indent for conditional statements ($indent, $sindent)\n" . $herecurr . "$stat_real\n");
			}
		}
		my $opline = $line; $opline =~ s/^./ /;
		my ($curr_values, $curr_vars) =
				annotate_values($opline . "\n", $prev_values);
		$curr_values = $prev_values . $curr_values;
		if ($dbg_values) {
			my $outline = $opline; $outline =~ s/\t/ /g;
			print "$linenr > .$outline\n";
			print "$linenr > $curr_values\n";
			print "$linenr >  $curr_vars\n";
		}
		$prev_values = substr($curr_values, -1);
		next if ($line =~ /^[^\+]/);
		if ($line =~ /^\+\s*(?:$Declare)?([A-Za-z_][A-Za-z\d_]*)\s*=/) {
			my $var = $1;
			if ($line =~ /^\+\s*(?:$Declare)?$var\s*=\s*(?:$var|\*\s*\(?\s*&\s*\(?\s*$var\s*\)?\s*\)?)\s*[;,]/) {
				WARN("SELF_ASSIGNMENT",
				     "Do not use self-assignments to avoid compiler warnings\n" . $herecurr);
			}
		}
		if ($prevline =~ /^\+.*$Lval\s*(?:\.|->)\s*$/ &&
		    $line =~ /^\+\s*(?!\
			$prevline =~ /($Lval\s*(?:\.|->))\s*$/;
			my $ref = $1;
			$line =~ /^.\s*($Lval)/;
			$ref .= $1;
			$ref =~ s/\s//g;
			WARN("MULTILINE_DEREFERENCE",
			     "Avoid multiple line dereference - prefer '$ref'\n" . $hereprev);
		}
		while ($line =~ m{\b($Declare)\s*(?!char\b|short\b|int\b|long\b)\s*($Ident)?\s*[=,;\[\)\(]}g) {
			my $type = $1;
			my $var = $2;
			$var = "" if (!defined $var);
			if ($type =~ /^(?:(?:$Storage|$Inline|$Attribute)\s+)*((?:un)?signed)((?:\s*\*)*)\s*$/) {
				my $sign = $1;
				my $pointer = $2;
				$pointer = "" if (!defined $pointer);
				if (WARN("UNSPECIFIED_INT",
					 "Prefer '" . trim($sign) . " int" . rtrim($pointer) . "' to bare use of '$sign" . rtrim($pointer) . "'\n" . $herecurr) &&
				    $fix) {
					my $decl = trim($sign) . " int ";
					my $comp_pointer = $pointer;
					$comp_pointer =~ s/\s//g;
					$decl .= $comp_pointer;
					$decl = rtrim($decl) if ($var eq "");
					$fixed[$fixlinenr] =~ s@\b$sign\s*\Q$pointer\E\s*$var\b@$decl$var@;
				}
			}
		}
		if ($dbg_type) {
			if ($line =~ /^.\s*$Declare\s*$/) {
				ERROR("TEST_TYPE",
				      "TEST: is type\n" . $herecurr);
			} elsif ($dbg_type > 1 && $line =~ /^.+($Declare)/) {
				ERROR("TEST_NOT_TYPE",
				      "TEST: is not type ($1 is)\n". $herecurr);
			}
			next;
		}
		if ($dbg_attr) {
			if ($line =~ /^.\s*$Modifier\s*$/) {
				ERROR("TEST_ATTR",
				      "TEST: is attr\n" . $herecurr);
			} elsif ($dbg_attr > 1 && $line =~ /^.+($Modifier)/) {
				ERROR("TEST_NOT_ATTR",
				      "TEST: is not attr ($1 is)\n". $herecurr);
			}
			next;
		}
		if ($line =~ /^.\s*{/ &&
		    $prevline =~ /(?:^|[^=])=\s*$/) {
			if (ERROR("OPEN_BRACE",
				  "that open brace { should be on the previous line\n" . $hereprev) &&
			    $fix && $prevline =~ /^\+/ && $line =~ /^\+/) {
				fix_delete_line($fixlinenr - 1, $prevrawline);
				fix_delete_line($fixlinenr, $rawline);
				my $fixedline = $prevrawline;
				$fixedline =~ s/\s*=\s*$/ = {/;
				fix_insert_line($fixlinenr, $fixedline);
				$fixedline = $line;
				$fixedline =~ s/^(.\s*)\{\s*/$1/;
				fix_insert_line($fixlinenr, $fixedline);
			}
		}
		if ($rawline =~ m{^.\s*\
			my $path = $1;
			if ($path =~ m{//}) {
				ERROR("MALFORMED_INCLUDE",
				      "malformed #include filename\n" . $herecurr);
			}
			if ($path =~ "^uapi/" && $realfile =~ m@\binclude/uapi/@) {
				ERROR("UAPI_INCLUDE",
				      "No #include in ...include/uapi/... should use a uapi/ path prefix\n" . $herecurr);
			}
		}
		if ($line =~ m{//}) {
			if (ERROR("C99_COMMENTS",
				  "do not use C99 // comments\n" . $herecurr) &&
			    $fix) {
				my $line = $fixed[$fixlinenr];
				if ($line =~ /\/\/(.*)$/) {
					my $comment = trim($1);
					$fixed[$fixlinenr] =~ s@\/\/(.*)$@/\* $comment \*/@;
				}
			}
		}
		$line =~ s@//.*@@;
		$opline =~ s@//.*@@;
		if (defined $realline_next &&
		    exists $lines[$realline_next - 1] &&
		    !defined $suppress_export{$realline_next} &&
		    ($lines[$realline_next - 1] =~ /EXPORT_SYMBOL.*\((.*)\)/)) {
			my $name = $1;
			$name =~ s/^\s*($Ident).*/$1/;
			if ($stat =~ /^(?:.\s*}\s*\n)?.([A-Z_]+)\s*\(\s*($Ident)/ &&
			    $name =~ /^${Ident}_$2/) {
				$suppress_export{$realline_next} = 1;
			} elsif ($stat !~ /(?:
				\n.}\s*$|
				^.DEFINE_$Ident\(\Q$name\E\)|
				^.DECLARE_$Ident\(\Q$name\E\)|
				^.LIST_HEAD\(\Q$name\E\)|
				^.(?:$Storage\s+)?$Type\s*\(\s*\*\s*\Q$name\E\s*\)\s*\(|
				\b\Q$name\E(?:\s+$Attribute)*\s*(?:;|=|\[|\()
			    )/x) {
				$suppress_export{$realline_next} = 2;
			} else {
				$suppress_export{$realline_next} = 1;
			}
		}
		if (!defined $suppress_export{$linenr} &&
		    $prevline =~ /^.\s*$/ &&
		    ($line =~ /EXPORT_SYMBOL.*\((.*)\)/)) {
			$suppress_export{$linenr} = 2;
		}
		if (defined $suppress_export{$linenr} &&
		    $suppress_export{$linenr} == 2) {
			WARN("EXPORT_SYMBOL",
			     "EXPORT_SYMBOL(foo); should immediately follow its function/variable\n" . $herecurr);
		}
		if ($line =~ /^\+$Type\s*$Ident(?:\s+$Modifier)*\s*=\s*($zero_initializer)\s*;/ &&
		    !exclude_global_initialisers($realfile)) {
			if (ERROR("GLOBAL_INITIALISERS",
				  "do not initialise globals to $1\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/(^.$Type\s*$Ident(?:\s+$Modifier)*)\s*=\s*$zero_initializer\s*;/$1;/;
			}
		}
		if ($line =~ /^\+.*\bstatic\s.*=\s*($zero_initializer)\s*;/) {
			if (ERROR("INITIALISED_STATIC",
				  "do not initialise statics to $1\n" .
				      $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/(\bstatic\s.*?)\s*=\s*$zero_initializer\s*;/$1;/;
			}
		}
		while ($sline =~ m{(\b$TypeMisordered\b)}g) {
			my $tmp = trim($1);
			WARN("MISORDERED_TYPE",
			     "type '$tmp' should be specified in [[un]signed] [short|int|long|long long] order\n" . $herecurr);
		}
		while ($sline =~ m{\b($TypeMisordered(\s*\*)*|$C90_int_types)\b}g) {
			my $type = trim($1);
			next if ($type !~ /\bint\b/);
			next if ($type !~ /\b(?:short|long\s+long|long)\b/);
			my $new_type = $type;
			$new_type =~ s/\b\s*int\s*\b/ /;
			$new_type =~ s/\b\s*(?:un)?signed\b\s*/ /;
			$new_type =~ s/^const\s+//;
			$new_type = "unsigned $new_type" if ($type =~ /\bunsigned\b/);
			$new_type = "const $new_type" if ($type =~ /^const\b/);
			$new_type =~ s/\s+/ /g;
			$new_type = trim($new_type);
			if (WARN("UNNECESSARY_INT",
				 "Prefer '$new_type' over '$type' as the int is unnecessary\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/\b\Q$type\E\b/$new_type/;
			}
		}
		if ($line =~ /\bstatic\s+const\s+char\s*\*\s*(\w+)\s*\[\s*\]\s*=\s*/) {
			WARN("STATIC_CONST_CHAR_ARRAY",
			     "static const char * array should probably be static const char * const\n" .
				$herecurr);
		}
		if ($line =~ /^\+\s*const\s+(char|unsigned\s+char|_*u8|(?:[us]_)?int8_t)\s+\w+\s*\[\s*(?:\w+\s*)?\]\s*=\s*"/) {
			if (WARN("STATIC_CONST_CHAR_ARRAY",
				 "const array should probably be static const\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/(^.\s*)const\b/${1}static const/;
			}
		}
		if ($line =~ /\bstatic\s+char\s+(\w+)\s*\[\s*\]\s*=\s*"/) {
			WARN("STATIC_CONST_CHAR_ARRAY",
			     "static char array declaration should probably be static const char\n" .
				$herecurr);
		}
		if ($sline =~ /\bconst\s+($BasicType)\s+const\b/) {
			my $found = $1;
			if ($sline =~ /\bconst\s+\Q$found\E\s+const\b\s*\*/) {
				WARN("CONST_CONST",
				     "'const $found const *' should probably be 'const $found * const'\n" . $herecurr);
			} elsif ($sline !~ /\bconst\s+\Q$found\E\s+const\s+\w+\s*\[/) {
				WARN("CONST_CONST",
				     "'const $found const' should probably be 'const $found'\n" . $herecurr);
			}
		}
		if ($sline =~ /^\+\s*const\s+static\s+($Type)\b/ ||
		    $sline =~ /^\+\s*static\s+($BasicType)\s+const\b/) {
			if (WARN("STATIC_CONST",
				 "Move const after static - use 'static const $1'\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/\bconst\s+static\b/static const/;
				$fixed[$fixlinenr] =~ s/\bstatic\s+($BasicType)\s+const\b/static const $1/;
			}
		}
		if ($line =~ /^.\s+(?:static\s+|const\s+)?char\s+\*\s*\w+\s*\[\s*\]\s*=\s*\{/) {
			WARN("STATIC_CONST_CHAR_ARRAY",
			     "char * array declaration might be better as static const\n" .
				$herecurr);
		}
		if ($line =~ m@\bsizeof\s*\(\s*($Lval)\s*\)@) {
			my $array = $1;
			if ($line =~ m@\b(sizeof\s*\(\s*\Q$array\E\s*\)\s*/\s*sizeof\s*\(\s*\Q$array\E\s*\[\s*0\s*\]\s*\))@) {
				my $array_div = $1;
				if (WARN("ARRAY_SIZE",
					 "Prefer ARRAY_SIZE($array)\n" . $herecurr) &&
				    $fix) {
					$fixed[$fixlinenr] =~ s/\Q$array_div\E/ARRAY_SIZE($array)/;
				}
			}
		}
		if ($line =~ /(\b$Type\s*$Ident)\s*\(\s*\)/) {
			if (ERROR("FUNCTION_WITHOUT_ARGS",
				  "Bad function definition - $1() should probably be $1(void)\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/(\b($Type)\s+($Ident))\s*\(\s*\)/$2 $3(void)/;
			}
		}
		if ($line =~ /\btypedef\s/ &&
		    $line !~ /\btypedef\s+$Type\s*\(\s*\*?$Ident\s*\)\s*\(/ &&
		    $line !~ /\btypedef\s+$Type\s+$Ident\s*\(/ &&
		    $line !~ /\b$typeTypedefs\b/ &&
		    $line !~ /\b__bitwise\b/) {
			WARN("NEW_TYPEDEFS",
			     "do not add new typedefs\n" . $herecurr);
		}
		while ($line =~ m{(\($NonptrType(\s*(?:$Modifier\b\s*|\*\s*)+)\))}g) {
			my ($ident, $from, $to) = ($1, $2, $2);
			$to =~ s/^(\S)/ $1/;
			$to =~ s/\s+$//;
			while ($to =~ s/\*\s+\*/\*\*/) {
			}
			if ($from ne $to) {
				if (ERROR("POINTER_LOCATION",
					  "\"(foo$from)\" should be \"(foo$to)\"\n" .  $herecurr) &&
				    $fix) {
					my $sub_from = $ident;
					my $sub_to = $ident;
					$sub_to =~ s/\Q$from\E/$to/;
					$fixed[$fixlinenr] =~
					    s@\Q$sub_from\E@$sub_to@;
				}
			}
		}
		while ($line =~ m{(\b$NonptrType(\s*(?:$Modifier\b\s*|\*\s*)+)($Ident))}g) {
			my ($match, $from, $to, $ident) = ($1, $2, $2, $3);
			$to =~ s/^(\S)/ $1/;
			$to =~ s/\s+$//;
			while ($to =~ s/\*\s+\*/\*\*/) {
			}
			$to =~ s/(\b$Modifier$)/$1 /;
			if ($from ne $to && $ident !~ /^$Modifier$/) {
				if (ERROR("POINTER_LOCATION",
					  "\"foo${from}bar\" should be \"foo${to}bar\"\n" .  $herecurr) &&
				    $fix) {
					my $sub_from = $match;
					my $sub_to = $match;
					$sub_to =~ s/\Q$from\E/$to/;
					$fixed[$fixlinenr] =~
					    s@\Q$sub_from\E@$sub_to@;
				}
			}
		}
		if ($line =~ /\b(?!AA_|BUILD_|DCCP_|IDA_|KVM_|RWLOCK_|snd_|SPIN_)(?:[a-zA-Z_]*_)?BUG(?:_ON)?(?:_[A-Z_]+)?\s*\(/) {
			my $msg_level = \&WARN;
			$msg_level = \&CHK if ($file);
			&{$msg_level}("AVOID_BUG",
				      "Do not crash the kernel unless it is absolutely unavoidable--use WARN_ON_ONCE() plus recovery code (if feasible) instead of BUG() or variants\n" . $herecurr);
		}
		if ($line =~ /\bLINUX_VERSION_CODE\b/) {
			WARN("LINUX_VERSION_CODE",
			     "LINUX_VERSION_CODE should be avoided, code should be for the version to which it is merged\n" . $herecurr);
		}
		if ($line =~ /\bprintk_ratelimit\s*\(/) {
			WARN("PRINTK_RATELIMITED",
			     "Prefer printk_ratelimited or pr_<level>_ratelimited to printk_ratelimit\n" . $herecurr);
		}
		if ($line =~ /\bprintk\s*\(\s*(?!KERN_[A-Z]+\b)/) {
			WARN("PRINTK_WITHOUT_KERN_LEVEL",
			     "printk() should include KERN_<LEVEL> facility level\n" . $herecurr);
		}
		if ($line =~ /\b(printk(_once|_ratelimited)?)\s*\(\s*KERN_([A-Z]+)/) {
			my $printk = $1;
			my $modifier = $2;
			my $orig = $3;
			$modifier = "" if (!defined($modifier));
			my $level = lc($orig);
			$level = "warn" if ($level eq "warning");
			my $level2 = $level;
			$level2 = "dbg" if ($level eq "debug");
			$level .= $modifier;
			$level2 .= $modifier;
			WARN("PREFER_PR_LEVEL",
			     "Prefer [subsystem eg: netdev]_$level2([subsystem]dev, ... then dev_$level2(dev, ... then pr_$level(...  to $printk(KERN_$orig ...\n" . $herecurr);
		}
		if ($line =~ /\bdev_printk\s*\(\s*KERN_([A-Z]+)/) {
			my $orig = $1;
			my $level = lc($orig);
			$level = "warn" if ($level eq "warning");
			$level = "dbg" if ($level eq "debug");
			WARN("PREFER_DEV_LEVEL",
			     "Prefer dev_$level(... to dev_printk(KERN_$orig, ...\n" . $herecurr);
		}
		if ($line =~ /\b(trace_printk|trace_puts|ftrace_vprintk)\s*\(/) {
			WARN("TRACE_PRINTK",
			     "Do not use $1() in production code (this can be ignored if built only with a debug config option)\n" . $herecurr);
		}
		if ($line =~ /\bENOSYS\b/) {
			WARN("ENOSYS",
			     "ENOSYS means 'invalid syscall nr' and nothing else\n" . $herecurr);
		}
		if (!$file && $line =~ /\bENOTSUPP\b/) {
			if (WARN("ENOTSUPP",
				 "ENOTSUPP is not a SUSV4 error code, prefer EOPNOTSUPP\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/\bENOTSUPP\b/EOPNOTSUPP/;
			}
		}
		if ($perl_version_ok &&
		    $sline =~ /$Type\s*$Ident\s*$balanced_parens\s*\{/ &&
		    $sline !~ /\
		    $sline !~ /}/) {
			if (ERROR("OPEN_BRACE",
				  "open brace '{' following function definitions go on the next line\n" . $herecurr) &&
			    $fix) {
				fix_delete_line($fixlinenr, $rawline);
				my $fixed_line = $rawline;
				$fixed_line =~ /(^..*$Type\s*$Ident\(.*\)\s*)\{(.*)$/;
				my $line1 = $1;
				my $line2 = $2;
				fix_insert_line($fixlinenr, ltrim($line1));
				fix_insert_line($fixlinenr, "\+{");
				if ($line2 !~ /^\s*$/) {
					fix_insert_line($fixlinenr, "\+\t" . trim($line2));
				}
			}
		}
		if ($line =~ /^.\s*{/ &&
		    $prevline =~ /^.\s*(?:typedef\s+)?(enum|union|struct)(?:\s+$Ident)?\s*$/) {
			if (ERROR("OPEN_BRACE",
				  "open brace '{' following $1 go on the same line\n" . $hereprev) &&
			    $fix && $prevline =~ /^\+/ && $line =~ /^\+/) {
				fix_delete_line($fixlinenr - 1, $prevrawline);
				fix_delete_line($fixlinenr, $rawline);
				my $fixedline = rtrim($prevrawline) . " {";
				fix_insert_line($fixlinenr, $fixedline);
				$fixedline = $rawline;
				$fixedline =~ s/^(.\s*)\{\s*/$1\t/;
				if ($fixedline !~ /^\+\s*$/) {
					fix_insert_line($fixlinenr, $fixedline);
				}
			}
		}
		if ($line =~ /^.\s*(?:typedef\s+)?(enum|union|struct)(?:\s+$Ident){1,2}[=\{]/) {
			if (WARN("SPACING",
				 "missing space after $1 definition\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~
				    s/^(.\s*(?:typedef\s+)?(?:enum|union|struct)(?:\s+$Ident){1,2})([=\{])/$1 $2/;
			}
		}
		if ($line =~ /^.\s*($Declare)\((\s*)\*(\s*)($Ident)(\s*)\)(\s*)\(/) {
			my $declare = $1;
			my $pre_pointer_space = $2;
			my $post_pointer_space = $3;
			my $funcname = $4;
			my $post_funcname_space = $5;
			my $pre_args_space = $6;
			my $post_declare_space = "";
			if ($declare =~ /(\s+)$/) {
				$post_declare_space = $1;
				$declare = rtrim($declare);
			}
			if ($declare !~ /\*$/ && $post_declare_space =~ /^$/) {
				WARN("SPACING",
				     "missing space after return type\n" . $herecurr);
				$post_declare_space = " ";
			}
			if (defined $pre_pointer_space &&
			    $pre_pointer_space =~ /^\s/) {
				WARN("SPACING",
				     "Unnecessary space after function pointer open parenthesis\n" . $herecurr);
			}
			if (defined $post_pointer_space &&
			    $post_pointer_space =~ /^\s/) {
				WARN("SPACING",
				     "Unnecessary space before function pointer name\n" . $herecurr);
			}
			if (defined $post_funcname_space &&
			    $post_funcname_space =~ /^\s/) {
				WARN("SPACING",
				     "Unnecessary space after function pointer name\n" . $herecurr);
			}
			if (defined $pre_args_space &&
			    $pre_args_space =~ /^\s/) {
				WARN("SPACING",
				     "Unnecessary space before function pointer arguments\n" . $herecurr);
			}
			if (show_type("SPACING") && $fix) {
				$fixed[$fixlinenr] =~
				    s/^(.\s*)$Declare\s*\(\s*\*\s*$Ident\s*\)\s*\(/$1 . $declare . $post_declare_space . '(*' . $funcname . ')('/ex;
			}
		}
		while ($line =~ /(.*?\s)\[/g) {
			my ($where, $prefix) = ($-[1], $1);
			if ($prefix !~ /$Type\s+$/ &&
			    ($where != 0 || $prefix !~ /^.\s+$/) &&
			    $prefix !~ /[{,:]\s+$/) {
				if (ERROR("BRACKET_SPACE",
					  "space prohibited before open square bracket '['\n" . $herecurr) &&
				    $fix) {
				    $fixed[$fixlinenr] =~
					s/^(\+.*?)\s+\[/$1\[/;
				}
			}
		}
		while ($line =~ /($Ident)\s+\(/g) {
			my $name = $1;
			my $ctx_before = substr($line, 0, $-[1]);
			my $ctx = "$ctx_before$name";
			if ($name =~ /^(?:
				if|for|while|switch|return|case|
				volatile|__volatile__|
				__attribute__|format|__extension__|
				asm|__asm__|scoped_guard)$/x)
			{
			} elsif ($ctx_before =~ /^.\s*\
			} elsif ($ctx =~ /^.\s*\
			} elsif ($ctx =~ /$Type$/) {
			} else {
				if (WARN("SPACING",
					 "space prohibited between function name and open parenthesis '('\n" . $herecurr) &&
					     $fix) {
					$fixed[$fixlinenr] =~
					    s/\b$name\s+\(/$name\(/;
				}
			}
		}
		if (!($line=~/\
			my $fixed_line = "";
			my $line_fixed = 0;
			my $ops = qr{
				<<=|>>=|<=|>=|==|!=|
				\+=|-=|\*=|\/=|%=|\^=|\|=|&=|
				=>|->|<<|>>|<|>|=|!|~|
				&&|\|\||,|\^|\+\+|--|&|\||\+|-|\*|\/|%|
				\?:|\?|:
			}x;
			my @elements = split(/($ops|;)/, $opline);
			my @fix_elements = ();
			my $off = 0;
			foreach my $el (@elements) {
				push(@fix_elements, substr($rawline, $off, length($el)));
				$off += length($el);
			}
			$off = 0;
			my $blank = copy_spacing($opline);
			my $last_after = -1;
			for (my $n = 0; $n < $
				my $good = $fix_elements[$n] . $fix_elements[$n + 1];
				$off += length($elements[$n]);
				my $ca = substr($opline, 0, $off);
				my $cc = '';
				if (length($opline) >= ($off + length($elements[$n + 1]))) {
					$cc = substr($opline, $off + length($elements[$n + 1]));
				}
				my $cb = "$ca$;$cc";
				my $a = '';
				$a = 'V' if ($elements[$n] ne '');
				$a = 'W' if ($elements[$n] =~ /\s$/);
				$a = 'C' if ($elements[$n] =~ /$;$/);
				$a = 'B' if ($elements[$n] =~ /(\[|\()$/);
				$a = 'O' if ($elements[$n] eq '');
				$a = 'E' if ($ca =~ /^\s*$/);
				my $op = $elements[$n + 1];
				my $c = '';
				if (defined $elements[$n + 2]) {
					$c = 'V' if ($elements[$n + 2] ne '');
					$c = 'W' if ($elements[$n + 2] =~ /^\s/);
					$c = 'C' if ($elements[$n + 2] =~ /^$;/);
					$c = 'B' if ($elements[$n + 2] =~ /^(\)|\]|;)/);
					$c = 'O' if ($elements[$n + 2] eq '');
					$c = 'E' if ($elements[$n + 2] =~ /^\s*\\$/);
				} else {
					$c = 'E';
				}
				my $ctx = "${a}x${c}";
				my $at = "(ctx:$ctx)";
				my $ptr = substr($blank, 0, $off) . "^";
				my $hereptr = "$hereline$ptr\n";
				my $op_type = substr($curr_values, $off + 1, 1);
				my $opv = $op . substr($curr_vars, $off, 1);
				if ($op_type ne 'V' &&
				    $ca =~ /\s$/ && $cc =~ /^\s*[,\)]/) {
				} elsif ($op eq ';') {
					if ($ctx !~ /.x[WEBC]/ &&
					    $cc !~ /^\\/ && $cc !~ /^;/) {
						if (ERROR("SPACING",
							  "space required after that '$op' $at\n" . $hereptr)) {
							$good = $fix_elements[$n] . trim($fix_elements[$n + 1]) . " ";
							$line_fixed = 1;
						}
					}
				} elsif ($op eq '//') {
				} elsif ($opv eq ':B') {
				} elsif ($op eq '->') {
					if ($ctx =~ /Wx.|.xW/) {
						if (ERROR("SPACING",
							  "spaces prohibited around that '$op' $at\n" . $hereptr)) {
							$good = rtrim($fix_elements[$n]) . trim($fix_elements[$n + 1]);
							if (defined $fix_elements[$n + 2]) {
								$fix_elements[$n + 2] =~ s/^\s+//;
							}
							$line_fixed = 1;
						}
					}
				} elsif ($op eq ',') {
					my $rtrim_before = 0;
					my $space_after = 0;
					if ($ctx =~ /Wx./) {
						if (ERROR("SPACING",
							  "space prohibited before that '$op' $at\n" . $hereptr)) {
							$line_fixed = 1;
							$rtrim_before = 1;
						}
					}
					if ($ctx !~ /.x[WEC]/ && $cc !~ /^}/) {
						if (ERROR("SPACING",
							  "space required after that '$op' $at\n" . $hereptr)) {
							$line_fixed = 1;
							$last_after = $n;
							$space_after = 1;
						}
					}
					if ($rtrim_before || $space_after) {
						if ($rtrim_before) {
							$good = rtrim($fix_elements[$n]) . trim($fix_elements[$n + 1]);
						} else {
							$good = $fix_elements[$n] . trim($fix_elements[$n + 1]);
						}
						if ($space_after) {
							$good .= " ";
						}
					}
				} elsif ($opv eq '*_') {
				} elsif ($op eq '!' || $op eq '~' ||
					 $opv eq '*U' || $opv eq '-U' ||
					 $opv eq '&U' || $opv eq '&&U') {
					if ($ctx !~ /[WEBC]x./ && $ca !~ /(?:\)|!|~|\*|-|\&|\||\+\+|\-\-|\{)$/) {
						if (ERROR("SPACING",
							  "space required before that '$op' $at\n" . $hereptr)) {
							if ($n != $last_after + 2) {
								$good = $fix_elements[$n] . " " . ltrim($fix_elements[$n + 1]);
								$line_fixed = 1;
							}
						}
					}
					if ($op eq '*' && $cc =~/\s*$Modifier\b/) {
					} elsif ($ctx =~ /.xW/) {
						if (ERROR("SPACING",
							  "space prohibited after that '$op' $at\n" . $hereptr)) {
							$good = $fix_elements[$n] . rtrim($fix_elements[$n + 1]);
							if (defined $fix_elements[$n + 2]) {
								$fix_elements[$n + 2] =~ s/^\s+//;
							}
							$line_fixed = 1;
						}
					}
				} elsif ($op eq '++' or $op eq '--') {
					if ($ctx !~ /[WEOBC]x[^W]/ && $ctx !~ /[^W]x[WOBEC]/) {
						if (ERROR("SPACING",
							  "space required one side of that '$op' $at\n" . $hereptr)) {
							$good = $fix_elements[$n] . trim($fix_elements[$n + 1]) . " ";
							$line_fixed = 1;
						}
					}
					if ($ctx =~ /Wx[BE]/ ||
					    ($ctx =~ /Wx./ && $cc =~ /^;/)) {
						if (ERROR("SPACING",
							  "space prohibited before that '$op' $at\n" . $hereptr)) {
							$good = rtrim($fix_elements[$n]) . trim($fix_elements[$n + 1]);
							$line_fixed = 1;
						}
					}
					if ($ctx =~ /ExW/) {
						if (ERROR("SPACING",
							  "space prohibited after that '$op' $at\n" . $hereptr)) {
							$good = $fix_elements[$n] . trim($fix_elements[$n + 1]);
							if (defined $fix_elements[$n + 2]) {
								$fix_elements[$n + 2] =~ s/^\s+//;
							}
							$line_fixed = 1;
						}
					}
				} elsif ($op eq '<<' or $op eq '>>' or
					 $op eq '&' or $op eq '^' or $op eq '|' or
					 $op eq '+' or $op eq '-' or
					 $op eq '*' or $op eq '/' or
					 $op eq '%')
				{
					if ($check) {
						if (defined $fix_elements[$n + 2] && $ctx !~ /[EW]x[EW]/) {
							if (CHK("SPACING",
								"spaces preferred around that '$op' $at\n" . $hereptr)) {
								$good = rtrim($fix_elements[$n]) . " " . trim($fix_elements[$n + 1]) . " ";
								$fix_elements[$n + 2] =~ s/^\s+//;
								$line_fixed = 1;
							}
						} elsif (!defined $fix_elements[$n + 2] && $ctx !~ /Wx[OE]/) {
							if (CHK("SPACING",
								"space preferred before that '$op' $at\n" . $hereptr)) {
								$good = rtrim($fix_elements[$n]) . " " . trim($fix_elements[$n + 1]);
								$line_fixed = 1;
							}
						}
					} elsif ($ctx =~ /Wx[^WCE]|[^WCE]xW/) {
						if (ERROR("SPACING",
							  "need consistent spacing around '$op' $at\n" . $hereptr)) {
							$good = rtrim($fix_elements[$n]) . " " . trim($fix_elements[$n + 1]) . " ";
							if (defined $fix_elements[$n + 2]) {
								$fix_elements[$n + 2] =~ s/^\s+//;
							}
							$line_fixed = 1;
						}
					}
				} elsif ($opv eq ':C' || $opv eq ':L') {
					if ($ctx =~ /Wx./ and $realfile !~ m@.*\.lds\.h$@) {
						if (ERROR("SPACING",
							  "space prohibited before that '$op' $at\n" . $hereptr)) {
							$good = rtrim($fix_elements[$n]) . trim($fix_elements[$n + 1]);
							$line_fixed = 1;
						}
					}
				} elsif ($ctx !~ /[EWC]x[CWE]/) {
					my $ok = 0;
					if (($op eq '<' &&
					     $cc =~ /^\S+\@\S+>/) ||
					    ($op eq '>' &&
					     $ca =~ /<\S+\@\S+$/))
					{
						$ok = 1;
					}
					if (($op eq ':') &&
					    ($ca =~ /:$/ || $cc =~ /^:/)) {
						$ok = 1;
					}
					if ($ok == 0) {
						my $msg_level = \&ERROR;
						$msg_level = \&CHK if (($op eq '?:' || $op eq '?' || $op eq ':') && $ctx =~ /VxV/);
						if (&{$msg_level}("SPACING",
								  "spaces required around that '$op' $at\n" . $hereptr)) {
							$good = rtrim($fix_elements[$n]) . " " . trim($fix_elements[$n + 1]) . " ";
							if (defined $fix_elements[$n + 2]) {
								$fix_elements[$n + 2] =~ s/^\s+//;
							}
							$line_fixed = 1;
						}
					}
				}
				$off += length($elements[$n + 1]);
				$fixed_line = $fixed_line . $good;
			}
			if (($
				$fixed_line = $fixed_line . $fix_elements[$
			}
			if ($fix && $line_fixed && $fixed_line ne $fixed[$fixlinenr]) {
				$fixed[$fixlinenr] = $fixed_line;
			}
		}
		if ($line =~ /^\+.*\S\s+;\s*$/) {
			if (WARN("SPACING",
				 "space prohibited before semicolon\n" . $herecurr) &&
			    $fix) {
				1 while $fixed[$fixlinenr] =~
				    s/^(\+.*\S)\s+;/$1;/;
			}
		}
		if ($line =~ /^.\s*$Lval\s*=\s*$Lval\s*=(?!=)/) {
			CHK("MULTIPLE_ASSIGNMENTS",
			    "multiple assignments should be avoided\n" . $herecurr);
		}
		if (($line =~ /\(.*\)\{/ && $line !~ /\($Type\)\{/) ||
		    $line =~ /\b(?:else|do)\{/) {
			if (ERROR("SPACING",
				  "space required before the open brace '{'\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/^(\+.*(?:do|else|\)))\{/$1 {/;
			}
		}
		if ($line =~ /}(?!(?:,|;|\)|\}))\S/) {
			if (ERROR("SPACING",
				  "space required after that close brace '}'\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~
				    s/}((?!(?:,|;|\)))\S)/} $1/;
			}
		}
		if ($line =~ /\[\s/ && $line !~ /\[\s*$/) {
			if (ERROR("SPACING",
				  "space prohibited after that open square bracket '['\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~
				    s/\[\s+/\[/;
			}
		}
		if ($line =~ /\s\]/) {
			if (ERROR("SPACING",
				  "space prohibited before that close square bracket ']'\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~
				    s/\s+\]/\]/;
			}
		}
		if ($line =~ /\(\s/ && $line !~ /\(\s*(?:\\)?$/ &&
		    $line !~ /for\s*\(\s+;/) {
			if (ERROR("SPACING",
				  "space prohibited after that open parenthesis '('\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~
				    s/\(\s+/\(/;
			}
		}
		if ($line =~ /(\s+)\)/ && $line !~ /^.\s*\)/ &&
		    $line !~ /for\s*\(.*;\s+\)/ &&
		    $line !~ /:\s+\)/) {
			if (ERROR("SPACING",
				  "space prohibited before that close parenthesis ')'\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~
				    s/\s+\)/\)/;
			}
		}
		while ($line =~ /(?:[^&]&\s*|\*)\(\s*($Ident\s*(?:$Member\s*)+)\s*\)/g) {
			my $var = $1;
			if (CHK("UNNECESSARY_PARENTHESES",
				"Unnecessary parentheses around $var\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/\(\s*\Q$var\E\s*\)/$var/;
			}
		}
		if ($line =~ /(\bif\s*|)(\(\s*$Ident\s*(?:$Member\s*)+\))[ \t]*\(/ && $1 !~ /^if/) {
			my $var = $2;
			if (CHK("UNNECESSARY_PARENTHESES",
				"Unnecessary parentheses around function pointer $var\n" . $herecurr) &&
			    $fix) {
				my $var2 = deparenthesize($var);
				$var2 =~ s/\s//g;
				$fixed[$fixlinenr] =~ s/\Q$var\E/$var2/;
			}
		}
		if (($realfile !~ m@^(?:drivers/staging/)@ || $check_orig) &&
		    $perl_version_ok && defined($stat) &&
		    $stat =~ /(^.\s*if\s*($balanced_parens))/) {
			my $if_stat = $1;
			my $test = substr($2, 1, -1);
			my $herectx;
			while ($test =~ /(?:^|[^\w\&\!\~])+\s*\(\s*([\&\!\~]?\s*$Lval\s*(?:$Compare\s*$FuncArg)?)\s*\)/g) {
				my $match = $1;
				next if ($match =~ /^\s*\w+\s*$/);
				if (!defined($herectx)) {
					$herectx = $here . "\n";
					my $cnt = statement_rawlines($if_stat);
					for (my $n = 0; $n < $cnt; $n++) {
						my $rl = raw_line($linenr, $n);
						$herectx .=  $rl . "\n";
						last if $rl =~ /^[ \+].*\{/;
					}
				}
				CHK("UNNECESSARY_PARENTHESES",
				    "Unnecessary parentheses around '$match'\n" . $herectx);
			}
		}
		if ($sline =~ /^.\s+[A-Za-z_][A-Za-z\d_]*:(?!\s*\d+)/ &&
		    $sline !~ /^. [A-Za-z\d_][A-Za-z\d_]*:/ &&
		    $sline !~ /^.\s+default:/) {
			if (WARN("INDENTED_LABEL",
				 "labels should not be indented\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~
				    s/^(.)\s+/$1/;
			}
		}
		if (defined($stat) &&
		    $stat =~ /^\+\s*(?:$Lval\s*$Assignment\s*)?$FuncArg\s*,\s*(?:$Lval\s*$Assignment\s*)?$FuncArg\s*;\s*$/) {
			my $cnt = statement_rawlines($stat);
			my $herectx = get_stat_here($linenr, $cnt, $here);
			WARN("SUSPECT_COMMA_SEMICOLON",
			     "Possible comma where semicolon could be used\n" . $herectx);
		}
		if (defined($stat) && $stat =~ /^.\s*return(\s*)\(/s) {
			my $spacing = $1;
			if ($perl_version_ok &&
			    $stat =~ /^.\s*return\s*($balanced_parens)\s*;\s*$/) {
				my $value = $1;
				$value = deparenthesize($value);
				if ($value =~ m/^\s*$FuncArg\s*(?:\?|$)/) {
					ERROR("RETURN_PARENTHESES",
					      "return is not a function, parentheses are not required\n" . $herecurr);
				}
			} elsif ($spacing !~ /\s+/) {
				ERROR("SPACING",
				      "space required before the open parenthesis '('\n" . $herecurr);
			}
		}
		if ($sline =~ /^[ \+]}\s*$/ &&
		    $prevline =~ /^\+\treturn\s*;\s*$/ &&
		    $linenr >= 3 &&
		    $lines[$linenr - 3] =~ /^[ +]/ &&
		    $lines[$linenr - 3] !~ /^[ +]\s*$Ident\s*:/) {
			WARN("RETURN_VOID",
			     "void function return statements are not generally useful\n" . $hereprev);
		}
		if ($perl_version_ok &&
		    $line =~ /\bif\s*((?:\(\s*){2,})/) {
			my $openparens = $1;
			my $count = $openparens =~ tr@\(@\(@;
			my $msg = "";
			if ($line =~ /\bif\s*(?:\(\s*){$count,$count}$LvalOrFunc\s*($Compare)\s*$LvalOrFunc(?:\s*\)){$count,$count}/) {
				my $comp = $4;	
				$msg = " - maybe == should be = ?" if ($comp eq "==");
				WARN("UNNECESSARY_PARENTHESES",
				     "Unnecessary parentheses$msg\n" . $herecurr);
			}
		}
		if ($perl_version_ok &&
		    $line =~ /^\+(.*)\b($Constant|[A-Z_][A-Z0-9_]*)\s*($Compare)\s*($LvalOrFunc)/) {
			my $lead = $1;
			my $const = $2;
			my $comp = $3;
			my $to = $4;
			my $newcomp = $comp;
			if ($lead !~ /(?:$Operators|\.)\s*$/ &&
			    $to !~ /^(?:Constant|[A-Z_][A-Z0-9_]*)$/ &&
			    WARN("CONSTANT_COMPARISON",
				 "Comparisons should place the constant on the right side of the test\n" . $herecurr) &&
			    $fix) {
				if ($comp eq "<") {
					$newcomp = ">";
				} elsif ($comp eq "<=") {
					$newcomp = ">=";
				} elsif ($comp eq ">") {
					$newcomp = "<";
				} elsif ($comp eq ">=") {
					$newcomp = "<=";
				}
				$fixed[$fixlinenr] =~ s/\(\s*\Q$const\E\s*$Compare\s*\Q$to\E\s*\)/($to $newcomp $const)/;
			}
		}
		if ($sline =~ /\breturn(?:\s*\(+\s*|\s+)(E[A-Z]+)(?:\s*\)+\s*|\s*)[;:,]/) {
			my $name = $1;
			if ($name ne 'EOF' && $name ne 'ERROR' && $name !~ /^EPOLL/) {
				WARN("USE_NEGATIVE_ERRNO",
				     "return of an errno should typically be negative (ie: return -$1)\n" . $herecurr);
			}
		}
		if ($line =~ /\b(if|while|for|switch)\(/) {
			if (ERROR("SPACING",
				  "space required before the open parenthesis '('\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~
				    s/\b(if|while|for|switch)\(/$1 \(/;
			}
		}
		if ($line =~ /do\s*(?!{)/) {
			($stat, $cond, $line_nr_next, $remain_next, $off_next) =
				ctx_statement_block($linenr, $realcnt, 0)
					if (!defined $stat);
			my ($stat_next) = ctx_statement_block($line_nr_next,
						$remain_next, $off_next);
			$stat_next =~ s/\n./\n /g;
			if ($stat_next =~ /^\s*while\b/) {
				my ($whitespace) =
					($stat_next =~ /^((?:\s*\n[+-])*\s*)/s);
				my $offset =
					statement_rawlines($whitespace) - 1;
				$suppress_whiletrailers{$line_nr_next +
								$offset} = 1;
			}
		}
		if (!defined $suppress_whiletrailers{$linenr} &&
		    defined($stat) && defined($cond) &&
		    $line =~ /\b(?:if|while|for)\s*\(/ && $line !~ /^.\s*
			my ($s, $c) = ($stat, $cond);
			my $fixed_assign_in_if = 0;
			if ($c =~ /\bif\s*\(.*[^<>!=]=[^=].*/s) {
				if (ERROR("ASSIGN_IN_IF",
					  "do not use assignment in if condition\n" . $herecurr) &&
				    $fix && $perl_version_ok) {
					if ($rawline =~ /^\+(\s+)if\s*\(\s*(\!)?\s*\(\s*(($Lval)\s*=\s*$LvalOrFunc)\s*\)\s*(?:($Compare)\s*($FuncArg))?\s*\)\s*(\{)?\s*$/) {
						my $space = $1;
						my $not = $2;
						my $statement = $3;
						my $assigned = $4;
						my $test = $8;
						my $against = $9;
						my $brace = $15;
						fix_delete_line($fixlinenr, $rawline);
						fix_insert_line($fixlinenr, "$space$statement;");
						my $newline = "${space}if (";
						$newline .= '!' if defined($not);
						$newline .= '(' if (defined $not && defined($test) && defined($against));
						$newline .= "$assigned";
						$newline .= " $test $against" if (defined($test) && defined($against));
						$newline .= ')' if (defined $not && defined($test) && defined($against));
						$newline .= ')';
						$newline .= " {" if (defined($brace));
						fix_insert_line($fixlinenr + 1, $newline);
						$fixed_assign_in_if = 1;
					}
				}
			}
			substr($s, 0, length($c), '');
			$s =~ s/\n.*//g;
			$s =~ s/$;//g;	
			if (length($c) && $s !~ /^\s*{?\s*\\*\s*$/ &&
			    $c !~ /}\s*while\s*/)
			{
				my @newlines = ($c =~ /\n/gs);
				my $cond_lines = 1 + $
				my $stat_real = '';
				$stat_real = raw_line($linenr, $cond_lines)
							. "\n" if ($cond_lines);
				if (defined($stat_real) && $cond_lines > 1) {
					$stat_real = "[...]\n$stat_real";
				}
				if (ERROR("TRAILING_STATEMENTS",
					  "trailing statements should be on next line\n" . $herecurr . $stat_real) &&
				    !$fixed_assign_in_if &&
				    $cond_lines == 0 &&
				    $fix && $perl_version_ok &&
				    $fixed[$fixlinenr] =~ /^\+(\s*)((?:if|while|for)\s*$balanced_parens)\s*(.*)$/) {
					my $indent = $1;
					my $test = $2;
					my $rest = rtrim($4);
					if ($rest =~ /;$/) {
						$fixed[$fixlinenr] = "\+$indent$test";
						fix_insert_line($fixlinenr + 1, "$indent\t$rest");
					}
				}
			}
		}
		if ($line =~ /
			(?:
				(?:\[|\(|\&\&|\|\|)
				\s*0[xX][0-9]+\s*
				(?:\&\&|\|\|)
			|
				(?:\&\&|\|\|)
				\s*0[xX][0-9]+\s*
				(?:\&\&|\|\||\)|\])
			)/x)
		{
			WARN("HEXADECIMAL_BOOLEAN_TEST",
			     "boolean test with hexadecimal, perhaps just 1 \& or \|?\n" . $herecurr);
		}
		if ($line =~ /^.\s*(?:}\s*)?else\b(.*)/) {
			my $s = $1;
			$s =~ s/$;//g;	
			if ($s !~ /^\s*(?:\sif|(?:{|)\s*\\?\s*$)/) {
				ERROR("TRAILING_STATEMENTS",
				      "trailing statements should be on next line\n" . $herecurr);
			}
		}
		if ($line =~ /}\s*if\b/) {
			ERROR("TRAILING_STATEMENTS",
			      "trailing statements should be on next line (or did you mean 'else if'?)\n" .
				$herecurr);
		}
		if ($line =~ /^.\s*(?:case\s*.*|default\s*):/g &&
		    $line !~ /\G(?:
			(?:\s*$;*)(?:\s*{)?(?:\s*$;*)(?:\s*\\)?\s*$|
			\s*return\s+
		    )/xg)
		{
			ERROR("TRAILING_STATEMENTS",
			      "trailing statements should be on next line\n" . $herecurr);
		}
		if ($prevline=~/}\s*$/ and $line=~/^.\s*else\s*/ &&
		    $previndent == $indent) {
			if (ERROR("ELSE_AFTER_BRACE",
				  "else should follow close brace '}'\n" . $hereprev) &&
			    $fix && $prevline =~ /^\+/ && $line =~ /^\+/) {
				fix_delete_line($fixlinenr - 1, $prevrawline);
				fix_delete_line($fixlinenr, $rawline);
				my $fixedline = $prevrawline;
				$fixedline =~ s/}\s*$//;
				if ($fixedline !~ /^\+\s*$/) {
					fix_insert_line($fixlinenr, $fixedline);
				}
				$fixedline = $rawline;
				$fixedline =~ s/^(.\s*)else/$1} else/;
				fix_insert_line($fixlinenr, $fixedline);
			}
		}
		if ($prevline=~/}\s*$/ and $line=~/^.\s*while\s*/ &&
		    $previndent == $indent) {
			my ($s, $c) = ctx_statement_block($linenr, $realcnt, 0);
			substr($s, 0, length($c), '');
			$s =~ s/\n.*//g;
			if ($s =~ /^\s*;/) {
				if (ERROR("WHILE_AFTER_BRACE",
					  "while should follow close brace '}'\n" . $hereprev) &&
				    $fix && $prevline =~ /^\+/ && $line =~ /^\+/) {
					fix_delete_line($fixlinenr - 1, $prevrawline);
					fix_delete_line($fixlinenr, $rawline);
					my $fixedline = $prevrawline;
					my $trailing = $rawline;
					$trailing =~ s/^\+//;
					$trailing = trim($trailing);
					$fixedline =~ s/}\s*$/} $trailing/;
					fix_insert_line($fixlinenr, $fixedline);
				}
			}
		}
		while ($line =~ m{($Constant|$Lval)}g) {
			my $var = $1;
			if ($var !~ /^$Constant$/ &&
			    $var =~ /[A-Z][a-z]|[a-z][A-Z]/ &&
			    $var !~ /^(?:[A-Z]+_){1,5}[A-Z]{1,3}[a-z]/ &&
			    $var !~ /^(?:Clear|Set|TestClear|TestSet|)Page[A-Z]/ &&
			    $var !~ /^ETHTOOL_LINK_MODE_/ &&
			    $var !~ /^(?:[a-z0-9_]*|[A-Z0-9_]*)?_?[a-z][A-Z](?:_[a-z0-9_]+|_[A-Z0-9_]+)?$/ &&
			    $var !~ /^(?:[a-z_]*?)_?(?:[KMGT]iB|[KMGT]?Hz)(?:_[a-z_]+)?$/) {
				while ($var =~ m{\b($Ident)}g) {
					my $word = $1;
					next if ($word !~ /[A-Z][a-z]|[a-z][A-Z]/);
					if ($check) {
						seed_camelcase_includes();
						if (!$file && !$camelcase_file_seeded) {
							seed_camelcase_file($realfile);
							$camelcase_file_seeded = 1;
						}
					}
					if (!defined $camelcase{$word}) {
						$camelcase{$word} = 1;
						CHK("CAMELCASE",
						    "Avoid CamelCase: <$word>\n" . $herecurr);
					}
				}
			}
		}
		if ($line =~ /\
			if (WARN("WHITESPACE_AFTER_LINE_CONTINUATION",
				 "Whitespace after \\ makes next lines useless\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/\s+$//;
			}
		}
		if ($tree && $rawline =~ m{^.\s*\
			my $file = "$1.h";
			my $checkfile = "include/linux/$file";
			if (-f "$root/$checkfile" &&
			    $realfile ne $checkfile &&
			    $1 !~ /$allowed_asm_includes/)
			{
				my $asminclude = `grep -Ec "#include\\s+<asm/$file>" $root/$checkfile`;
				if ($asminclude > 0) {
					if ($realfile =~ m{^arch/}) {
						CHK("ARCH_INCLUDE_LINUX",
						    "Consider using #include <linux/$file> instead of <asm/$file>\n" . $herecurr);
					} else {
						WARN("INCLUDE_LINUX",
						     "Use #include <linux/$file> instead of <asm/$file>\n" . $herecurr);
					}
				}
			}
		}
		if ($realfile !~ m@/vmlinux.lds.h$@ &&
		    $line =~ /^.\s*\
			my $ln = $linenr;
			my $cnt = $realcnt;
			my ($off, $dstat, $dcond, $rest);
			my $ctx = '';
			my $has_flow_statement = 0;
			my $has_arg_concat = 0;
			($dstat, $dcond, $ln, $cnt, $off) =
				ctx_statement_block($linenr, $realcnt, 0);
			$ctx = $dstat;
			$has_flow_statement = 1 if ($ctx =~ /\b(goto|return)\b/);
			$has_arg_concat = 1 if ($ctx =~ /\
			$dstat =~ s/^.\s*\
			my $define_args = $1;
			my $define_stmt = $dstat;
			my @def_args = ();
			if (defined $define_args && $define_args ne "") {
				$define_args = substr($define_args, 1, length($define_args) - 2);
				$define_args =~ s/\s*//g;
				$define_args =~ s/\\\+?//g;
				@def_args = split(",", $define_args);
			}
			$dstat =~ s/$;//g;
			$dstat =~ s/\\\n.//g;
			$dstat =~ s/^\s*//s;
			$dstat =~ s/\s*$//s;
			while ($dstat =~ s/\([^\(\)]*\)/1u/ ||
			       $dstat =~ s/\{[^\{\}]*\}/1u/ ||
			       $dstat =~ s/.\[[^\[\]]*\]/1u/)
			{
			}
			while ($dstat =~ s/($String)\s*$Ident/$1/ ||
			       $dstat =~ s/$Ident\s*($String)/$1/)
			{
			}
			$dstat =~ s/\b_*asm_*\s+_*volatile_*\b/asm_volatile/g;
			my $exceptions = qr{
				$Declare|
				module_param_named|
				MODULE_PARM_DESC|
				DECLARE_PER_CPU|
				DEFINE_PER_CPU|
				__typeof__\(|
				union|
				struct|
				\.$Ident\s*=\s*|
				^\"|\"$|
				^\[
			}x;
			$ctx =~ s/\n*$//;
			my $stmt_cnt = statement_rawlines($ctx);
			my $herectx = get_stat_here($linenr, $stmt_cnt, $here);
			if ($dstat ne '' &&
			    $dstat !~ /^(?:$Ident|-?$Constant),$/ &&			
			    $dstat !~ /^(?:$Ident|-?$Constant);$/ &&			
			    $dstat !~ /^[!~-]?(?:$Lval|$Constant)$/ &&		
			    $dstat !~ /^'X'$/ && $dstat !~ /^'XX'$/ &&			
			    $dstat !~ /$exceptions/ &&
			    $dstat !~ /^\.$Ident\s*=/ &&				
			    $dstat !~ /^(?:\
			    $dstat !~ /^case\b/ &&					
			    $dstat !~ /^do\s*$Constant\s*while\s*$Constant;?$/ &&	
			    $dstat !~ /^while\s*$Constant\s*$Constant\s*$/ &&		
			    $dstat !~ /^for\s*$Constant$/ &&				
			    $dstat !~ /^for\s*$Constant\s+(?:$Ident|-?$Constant)$/ &&	
			    $dstat !~ /^do\s*{/ &&					
			    $dstat !~ /^\(\{/ &&						
			    $ctx !~ /^.\s*
			{
				if ($dstat =~ /^\s*if\b/) {
					ERROR("MULTISTATEMENT_MACRO_USE_DO_WHILE",
					      "Macros starting with if should be enclosed by a do - while loop to avoid possible if/else logic defects\n" . "$herectx");
				} elsif ($dstat =~ /;/) {
					ERROR("MULTISTATEMENT_MACRO_USE_DO_WHILE",
					      "Macros with multiple statements should be enclosed in a do - while loop\n" . "$herectx");
				} else {
					ERROR("COMPLEX_MACRO",
					      "Macros with complex values should be enclosed in parentheses\n" . "$herectx");
				}
			}
			my @stmt_array = split('\n', $define_stmt);
			my $first = 1;
			$define_stmt = "";
			foreach my $l (@stmt_array) {
				$l =~ s/\\$//;
				if ($first) {
					$define_stmt = $l;
					$first = 0;
				} elsif ($l =~ /^[\+ ]/) {
					$define_stmt .= substr($l, 1);
				}
			}
			$define_stmt =~ s/$;//g;
			$define_stmt =~ s/\s+/ /g;
			$define_stmt = trim($define_stmt);
			foreach my $arg (@def_args) {
			        next if ($arg =~ /\.\.\./);
			        next if ($arg =~ /^type$/i);
				my $tmp_stmt = $define_stmt;
				$tmp_stmt =~ s/\b(__must_be_array|offsetof|sizeof|sizeof_field|__stringify|typeof|__typeof__|__builtin\w+|typecheck\s*\(\s*$Type\s*,|\
				$tmp_stmt =~ s/\
				$tmp_stmt =~ s/\b$arg\s*\
				my $use_cnt = () = $tmp_stmt =~ /\b$arg\b/g;
				if ($use_cnt > 1) {
					CHK("MACRO_ARG_REUSE",
					    "Macro argument reuse '$arg' - possible side-effects?\n" . "$herectx");
				    }
				if ($tmp_stmt =~ m/($Operators)?\s*\b$arg\b\s*($Operators)?/m &&
				    ((defined($1) && $1 ne ',') ||
				     (defined($2) && $2 ne ','))) {
					CHK("MACRO_ARG_PRECEDENCE",
					    "Macro argument '$arg' may be better as '($arg)' to avoid precedence issues\n" . "$herectx");
				}
			}
			if ($has_flow_statement && !$has_arg_concat) {
				my $cnt = statement_rawlines($ctx);
				my $herectx = get_stat_here($linenr, $cnt, $here);
				WARN("MACRO_WITH_FLOW_CONTROL",
				     "Macros with flow control statements should be avoided\n" . "$herectx");
			}
		} elsif ($realfile =~ m@/vmlinux.lds.h$@) {
		    $line =~ s/(\w+)/$maybe_linker_symbol{$1}++/ge;
		} else {
			if ($prevline !~ /^..*\\$/ &&
			    $line !~ /^\+\s*\
			    $line !~ /^\+.*\b(__asm__|asm)\b.*\\$/ &&	
			    $line =~ /^\+.*\\$/) {
				WARN("LINE_CONTINUATIONS",
				     "Avoid unnecessary line continuations\n" . $herecurr);
			}
		}
		if ($perl_version_ok &&
		    $realfile !~ m@/vmlinux.lds.h$@ &&
		    $line =~ /^.\s*\
			my $ln = $linenr;
			my $cnt = $realcnt;
			my ($off, $dstat, $dcond, $rest);
			my $ctx = '';
			($dstat, $dcond, $ln, $cnt, $off) =
				ctx_statement_block($linenr, $realcnt, 0);
			$ctx = $dstat;
			$dstat =~ s/\\\n.//g;
			$dstat =~ s/$;/ /g;
			if ($dstat =~ /^\+\s*
				my $stmts = $2;
				my $semis = $3;
				$ctx =~ s/\n*$//;
				my $cnt = statement_rawlines($ctx);
				my $herectx = get_stat_here($linenr, $cnt, $here);
				if (($stmts =~ tr/;/;/) == 1 &&
				    $stmts !~ /^\s*(if|while|for|switch)\b/) {
					WARN("SINGLE_STATEMENT_DO_WHILE_MACRO",
					     "Single statement macros should not use a do {} while (0) loop\n" . "$herectx");
				}
				if (defined $semis && $semis ne "") {
					WARN("DO_WHILE_MACRO_WITH_TRAILING_SEMICOLON",
					     "do {} while (0) macros should not be semicolon terminated\n" . "$herectx");
				}
			} elsif ($dstat =~ /^\+\s*
				$ctx =~ s/\n*$//;
				my $cnt = statement_rawlines($ctx);
				my $herectx = get_stat_here($linenr, $cnt, $here);
				WARN("TRAILING_SEMICOLON",
				     "macros should not use a trailing semicolon\n" . "$herectx");
			}
		}
		if ($line =~ /(^.*)\bif\b/ && $1 !~ /else\s*$/) {
			my ($level, $endln, @chunks) =
				ctx_statement_full($linenr, $realcnt, 1);
			if ($
				my @allowed = ();
				my $allow = 0;
				my $seen = 0;
				my $herectx = $here . "\n";
				my $ln = $linenr - 1;
				for my $chunk (@chunks) {
					my ($cond, $block) = @{$chunk};
					my ($whitespace) = ($cond =~ /^((?:\s*\n[+-])*\s*)/s);
					my $offset = statement_rawlines($whitespace) - 1;
					$allowed[$allow] = 0;
					$suppress_ifbraces{$ln + $offset} = 1;
					$herectx .= "$rawlines[$ln + $offset]\n[...]\n";
					$ln += statement_rawlines($block) - 1;
					substr($block, 0, length($cond), '');
					$seen++ if ($block =~ /^\s*{/);
					if (statement_lines($cond) > 1) {
						$allowed[$allow] = 1;
					}
					if ($block =~/\b(?:if|for|while)\b/) {
						$allowed[$allow] = 1;
					}
					if (statement_block_size($block) > 1) {
						$allowed[$allow] = 1;
					}
					$allow++;
				}
				if ($seen) {
					my $sum_allowed = 0;
					foreach (@allowed) {
						$sum_allowed += $_;
					}
					if ($sum_allowed == 0) {
						WARN("BRACES",
						     "braces {} are not necessary for any arm of this statement\n" . $herectx);
					} elsif ($sum_allowed != $allow &&
						 $seen != $allow) {
						CHK("BRACES",
						    "braces {} should be used on all arms of this statement\n" . $herectx);
					}
				}
			}
		}
		if (!defined $suppress_ifbraces{$linenr - 1} &&
					$line =~ /\b(if|while|for|else)\b/) {
			my $allowed = 0;
			if (substr($line, 0, $-[0]) =~ /(\}\s*)$/) {
				$allowed = 1;
			}
			my ($level, $endln, @chunks) =
				ctx_statement_full($linenr, $realcnt, $-[0]);
			my ($cond, $block) = @{$chunks[0]};
			if (defined $cond) {
				substr($block, 0, length($cond), '');
			}
			if (statement_lines($cond) > 1) {
				$allowed = 1;
			}
			if ($block =~/\b(?:if|for|while)\b/) {
				$allowed = 1;
			}
			if (statement_block_size($block) > 1) {
				$allowed = 1;
			}
			if (defined $chunks[1]) {
				my ($cond, $block) = @{$chunks[1]};
				if (defined $cond) {
					substr($block, 0, length($cond), '');
				}
				if ($block =~ /^\s*\{/) {
					$allowed = 1;
				}
			}
			if ($level == 0 && $block =~ /^\s*\{/ && !$allowed) {
				my $cnt = statement_rawlines($block);
				my $herectx = get_stat_here($linenr, $cnt, $here);
				WARN("BRACES",
				     "braces {} are not necessary for single statement blocks\n" . $herectx);
			}
		}
		if ($sline =~ /^.\s*\}\s*else\s*$/ ||
		    $sline =~ /^.\s*else\s*\{\s*$/) {
			CHK("BRACES", "Unbalanced braces around else statement\n" . $herecurr);
		}
		if (($line =~ /^.\s*}\s*$/ && $prevrawline =~ /^.\s*$/)) {
			if (CHK("BRACES",
				"Blank lines aren't necessary before a close brace '}'\n" . $hereprev) &&
			    $fix && $prevrawline =~ /^\+/) {
				fix_delete_line($fixlinenr - 1, $prevrawline);
			}
		}
		if (($rawline =~ /^.\s*$/ && $prevline =~ /^..*{\s*$/)) {
			if (CHK("BRACES",
				"Blank lines aren't necessary after an open brace '{'\n" . $hereprev) &&
			    $fix) {
				fix_delete_line($fixlinenr, $rawline);
			}
		}
		my $asm_volatile = qr{\b(__asm__|asm)\s+(__volatile__|volatile)\b};
		if ($line =~ /\bvolatile\b/ && $line !~ /$asm_volatile/) {
			WARN("VOLATILE",
			     "Use of volatile is usually wrong: see Documentation/process/volatile-considered-harmful.rst\n" . $herecurr);
		}
		if ($line =~ /^\+\s*$String/ &&
		    $prevline =~ /"\s*$/ &&
		    $prevrawline !~ /(?:\\(?:[ntr]|[0-7]{1,3}|x[0-9a-fA-F]{1,2})|;\s*|\{\s*)"\s*$/) {
			if (WARN("SPLIT_STRING",
				 "quoted string split across lines\n" . $hereprev) &&
				     $fix &&
				     $prevrawline =~ /^\+.*"\s*$/ &&
				     $last_coalesced_string_linenr != $linenr - 1) {
				my $extracted_string = get_quoted_string($line, $rawline);
				my $comma_close = "";
				if ($rawline =~ /\Q$extracted_string\E(\s*\)\s*;\s*$|\s*,\s*)/) {
					$comma_close = $1;
				}
				fix_delete_line($fixlinenr - 1, $prevrawline);
				fix_delete_line($fixlinenr, $rawline);
				my $fixedline = $prevrawline;
				$fixedline =~ s/"\s*$//;
				$fixedline .= substr($extracted_string, 1) . trim($comma_close);
				fix_insert_line($fixlinenr - 1, $fixedline);
				$fixedline = $rawline;
				$fixedline =~ s/\Q$extracted_string\E\Q$comma_close\E//;
				if ($fixedline !~ /\+\s*$/) {
					fix_insert_line($fixlinenr, $fixedline);
				}
				$last_coalesced_string_linenr = $linenr;
			}
		}
		if ($prevrawline =~ /[^\\]\w"$/ && $rawline =~ /^\+[\t ]+"\w/) {
			WARN('MISSING_SPACE',
			     "break quoted strings at a space character\n" . $hereprev);
		}
		if ($line =~ /^\+.*$String/ &&
		    defined($context_function) &&
		    get_quoted_string($line, $rawline) =~ /\b$context_function\b/ &&
		    length(get_quoted_string($line, $rawline)) != (length($context_function) + 2)) {
			WARN("EMBEDDED_FUNCTION_NAME",
			     "Prefer using '\"%s...\", __func__' to using '$context_function', this function's name, in a string\n" . $herecurr);
		}
		if ($rawline =~ /^\+.*\([^"]*"$tracing_logging_tags{0,3}%s(?:\s*\(\s*\)\s*)?$tracing_logging_tags{0,3}(?:\\n)?"\s*,\s*__func__\s*\)\s*;/) {
			if (WARN("TRACING_LOGGING",
				 "Unnecessary ftrace-like logging - prefer using ftrace\n" . $herecurr) &&
			    $fix) {
                                fix_delete_line($fixlinenr, $rawline);
			}
		}
		if ($rawline =~ /^.*\".*\s\\n/) {
			if (WARN("QUOTED_WHITESPACE_BEFORE_NEWLINE",
				 "unnecessary whitespace before a quoted newline\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/^(\+.*\".*)\s+\\n/$1\\n/;
			}
		}
		if ($line =~ /$String[A-Z_]/ ||
		    ($line =~ /([A-Za-z0-9_]+)$String/ && $1 !~ /^[Lu]$/)) {
			if (CHK("CONCATENATED_STRING",
				"Concatenated strings should use spaces between elements\n" . $herecurr) &&
			    $fix) {
				while ($line =~ /($String)/g) {
					my $extracted_string = substr($rawline, $-[0], $+[0] - $-[0]);
					$fixed[$fixlinenr] =~ s/\Q$extracted_string\E([A-Za-z0-9_])/$extracted_string $1/;
					$fixed[$fixlinenr] =~ s/([A-Za-z0-9_])\Q$extracted_string\E/$1 $extracted_string/;
				}
			}
		}
		if ($line =~ /$String\s*[Lu]?"/) {
			if (WARN("STRING_FRAGMENTS",
				 "Consecutive strings are generally better as a single string\n" . $herecurr) &&
			    $fix) {
				while ($line =~ /($String)(?=\s*")/g) {
					my $extracted_string = substr($rawline, $-[0], $+[0] - $-[0]);
					$fixed[$fixlinenr] =~ s/\Q$extracted_string\E\s*"/substr($extracted_string, 0, -1)/e;
				}
			}
		}
		my $show_L = 1;	
		my $show_Z = 1;
		while ($line =~ /(?:^|")([X\t]*)(?:"|$)/g) {
			my $string = substr($rawline, $-[1], $+[1] - $-[1]);
			$string =~ s/%%/__/g;
			if ($show_L && $string =~ /%[\*\d\.\$]*L([diouxX])/) {
				WARN("PRINTF_L",
				     "\%L$1 is non-standard C, use %ll$1\n" . $herecurr);
				$show_L = 0;
			}
			if ($show_Z && $string =~ /%[\*\d\.\$]*Z([diouxX])/) {
				WARN("PRINTF_Z",
				     "%Z$1 is non-standard C, use %z$1\n" . $herecurr);
				$show_Z = 0;
			}
			if ($string =~ /0x%[\*\d\.\$\Llzth]*[diou]/) {
				ERROR("PRINTF_0XDECIMAL",
				      "Prefixing 0x with decimal output is defective\n" . $herecurr);
			}
		}
		if ($rawline =~ /\\$/ && $sline =~ tr/"/"/ % 2) {
			WARN("LINE_CONTINUATIONS",
			     "Avoid line continuations in quoted strings\n" . $herecurr);
		}
		if ($line =~ /^.\s*\
			WARN("IF_0",
			     "Consider removing the code enclosed by this #if 0 and its #endif\n" . $herecurr);
		}
		if ($line =~ /^.\s*\
			WARN("IF_1",
			     "Consider removing the #if 1 and its #endif\n" . $herecurr);
		}
		if ($prevline =~ /\bif\s*\(\s*($Lval)\s*\)/) {
			my $tested = quotemeta($1);
			my $expr = '\s*\(\s*' . $tested . '\s*\)\s*;';
			if ($line =~ /\b(kfree|usb_free_urb|debugfs_remove(?:_recursive)?|(?:kmem_cache|mempool|dma_pool)_destroy)$expr/) {
				my $func = $1;
				if (WARN('NEEDLESS_IF',
					 "$func(NULL) is safe and this check is probably not required\n" . $hereprev) &&
				    $fix) {
					my $do_fix = 1;
					my $leading_tabs = "";
					my $new_leading_tabs = "";
					if ($lines[$linenr - 2] =~ /^\+(\t*)if\s*\(\s*$tested\s*\)\s*$/) {
						$leading_tabs = $1;
					} else {
						$do_fix = 0;
					}
					if ($lines[$linenr - 1] =~ /^\+(\t+)$func\s*\(\s*$tested\s*\)\s*;\s*$/) {
						$new_leading_tabs = $1;
						if (length($leading_tabs) + 1 ne length($new_leading_tabs)) {
							$do_fix = 0;
						}
					} else {
						$do_fix = 0;
					}
					if ($do_fix) {
						fix_delete_line($fixlinenr - 1, $prevrawline);
						$fixed[$fixlinenr] =~ s/^\+$new_leading_tabs/\+$leading_tabs/;
					}
				}
			}
		}
		if ($line =~ /\bk[v]?free_rcu\s*\([^(]+\)/) {
			if ($line =~ /\bk[v]?free_rcu\s*\([^,]+\)/) {
				ERROR("DEPRECATED_API",
				      "Single-argument k[v]free_rcu() API is deprecated, please pass rcu_head object or call k[v]free_rcu_mightsleep()." . $herecurr);
			}
		}
		if ($line =~ /^\+.*\b$logFunctions\s*\(/ &&
		    $prevline =~ /^[ \+]\s*if\s*\(\s*(\!\s*|NULL\s*==\s*)?($Lval)(\s*==\s*NULL\s*)?\s*\)/ &&
		    (defined $1 || defined $3) &&
		    $linenr > 3) {
			my $testval = $2;
			my $testline = $lines[$linenr - 3];
			my ($s, $c) = ctx_statement_block($linenr - 3, $realcnt, 0);
			if ($s =~ /(?:^|\n)[ \+]\s*(?:$Type\s*)?\Q$testval\E\s*=\s*(?:\([^\)]*\)\s*)?\s*$allocFunctions\s*\(/ &&
			    $s !~ /\b__GFP_NOWARN\b/ ) {
				WARN("OOM_MESSAGE",
				     "Possible unnecessary 'out of memory' message\n" . $hereprev);
			}
		}
		if ($line !~ /printk(?:_ratelimited|_once)?\s*\(/ &&
		    $line =~ /\b$logFunctions\s*\(.*\b(KERN_[A-Z]+)\b/) {
			my $level = $1;
			if (WARN("UNNECESSARY_KERN_LEVEL",
				 "Possible unnecessary $level\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/\s*$level\s*//;
			}
		}
		if ($line =~ /\bprintk\s*\(\s*KERN_CONT\b|\bpr_cont\s*\(/) {
			WARN("LOGGING_CONTINUATION",
			     "Avoid logging continuation uses where feasible\n" . $herecurr);
		}
		if (defined $stat &&
		    $line =~ /\b$logFunctions\s*\(/ &&
		    index($stat, '"') >= 0) {
			my $lc = $stat =~ tr@\n@@;
			$lc = $lc + $linenr;
			my $stat_real = get_stat_real($linenr, $lc);
			pos($stat_real) = index($stat_real, '"');
			while ($stat_real =~ /[^\"%]*(%[\
				my $pspec = $1;
				my $h = $2;
				my $lineoff = substr($stat_real, 0, $-[1]) =~ tr@\n@@;
				if (WARN("UNNECESSARY_MODIFIER",
					 "Integer promotion: Using '$h' in '$pspec' is unnecessary\n" . "$here\n$stat_real\n") &&
				    $fix && $fixed[$fixlinenr + $lineoff] =~ /^\+/) {
					my $nspec = $pspec;
					$nspec =~ s/h//g;
					$fixed[$fixlinenr + $lineoff] =~ s/\Q$pspec\E/$nspec/;
				}
			}
		}
		if ($perl_version_ok &&
		    $line =~ /$LvalOrFunc\s*\&\s*($LvalOrFunc)\s*>>/ &&
		    $4 !~ /^\&/) { 
			WARN("MASK_THEN_SHIFT",
			     "Possible precedence defect with mask then right shift - may need parentheses\n" . $herecurr);
		}
		if ($perl_version_ok) {
			while ($line =~ /\b$LvalOrFunc\s*(==|\!=)\s*NULL\b/g) {
				my $val = $1;
				my $equal = "!";
				$equal = "" if ($4 eq "!=");
				if (CHK("COMPARISON_TO_NULL",
					"Comparison to NULL could be written \"${equal}${val}\"\n" . $herecurr) &&
					    $fix) {
					$fixed[$fixlinenr] =~ s/\b\Q$val\E\s*(?:==|\!=)\s*NULL\b/$equal$val/;
				}
			}
		}
		if ($line =~ /(\b$InitAttribute\b)/) {
			my $attr = $1;
			if ($line =~ /^\+\s*static\s+(?:const\s+)?(?:$attr\s+)?($NonptrTypeWithAttr)\s+(?:$attr\s+)?($Ident(?:\[[^]]*\])?)\s*[=;]/) {
				my $ptr = $1;
				my $var = $2;
				if ((($ptr =~ /\b(union|struct)\s+$attr\b/ &&
				      ERROR("MISPLACED_INIT",
					    "$attr should be placed after $var\n" . $herecurr)) ||
				     ($ptr !~ /\b(union|struct)\s+$attr\b/ &&
				      WARN("MISPLACED_INIT",
					   "$attr should be placed after $var\n" . $herecurr))) &&
				    $fix) {
					$fixed[$fixlinenr] =~ s/(\bstatic\s+(?:const\s+)?)(?:$attr\s+)?($NonptrTypeWithAttr)\s+(?:$attr\s+)?($Ident(?:\[[^]]*\])?)\s*([=;])\s*/"$1" . trim(string_find_replace($2, "\\s*$attr\\s*", " ")) . " " . trim(string_find_replace($3, "\\s*$attr\\s*", "")) . " $attr" . ("$4" eq ";" ? ";" : " = ")/e;
				}
			}
		}
		if ($line =~ /\bconst\b/ && $line =~ /($InitAttributeData)/) {
			my $attr = $1;
			$attr =~ /($InitAttributePrefix)(.*)/;
			my $attr_prefix = $1;
			my $attr_type = $2;
			if (ERROR("INIT_ATTRIBUTE",
				  "Use of const init definition must use ${attr_prefix}initconst\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~
				    s/$InitAttributeData/${attr_prefix}initconst/;
			}
		}
		if ($line !~ /\bconst\b/ && $line =~ /($InitAttributeConst)/) {
			my $attr = $1;
			if (ERROR("INIT_ATTRIBUTE",
				  "Use of $attr requires a separate use of const\n" . $herecurr) &&
			    $fix) {
				my $lead = $fixed[$fixlinenr] =~
				    /(^\+\s*(?:static\s+))/;
				$lead = rtrim($1);
				$lead = "$lead " if ($lead !~ /^\+$/);
				$lead = "${lead}const ";
				$fixed[$fixlinenr] =~ s/(^\+\s*(?:static\s+))/$lead/;
			}
		}
		if ($line =~ /\b__read_mostly\b/ &&
		    $line =~ /($Type)\s*$Ident/ && $1 !~ /\*\s*$/ && $1 =~ /\bconst\b/) {
			if (ERROR("CONST_READ_MOSTLY",
				  "Invalid use of __read_mostly with const type\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/\s+__read_mostly\b//;
			}
		}
		if ($realfile !~ m@^include/uapi/@ &&
		    $line =~ /(__constant_(?:htons|ntohs|[bl]e(?:16|32|64)_to_cpu|cpu_to_[bl]e(?:16|32|64)))\s*\(/) {
			my $constant_func = $1;
			my $func = $constant_func;
			$func =~ s/^__constant_//;
			if (WARN("CONSTANT_CONVERSION",
				 "$constant_func should be $func\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/\b$constant_func\b/$func/g;
			}
		}
		if ($line =~ /\budelay\s*\(\s*(\d+)\s*\)/) {
			my $delay = $1;
			if (! ($delay < 10) ) {
				CHK("USLEEP_RANGE",
				    "usleep_range is preferred over udelay; see Documentation/timers/timers-howto.rst\n" . $herecurr);
			}
			if ($delay > 2000) {
				WARN("LONG_UDELAY",
				     "long udelay - prefer mdelay; see arch/arm/include/asm/delay.h\n" . $herecurr);
			}
		}
		if ($line =~ /\bmsleep\s*\((\d+)\);/) {
			if ($1 < 20) {
				WARN("MSLEEP",
				     "msleep < 20ms can sleep for up to 20ms; see Documentation/timers/timers-howto.rst\n" . $herecurr);
			}
		}
		if ($line =~ /\bjiffies\s*$Compare|$Compare\s*jiffies\b/) {
			WARN("JIFFIES_COMPARISON",
			     "Comparing jiffies is almost always wrong; prefer time_after, time_before and friends\n" . $herecurr);
		}
		if ($line =~ /\bget_jiffies_64\s*\(\s*\)\s*$Compare|$Compare\s*get_jiffies_64\s*\(\s*\)/) {
			WARN("JIFFIES_COMPARISON",
			     "Comparing get_jiffies_64() is almost always wrong; prefer time_after64, time_before64 and friends\n" . $herecurr);
		}
		if ($line =~ /^.\s*\
			if (ERROR("SPACING",
				  "exactly one space required after that #$1\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~
				    s/^(.\s*\
			}
		}
		if ($line =~ /^.\s*(struct\s+mutex|spinlock_t)\s+\S+;/ ||
		    $line =~ /^.\s*(DEFINE_MUTEX)\s*\(/) {
			my $which = $1;
			if (!ctx_has_comment($first_line, $linenr)) {
				CHK("UNCOMMENTED_DEFINITION",
				    "$1 definition without comment\n" . $herecurr);
			}
		}
		my $barriers = qr{
			mb|
			rmb|
			wmb
		}x;
		my $barrier_stems = qr{
			mb__before_atomic|
			mb__after_atomic|
			store_release|
			load_acquire|
			store_mb|
			(?:$barriers)
		}x;
		my $all_barriers = qr{
			(?:$barriers)|
			smp_(?:$barrier_stems)|
			virt_(?:$barrier_stems)
		}x;
		if ($line =~ /\b(?:$all_barriers)\s*\(/) {
			if (!ctx_has_comment($first_line, $linenr)) {
				WARN("MEMORY_BARRIER",
				     "memory barrier without comment\n" . $herecurr);
			}
		}
		my $underscore_smp_barriers = qr{__smp_(?:$barrier_stems)}x;
		if ($realfile !~ m@^include/asm-generic/@ &&
		    $realfile !~ m@/barrier\.h$@ &&
		    $line =~ m/\b(?:$underscore_smp_barriers)\s*\(/ &&
		    $line !~ m/^.\s*\
			WARN("MEMORY_BARRIER",
			     "__smp memory barriers shouldn't be used outside barrier.h and asm-generic\n" . $herecurr);
		}
		if ($line =~ /\bwaitqueue_active\s*\(/) {
			if (!ctx_has_comment($first_line, $linenr)) {
				WARN("WAITQUEUE_ACTIVE",
				     "waitqueue_active without comment\n" . $herecurr);
			}
		}
		if ($line =~ /\bdata_race\s*\(/) {
			if (!ctx_has_comment($first_line, $linenr)) {
				WARN("DATA_RACE",
				     "data_race without comment\n" . $herecurr);
			}
		}
		if ($line =~ m@^.\s*\
			CHK("ARCH_DEFINES",
			    "architecture specific defines should be avoided\n" .  $herecurr);
		}
		if ($line =~ /\b($Type)\s+($Storage)\b/) {
			WARN("STORAGE_CLASS",
			     "storage class '$2' should be located before type '$1'\n" . $herecurr);
		}
		if ($line =~ /\b$Storage\b/ &&
		    $line !~ /^.\s*$Storage/ &&
		    $line =~ /^.\s*(.+?)\$Storage\s/ &&
		    $1 !~ /[\,\)]\s*$/) {
			WARN("STORAGE_CLASS",
			     "storage class should be at the beginning of the declaration\n" . $herecurr);
		}
		if ($line =~ /\b$Type\s+$Inline\b/ ||
		    $line =~ /\b$Inline\s+$Storage\b/) {
			ERROR("INLINE_LOCATION",
			      "inline keyword should sit between storage class and type\n" . $herecurr);
		}
		if ($realfile !~ m@\binclude/uapi/@ &&
		    $line =~ /\b(__inline__|__inline)\b/) {
			if (WARN("INLINE",
				 "plain inline is preferred over $1\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/\b(__inline__|__inline)\b/inline/;
			}
		}
		if ($realfile !~ m@\binclude/uapi/@ &&
		    $rawline =~ /\b__attribute__\s*\(\s*($balanced_parens)\s*\)/) {
			my $attr = $1;
			$attr =~ s/\s*\(\s*(.*)\)\s*/$1/;
			my %attr_list = (
				"alias"				=> "__alias",
				"aligned"			=> "__aligned",
				"always_inline"			=> "__always_inline",
				"assume_aligned"		=> "__assume_aligned",
				"cold"				=> "__cold",
				"const"				=> "__attribute_const__",
				"copy"				=> "__copy",
				"designated_init"		=> "__designated_init",
				"externally_visible"		=> "__visible",
				"format"			=> "printf|scanf",
				"gnu_inline"			=> "__gnu_inline",
				"malloc"			=> "__malloc",
				"mode"				=> "__mode",
				"no_caller_saved_registers"	=> "__no_caller_saved_registers",
				"noclone"			=> "__noclone",
				"noinline"			=> "noinline",
				"nonstring"			=> "__nonstring",
				"noreturn"			=> "__noreturn",
				"packed"			=> "__packed",
				"pure"				=> "__pure",
				"section"			=> "__section",
				"used"				=> "__used",
				"weak"				=> "__weak"
			);
			while ($attr =~ /\s*(\w+)\s*(${balanced_parens})?/g) {
				my $orig_attr = $1;
				my $params = '';
				$params = $2 if defined($2);
				my $curr_attr = $orig_attr;
				$curr_attr =~ s/^[\s_]+|[\s_]+$//g;
				if (exists($attr_list{$curr_attr})) {
					my $new = $attr_list{$curr_attr};
					if ($curr_attr eq "format" && $params) {
						$params =~ /^\s*\(\s*(\w+)\s*,\s*(.*)/;
						$new = "__$1\($2";
					} else {
						$new = "$new$params";
					}
					if (WARN("PREFER_DEFINED_ATTRIBUTE_MACRO",
						 "Prefer $new over __attribute__(($orig_attr$params))\n" . $herecurr) &&
					    $fix) {
						my $remove = "\Q$orig_attr\E" . '\s*' . "\Q$params\E" . '(?:\s*,\s*)?';
						$fixed[$fixlinenr] =~ s/$remove//;
						$fixed[$fixlinenr] =~ s/\b__attribute__/$new __attribute__/;
						$fixed[$fixlinenr] =~ s/\}\Q$new\E/} $new/;
						$fixed[$fixlinenr] =~ s/ __attribute__\s*\(\s*\(\s*\)\s*\)//;
					}
				}
			}
			if ($attr =~ /^_*unused/) {
				WARN("PREFER_DEFINED_ATTRIBUTE_MACRO",
				     "__always_unused or __maybe_unused is preferred over __attribute__((__unused__))\n" . $herecurr);
			}
		}
		if ($perl_version_ok &&
		    $line =~ /(?:$Declare|$DeclareMisordered)\s*$Ident\s*$balanced_parens\s*(?:$Attribute)?\s*;/ &&
		    ($line =~ /\b__attribute__\s*\(\s*\(.*\bweak\b/ ||
		     $line =~ /\b__weak\b/)) {
			ERROR("WEAK_DECLARATION",
			      "Using weak declarations can have unintended link defects\n" . $herecurr);
		}
		if ($realfile !~ m@\binclude/uapi/@ &&
		    $realfile !~ m@\btools/@ &&
		    $line =~ /\b($Declare)\s*$Ident\s*[=;,\[]/) {
			my $type = $1;
			if ($type =~ /\b($typeC99Typedefs)\b/) {
				$type = $1;
				my $kernel_type = 'u';
				$kernel_type = 's' if ($type =~ /^_*[si]/);
				$type =~ /(\d+)/;
				$kernel_type .= $1;
				if (CHK("PREFER_KERNEL_TYPES",
					"Prefer kernel type '$kernel_type' over '$type'\n" . $herecurr) &&
				    $fix) {
					$fixed[$fixlinenr] =~ s/\b$type\b/$kernel_type/;
				}
			}
		}
		if ($line =~ /(\(\s*$C90_int_types\s*\)\s*)($Constant)\b/) {
			my $cast = $1;
			my $const = $2;
			my $suffix = "";
			my $newconst = $const;
			$newconst =~ s/${Int_type}$//;
			$suffix .= 'U' if ($cast =~ /\bunsigned\b/);
			if ($cast =~ /\blong\s+long\b/) {
			    $suffix .= 'LL';
			} elsif ($cast =~ /\blong\b/) {
			    $suffix .= 'L';
			}
			if (WARN("TYPECAST_INT_CONSTANT",
				 "Unnecessary typecast of c90 int constant - '$cast$const' could be '$const$suffix'\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/\Q$cast\E$const\b/$newconst$suffix/;
			}
		}
		if ($line =~ /\bsizeof\s*\(\s*\&/) {
			WARN("SIZEOF_ADDRESS",
			     "sizeof(& should be avoided\n" . $herecurr);
		}
		if ($line =~ /\bsizeof\s+((?:\*\s*|)$Lval|$Type(?:\s+$Lval|))/) {
			if (WARN("SIZEOF_PARENTHESIS",
				 "sizeof $1 should be sizeof($1)\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/\bsizeof\s+((?:\*\s*|)$Lval|$Type(?:\s+$Lval|))/"sizeof(" . trim($1) . ")"/ex;
			}
		}
		if ($line =~ /^.\s*\bstruct\s+spinlock\s+\w+\s*;/) {
			WARN("USE_SPINLOCK_T",
			     "struct spinlock should be spinlock_t\n" . $herecurr);
		}
		if ($sline =~ /\bseq_printf\s*\(.*"\s*\)\s*;\s*$/) {
			my $fmt = get_quoted_string($line, $rawline);
			$fmt =~ s/%%//g;
			if ($fmt !~ /%/) {
				if (WARN("PREFER_SEQ_PUTS",
					 "Prefer seq_puts to seq_printf\n" . $herecurr) &&
				    $fix) {
					$fixed[$fixlinenr] =~ s/\bseq_printf\b/seq_puts/;
				}
			}
		}
		if ($perl_version_ok &&
		    defined $stat &&
		    $stat =~ /^\+(?![^\{]*\{\s*).*\b(\w+)\s*\(.*$String\s*,/s &&
		    $1 !~ /^_*volatile_*$/) {
			my $stat_real;
			my $lc = $stat =~ tr@\n@@;
			$lc = $lc + $linenr;
		        for (my $count = $linenr; $count <= $lc; $count++) {
				my $specifier;
				my $extension;
				my $qualifier;
				my $bad_specifier = "";
				my $fmt = get_quoted_string($lines[$count - 1], raw_line($count, 0));
				$fmt =~ s/%%//g;
				while ($fmt =~ /(\%[\*\d\.]*p(\w)(\w*))/g) {
					$specifier = $1;
					$extension = $2;
					$qualifier = $3;
					if ($extension !~ /[4SsBKRraEehMmIiUDdgVCbGNOxtf]/ ||
					    ($extension eq "f" &&
					     defined $qualifier && $qualifier !~ /^w/) ||
					    ($extension eq "4" &&
					     defined $qualifier && $qualifier !~ /^cc/)) {
						$bad_specifier = $specifier;
						last;
					}
					if ($extension eq "x" && !defined($stat_real)) {
						if (!defined($stat_real)) {
							$stat_real = get_stat_real($linenr, $lc);
						}
						WARN("VSPRINTF_SPECIFIER_PX",
						     "Using vsprintf specifier '\%px' potentially exposes the kernel memory layout, if you don't really need the address please consider using '\%p'.\n" . "$here\n$stat_real\n");
					}
				}
				if ($bad_specifier ne "") {
					my $stat_real = get_stat_real($linenr, $lc);
					my $msg_level = \&WARN;
					my $ext_type = "Invalid";
					my $use = "";
					if ($bad_specifier =~ /p[Ff]/) {
						$use = " - use %pS instead";
						$use =~ s/pS/ps/ if ($bad_specifier =~ /pf/);
					} elsif ($bad_specifier =~ /pA/) {
						$use =  " - '%pA' is only intended to be used from Rust code";
						$msg_level = \&ERROR;
					}
					&{$msg_level}("VSPRINTF_POINTER_EXTENSION",
						      "$ext_type vsprintf pointer extension '$bad_specifier'$use\n" . "$here\n$stat_real\n");
				}
			}
		}
		if ($perl_version_ok &&
		    defined $stat &&
		    $stat =~ /^\+(?:.*?)\bmemset\s*\(\s*$FuncArg\s*,\s*$FuncArg\s*\,\s*$FuncArg\s*\)/) {
			my $ms_addr = $2;
			my $ms_val = $7;
			my $ms_size = $12;
			if ($ms_size =~ /^(0x|)0$/i) {
				ERROR("MEMSET",
				      "memset to 0's uses 0 as the 2nd argument, not the 3rd\n" . "$here\n$stat\n");
			} elsif ($ms_size =~ /^(0x|)1$/i) {
				WARN("MEMSET",
				     "single byte memset is suspicious. Swapped 2nd/3rd argument?\n" . "$here\n$stat\n");
			}
		}
		if ($line =~ /\bstrcpy\s*\(/) {
			WARN("STRCPY",
			     "Prefer strscpy over strcpy - see: https://github.com/KSPP/linux/issues/88\n" . $herecurr);
		}
		if ($line =~ /\bstrlcpy\s*\(/) {
			WARN("STRLCPY",
			     "Prefer strscpy over strlcpy - see: https://github.com/KSPP/linux/issues/89\n" . $herecurr);
		}
		if ($line =~ /\bstrncpy\s*\(/) {
			WARN("STRNCPY",
			     "Prefer strscpy, strscpy_pad, or __nonstring over strncpy - see: https://github.com/KSPP/linux/issues/90\n" . $herecurr);
		}
		if ($perl_version_ok &&
		    defined $stat &&
		    $stat =~ /^\+(?:.*?)\b(min|max)\s*\(\s*$FuncArg\s*,\s*$FuncArg\s*\)/) {
			if (defined $2 || defined $7) {
				my $call = $1;
				my $cast1 = deparenthesize($2);
				my $arg1 = $3;
				my $cast2 = deparenthesize($7);
				my $arg2 = $8;
				my $cast;
				if ($cast1 ne "" && $cast2 ne "" && $cast1 ne $cast2) {
					$cast = "$cast1 or $cast2";
				} elsif ($cast1 ne "") {
					$cast = $cast1;
				} else {
					$cast = $cast2;
				}
				WARN("MINMAX",
				     "$call() should probably be ${call}_t($cast, $arg1, $arg2)\n" . "$here\n$stat\n");
			}
		}
		if ($perl_version_ok &&
		    defined $stat &&
		    $stat =~ /^\+(?:.*?)\busleep_range\s*\(\s*($FuncArg)\s*,\s*($FuncArg)\s*\)/) {
			my $min = $1;
			my $max = $7;
			if ($min eq $max) {
				WARN("USLEEP_RANGE",
				     "usleep_range should not use min == max args; see Documentation/timers/timers-howto.rst\n" . "$here\n$stat\n");
			} elsif ($min =~ /^\d+$/ && $max =~ /^\d+$/ &&
				 $min > $max) {
				WARN("USLEEP_RANGE",
				     "usleep_range args reversed, use min then max; see Documentation/timers/timers-howto.rst\n" . "$here\n$stat\n");
			}
		}
		if ($perl_version_ok &&
		    defined $stat &&
		    $line =~ /\bsscanf\b/ &&
		    ($stat !~ /$Ident\s*=\s*sscanf\s*$balanced_parens/ &&
		     $stat !~ /\bsscanf\s*$balanced_parens\s*(?:$Compare)/ &&
		     $stat !~ /(?:$Compare)\s*\bsscanf\s*$balanced_parens/)) {
			my $lc = $stat =~ tr@\n@@;
			$lc = $lc + $linenr;
			my $stat_real = get_stat_real($linenr, $lc);
			WARN("NAKED_SSCANF",
			     "unchecked sscanf return value\n" . "$here\n$stat_real\n");
		}
		if ($perl_version_ok &&
		    defined $stat &&
		    $line =~ /\bsscanf\b/) {
			my $lc = $stat =~ tr@\n@@;
			$lc = $lc + $linenr;
			my $stat_real = get_stat_real($linenr, $lc);
			if ($stat_real =~ /\bsscanf\b\s*\(\s*$FuncArg\s*,\s*("[^"]+")/) {
				my $format = $6;
				my $count = $format =~ tr@%@%@;
				if ($count == 1 &&
				    $format =~ /^"\%(?i:ll[udxi]|[udxi]ll|ll|[hl]h?[udxi]|[udxi][hl]h?|[hl]h?|[udxi])"$/) {
					WARN("SSCANF_TO_KSTRTO",
					     "Prefer kstrto<type> to single variable sscanf\n" . "$here\n$stat_real\n");
				}
			}
		}
		if ($realfile =~ /\.h$/ &&
		    $line =~ /^\+\s*(extern\s+)$Type\s*$Ident\s*\(/s) {
			if (CHK("AVOID_EXTERNS",
				"extern prototypes should be avoided in .h files\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/(.*)\bextern\b\s*(.*)/$1$2/;
			}
		}
		if ($realfile =~ /\.c$/ && defined $stat &&
		    $stat =~ /^.\s*(?:extern\s+)?$Type\s+($Ident)(\s*)\(/s)
		{
			my $function_name = $1;
			my $paren_space = $2;
			my $s = $stat;
			if (defined $cond) {
				substr($s, 0, length($cond), '');
			}
			if ($s =~ /^\s*;/)
			{
				WARN("AVOID_EXTERNS",
				     "externs should be avoided in .c files\n" .  $herecurr);
			}
			if ($paren_space =~ /\n/) {
				WARN("FUNCTION_ARGUMENTS",
				     "arguments for function declarations should follow identifier\n" . $herecurr);
			}
		} elsif ($realfile =~ /\.c$/ && defined $stat &&
		    $stat =~ /^\+extern struct\s+(\w+)\s+(\w+)\[\];/)
		{
			my ($st_type, $st_name) = ($1, $2);
			for my $s (keys %maybe_linker_symbol) {
			    goto LIKELY_LINKER_SYMBOL
				if $st_name =~ /$s/;
			}
			WARN("AVOID_EXTERNS",
			     "found a file-scoped extern type:$st_type name:$st_name in .c file\n"
			     . "is this a linker symbol ?\n" . $herecurr);
		  LIKELY_LINKER_SYMBOL:
		} elsif ($realfile =~ /\.c$/ && defined $stat &&
		    $stat =~ /^.\s*extern\s+/)
		{
			WARN("AVOID_EXTERNS",
			     "externs should be avoided in .c files\n" .  $herecurr);
		}
		if (defined $stat &&
		    $stat =~ /^.\s*(?:extern\s+)?$Type\s*(?:$Ident|\(\s*\*\s*$Ident\s*\))\s*\(\s*([^{]+)\s*\)\s*;/s &&
		    $1 ne "void") {
			my $args = trim($1);
			while ($args =~ m/\s*($Type\s*(?:$Ident|\(\s*\*\s*$Ident?\s*\)\s*$balanced_parens)?)/g) {
				my $arg = trim($1);
				if ($arg =~ /^$Type$/ && $arg !~ /enum\s+$Ident$/) {
					WARN("FUNCTION_ARGUMENTS",
					     "function definition argument '$arg' should also have an identifier name\n" . $herecurr);
				}
			}
		}
		if ($perl_version_ok &&
		    defined $stat &&
		    $stat =~ /^.\s*(?:$Storage\s+)?$Type\s*($Ident)\s*$balanced_parens\s*{/s) {
			$context_function = $1;
			my $ok = 0;
			my $cnt = statement_rawlines($stat);
			my $herectx = $here . "\n";
			for (my $n = 0; $n < $cnt; $n++) {
				my $rl = raw_line($linenr, $n);
				$herectx .=  $rl . "\n";
				$ok = 1 if ($rl =~ /^[ \+]\{/);
				$ok = 1 if ($rl =~ /\{/ && $n == 0);
				last if $rl =~ /^[ \+].*\{/;
			}
			if (!$ok) {
				ERROR("OPEN_BRACE",
				      "open brace '{' following function definitions go on the next line\n" . $herectx);
			}
		}
		if ($rawline =~ /\b__setup\("([^"]*)"/) {
			my $name = $1;
			if (!grep(/$name/, @setup_docs)) {
				CHK("UNDOCUMENTED_SETUP",
				    "__setup appears un-documented -- check Documentation/admin-guide/kernel-parameters.txt\n" . $herecurr);
			}
		}
		if ($line =~ /\*\s*\)\s*$allocFunctions\b/) {
			WARN("UNNECESSARY_CASTS",
			     "unnecessary cast may hide bugs, see http://c-faq.com/malloc/mallocnocast.html\n" . $herecurr);
		}
		if ($perl_version_ok &&
		    $line =~ /\b($Lval)\s*\=\s*(?:$balanced_parens)?\s*((?:kv|k|v)[mz]alloc(?:_node)?)\s*\(\s*(sizeof\s*\(\s*struct\s+$Lval\s*\))/) {
			CHK("ALLOC_SIZEOF_STRUCT",
			    "Prefer $3(sizeof(*$1)...) over $3($4...)\n" . $herecurr);
		}
		if ($perl_version_ok &&
		    defined $stat &&
		    $stat =~ /^\+\s*($Lval)\s*\=\s*(?:$balanced_parens)?\s*((?:kv|k)[mz]alloc)\s*\(\s*($FuncArg)\s*\*\s*($FuncArg)\s*,/) {
			my $oldfunc = $3;
			my $a1 = $4;
			my $a2 = $10;
			my $newfunc = "kmalloc_array";
			$newfunc = "kvmalloc_array" if ($oldfunc eq "kvmalloc");
			$newfunc = "kvcalloc" if ($oldfunc eq "kvzalloc");
			$newfunc = "kcalloc" if ($oldfunc eq "kzalloc");
			my $r1 = $a1;
			my $r2 = $a2;
			if ($a1 =~ /^sizeof\s*\S/) {
				$r1 = $a2;
				$r2 = $a1;
			}
			if ($r1 !~ /^sizeof\b/ && $r2 =~ /^sizeof\s*\S/ &&
			    !($r1 =~ /^$Constant$/ || $r1 =~ /^[A-Z_][A-Z0-9_]*$/)) {
				my $cnt = statement_rawlines($stat);
				my $herectx = get_stat_here($linenr, $cnt, $here);
				if (WARN("ALLOC_WITH_MULTIPLY",
					 "Prefer $newfunc over $oldfunc with multiply\n" . $herectx) &&
				    $cnt == 1 &&
				    $fix) {
					$fixed[$fixlinenr] =~ s/\b($Lval)\s*\=\s*(?:$balanced_parens)?\s*((?:kv|k)[mz]alloc)\s*\(\s*($FuncArg)\s*\*\s*($FuncArg)/$1 . ' = ' . "$newfunc(" . trim($r1) . ', ' . trim($r2)/e;
				}
			}
		}
		if ($perl_version_ok &&
		    $line =~ /\b($Lval)\s*\=\s*(?:$balanced_parens)?\s*krealloc\s*\(\s*($Lval)\s*,/ &&
		    $1 eq $3) {
			WARN("KREALLOC_ARG_REUSE",
			     "Reusing the krealloc arg is almost always a bug\n" . $herecurr);
		}
		if ($line =~ /\b((?:devm_)?((?:k|kv)?(calloc|malloc_array)(?:_node)?))\s*\(\s*sizeof\b/) {
			WARN("ALLOC_ARRAY_ARGS",
			     "$1 uses number as first arg, sizeof is generally wrong\n" . $herecurr);
		}
		if ($line =~ /;\s*;\s*$/) {
			if (WARN("ONE_SEMICOLON",
				 "Statements terminations use 1 semicolon\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/(\s*;\s*){2,}$/;/g;
			}
		}
		if ($realfile !~ m@^include/uapi/@ &&
		    $line =~ /
			my $ull = "";
			$ull = "_ULL" if (defined($1) && $1 =~ /ll/i);
			if (CHK("BIT_MACRO",
				"Prefer using the BIT$ull macro\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/\(?\s*1\s*[ulUL]*\s*<<\s*(\d+|$Ident)\s*\)?/BIT${ull}($1)/;
			}
		}
		if ($rawline =~ /\bIS_ENABLED\s*\(\s*(\w+)\s*\)/ && $1 !~ /^${CONFIG_}/) {
			WARN("IS_ENABLED_CONFIG",
			     "IS_ENABLED($1) is normally used as IS_ENABLED(${CONFIG_}$1)\n" . $herecurr);
		}
		if ($line =~ /^\+\s*
			my $config = $1;
			if (WARN("PREFER_IS_ENABLED",
				 "Prefer IS_ENABLED(<FOO>) to ${CONFIG_}<FOO> || ${CONFIG_}<FOO>_MODULE\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] = "\+#if IS_ENABLED($config)";
			}
		}
		my @fallthroughs = (
			'fallthrough',
			'@fallthrough@',
			'lint -fallthrough[ \t]*',
			'intentional(?:ly)?[ \t]*fall(?:(?:s | |-)[Tt]|t)hr(?:ough|u|ew)',
			'(?:else,?\s*)?FALL(?:S | |-)?THR(?:OUGH|U|EW)[ \t.!]*(?:-[^\n\r]*)?',
			'Fall(?:(?:s | |-)[Tt]|t)hr(?:ough|u|ew)[ \t.!]*(?:-[^\n\r]*)?',
			'fall(?:s | |-)?thr(?:ough|u|ew)[ \t.!]*(?:-[^\n\r]*)?',
		    );
		if ($raw_comment ne '') {
			foreach my $ft (@fallthroughs) {
				if ($raw_comment =~ /$ft/) {
					my $msg_level = \&WARN;
					$msg_level = \&CHK if ($file);
					&{$msg_level}("PREFER_FALLTHROUGH",
						      "Prefer 'fallthrough;' over fallthrough comment\n" . $herecurr);
					last;
				}
			}
		}
		if ($perl_version_ok &&
		    defined $stat &&
		    $stat =~ /^\+[$;\s]*(?:case[$;\s]+\w+[$;\s]*:[$;\s]*|)*[$;\s]*\bdefault[$;\s]*:[$;\s]*;/g) {
			my $cnt = statement_rawlines($stat);
			my $herectx = get_stat_here($linenr, $cnt, $here);
			WARN("DEFAULT_NO_BREAK",
			     "switch default: should use break\n" . $herectx);
		}
		if ($line =~ /\b__FUNCTION__\b/) {
			if (WARN("USE_FUNC",
				 "__func__ should be used instead of gcc specific __FUNCTION__\n"  . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/\b__FUNCTION__\b/__func__/g;
			}
		}
		while ($line =~ /\b(__(?:DATE|TIME|TIMESTAMP)__)\b/g) {
			ERROR("DATE_TIME",
			      "Use of the '$1' macro makes the build non-deterministic\n" . $herecurr);
		}
		if ($line =~ /\byield\s*\(\s*\)/) {
			WARN("YIELD",
			     "Using yield() is generally wrong. See yield() kernel-doc (sched/core.c)\n"  . $herecurr);
		}
		if ($line =~ /\+\s*(.*?)\b(true|false|$Lval)\s*(==|\!=)\s*(true|false|$Lval)\b(.*)$/i) {
			my $lead = $1;
			my $arg = $2;
			my $test = $3;
			my $otype = $4;
			my $trail = $5;
			my $op = "!";
			($arg, $otype) = ($otype, $arg) if ($arg =~ /^(?:true|false)$/i);
			my $type = lc($otype);
			if ($type =~ /^(?:true|false)$/) {
				if (("$test" eq "==" && "$type" eq "true") ||
				    ("$test" eq "!=" && "$type" eq "false")) {
					$op = "";
				}
				CHK("BOOL_COMPARISON",
				    "Using comparison to $otype is error prone\n" . $herecurr);
			}
		}
		if ($line =~ /^.\s*sema_init.+,\W?0\W?\)/) {
			WARN("CONSIDER_COMPLETION",
			     "consider using a completion\n" . $herecurr);
		}
		if ($line =~ /\b((simple|strict)_(strto(l|ll|ul|ull)))\s*\(/) {
			WARN("CONSIDER_KSTRTO",
			     "$1 is obsolete, use k$3 instead\n" . $herecurr);
		}
		if ($line =~ /^.\s*__initcall\s*\(/) {
			WARN("USE_DEVICE_INITCALL",
			     "please use device_initcall() or more appropriate function instead of __initcall() (see include/linux/init.h)\n" . $herecurr);
		}
		if ($line =~ /\bspin_is_locked\(/) {
			WARN("USE_LOCKDEP",
			     "Where possible, use lockdep_assert_held instead of assertions based on spin_is_locked\n" . $herecurr);
		}
		if ($line =~ /\b($deprecated_apis_search)\b\s*\(/) {
			my $deprecated_api = $1;
			my $new_api = $deprecated_apis{$deprecated_api};
			WARN("DEPRECATED_API",
			     "Deprecated use of '$deprecated_api', prefer '$new_api' instead\n" . $herecurr);
		}
		if (defined($const_structs) &&
		    $line !~ /\bconst\b/ &&
		    $line =~ /\bstruct\s+($const_structs)\b(?!\s*\{)/) {
			WARN("CONST_STRUCT",
			     "struct $1 should normally be const\n" . $herecurr);
		}
		if ($line =~ /\bNR_CPUS\b/ &&
		    $line !~ /^.\s*\s*
		    $line !~ /^.\s*\s*
		    $line !~ /^.\s*$Declare\s.*\[[^\]]*NR_CPUS[^\]]*\]/ &&
		    $line !~ /\[[^\]]*\.\.\.[^\]]*NR_CPUS[^\]]*\]/ &&
		    $line !~ /\[[^\]]*NR_CPUS[^\]]*\.\.\.[^\]]*\]/ &&
		    $line !~ /^.\s*\.\w+\s*=\s*.*\bNR_CPUS\b/)
		{
			WARN("NR_CPUS",
			     "usage of NR_CPUS is often wrong - consider using cpu_possible(), num_possible_cpus(), for_each_possible_cpu(), etc\n" . $herecurr);
		}
		if ($line =~ /\+\s*
			ERROR("DEFINE_ARCH_HAS",
			      "#define of '$1' is wrong - use Kconfig variables or standard guards instead\n" . $herecurr);
		}
		if ($perl_version_ok &&
		    $line =~ /\b((?:un)?likely)\s*\(\s*$FuncArg\s*\)\s*$Compare/) {
			WARN("LIKELY_MISUSE",
			     "Using $1 should generally have parentheses around the comparison\n" . $herecurr);
		}
		if ($line =~ /\breturn\s+sysfs_emit\s*\(\s*$FuncArg\s*,\s*($String)/ &&
		    substr($rawline, $-[6], $+[6] - $-[6]) !~ /\\n"$/) {
			my $offset = $+[6] - 1;
			if (WARN("SYSFS_EMIT",
				 "return sysfs_emit(...) formats should include a terminating newline\n" . $herecurr) &&
			    $fix) {
				substr($fixed[$fixlinenr], $offset, 0) = '\\n';
			}
		}
		if ($sline =~ /^[\+ ]\s*\}(?:\s*__packed)?\s*;\s*$/ &&
		    $prevline =~ /^\+\s*(?:\}(?:\s*__packed\s*)?|$Type)\s*$Ident\s*\[\s*(0|1)\s*\]\s*;\s*$/) {
			if (ERROR("FLEXIBLE_ARRAY",
				  "Use C99 flexible arrays - see https://docs.kernel.org/process/deprecated.html#zero-length-and-one-element-arrays\n" . $hereprev) &&
			    $1 == '0' && $fix) {
				$fixed[$fixlinenr - 1] =~ s/\[\s*0\s*\]/[]/;
			}
		}
		if ($line =~ /\b(?:(?:un)?likely)\s*\(\s*!?\s*(IS_ERR(?:_OR_NULL|_VALUE)?|WARN)/) {
			WARN("LIKELY_MISUSE",
			     "nested (un)?likely() calls, $1 already uses unlikely() internally\n" . $herecurr);
		}
		if ($line =~ /\bin_atomic\s*\(/) {
			if ($realfile =~ m@^drivers/@) {
				ERROR("IN_ATOMIC",
				      "do not use in_atomic in drivers\n" . $herecurr);
			} elsif ($realfile !~ m@^kernel/@) {
				WARN("IN_ATOMIC",
				     "use of in_atomic() is incorrect outside core kernel code\n" . $herecurr);
			}
		}
		our $rcu_trace_funcs = qr{(?x:
			rcu_read_lock_trace |
			rcu_read_lock_trace_held |
			rcu_read_unlock_trace |
			call_rcu_tasks_trace |
			synchronize_rcu_tasks_trace |
			rcu_barrier_tasks_trace |
			rcu_request_urgent_qs_task
		)};
		our $rcu_trace_paths = qr{(?x:
			kernel/bpf/ |
			include/linux/bpf |
			net/bpf/ |
			kernel/rcu/ |
			include/linux/rcu
		)};
		if ($line =~ /\b($rcu_trace_funcs)\s*\(/) {
			if ($realfile !~ m{^$rcu_trace_paths}) {
				WARN("RCU_TASKS_TRACE",
				     "use of RCU tasks trace is incorrect outside BPF or core RCU code\n" . $herecurr);
			}
		}
		if ($line =~ /^.\s*lockdep_set_novalidate_class\s*\(/ ||
		    $line =~ /__lockdep_no_validate__\s*\)/ ) {
			if ($realfile !~ m@^kernel/lockdep@ &&
			    $realfile !~ m@^include/linux/lockdep@ &&
			    $realfile !~ m@^drivers/base/core@) {
				ERROR("LOCKDEP",
				      "lockdep_no_validate class is reserved for device->mutex.\n" . $herecurr);
			}
		}
		if ($line =~ /debugfs_create_\w+.*\b$mode_perms_world_writable\b/ ||
		    $line =~ /DEVICE_ATTR.*\b$mode_perms_world_writable\b/) {
			WARN("EXPORTED_WORLD_WRITABLE",
			     "Exporting world writable files is usually an error. Consider more restrictive permissions.\n" . $herecurr);
		}
		if ($perl_version_ok &&
		    defined $stat &&
		    $stat =~ /\bDEVICE_ATTR\s*\(\s*(\w+)\s*,\s*\(?\s*(\s*(?:${multi_mode_perms_string_search}|0[0-7]{3,3})\s*)\s*\)?\s*,\s*(\w+)\s*,\s*(\w+)\s*\)/) {
			my $var = $1;
			my $perms = $2;
			my $show = $3;
			my $store = $4;
			my $octal_perms = perms_to_octal($perms);
			if ($show =~ /^${var}_show$/ &&
			    $store =~ /^${var}_store$/ &&
			    $octal_perms eq "0644") {
				if (WARN("DEVICE_ATTR_RW",
					 "Use DEVICE_ATTR_RW\n" . $herecurr) &&
				    $fix) {
					$fixed[$fixlinenr] =~ s/\bDEVICE_ATTR\s*\(\s*$var\s*,\s*\Q$perms\E\s*,\s*$show\s*,\s*$store\s*\)/DEVICE_ATTR_RW(${var})/;
				}
			} elsif ($show =~ /^${var}_show$/ &&
				 $store =~ /^NULL$/ &&
				 $octal_perms eq "0444") {
				if (WARN("DEVICE_ATTR_RO",
					 "Use DEVICE_ATTR_RO\n" . $herecurr) &&
				    $fix) {
					$fixed[$fixlinenr] =~ s/\bDEVICE_ATTR\s*\(\s*$var\s*,\s*\Q$perms\E\s*,\s*$show\s*,\s*NULL\s*\)/DEVICE_ATTR_RO(${var})/;
				}
			} elsif ($show =~ /^NULL$/ &&
				 $store =~ /^${var}_store$/ &&
				 $octal_perms eq "0200") {
				if (WARN("DEVICE_ATTR_WO",
					 "Use DEVICE_ATTR_WO\n" . $herecurr) &&
				    $fix) {
					$fixed[$fixlinenr] =~ s/\bDEVICE_ATTR\s*\(\s*$var\s*,\s*\Q$perms\E\s*,\s*NULL\s*,\s*$store\s*\)/DEVICE_ATTR_WO(${var})/;
				}
			} elsif ($octal_perms eq "0644" ||
				 $octal_perms eq "0444" ||
				 $octal_perms eq "0200") {
				my $newshow = "$show";
				$newshow = "${var}_show" if ($show ne "NULL" && $show ne "${var}_show");
				my $newstore = $store;
				$newstore = "${var}_store" if ($store ne "NULL" && $store ne "${var}_store");
				my $rename = "";
				if ($show ne $newshow) {
					$rename .= " '$show' to '$newshow'";
				}
				if ($store ne $newstore) {
					$rename .= " '$store' to '$newstore'";
				}
				WARN("DEVICE_ATTR_FUNCTIONS",
				     "Consider renaming function(s)$rename\n" . $herecurr);
			} else {
				WARN("DEVICE_ATTR_PERMS",
				     "DEVICE_ATTR unusual permissions '$perms' used\n" . $herecurr);
			}
		}
		if ($perl_version_ok &&
		    defined $stat &&
		    $line =~ /$mode_perms_search/) {
			foreach my $entry (@mode_permission_funcs) {
				my $func = $entry->[0];
				my $arg_pos = $entry->[1];
				my $lc = $stat =~ tr@\n@@;
				$lc = $lc + $linenr;
				my $stat_real = get_stat_real($linenr, $lc);
				my $skip_args = "";
				if ($arg_pos > 1) {
					$arg_pos--;
					$skip_args = "(?:\\s*$FuncArg\\s*,\\s*){$arg_pos,$arg_pos}";
				}
				my $test = "\\b$func\\s*\\(${skip_args}($FuncArg(?:\\|\\s*$FuncArg)*)\\s*[,\\)]";
				if ($stat =~ /$test/) {
					my $val = $1;
					$val = $6 if ($skip_args ne "");
					if (!($func =~ /^(?:module_param|proc_create)/ && $val eq "0") &&
					    (($val =~ /^$Int$/ && $val !~ /^$Octal$/) ||
					     ($val =~ /^$Octal$/ && length($val) ne 4))) {
						ERROR("NON_OCTAL_PERMISSIONS",
						      "Use 4 digit octal (0777) not decimal permissions\n" . "$here\n" . $stat_real);
					}
					if ($val =~ /^$Octal$/ && (oct($val) & 02)) {
						ERROR("EXPORTED_WORLD_WRITABLE",
						      "Exporting writable files is usually an error. Consider more restrictive permissions.\n" . "$here\n" . $stat_real);
					}
				}
			}
		}
		while ($line =~ m{\b($multi_mode_perms_string_search)\b}g) {
			my $oval = $1;
			my $octal = perms_to_octal($oval);
			if (WARN("SYMBOLIC_PERMS",
				 "Symbolic permissions '$oval' are not preferred. Consider using octal permissions '$octal'.\n" . $herecurr) &&
			    $fix) {
				$fixed[$fixlinenr] =~ s/\Q$oval\E/$octal/;
			}
		}
		if ($line =~ /\bMODULE_LICENSE\s*\(\s*($String)\s*\)/) {
			my $extracted_string = get_quoted_string($line, $rawline);
			my $valid_licenses = qr{
						GPL|
						GPL\ v2|
						GPL\ and\ additional\ rights|
						Dual\ BSD/GPL|
						Dual\ MIT/GPL|
						Dual\ MPL/GPL|
						Proprietary
					}x;
			if ($extracted_string !~ /^"(?:$valid_licenses)"$/x) {
				WARN("MODULE_LICENSE",
				     "unknown module license " . $extracted_string . "\n" . $herecurr);
			}
			if (!$file && $extracted_string eq '"GPL v2"') {
				if (WARN("MODULE_LICENSE",
				     "Prefer \"GPL\" over \"GPL v2\" - see commit bf7fbeeae6db (\"module: Cure the MODULE_LICENSE \"GPL\" vs. \"GPL v2\" bogosity\")\n" . $herecurr) &&
				    $fix) {
					$fixed[$fixlinenr] =~ s/\bMODULE_LICENSE\s*\(\s*"GPL v2"\s*\)/MODULE_LICENSE("GPL")/;
				}
			}
		}
		if ($line =~ /\.extra[12]\s*=\s*&(zero|one|int_max)\b/) {
			WARN("DUPLICATED_SYSCTL_CONST",
				"duplicated sysctl range checking value '$1', consider using the shared one in include/linux/sysctl.h\n" . $herecurr);
		}
	}
	if ($
		exit(0);
	}
	if ($mailback && ($clean == 1 || !$is_patch)) {
		exit(0);
	}
	if (!$chk_patch && !$is_patch) {
		exit(0);
	}
	if (!$is_patch && $filename !~ /cover-letter\.patch$/) {
		ERROR("NOT_UNIFIED_DIFF",
		      "Does not appear to be a unified-diff format patch\n");
	}
	if ($is_patch && $has_commit_log && $chk_signoff) {
		if ($signoff == 0) {
			ERROR("MISSING_SIGN_OFF",
			      "Missing Signed-off-by: line(s)\n");
		} elsif ($authorsignoff != 1) {
			my $sob_msg = "'From: $author' != 'Signed-off-by: $author_sob'";
			if ($authorsignoff == 0) {
				ERROR("NO_AUTHOR_SIGN_OFF",
				      "Missing Signed-off-by: line by nominal patch author '$author'\n");
			} elsif ($authorsignoff == 2) {
				CHK("FROM_SIGN_OFF_MISMATCH",
				    "From:/Signed-off-by: email comments mismatch: $sob_msg\n");
			} elsif ($authorsignoff == 3) {
				WARN("FROM_SIGN_OFF_MISMATCH",
				     "From:/Signed-off-by: email name mismatch: $sob_msg\n");
			} elsif ($authorsignoff == 4) {
				WARN("FROM_SIGN_OFF_MISMATCH",
				     "From:/Signed-off-by: email address mismatch: $sob_msg\n");
			} elsif ($authorsignoff == 5) {
				WARN("FROM_SIGN_OFF_MISMATCH",
				     "From:/Signed-off-by: email subaddress mismatch: $sob_msg\n");
			}
		}
	}
	print report_dump();
	if ($summary && !($clean == 1 && $quiet == 1)) {
		print "$filename " if ($summary_file);
		print "total: $cnt_error errors, $cnt_warn warnings, " .
			(($check)? "$cnt_chk checks, " : "") .
			"$cnt_lines lines checked\n";
	}
	if ($quiet == 0) {
		if (!$clean and !$fix) {
			print << "EOM"
NOTE: For some of the reported defects, checkpatch may be able to
      mechanically convert to the typical style using --fix or --fix-inplace.
EOM
		}
		if ($rpt_cleaners) {
			$rpt_cleaners = 0;
			print << "EOM"
NOTE: Whitespace errors detected.
      You may wish to use scripts/cleanpatch or scripts/cleanfile
EOM
		}
	}
	if ($clean == 0 && $fix &&
	    ("@rawlines" ne "@fixed" ||
	     $
		my $newfile = $filename;
		$newfile .= ".EXPERIMENTAL-checkpatch-fixes" if (!$fix_inplace);
		my $linecount = 0;
		my $f;
		@fixed = fix_inserted_deleted_lines(\@fixed, \@fixed_inserted, \@fixed_deleted);
		open($f, '>', $newfile)
		    or die "$P: Can't open $newfile for write\n";
		foreach my $fixed_line (@fixed) {
			$linecount++;
			if ($file) {
				if ($linecount > 3) {
					$fixed_line =~ s/^\+//;
					print $f $fixed_line . "\n";
				}
			} else {
				print $f $fixed_line . "\n";
			}
		}
		close($f);
		if (!$quiet) {
			print << "EOM";
Wrote EXPERIMENTAL --fix correction(s) to '$newfile'
Do _NOT_ trust the results written to this file.
Do _NOT_ submit these changes without inspecting them for correctness.
This EXPERIMENTAL file is simply a convenience to help rewrite patches.
No warranties, expressed or implied...
EOM
		}
	}
	if ($quiet == 0) {
		print "\n";
		if ($clean == 1) {
			print "$vname has no obvious style problems and is ready for submission.\n";
		} else {
			print "$vname has style problems, please review.\n";
		}
	}
	return $clean;
}
