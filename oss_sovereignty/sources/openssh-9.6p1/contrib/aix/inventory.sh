find . ! -name . -print | perl -ne '{
	chomp;
	if ( -l $_ ) {
		($dev,$ino,$mod,$nl,$uid,$gid,$rdev,$sz,$at,$mt,$ct,$bsz,$blk)=lstat;
	} else {
		($dev,$ino,$mod,$nl,$uid,$gid,$rdev,$sz,$at,$mt,$ct,$bsz,$blk)=stat;
	}
	$name = $_;
	$name =~ s|^.||;	
	print "$name:\n";
	print "\tclass=apply,inventory,openssh\n";
	print "\towner=root\n";
	print "\tgroup=system\n";
	printf "\tmode=%lo\n", $mod & 07777;	
	if ( -l $_ ) {
		print "\ttype=SYMLINK\n";
		printf "\ttarget=%s\n", readlink($_);
	} elsif ( -f $_ ) {
		print "\ttype=FILE\n";
		print "\tsize=$sz\n";
		print "\tchecksum=VOLATILE\n";
	} elsif ( -d $_ ) {
		print "\ttype=DIRECTORY\n";
	}
}'
