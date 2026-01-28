use 5.010;
use warnings;
use strict;
my %display_name;
my %display_email;
my %authors_name;
my %authors_email;
my @authors_header;
for my $line (do { local (@ARGV) = ('AUTHORS'); <> }) {
	chomp $line;
	state $in_header = 1;
	if ($in_header) {
		push @authors_header, $line;
		$in_header = 0 if $line =~ m/^CONTRIBUTORS:/;
	} else {
		my ($name, $email) = $line =~ m/^\s+(.+)(?= <) <([^>]+)/;
		next unless $name;
		my $semail = email_slug($email);
		my $sname = name_slug($name);
		$authors_name{$semail} = $sname;
		$authors_email{$sname} = $semail;
		$display_name{$sname} = $name;
		$display_email{$semail} = $email;
	}
}
my %git_names;
my %git_emails;
for my $line (reverse qx(git log --pretty=tformat:'%aN:::%aE')) {
	chomp $line;
	my ($name, $email) = $line =~ m/^(.*):::(.*)/;
	next unless $name && $email;
	my $semail = email_slug($email);
	my $sname = name_slug($name);
	$git_names{$semail}{$sname} = 1;
	$git_emails{$sname}{$semail} = 1;
	if (!$authors_name{email_slug($email)}) {
		update_display_email($email);
	}
	if (!$authors_email{name_slug($name)}) {
		update_display_name($name);
	}
}
my @committers;
for my $start_email (sort keys %git_names) {
	next unless $git_names{$start_email};
	my %emails;
	my %names;
	my @check_emails = ($start_email);
	my @check_names;
	while (@check_emails || @check_names) {
		while (my $email = shift @check_emails) {
			next if $emails{$email}++;
			push @check_names,
			    sort keys %{delete $git_names{$email}};
		}
		while (my $name = shift @check_names) {
			next if $names{$name}++;
			push @check_emails,
			    sort keys %{delete $git_emails{$name}};
		}
	}
	push @committers, [[sort keys %emails], [sort keys %names]];
}
for my $committer (@committers) {
	my ($emails, $names) = @$committer;
	next if grep { $authors_name{$_} } @$emails;
	next if grep { $authors_email{$_} } @$names;
	my $email = best_email(@$emails);
	my $name = best_name(@$names);
	$authors_email{$name} = $email;
	$authors_name{$email} = $name;
}
open my $fh, '>', 'AUTHORS' or die "E: couldn't open AUTHORS for write: $!\n";
say $fh join("\n", @authors_header, "");
for my $name (sort keys %authors_email) {
	my $cname = $display_name{$name};
	my $cemail = $display_email{email_slug($authors_email{$name})};
	say $fh "    $cname <$cemail>";
}
exit 0;
sub name_slug {
	my ($name) = @_;
	$name =~ s/[\s\.]//g;
	return lc $name;
}
sub email_slug {
	my ($email) = @_;
	$email =~ s/^(.*\s+)|(\s+.*)$//g;
	$email =~ s/^[^\+]*\+//g if $email =~ m/\.noreply\.github\.com$/;
	return lc $email;
}
sub update_display_name {
	my ($name) = @_;
	my $sname = name_slug($name);
	my $cname = $display_name{$sname};
	if (!$cname ||
	    ($name =~ tr/a-z //) < ($cname =~ tr/a-z //)) {
		$display_name{$sname} = $name;
	}
}
sub update_display_email {
	my ($email) = @_;
	my $semail = email_slug($email);
	$email =~ s/^[^\+]*\+//g if $email =~ m/\.noreply\.github\.com$/;
	my $cemail = $display_email{$semail};
	if (!$cemail ||
	    ($email =~ tr/a-z //) < ($cemail =~ tr/a-z //)) {
		$display_email{$semail} = $email;
	}
}
sub best_name {
	my @names = sort {
		my $cmp;
		my ($aa) = $display_name{$a};
		my ($bb) = $display_name{$b};
		return ($aa cmp $bb);
	} @_;
	return shift @names;
}
sub best_email {
	state $internal_re = qr/\.(?:internal|local|\(none\))$/;
	state $noreply_re  = qr/\.noreply\.github\.com$/;
	state $freemail_re = qr/\@(?:gmail|hotmail)\.com$/;
	my @emails = sort {
		my $cmp;
		$cmp = (($b =~ tr/@//) == 1) <=> (($a =~ tr/@//) == 1);
		return $cmp unless $cmp == 0;
		$cmp = (($a =~ $internal_re) <=> ($b =~ $internal_re));
		return $cmp unless $cmp == 0;
		$cmp = (($a =~ $noreply_re) <=> ($b =~ $noreply_re));
		return $cmp unless $cmp == 0;
		$cmp = (($a =~ $freemail_re) <=> ($b =~ $freemail_re));
		return $cmp unless $cmp == 0;
		my ($alocal, $adom) = split /\@/, $a;
		my ($blocal, $bdom) = split /\@/, $b;
		$cmp = ($adom cmp $bdom);
		return $cmp unless $cmp == 0;
		return ($alocal cmp $blocal);
	} @_;
	return shift @emails;
}
