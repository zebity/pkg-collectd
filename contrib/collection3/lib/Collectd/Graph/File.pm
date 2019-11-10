package Collectd::Graph::File;

use strict;
use warnings;
use utf8;

use Carp (qw(cluck confess));
use File::Spec ();

my %valid_keys =
(
  host            => qr|^[^/]+$|,
  plugin          => qr|^[^/-]+$|,
  plugin_instance => qr|^[^/]*$|,
  type            => qr|^[^/-]+$|,
  type_instance   => qr|^[^/]*$|,
  abs_path        => \&setAbsolutePath
);

sub new
{
  my $pkg = shift;
  my $file = shift;
  my $obj;

  $obj = bless ({}, $pkg);
  $obj->setAbsolutePath ($file);

  return ($obj);
} # new

sub scanDirectory
{
  my $pkg = shift;
  my $top_dir = shift;
  my $filter = shift;
  my $ret = [];
  my $dh;

  if (!opendir ($dh, $top_dir))
  {
    cluck ("opendir ($top_dir) failed: $!");
    return;
  }

  while (my $entry = readdir ($dh))
  {
    if ($entry =~ m/^\./)
    {
      next;
    }

    if (-f "$top_dir/$entry")
    {
      my $file;

      $file = new ($pkg, "$top_dir/$entry");
      if (!$file)
      {
        next;
      }

      if ($filter && !$filter->checkFile ($file))
      {
        next;
      }

      push (@$ret, $file);
    }
    elsif (-e "$top_dir/$entry")
    {
      my $tmp;

      $tmp = scanDirectory ($pkg, "$top_dir/$entry", $filter);
      if ($tmp)
      {
        push (@$ret, @$tmp);
      }
    }
  } # while (readdir)

  closedir ($dh);

  return ($ret);
} # scanDirectory

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
  my $value = shift;

  if (!defined ($value))
  {
    cluck ("Invalid argument");
  }

  if (!exists ($valid_keys{$key}))
  {
    return;
  }
  elsif (!defined ($valid_keys{$key}))
  {
    $obj->{$key} = $value;
    return (1);
  }
  elsif (ref ($valid_keys{$key}) eq 'Regexp')
  {
    if ($value =~ $valid_keys{$key})
    {
      $obj->{$key} = $value;
      return (1);
    }
    return;
  }
  elsif (ref ($valid_keys{$key}) eq 'CODE')
  {
    return ($valid_keys{$key}->($obj, $value));
  }
  else
  {
    cluck ("Unknown key type");
    return;
  }
} # set

sub getAbsolutePath
{
  my $obj = shift;
  return ($obj->{'abs_path'});
}

sub setAbsolutePath
{
  my $obj = shift;
  my $abs_path = shift;
  my $volume;
  my $directories;
  my @directories;
  my $file;

  my $host;
  my $plugin;
  my $plugin_instance;
  my $type;
  my $type_instance;

  if (!-e $abs_path)
  {
    cluck ("No such file: $abs_path");
    return;
  }

  if (!File::Spec->file_name_is_absolute ($abs_path))
  {
    $abs_path = File::Spec->rel2abs ($abs_path);
  }

  ($volume, $directories, $file) = File::Spec->splitpath ($abs_path);
  @directories = File::Spec->splitdir ($directories);

  $file =~ s/\.rrd$//i;

  # Strip empty components from the end.
  while ((@directories > 0) && !$directories[-1])
  {
    pop (@directories);
  }

  if (@directories < 2)
  {
    cluck ("Absolute file name does not have enough directories");
    return;
  }

  ($type, $type_instance) = split (m/-/, $file, 2);
  $type_instance ||= '';

  ($plugin, $plugin_instance) = split (m/-/, $directories[-1], 2);
  $plugin_instance ||= '';

  $host = $directories[-2];

  $obj->set ('host', $host);
  $obj->set ('plugin', $plugin);
  $obj->set ('plugin_instance', $plugin_instance);
  $obj->set ('type', $type);
  $obj->set ('type_instance', $type_instance);

  $obj->{'abs_path'} = $abs_path;

  return (1);
} # setAbsolutePath

# vim: set sw=2 sts=2 et fdm=marker :
