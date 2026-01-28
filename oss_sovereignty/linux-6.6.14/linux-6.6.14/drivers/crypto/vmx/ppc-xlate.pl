my $flavour = shift;
my $output = shift;
open STDOUT,">$output" || die "can't open $output: $!";
my %GLOBALS;
my $dotinlocallabels=($flavour=~/linux/)?1:0;
my $elfv2abi=(($flavour =~ /linux-ppc64le/) or ($flavour =~ /linux-ppc64-elfv2/))?1:0;
my $dotfunctions=($elfv2abi=~1)?0:1;
my $globl = sub {
    my $junk = shift;
    my $name = shift;
    my $global = \$GLOBALS{$name};
    my $ret;
    $name =~ s|^[\.\_]||;
    SWITCH: for ($flavour) {
	/aix/		&& do { $name = ".$name";
				last;
			      };
	/osx/		&& do { $name = "_$name";
				last;
			      };
	/linux/
			&& do {	$ret = "_GLOBAL($name)";
				last;
			      };
    }
    $ret = ".globl	$name\nalign 5\n$name:" if (!$ret);
    $$global = $name;
    $ret;
};
my $text = sub {
    my $ret = ($flavour =~ /aix/) ? ".csect\t.text[PR],7" : ".text";
    $ret = ".abiversion	2\n".$ret	if ($elfv2abi);
    $ret;
};
my $machine = sub {
    my $junk = shift;
    my $arch = shift;
    if ($flavour =~ /osx/)
    {	$arch =~ s/\"//g;
	$arch = ($flavour=~/64/) ? "ppc970-64" : "ppc970" if ($arch eq "any");
    }
    ".machine	$arch";
};
my $size = sub {
    if ($flavour =~ /linux/)
    {	shift;
	my $name = shift; $name =~ s|^[\.\_]||;
	my $ret  = ".size	$name,.-".($dotfunctions?".":"").$name;
	$ret .= "\n.size	.$name,.-.$name" if ($dotfunctions);
	$ret;
    }
    else
    {	"";	}
};
my $asciz = sub {
    shift;
    my $line = join(",",@_);
    if ($line =~ /^"(.*)"$/)
    {	".byte	" . join(",",unpack("C*",$1),0) . "\n.align	2";	}
    else
    {	"";	}
};
my $quad = sub {
    shift;
    my @ret;
    my ($hi,$lo);
    for (@_) {
	if (/^0x([0-9a-f]*?)([0-9a-f]{1,8})$/io)
	{  $hi=$1?"0x$1":"0"; $lo="0x$2";  }
	elsif (/^([0-9]+)$/o)
	{  $hi=$1>>32; $lo=$1&0xffffffff;  } 
	else
	{  $hi=undef; $lo=$_; }
	if (defined($hi))
	{  push(@ret,$flavour=~/le$/o?".long\t$lo,$hi":".long\t$hi,$lo");  }
	else
	{  push(@ret,".quad	$lo");  }
    }
    join("\n",@ret);
};
my $cmplw = sub {
    my $f = shift;
    my $cr = 0; $cr = shift if ($
    ($flavour =~ /linux.*32/) ?
	"	.long	".sprintf "0x%x",31<<26|$cr<<23|$_[0]<<16|$_[1]<<11|64 :
	"	cmplw	".join(',',$cr,@_);
};
my $bdnz = sub {
    my $f = shift;
    my $bo = $f=~/[\+\-]/ ? 16+9 : 16;	
    "	bc	$bo,0,".shift;
} if ($flavour!~/linux/);
my $bltlr = sub {
    my $f = shift;
    my $bo = $f=~/\-/ ? 12+2 : 12;	
    ($flavour =~ /linux/) ?		
	"	.long	".sprintf "0x%x",19<<26|$bo<<21|16<<1 :
	"	bclr	$bo,0";
};
my $bnelr = sub {
    my $f = shift;
    my $bo = $f=~/\-/ ? 4+2 : 4;	
    ($flavour =~ /linux/) ?		
	"	.long	".sprintf "0x%x",19<<26|$bo<<21|2<<16|16<<1 :
	"	bclr	$bo,2";
};
my $beqlr = sub {
    my $f = shift;
    my $bo = $f=~/-/ ? 12+2 : 12;	
    ($flavour =~ /linux/) ?		
	"	.long	".sprintf "0x%X",19<<26|$bo<<21|2<<16|16<<1 :
	"	bclr	$bo,2";
};
my $extrdi = sub {
    my ($f,$ra,$rs,$n,$b) = @_;
    $b = ($b+$n)&63; $n = 64-$n;
    "	rldicl	$ra,$rs,$b,$n";
};
my $vmr = sub {
    my ($f,$vx,$vy) = @_;
    "	vor	$vx,$vy,$vy";
};
my $no_vrsave = ($elfv2abi);
my $mtspr = sub {
    my ($f,$idx,$ra) = @_;
    if ($idx == 256 && $no_vrsave) {
	"	or	$ra,$ra,$ra";
    } else {
	"	mtspr	$idx,$ra";
    }
};
my $mfspr = sub {
    my ($f,$rd,$idx) = @_;
    if ($idx == 256 && $no_vrsave) {
	"	li	$rd,-1";
    } else {
	"	mfspr	$rd,$idx";
    }
};
sub vsxmem_op {
    my ($f, $vrt, $ra, $rb, $op) = @_;
    "	.long	".sprintf "0x%X",(31<<26)|($vrt<<21)|($ra<<16)|($rb<<11)|($op*2+1);
}
my $lvx_u	= sub {	vsxmem_op(@_, 844); };	
my $stvx_u	= sub {	vsxmem_op(@_, 972); };	
my $lvdx_u	= sub {	vsxmem_op(@_, 588); };	
my $stvdx_u	= sub {	vsxmem_op(@_, 716); };	
my $lvx_4w	= sub { vsxmem_op(@_, 780); };	
my $stvx_4w	= sub { vsxmem_op(@_, 908); };	
sub vcrypto_op {
    my ($f, $vrt, $vra, $vrb, $op) = @_;
    "	.long	".sprintf "0x%X",(4<<26)|($vrt<<21)|($vra<<16)|($vrb<<11)|$op;
}
my $vcipher	= sub { vcrypto_op(@_, 1288); };
my $vcipherlast	= sub { vcrypto_op(@_, 1289); };
my $vncipher	= sub { vcrypto_op(@_, 1352); };
my $vncipherlast= sub { vcrypto_op(@_, 1353); };
my $vsbox	= sub { vcrypto_op(@_, 0, 1480); };
my $vshasigmad	= sub { my ($st,$six)=splice(@_,-2); vcrypto_op(@_, $st<<4|$six, 1730); };
my $vshasigmaw	= sub { my ($st,$six)=splice(@_,-2); vcrypto_op(@_, $st<<4|$six, 1666); };
my $vpmsumb	= sub { vcrypto_op(@_, 1032); };
my $vpmsumd	= sub { vcrypto_op(@_, 1224); };
my $vpmsubh	= sub { vcrypto_op(@_, 1096); };
my $vpmsumw	= sub { vcrypto_op(@_, 1160); };
my $vaddudm	= sub { vcrypto_op(@_, 192);  };
my $vadduqm	= sub { vcrypto_op(@_, 256);  };
my $mtsle	= sub {
    my ($f, $arg) = @_;
    "	.long	".sprintf "0x%X",(31<<26)|($arg<<21)|(147*2);
};
print "#include <asm/ppc_asm.h>\n" if $flavour =~ /linux/;
while($line=<>) {
    $line =~ s|[
    $line =~ s|/\*.*\*/||;	
    $line =~ s|^\s+||;		
    $line =~ s|\s+$||;		
    {
	$line =~ s|\b\.L(\w+)|L$1|g;	
	$line =~ s|\bL(\w+)|\.L$1|g	if ($dotinlocallabels);
    }
    {
	$line =~ s|^\s*(\.?)(\w+)([\.\+\-]?)\s*||;
	my $c = $1; $c = "\t" if ($c eq "");
	my $mnemonic = $2;
	my $f = $3;
	my $opcode = eval("\$$mnemonic");
	$line =~ s/\b(c?[rf]|v|vs)([0-9]+)\b/$2/g if ($c ne "." and $flavour !~ /osx/);
	if (ref($opcode) eq 'CODE') { $line = &$opcode($f,split(',',$line)); }
	elsif ($mnemonic)           { $line = $c.$mnemonic.$f."\t".$line; }
    }
    print $line if ($line);
    print "\n";
}
close STDOUT;
