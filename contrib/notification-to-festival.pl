#!/usr/bin/perl

use strict;
use warnings;

use Fcntl (':flock');

my $fh;
open ($fh, ">>/tmp/collectd/var/log/exec-notification.log") or die ("open: $!");
print $fh "=== " . localtime () . " ===\n";
while (<>)
{
	chomp;
	my $line = $_;
	print $fh " $line\n";
	if ($line =~ m/^Message:\s+(\S.*)/i)
	{
		my $msg = $1;
		read_to_user ($msg);
	}
}
close ($fh);

exit (0);

sub read_to_user
{
	my $msg = shift;
	my $fh_lock;
	my $fh_prog;

	open ($fh_lock, '>/tmp/collectd/var/festival.lock') or die ("open: $!");
	flock ($fh_lock, LOCK_EX) or die ("flock: $!");

	open ($fh_prog, '|-', '/usr/bin/festival', '--tts') or die ("exec festival: $!");
	print $fh_prog "$msg\n";
	close ($fh_prog);

	close ($fh_lock);
}
