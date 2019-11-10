package Collectd::Graph::Filter;

use strict;
use warnings;
use utf8;

use Carp (qw(cluck confess));

my %valid_keys =
(
  host            => undef,
  plugin          => undef,
  plugin_instance => undef,
  type            => undef,
  type_instance   => undef
);

sub get
{
  my $obj = shift;
  my $key = shift;

  if (exists ($obj->{$key}))
  {
    return ($obj->{$key});
  }

  cluck ("Invalid key: $key");
  return;
} # get

sub set
{
  my $obj = shift;
  my $key = shift;
  my @values = @_;

  if (!exists ($valid_keys{$key}))
  {
    cluck ("Invalid argument");
    return;
  }

  $obj->{$key} = \@values;

  return (1);
} # set

sub add
{
  my $obj = shift;
  my $key = shift;
  my @values = @_;

  if (!exists ($valid_keys{$key}))
  {
    cluck ("Invalid argument");
    return;
  }

  $obj->{$key} ||= [];
  push (@{$obj->{$key}}, @values);

  return (1);
} # add

sub clear
{
  my $obj = shift;
  my $key = shift;

  if (!exists ($valid_keys{$key}))
  {
    cluck ("Invalid argument");
    return;
  }

  $obj->{$key} = [];

  return (1);
} # clear

sub checkFile
{
  my $obj = shift;
  my $file = shift;

  for (keys %valid_keys)
  {
    my $key = $_;
    my $file_field;

    if (!$obj->{$key} || !@{$obj->{$key}})
    {
      next;
    }

    $file_field = $file->get ($field);

    if (grep { $_ eq $file_field } (@{$obj->{$key}}))
    {
      next;
    }

    # No match
    return;
  }

  return (1);
} # checkFile

sub grep
{
  my $obj = shift;
  my @files_in = @_;
  my @files_out;
  my $i;

  for ($i = 0; $i < @files_in, $i++)
  {
    if ($obj->checkFile ($files_in[$i]))
    {
      push (@files_out, $files_in[$i]);
    }
  }

  return (@files_out);
} # grep

# vim: set sw=2 sts=2 et fdm=marker :
