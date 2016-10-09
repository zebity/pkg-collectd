#!/usr/bin/perl

use strict;
use warnings;

=head1 NAME

wiki2changelog.pl

=head1 DESCRIPTION

This script takes the change log from one of the "Version x.y" pages in
collectd's wiki and converts it to the format used by the "ChangeLog" file.
This is usually done as part of the release process.

=cut

our %GithubUsers = (
	'Octo'      => '@octo',
	'Mfournier' => '@mfournier',
	'Tokkee'    => '@tokkee',
);

sub format_link
{
	my $page = shift;
	my $text = @_ ? shift : $page;

	if ($page =~ m/^User:([A-Z][a-z]*)/) {
		if (exists ($GithubUsers{$1})) {
			return $GithubUsers{$1};
		}
	}

	return "[$text](https://collectd.org/wiki/index.php/$page)";
}

while (<>)
{
	chomp;
	my $line = $_;

	if ($line =~ m#^\* (.*)#) {
		$line = $1;
	} else {
		next;
	}

	$line =~ s#\{\{Plugin\|([^}]+)\}\}#[$1 plugin](https://collectd.org/wiki/index.php/Plugin:$1)#g;
	$line =~ s@\{\{Issue\|([^}]+)\}\}@#$1@g;
	$line =~ s#\[\[([^|]+)\|([^\]]+)\]\]#format_link($1, $2)#ge;
	$line =~ s#\[\[([^|]+)\]\]#format_link($1)#ge;

	$line =~ s#('')?Ruben Kerkhof('')?#\@rubenk#g;

	$line =~ s#'''(.*?)'''#**$1**#g;
	$line =~ s#''(.*?)''#*$1*#g;
	$line =~ s#<code>(.*?)</code>#`$1`#g;

	print "$line\n";
}
