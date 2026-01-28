require 5.0;
use warnings;
use IO::File;
use Getopt::Std;
use strict;
my $usage =
"usage: cstyle [-cgpvP] file...
	-c	check continuation indentation inside functions
	-g	print github actions' workflow commands
	-p	perform some of the more picky checks
	-v	verbose
	-P	check for use of non-POSIX types
";
my %opts;
if (!getopts("cghpvCP", \%opts)) {
	print $usage;
	exit 2;
}
my $check_continuation = $opts{'c'};
my $github_workflow = $opts{'g'} || $ENV{'CI'};
my $picky = $opts{'p'};
my $verbose = $opts{'v'};
my $check_posix_types = $opts{'P'};
my ($filename, $line, $prev);		
my $fmt;
my $hdr_comment_start;
if ($verbose) {
	$fmt = "%s: %d: %s\n%s\n";
} else {
	$fmt = "%s: %d: %s\n";
}
$hdr_comment_start = qr/^\s*\/\*$/;
my $typename = '(int|char|short|long|unsigned|float|double' .
    '|\w+_t|struct\s+\w+|union\s+\w+|FILE)';
my %old2posix = (
	'unchar' => 'uchar_t',
	'ushort' => 'ushort_t',
	'uint' => 'uint_t',
	'ulong' => 'ulong_t',
	'u_int' => 'uint_t',
	'u_short' => 'ushort_t',
	'u_long' => 'ulong_t',
	'u_char' => 'uchar_t',
	'quad' => 'quad_t'
);
my $lint_re = qr/\/\*(?:
	NOTREACHED|LINTLIBRARY|VARARGS[0-9]*|
	CONSTCOND|CONSTANTCOND|CONSTANTCONDITION|EMPTY|
	FALLTHRU|FALLTHROUGH|LINTED.*?|PRINTFLIKE[0-9]*|
	PROTOLIB[0-9]*|SCANFLIKE[0-9]*|CSTYLED.*?
    )\*\//x;
my $warlock_re = qr/\/\*\s*(?:
	VARIABLES\ PROTECTED\ BY|
	MEMBERS\ PROTECTED\ BY|
	ALL\ MEMBERS\ PROTECTED\ BY|
	READ-ONLY\ VARIABLES:|
	READ-ONLY\ MEMBERS:|
	VARIABLES\ READABLE\ WITHOUT\ LOCK:|
	MEMBERS\ READABLE\ WITHOUT\ LOCK:|
	LOCKS\ COVERED\ BY|
	LOCK\ UNNEEDED\ BECAUSE|
	LOCK\ NEEDED:|
	LOCK\ HELD\ ON\ ENTRY:|
	READ\ LOCK\ HELD\ ON\ ENTRY:|
	WRITE\ LOCK\ HELD\ ON\ ENTRY:|
	LOCK\ ACQUIRED\ AS\ SIDE\ EFFECT:|
	READ\ LOCK\ ACQUIRED\ AS\ SIDE\ EFFECT:|
	WRITE\ LOCK\ ACQUIRED\ AS\ SIDE\ EFFECT:|
	LOCK\ RELEASED\ AS\ SIDE\ EFFECT:|
	LOCK\ UPGRADED\ AS\ SIDE\ EFFECT:|
	LOCK\ DOWNGRADED\ AS\ SIDE\ EFFECT:|
	FUNCTIONS\ CALLED\ THROUGH\ POINTER|
	FUNCTIONS\ CALLED\ THROUGH\ MEMBER|
	LOCK\ ORDER:
    )/x;
my $err_stat = 0;		
if ($
	foreach my $arg (@ARGV) {
		my $fh = new IO::File $arg, "r";
		if (!defined($fh)) {
			printf "%s: can not open\n", $arg;
		} else {
			&cstyle($arg, $fh);
			close $fh;
		}
	}
} else {
	&cstyle("<stdin>", *STDIN);
}
exit $err_stat;
my $no_errs = 0;		
sub err($) {
	my ($error) = @_;
	unless ($no_errs) {
		if ($verbose) {
			printf $fmt, $filename, $., $error, $line;
		} else {
			printf $fmt, $filename, $., $error;
		}
		if ($github_workflow) {
			printf "::error file=%s,line=%s::%s\n", $filename, $., $error;
		}
		$err_stat = 1;
	}
}
sub err_prefix($$) {
	my ($prevline, $error) = @_;
	my $out = $prevline."\n".$line;
	unless ($no_errs) {
		if ($verbose) {
			printf $fmt, $filename, $., $error, $out;
		} else {
			printf $fmt, $filename, $., $error;
		}
		$err_stat = 1;
	}
}
sub err_prev($) {
	my ($error) = @_;
	unless ($no_errs) {
		if ($verbose) {
			printf $fmt, $filename, $. - 1, $error, $prev;
		} else {
			printf $fmt, $filename, $. - 1, $error;
		}
		$err_stat = 1;
	}
}
sub cstyle($$) {
my ($fn, $filehandle) = @_;
$filename = $fn;			
my $in_cpp = 0;
my $next_in_cpp = 0;
my $in_comment = 0;
my $comment_done = 0;
my $in_warlock_comment = 0;
my $in_function = 0;
my $in_function_header = 0;
my $function_header_full_indent = 0;
my $in_declaration = 0;
my $note_level = 0;
my $nextok = 0;
my $nocheck = 0;
my $in_string = 0;
my ($okmsg, $comment_prefix);
$line = '';
$prev = '';
reset_indent();
line: while (<$filehandle>) {
	s/\r?\n$//;	
	$line = $_;
	$_ = '"' . $_		if ($in_string && !$nocheck && !$in_comment);
	s/'([^\\']|\\[^xX0]|\\0[0-9]*|\\[xX][0-9a-fA-F]*)'/''/g;
	s/"([^\\"]|\\.)*"/\"\"/g;
	if ($nocheck || $in_comment) {
		$in_string = 0;
	} else {
		$in_string =
		    (s/([^"](?:"")*)"([^\\"]|\\.)*\\$/$1""/ ||
		    s/^("")*"([^\\"]|\\.)*\\$/""/);
	}
	$in_cpp = $next_in_cpp || /^\s*
	$next_in_cpp = $in_cpp && /\\$/;	
	s/\s*\\$//;
	if ($nocheck) {
		if (/\/\* *END *CSTYLED *\*\//) {
			$nocheck = 0;
		} else {
			reset_indent();
			next line;
		}
	}
	if ($nextok) {
		if ($okmsg) {
			err($okmsg);
		}
		$nextok = 0;
		$okmsg = 0;
		if (/\/\* *CSTYLED.*\*\//) {
			/^.*\/\* *CSTYLED *(.*) *\*\/.*$/;
			$okmsg = $1;
			$nextok = 1;
		}
		$no_errs = 1;
	} elsif ($no_errs) {
		$no_errs = 0;
	}
	if (($line =~ tr/\t/\t/) * 7 + length($line) > 80) {
		my $eline = $line;
		1 while $eline =~
		    s/\t+/' ' x (length($&) * 8 - length($`) % 8)/e;
		if (length($eline) > 80) {
			err("line > 80 characters");
		}
	}
	if ($note_level || /\b_?NOTE\s*\(/) { 
		s/[^()]//g;			  
		$note_level += s/\(//g - length;  
		next;
	}
	if (/\/\* *BEGIN *CSTYLED *\*\//) {
		$nocheck = 1;
	}
	if (/\/\* *CSTYLED.*\*\//) {
		/^.*\/\* *CSTYLED *(.*) *\*\/.*$/;
		$okmsg = $1;
		$nextok = 1;
	}
	if (/\/\/ *CSTYLED/) {
		/^.*\/\/ *CSTYLED *(.*)$/;
		$okmsg = $1;
		$nextok = 1;
	}
	if (/\t +\t/) {
		err("spaces between tabs");
	}
	if (/ \t+ /) {
		err("tabs between spaces");
	}
	if (/\s$/) {
		err("space or tab at end of line");
	}
	if (/[^ \t(]\/\*/ && !/\w\(\/\*.*\*\/\);/) {
		err("comment preceded by non-blank");
	}
	if (/ARGSUSED/) {
		err("ARGSUSED directive");
	}
	if (/^\{$/ && $prev =~ /\)\s*(const\s*)?(\/\*.*\*\/\s*)?\\?$/) {
		$in_function = 1;
		$in_declaration = 1;
		$in_function_header = 0;
		$function_header_full_indent = 0;
		$prev = $line;
		next line;
	}
	if (/^\}\s*(\/\*.*\*\/\s*)*$/) {
		if ($prev =~ /^\s*return\s*;/) {
			err_prev("unneeded return at end of function");
		}
		$in_function = 0;
		reset_indent();		
		$prev = $line;
		next line;
	}
	if ($in_function_header && ! /^    (\w|\.)/ ) {
		if (/^\{\}$/ 
		|| /;/ 
		|| /
		|| /^[^\s\\]*\(.*\)$/ 
		|| /^$/ 
		) {
			$in_function_header = 0;
			$function_header_full_indent = 0;
		} elsif ($prev =~ /^__attribute__/) { 
			$in_function_header = 0;
			$function_header_full_indent = 0;
			$prev = $line;
			next line;
		} elsif ($picky	&& ! (/^\t/ && $function_header_full_indent != 0)) {
			err("continuation line should be indented by 4 spaces");
		}
	}
	if (/^\w+\(/ && !/\) \w+;/) {
		$in_function_header = 1;
		if (/\($/) {
			$function_header_full_indent = 1;
		}
	}
	if ($in_function_header && /^\{$/) {
		$in_function_header = 0;
		$function_header_full_indent = 0;
		$in_function = 1;
	}
	if ($in_function_header && /\);$/) {
		$in_function_header = 0;
		$function_header_full_indent = 0;
	}
	if ($in_function_header && /\{$/ ) {
		if ($picky) {
			err("opening brace on same line as function header");
		}
		$in_function_header = 0;
		$function_header_full_indent = 0;
		$in_function = 1;
		next line;
	}
	if ($in_warlock_comment && /\*\//) {
		$in_warlock_comment = 0;
		$prev = $line;
		next line;
	}
	if ($in_declaration && /^$/) {
		$in_declaration = 0;
	}
	if ($comment_done) {
		$in_comment = 0;
		$comment_done = 0;
	}
	if (/$hdr_comment_start/) {
		if (!/^\t*\/\*/) {
			err("block comment not indented by tabs");
		}
		$in_comment = 1;
		/^(\s*)\//;
		$comment_prefix = $1;
		$prev = $line;
		next line;
	}
	if ($in_comment) {
		if (/^$comment_prefix \*\/$/) {
			$comment_done = 1;
		} elsif (/\*\//) {
			$comment_done = 1;
			err("improper block comment close");
		} elsif (!/^$comment_prefix \*[ \t]/ &&
		    !/^$comment_prefix \*$/) {
			err("improper block comment");
		}
	}
	if (/[^ ]     / && !/".*     .*"/ && !$in_comment) {
		err("spaces instead of tabs");
	}
	if (/^ / && !/^ \*[ \t\/]/ && !/^ \*$/ &&
	    (!/^    (\w|\.)/ || $in_function != 0)) {
		err("indent by spaces instead of tabs");
	}
	if (/^\t+ [^ \t\*]/ || /^\t+  \S/ || /^\t+   \S/) {
		err("continuation line not indented by 4 spaces");
	}
	if (/$warlock_re/ && !/\*\//) {
		$in_warlock_comment = 1;
		$prev = $line;
		next line;
	}
	if (/^\s*\/\*./ && !/^\s*\/\*.*\*\// && !/$hdr_comment_start/) {
		err("improper first line of block comment");
	}
	if ($in_comment) {	
		$prev = $line;
		next line;
	}
	if ((/[^(]\/\*\S/ || /^\/\*\S/) && !/$lint_re/) {
		err("missing blank after open comment");
	}
	if (/\S\*\/[^)]|\S\*\/$/ && !/$lint_re/) {
		err("missing blank before close comment");
	}
	if (/\S.*\/\*/ && !/\S.*\/\*.*\*\// && !/\(\/\*/) {
		err("unterminated single line comment");
	}
	if (/^(
		$prev = $line;
		if ($picky) {
			my $directive = $1;
			my $clause = $2;
			if ((($1 eq "#endif") || ($1 eq "#else")) &&
			    ($clause ne "") &&
			    (!($clause =~ /^\s+\/\*.*\*\/$/)) &&
			    (!($clause =~ /^\s+\/\/.*$/))) {
				err("non-comment text following " .
				    "$directive (or malformed $directive " .
				    "directive)");
			}
		}
		next line;
	}
	s/\/\*.*?\*\///g;
	s/\/\/(?:\s.*)?$//;	
	if (s!//.*$!!) {		
		err("missing blank after start comment");
	}
	s/\s*$//;
	if (/[^<>\s][!<>=]=/ || /[^<>][!<>=]=[^\s,]/ ||
	    (/[^->]>[^,=>\s]/ && !/[^->]>$/) ||
	    (/[^<]<[^,=<\s]/ && !/[^<]<$/) ||
	    /[^<\s]<[^<]/ || /[^->\s]>[^>]/) {
		err("missing space around relational operator");
	}
	if (/\S>>=/ || /\S<<=/ || />>=\S/ || /<<=\S/ || /\S[-+*\/&|^%]=/ ||
	    (/[^-+*\/&|^%!<>=\s]=[^=]/ && !/[^-+*\/&|^%!<>=\s]=$/) ||
	    (/[^!<>=]=[^=\s]/ && !/[^!<>=]=$/)) {
		if (!/\soperator=/) {
			err("missing space around assignment operator");
		}
	}
	if (/[,;]\S/ && !/\bfor \(;;\)/) {
		err("comma or semicolon followed by non-blank");
	}
	if (/\s[,;]/ && !/^[\t]+;$/ && !/^\s*for \([^;]*; ;[^;]*\)/) {
		err("comma or semicolon preceded by blank");
	}
	if (/^\s*(&&|\|\|)/) {
		err("improper boolean continuation");
	}
	if (/\S   *(&&|\|\|)/ || /(&&|\|\|)   *\S/) {
		err("more than one space around boolean operator");
	}
	if (/\b(for|if|while|switch|sizeof|return|case)\(/) {
		err("missing space between keyword and paren");
	}
	if (/(\b(for|if|while|switch|return)\b.*){2,}/ && !/^
		err("more than one keyword on line");
	}
	if (/\b(for|if|while|switch|sizeof|return|case)\s\s+\(/ &&
	    !/^
		err("extra space between keyword and paren");
	}
	if (/\w\s\(/) {
		my $s = $_;
		s/\b(for|if|while|switch|return|case|sizeof)\s\(/XXX(/g;
		s/
		s/^
		s/\w\s\(+\*/XXX(*/g;
		s/\b($typename|void)\s+\(+/XXX(/og;
		if (/\w\s\(/) {
			err("extra space between function name and left paren");
		}
		$_ = $s;
	}
	if (/^(\w+(\s|\*)+)+\w+\(.*\)(\s|)*$/ &&
	    !/^(extern|static)\b/) {
		err("return type of function not on separate line");
	}
	if (/^
		err("#define followed by space instead of tab");
	}
	if (/^\s*return\W[^;]*;/ && !/^\s*return\s*\(.*\);/) {
		err("unparenthesized return expression");
	}
	if (/\bsizeof\b/ && !/\bsizeof\s*\(.*\)/) {
		err("unparenthesized sizeof expression");
	}
	if (/\(\s/) {
		err("whitespace after left paren");
	}
	if (/\s\)/ && !/^\s*for \([^;]*;[^;]*; \)/ && ($picky || !/^\s*\)/)) {
		err("whitespace before right paren");
	}
	if (/^\s*\(void\)[^ ]/) {
		err("missing space after (void) cast");
	}
	if (/\S\{/ && !/\{\{/) {
		err("missing space before left brace");
	}
	if ($in_function && /^\s+\{/ &&
	    ($prev =~ /\)\s*$/ || $prev =~ /\bstruct\s+\w+$/)) {
		err("left brace starting a line");
	}
	if (/\}(else|while)/) {
		err("missing space after right brace");
	}
	if (/\}\s\s+(else|while)/) {
		err("extra space after right brace");
	}
	if (/\b_VOID\b|\bVOID\b|\bSTATIC\b/) {
		err("obsolete use of VOID or STATIC");
	}
	if (/\b$typename\*/o) {
		err("missing space between type name and *");
	}
	if (/^\s+
		err("preprocessor statement not in column 1");
	}
	if (/^
		err("blank after preprocessor #");
	}
	if (/!\s*(strcmp|strncmp|bcmp)\s*\(/) {
		err("don't use boolean ! with comparison functions");
	}
	if ($check_continuation && $in_function && !$in_cpp) {
		process_indent($_);
	}
	if ($picky) {
		if ((/^\($typename( \*+)?\)\s/o ||
		    /\W\($typename( \*+)?\)\s/o) &&
		    !/sizeof\s*\($typename( \*)?\)\s/o &&
		    !/\($typename( \*+)?\)\s+=[^=]/o) {
			err("space after cast");
		}
		if (/\b$typename\s*\*\s/o &&
		    !/\b$typename\s*\*\s+const\b/o) {
			err("unary * followed by space");
		}
	}
	if ($check_posix_types) {
		if (/\b(unchar|ushort|uint|ulong|u_int|u_short|u_long|u_char|quad)\b/) {
			err("non-POSIX typedef $1 used: use $old2posix{$1} instead");
		}
	}
	if (/^\s*else\W/) {
		if ($prev =~ /^\s*\}$/) {
			err_prefix($prev,
			    "else and right brace should be on same line");
		}
	}
	$prev = $line;
}
if ($prev eq "") {
	err("last line in file is blank");
}
}
my $cont_in;		
my $cont_off;		
my $cont_noerr;		
my $cont_start;		
my $cont_base;		
my $cont_first;		
my $cont_multiseg;	
my $cont_special;	
my $cont_macro;		
my $cont_case;		
my @cont_paren;		
sub
reset_indent()
{
	$cont_in = 0;
	$cont_off = 0;
}
sub
delabel($)
{
	local $_ = $_[0];
	while (/^(\t*)( *(?:(?:\w+\s*)|(?:case\b[^:]*)): *)(.*)$/) {
		my ($pre_tabs, $label, $rest) = ($1, $2, $3);
		$_ = $pre_tabs;
		while ($label =~ s/^([^\t]*)(\t+)//) {
			$_ .= "\t" x (length($2) + length($1) / 8);
		}
		$_ .= ("\t" x (length($label) / 8)).$rest;
	}
	return ($_);
}
sub
process_indent($)
{
	require strict;
	local $_ = $_[0];			
	s///g;	
	s/\s+$//;	
	return			if (/^$/);	
	my $special = '(?:(?:\}\s*)?else\s+)?(?:if|for|while|switch)\b';
	my $macro = '[A-Z_][A-Z_0-9]*\(';
	my $case = 'case\b[^:]*$';
	if ($cont_off <= 0 && !/^\s*$special/ &&
	    (/(?:(?:\b(?:enum|struct|union)\s*[^\{]*)|(?:\s+=\s*))\{/ ||
	    (/^\s*\{/ && $prev =~ /=\s*(?:\/\*.*\*\/\s*)*$/))) {
		$cont_in = 0;
		$cont_off = tr/{/{/ - tr/}/}/;
		return;
	}
	if ($cont_off) {
		$cont_off += tr/{/{/ - tr/}/}/;
		return;
	}
	if (!$cont_in) {
		$cont_start = $line;
		if (/^\t* /) {
			err("non-continuation indented 4 spaces");
			$cont_noerr = 1;		
		}
		$_ = delabel($_);	
		return		if (/^\s*\}?$/);
		return		if (/^\s*\}?\s*else\s*\{?$/);
		return		if (/^\s*do\s*\{?$/);
		return		if (/\{$/);
		return		if (/\}[,;]?$/);
		return		if (/^\s*[A-Z_][A-Z_0-9]*$/);
		if (/\{/) {
			err("stuff after {");
			return;
		}
		/^(\t*)/;
		$cont_base = $1;
		$cont_in = 1;
		@cont_paren = ();
		$cont_first = 1;
		$cont_multiseg = 0;
		$cont_special = /^\s*$special/? 1 : 0;
		$cont_macro = /^\s*$macro/? 1 : 0;
		$cont_case = /^\s*$case/? 1 : 0;
	} else {
		$cont_first = 0;
		unless ($cont_noerr || /^$cont_base    / ||
		    (/^\t*(?:    )?(?:gettext\()?\"/ && !/^$cont_base\t/)) {
			err_prefix($cont_start,
			    "continuation should be indented 4 spaces");
		}
	}
	my $rest = $_;			
	foreach $_ (split /[^\(\)\[\]\{\}\;\:]*/) {
		next		if (length($_) == 0);
		my $rxp = "[^\Q$_\E]*\Q$_\E";
		$rest =~ s/^$rxp//;
		if (/\(/ || /\[/) {
			push @cont_paren, $_;
		} elsif (/\)/ || /\]/) {
			my $cur = $_;
			tr/\)\]/\(\[/;
			my $old = (pop @cont_paren);
			if (!defined($old)) {
				err("unexpected '$cur'");
				$cont_in = 0;
				last;
			} elsif ($old ne $_) {
				err("'$cur' mismatched with '$old'");
				$cont_in = 0;
				last;
			}
			next		if (@cont_paren != 0);
			if ($cont_special) {
				if ($rest =~ /^\s*\{?$/) {
					$cont_in = 0;
					last;
				}
				if ($rest =~ /^\s*;$/) {
					err("empty if/for/while body ".
					    "not on its own line");
					$cont_in = 0;
					last;
				}
				if (!$cont_first && $cont_multiseg == 1) {
					err_prefix($cont_start,
					    "multiple statements continued ".
					    "over multiple lines");
					$cont_multiseg = 2;
				} elsif ($cont_multiseg == 0) {
					$cont_multiseg = 1;
				}
				goto section_ended;
			}
			if ($cont_macro) {
				if ($rest =~ /^$/) {
					$cont_in = 0;
					last;
				}
			}
		} elsif (/\;/) {
			if ($cont_case) {
				err("unexpected ;");
			} elsif (!$cont_special) {
				err("unexpected ;")	if (@cont_paren != 0);
				if (!$cont_first && $cont_multiseg == 1) {
					err_prefix($cont_start,
					    "multiple statements continued ".
					    "over multiple lines");
					$cont_multiseg = 2;
				} elsif ($cont_multiseg == 0) {
					$cont_multiseg = 1;
				}
				if ($rest =~ /^$/) {
					$cont_in = 0;
					last;
				}
				if ($rest =~ /^\s*special/) {
					err("if/for/while/switch not started ".
					    "on its own line");
				}
				goto section_ended;
			}
		} elsif (/\{/) {
			err("{ while in parens/brackets") if (@cont_paren != 0);
			err("stuff after {")		if ($rest =~ /[^\s}]/);
			$cont_in = 0;
			last;
		} elsif (/\}/) {
			err("} while in parens/brackets") if (@cont_paren != 0);
			if (!$cont_special && $rest !~ /^\s*(while|else)\b/) {
				if ($rest =~ /^$/) {
					err("unexpected }");
				} else {
					err("stuff after }");
				}
				$cont_in = 0;
				last;
			}
		} elsif (/\:/ && $cont_case && @cont_paren == 0) {
			err("stuff after multi-line case") if ($rest !~ /$^/);
			$cont_in = 0;
			last;
		}
		next;
section_ended:
		$cont_special = ($rest =~ /^\s*$special/)? 1 : 0;
		$cont_macro = ($rest =~ /^\s*$macro/)? 1 : 0;
		$cont_case = 0;
		next;
	}
	$cont_noerr = 0			if (!$cont_in);
}
