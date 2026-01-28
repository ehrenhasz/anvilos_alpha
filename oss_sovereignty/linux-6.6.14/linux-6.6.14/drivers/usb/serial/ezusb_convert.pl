my $basename = $ARGV[0];
die "no base name specified" unless $basename;
while (<STDIN>) {
    my($lenstring, $addrstring, $typestring, $reststring, $doscrap) =
      /^:(\w\w)(\w\w\w\w)(\w\w)(\w+)(\r?)$/;
    die "malformed line: $_" unless $reststring;
    last if $typestring eq '01';
    my($len) = hex($lenstring);
    my($addr) = hex($addrstring);
    my(@bytes) = unpack("C*", pack("H".(2*$len), $reststring));
    push(@records, [$addr, \@bytes]);
}
@sorted_records = sort { $a->[0] <=> $b->[0] } @records;
print <<"EOF";
/*
 * ${basename}_fw.h
 *
 * Generated from ${basename}.s by ezusb_convert.pl
 * This file is presumed to be under the same copyright as the source file
 * from which it was derived.
 */
EOF
print "static const struct ezusb_hex_record ${basename}_firmware[] = {\n";
foreach $r (@sorted_records) {
    printf("{ 0x%04x,\t%d,\t{", $r->[0], scalar(@{$r->[1]}));
    print join(", ", map {sprintf('0x%02x', $_);} @{$r->[1]});
    print "} },\n";
}
print "{ 0xffff,\t0,\t{0x00} }\n";
print "};\n";
