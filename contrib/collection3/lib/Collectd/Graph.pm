package Collectd::Graph;

use strict;
use warnings;
use utf8;

use Carp (qw(cluck confess));

use Collectd::Graph::Data ();
use Collectd::Graph::MetaData ();

my %valid_keys =
(
  name           => undef,
  title          => undef,
  number_format  => undef,
  vertical_title => undef,

  _graph_data    => \&addGraphData
);

sub get
{
  my $obj = shift;
  my $key = shift;

  if ($key =~ m/^_/)
  {
    cluck ("Invalid key: $key");
    return;
  }

  if (exists ($obj->{$key}))
  {
    return ($obj->{$key});
  }

  cluck ("Invalid key: $key");
  return;
} # get

sub getNumberFormat
{
  my $obj = shift;
  my $format;

  $format = $obj->{'number_format'} || '%lg';
  return ($format);
}

sub set
{
  my $obj = shift;
  my $key = shift;
  my $value = shift;

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
  else
  {
    cluck ("Unknown key type");
    return;
  }
} # set

sub addGraphData
{
  my $obj = shift;
  my $gd = shift;

  if (!$gd || (ref ($gd) ne 'Collectd::Graph::Data'))
  {
    cluck ("Invalid argument");
    return;
  }

  $obj->{'_graph_data'} ||= [];
  push (@{$obj->{'_graph_data'}}, $gd);

  return (1);
}

sub new
{
  my $pkg = shift;
  my %args = @_;
  my $obj = bless {}, $pkg;

  for (keys %args)
  {
    if (!exists ($valid_keys{$_}))
    {
      cluck ("Ignoring invalid field $_");
      next;
    }
    if (exists $args{$_})
    {
      $obj->{$_} = $args{$_};
      delete $args{$_};
    }
  }

  return ($obj);
} # new

sub parseConfig
{
  my $pkg = shift;
  my $config = shift;

  my $obj = new ($pkg);

  my %config_map =
  (
    name          => 'name',
    title         => 'title',
    verticaltitle => 'vertical_title',
    numberformat  => 'number_format'

  );

  if (ref ($config) ne 'HASH')
  {
    cluck ("Invalid argument type " . ref ($config));
    return;
  }

  for (keys %$config)
  {
    my $key = $_;

    if (exists ($config_map{$key}))
    {
      $obj->set ($config_map{$key}, $config->{$key});
    }
    elsif ($key eq 'data')
    {
      my $tmp;
      if (ref ($config->{$key}) eq 'ARRAY')
      {
        $tmp = $config->{$key};
      }
      else
      {
        $tmp = [ $config->{$key} ];
      }

      for (@$tmp)
      {
        my $gd = Collectd::Graph::Data->parseConfig ($_);
        if ($gd)
        {
          $obj->addGraphData ($gd);
        }
      }
    }
    else
    {
      cluck ("Ignoring invalid configuration option $key");
    }
  }

  return ($obj);
} # parseConfig

sub checkAvailable
{
  my $obj = shift;
  my $files = shift;

  for (@{$obj->{'_graph_data'}})
  {
    my $gd = $_;

    if (!$gd->checkAvailable ($files))
    {
      return;
    }
  }

  return (1);
} # checkAvailable

sub getRRDArgs
{
  my $obj = shift;
  my $files = shift;

  my @args = ();
  my $meta = Collectd::Graph::MetaData->new ($obj);

  if ($obj->{'title'})
  {
    push (@args, '-t', $obj->{'title'});
  }

  if ($obj->{'vertical_title'})
  {
    push (@args, '-v', $obj->{'vertical_title'});
  }

  for (@{$obj->{'_graph_data'}})
  {
    my $gd = $_;
    my @tmp;

    @tmp = $gd->getRRDArgs ($meta, $files);
    if (@tmp)
    {
      push (@args, @tmp);
    }
  }

  return (@args);
} # getRRDArgs

# vim: set sw=2 sts=2 et fdm=marker :
