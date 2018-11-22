#!/usr/bin/perl

use strict;
use warnings;

sub format_link
{
	my $page = shift;
	my $text = @_ ? shift : $page;

	if ($page =~ m/^User:([A-Z][a-z]*)/) {
		return "<em>$text</em>";
	}

	return qq#<a href="/wiki/index.php/$page">$text</a>#;
}

while (<>)
{
	chomp;
	my $line = $_;

	if ($line =~ m/^\* (.+)/) {
		$line = $1;
	} else {
		next;
	}

	$line =~ s#{{Plugin\|([^}]+)}}#<a href="/wiki/index.php/Plugin:$1">$1 plugin</a>#g;
	$line =~ s@\{\{Issue\|([^}]+)\}\}@<a href="/bugs/$1">#$1</a>@g;
	$line =~ s#\[\[([^|]+)\|([^\]]+)\]\]#format_link($1, $2)#ge;
	$line =~ s#\[\[([^|]+)\]\]#format_link($1)#ge;

	$line =~ s#'''(.*?)'''#<strong>$1</strong>#g;
	$line =~ s#''(.*?)''#<em>$1</em>#g;

	print "$line<br />\n";
}
