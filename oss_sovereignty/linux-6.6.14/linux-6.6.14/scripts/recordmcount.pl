use warnings;
use strict;
my $P = $0;
$P =~ s@.*/@@g;
my $V = '0.1';
if ($
	print "usage: $P arch endian bits objdump objcopy cc ld nm rm mv is_module inputfile\n";
	print "version: $V\n";
	exit(1);
}
my ($arch, $endian, $bits, $objdump, $objcopy, $cc,
    $ld, $nm, $rm, $mv, $is_module, $inputfile) = @ARGV;
if ($inputfile =~ m,kernel/trace/ftrace\.o$,) {
    exit(0);
}
my %text_sections = (
     ".text" => 1,
     ".init.text" => 1,
     ".ref.text" => 1,
     ".sched.text" => 1,
     ".spinlock.text" => 1,
     ".irqentry.text" => 1,
     ".softirqentry.text" => 1,
     ".kprobes.text" => 1,
     ".cpuidle.text" => 1,
     ".text.unlikely" => 1,
);
my %text_section_prefixes = (
     ".text." => 1,
);
$objdump = 'objdump' if (!$objdump);
$objcopy = 'objcopy' if (!$objcopy);
$cc = 'gcc' if (!$cc);
$ld = 'ld' if (!$ld);
$nm = 'nm' if (!$nm);
$rm = 'rm' if (!$rm);
$mv = 'mv' if (!$mv);
my %locals;		
my %weak;		
my %convert;		
my $type;
my $local_regex;	
my $weak_regex; 	
my $section_regex;	
my $function_regex;	
my $mcount_regex;	
my $mcount_adjust;	
my $alignment;		
my $section_type;	
if ($arch =~ /(x86(_64)?)|(i386)/) {
    if ($bits == 64) {
	$arch = "x86_64";
    } else {
	$arch = "i386";
    }
}
$local_regex = "^[0-9a-fA-F]+\\s+t\\s+(\\S+)";
$weak_regex = "^[0-9a-fA-F]+\\s+([wW])\\s+(\\S+)";
$section_regex = "Disassembly of section\\s+(\\S+):";
$function_regex = "^([0-9a-fA-F]+)\\s+<([^^]*?)>:";
$mcount_regex = "^\\s*([0-9a-fA-F]+):.*\\s(mcount|__fentry__)\$";
$section_type = '@progbits';
$mcount_adjust = 0;
$type = ".long";
if ($arch eq "x86_64") {
    $mcount_regex = "^\\s*([0-9a-fA-F]+):.*\\s(mcount|__fentry__)([+-]0x[0-9a-zA-Z]+)?\$";
    $type = ".quad";
    $alignment = 8;
    $mcount_adjust = -1;
    $ld .= " -m elf_x86_64";
    $objdump .= " -M x86-64";
    $objcopy .= " -O elf64-x86-64";
    $cc .= " -m64";
} elsif ($arch eq "i386") {
    $alignment = 4;
    $mcount_adjust = -1;
    $ld .= " -m elf_i386";
    $objdump .= " -M i386";
    $objcopy .= " -O elf32-i386";
    $cc .= " -m32";
} elsif ($arch eq "s390" && $bits == 64) {
    if ($cc =~ /-DCC_USING_HOTPATCH/) {
	$mcount_regex = "^\\s*([0-9a-fA-F]+):\\s*c0 04 00 00 00 00\\s*(brcl\\s*0,|jgnop\\s*)[0-9a-f]+ <([^\+]*)>\$";
	$mcount_adjust = 0;
    }
    $alignment = 8;
    $type = ".quad";
    $ld .= " -m elf64_s390";
    $cc .= " -m64";
} elsif ($arch eq "sh") {
    $alignment = 2;
    $ld .= " -m shlelf_linux";
    if ($endian eq "big") {
	$objcopy .= " -O elf32-shbig-linux";
    } else {
	$objcopy .= " -O elf32-sh-linux";
    }
} elsif ($arch eq "powerpc") {
    my $ldemulation;
    $local_regex = "^[0-9a-fA-F]+\\s+t\\s+(\\.?\\S+)";
    $function_regex = "^([0-9a-fA-F]+)\\s+<(\\.?\\w*?)>:";
    $mcount_regex = "^\\s*([0-9a-fA-F]+):.*\\s\\.?_mcount\$";
    if ($endian eq "big") {
	    $cc .= " -mbig-endian ";
	    $ld .= " -EB ";
	    $ldemulation = "ppc"
    } else {
	    $cc .= " -mlittle-endian ";
	    $ld .= " -EL ";
	    $ldemulation = "lppc"
    }
    if ($bits == 64) {
	$type = ".quad";
	$cc .= " -m64 ";
	$ld .= " -m elf64".$ldemulation." ";
    } else {
	$cc .= " -m32 ";
	$ld .= " -m elf32".$ldemulation." ";
    }
} elsif ($arch eq "arm") {
    $alignment = 2;
    $section_type = '%progbits';
    $mcount_regex = "^\\s*([0-9a-fA-F]+):\\s*R_ARM_(CALL|PC24|THM_CALL)" .
			"\\s+(__gnu_mcount_nc|mcount)\$";
} elsif ($arch eq "arm64") {
    $alignment = 3;
    $section_type = '%progbits';
    $mcount_regex = "^\\s*([0-9a-fA-F]+):\\s*R_AARCH64_CALL26\\s+_mcount\$";
    $type = ".quad";
} elsif ($arch eq "ia64") {
    $mcount_regex = "^\\s*([0-9a-fA-F]+):.*\\s_mcount\$";
    $type = "data8";
    if ($is_module eq "0") {
	$cc .= " -mconstant-gp";
    }
} elsif ($arch eq "sparc64") {
    $function_regex = "^([0-9a-fA-F]+)\\s+<(\\w*?)>:";
    $mcount_regex = "^\\s*([0-9a-fA-F]+):.*\\s_mcount\$";
    $alignment = 8;
    $type = ".xword";
    $ld .= " -m elf64_sparc";
    $cc .= " -m64";
    $objcopy .= " -O elf64-sparc";
} elsif ($arch eq "mips") {
    if ($is_module eq "0") {
	    $mcount_regex = "^\\s*([0-9a-fA-F]+): R_MIPS_26\\s+_mcount\$";
    } else {
	    $mcount_regex = "^\\s*([0-9a-fA-F]+): R_MIPS_HI16\\s+_mcount\$";
    }
    $objdump .= " -Melf-trad".$endian."mips ";
    if ($endian eq "big") {
	    $endian = " -EB ";
	    $ld .= " -melf".$bits."btsmip";
    } else {
	    $endian = " -EL ";
	    $ld .= " -melf".$bits."ltsmip";
    }
    $cc .= " -mno-abicalls -fno-pic -mabi=" . $bits . $endian;
    $ld .= $endian;
    if ($bits == 64) {
	    $function_regex =
		"^([0-9a-fA-F]+)\\s+<(.|[^\$]L.*?|\$[^L].*?|[^\$][^L].*?)>:";
	    $type = ".dword";
    }
} elsif ($arch eq "microblaze") {
    $mcount_regex = "^\\s*([0-9a-fA-F]+):.*\\s_mcount\$";
} elsif ($arch eq "riscv") {
    $function_regex = "^([0-9a-fA-F]+)\\s+<([^.0-9][0-9a-zA-Z_\\.]+)>:";
    $mcount_regex = "^\\s*([0-9a-fA-F]+):\\sR_RISCV_CALL(_PLT)?\\s_?mcount\$";
    $type = ".quad";
    $alignment = 2;
} elsif ($arch eq "csky") {
    $mcount_regex = "^\\s*([0-9a-fA-F]+):\\s*R_CKCORE_PCREL_JSR_IMM26BY2\\s+_mcount\$";
    $alignment = 2;
} else {
    die "Arch $arch is not supported with CONFIG_FTRACE_MCOUNT_RECORD";
}
my $text_found = 0;
my $read_function = 0;
my $opened = 0;
my $mcount_section = "__mcount_loc";
my $dirname;
my $filename;
my $prefix;
my $ext;
if ($inputfile =~ m,^(.*)/([^/]*)$,) {
    $dirname = $1;
    $filename = $2;
} else {
    $dirname = ".";
    $filename = $inputfile;
}
if ($filename =~ m,^(.*)(\.\S),) {
    $prefix = $1;
    $ext = $2;
} else {
    $prefix = $filename;
    $ext = "";
}
my $mcount_s = $dirname . "/.tmp_mc_" . $prefix . ".s";
my $mcount_o = $dirname . "/.tmp_mc_" . $prefix . ".o";
open (IN, "$nm $inputfile|") || die "error running $nm";
while (<IN>) {
    if (/$local_regex/) {
	$locals{$1} = 1;
    } elsif (/$weak_regex/) {
	$weak{$2} = $1;
    }
}
close(IN);
my @offsets;		
my $ref_func;		
my $offset = 0;		
sub update_funcs
{
    return unless ($ref_func and @offsets);
    if (defined $weak{$ref_func}) {
	die "$inputfile: ERROR: referencing weak function" .
	    " $ref_func for mcount\n";
    }
    if (defined $locals{$ref_func}) {
	$convert{$ref_func} = 1;
    }
    if (!$opened) {
	open(FILE, ">$mcount_s") || die "can't create $mcount_s\n";
	$opened = 1;
	print FILE "\t.section $mcount_section,\"a\",$section_type\n";
	print FILE "\t.align $alignment\n" if (defined($alignment));
    }
    foreach my $cur_offset (@offsets) {
	printf FILE "\t%s %s + %d\n", $type, $ref_func, $cur_offset - $offset;
    }
}
open(IN, "LC_ALL=C $objdump -hdr $inputfile|") || die "error running $objdump";
my $text;
my $read_headers = 1;
while (<IN>) {
    if ($read_headers && /$mcount_section/) {
	print STDERR "ERROR: $mcount_section already in $inputfile\n" .
	    "\tThis may be an indication that your build is corrupted.\n" .
	    "\tDelete $inputfile and try again. If the same object file\n" .
	    "\tstill causes an issue, then disable CONFIG_DYNAMIC_FTRACE.\n";
	exit(-1);
    }
    if (/$section_regex/) {
	$read_headers = 0;
	$read_function = defined($text_sections{$1});
	if (!$read_function) {
	    foreach my $prefix (keys %text_section_prefixes) {
		if (substr($1, 0, length $prefix) eq $prefix) {
		    $read_function = 1;
		    last;
		}
	    }
	}
	update_funcs();
	$text_found = 0;
	undef($ref_func);
	undef(@offsets);
    } elsif ($read_function && /$function_regex/) {
	$text_found = 1;
	$text = $2;
	if (!defined($locals{$text}) && !defined($weak{$text})) {
	    $ref_func = $text;
	    $read_function = 0;
	    $offset = hex $1;
	} else {
	    if (!defined($ref_func) && !defined($weak{$text}) &&
		 $text !~ /^\.L/) {
		$ref_func = $text;
		$offset = hex $1;
	    }
	}
    }
    if ($text_found && /$mcount_regex/) {
	push(@offsets, (hex $1) + $mcount_adjust);
    }
}
update_funcs();
if (!$opened) {
    exit(0);
}
close(FILE);
`$cc -o $mcount_o -c $mcount_s`;
my @converts = keys %convert;
if ($
    my $globallist = "";
    my $locallist = "";
    foreach my $con (@converts) {
	$globallist .= " --globalize-symbol $con";
	$locallist .= " --localize-symbol $con";
    }
    my $globalobj = $dirname . "/.tmp_gl_" . $filename;
    my $globalmix = $dirname . "/.tmp_mx_" . $filename;
    `$objcopy $globallist $inputfile $globalobj`;
    `$ld -r $globalobj $mcount_o -o $globalmix`;
    `$objcopy $locallist $globalmix $inputfile`;
    `$rm $globalobj $globalmix`;
} else {
    my $mix = $dirname . "/.tmp_mx_" . $filename;
    `$ld -r $inputfile $mcount_o -o $mix`;
    `$mv $mix $inputfile`;
}
`$rm $mcount_o $mcount_s`;
exit(0);
