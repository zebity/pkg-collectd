package Collectd::Graph::Data;

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
  type_instance   => undef,
  data_source     => undef,
  style           => qr/^(CombinedArea|StackedArea|SeparateLine|CombinedLine|StackedLine)$/i,
  stack           => qr/^(true|false)$/i,
  legend          => undef,
  color           => qr/^[0-9A-Fa-f]{6}$/
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
    else
    {
      cluck ("Invalid value for field $key: $value");
      return;
    }
  }
  else
  {
    cluck ("Unknown key type");
    return;
  }
} # set

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
    host           => 'host',
    plugin         => 'plugin',
    plugininstance => 'plugin_instance',
    type           => 'type',
    typeinstance   => 'type_instance',
    datasource     => 'data_source',
    style          => 'style',
    stack          => 'stack',
    legend         => 'legend',
    color          => 'color'
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
    else
    {
      cluck ("Ignoring invalid configuration option $key");
    }
  }

  return ($obj);
} # parseConfig

sub _check_field
{
  my $obj = shift;
  my $field = shift;
  my $file_obj = shift;

  my $file_field = $file_obj->get ($field);

  if (defined ($obj->{$field}) && ($obj->{$field} eq '*'))
  {
    # Group files by this key
    if (!$file_field)
    {
      # Unsure about this. This shouldn't happen, really, so I'll return false
      # for now.
      return;
    }
    else
    {
      return (1);
    }
  }
  elsif (defined ($obj->{$field}))
  {
    # All files must have `field' set to this value to apply.
    if ($file_field eq $obj->{$field})
    {
      return (1);
    }
    else
    {
      return;
    }
  }
  else # if (!defined ($obj->{$field}))
  {
    # We don't care.
    return (1);
  }
} # _check_field

sub checkFile
{
  my $obj = shift;
  my $file = shift;

  for (qw(host plugin plugin_instance type type_instance))
  {
    my $field = $_;

    if (!$obj->_check_field ($field, $file))
    {
      return;
    }
  }

  return (1);
} # checkFile

sub checkAvailable
{
  my $obj = shift;
  my $files = shift;
  my $files_match_num;

  $files_match_num = 0;
  for (@$files)
  {
    my $file = $_;
    my $file_matches = 1;

    if ($obj->checkFile ($file))
    {
      $files_match_num++;
    }
  }

  if (!$files_match_num)
  {
    return;
  }
  return ($files_match_num);
}

sub getRRDArgs
{
  my $obj = shift;
  my $meta = shift;
  my $files = shift;
  my $graph;
  my @ret = ();

  my $base_id;
  my $current_top_id;

  if (!$files)
  {
    cluck ("Invalid argument: No files");
    return;
  }

  $graph = $meta->getGraph ();

  if (defined ($obj->{'stack'}) && ($obj->{'stack'} =~ m/^true$/i))
  {
    $base_id = $meta->getStackTop ();
  }

  for (@$files)
  {
    my $file = $_;
    my $def;
    my $file_id;
    my $abs_path;

    if (!$obj->checkFile ($file))
    {
      next;
    }

    $file_id = $meta->getID ();
    $abs_path = $file->getAbsolutePath ();

    push (@ret, "DEF:${file_id}=${abs_path}:" . $obj->{'data_source'} . ':AVERAGE');
    push (@ret, "DEF:${file_id}_min=${abs_path}:" . $obj->{'data_source'} . ':MIN');
    push (@ret, "DEF:${file_id}_avg=${abs_path}:" . $obj->{'data_source'} . ':AVERAGE');
    push (@ret, "DEF:${file_id}_max=${abs_path}:" . $obj->{'data_source'} . ':MAX');

    # CombinedArea, CombinedLine: Add up all values, then draw one area or line.
    if (($obj->{'style'} eq 'CombinedArea')
      || ($obj->{'style'} eq 'CombinedLine'))
    {
      my $calc_id;
      my $graph_base;

      $calc_id = $meta->getID ();

      if ($current_top_id)
      {
        $graph_base = $current_top_id;
      }
      elsif ($base_id)
      {
        $graph_base = $base_id;
      }

      # Create the CDEF for the graph.
      if ($graph_base)
      {
        push (@ret, "CDEF:${calc_id}=${file_id},UN,0,${file_id},IF,${graph_base},+");
      }
      else
      {
        push (@ret, "CDEF:${calc_id}=${file_id},UN,0,${file_id},IF");
      }

      # Additional CDEFs for the legend.
      if ($current_top_id)
      {
        push (@ret, "CDEF:${calc_id}_min=${file_id}_min,UN,0,${file_id}_min,IF,"
          . "${current_top_id}_min,+");
        push (@ret, "CDEF:${calc_id}_avg=${file_id}_avg,UN,0,${file_id}_avg,IF,"
          . "${current_top_id}_avg,+");
        push (@ret, "CDEF:${calc_id}_max=${file_id}_max,UN,0,${file_id}_max,IF,"
          . "${current_top_id}_max,+");
      }
      else
      {
        push (@ret, "CDEF:${calc_id}_min=${file_id}_min,UN,0,${file_id}_min,IF");
        push (@ret, "CDEF:${calc_id}_avg=${file_id}_avg,UN,0,${file_id}_avg,IF");
        push (@ret, "CDEF:${calc_id}_max=${file_id}_max,UN,0,${file_id}_max,IF");
      }

      $current_top_id = $calc_id;
    }

    # StackedArea, StackedLine: Draw each line or area on top of each other.
    elsif (($obj->{'style'} eq 'StackedArea')
      || ($obj->{'style'} eq 'StackedLine'))
    {
      my $calc_id;
      my $graph_base;
      
      $calc_id = $meta->getID ();

      if ($current_top_id)
      {
        $graph_base = $current_top_id;
      }
      elsif ($base_id)
      {
        $graph_base = $base_id;
      }

      # Create the CDEF for the graph.
      if ($graph_base)
      {
        push (@ret, "CDEF:${calc_id}=${file_id},UN,0,${file_id},IF,${graph_base},+");
      }
      else
      {
        push (@ret, "CDEF:${calc_id}=${file_id},UN,0,${file_id},IF");
      }

      # Create the VDEFs for the legend.
      push (@ret, "VDEF:${calc_id}_min=${file_id}_min,MINIMUM");
      push (@ret, "VDEF:${calc_id}_avg=${file_id}_avg,AVERAGE");
      push (@ret, "VDEF:${calc_id}_max=${file_id}_max,MAXIMUM");
      push (@ret, "VDEF:${calc_id}_lst=${file_id}_avg,LAST");

      if ($obj->{'style'} eq 'StackedArea')
      {
        push (@ret, "AREA:${calc_id}#000000:legend");
      }
      else
      {
        push (@ret, "LINE1:${calc_id}#000000:legend");
      }
      # XXX format
      push (@ret, "GPRINT:${calc_id}_min:%lg min,");
      push (@ret, "GPRINT:${calc_id}_avg:%lg avg,");
      push (@ret, "GPRINT:${calc_id}_max:%lg max,");
      push (@ret, "GPRINT:${calc_id}_lst:%lg last\\j");
    }

    # SeparateLine: Draw each line separately.
    elsif ($obj->{'style'} eq 'SeparateLine')
    {
      my $calc_id;
      
      my $color = $obj->{'color'} || '000000';
      my $legend = $obj->{'legend'} || 'no name';
      my $format = $graph->getNumberFormat ();

      $calc_id = $meta->getID ();
      if ($base_id)
      {
        push (@ret, "CDEF:${calc_id}=${base_id},${file_id},+");
      }
      else
      {
        push (@ret, "CDEF:${calc_id}=0,${file_id},+");
      }

      # Create the VDEFs for the legend.
      push (@ret, "VDEF:${calc_id}_min=${file_id}_min,MINIMUM");
      push (@ret, "VDEF:${calc_id}_avg=${file_id}_avg,AVERAGE");
      push (@ret, "VDEF:${calc_id}_max=${file_id}_max,MAXIMUM");
      push (@ret, "VDEF:${calc_id}_lst=${file_id}_avg,LAST");

      push (@ret, "LINE1:${calc_id}#${color}:${legend}");

      # XXX format
      push (@ret, "GPRINT:${calc_id}_min:${format} min,");
      push (@ret, "GPRINT:${calc_id}_avg:${format} avg,");
      push (@ret, "GPRINT:${calc_id}_max:${format} max,");
      push (@ret, "GPRINT:${calc_id}_lst:${format} last\\j");
    }

    else
    {
      cluck ("Unknown style: " . $obj->{'style'});
      return;
    }
  } # for (@$files)

  # CombinedArea, CombinedLine: Add up all values, then draw one area or line.
  if (($obj->{'style'} eq 'CombinedArea')
    || ($obj->{'style'} eq 'CombinedLine'))
  {
    if ($current_top_id)
    {
      my $calc_id;

      my $color = $obj->{'color'} || '000000';
      my $legend = $obj->{'legend'} || 'no name';
      
      $calc_id = $meta->getID ();

      # Create the VDEFs for the legend.
      push (@ret, "VDEF:${calc_id}_min=${current_top_id}_min,MINIMUM");
      push (@ret, "VDEF:${calc_id}_avg=${current_top_id}_avg,AVERAGE");
      push (@ret, "VDEF:${calc_id}_max=${current_top_id}_max,MAXIMUM");
      push (@ret, "VDEF:${calc_id}_lst=${current_top_id}_avg,LAST");

      if ($obj->{'style'} eq 'CombinedArea')
      {
        push (@ret, "AREA:${current_top_id}#${color}:${legend}");
      }
      else
      {
        push (@ret, "LINE1:${current_top_id}#${color}:${legend}");
      }

      # XXX format
      push (@ret, "GPRINT:${calc_id}_min:%lg min,");
      push (@ret, "GPRINT:${calc_id}_avg:%lg avg,");
      push (@ret, "GPRINT:${calc_id}_max:%lg max,");
      push (@ret, "GPRINT:${calc_id}_lst:%lg last\\j");
    }
  }

  if ($current_top_id)
  {
    $meta->setStackTop ($current_top_id);
  }

  return (@ret);
} # sub getRRDArgs

# vim: set sw=2 sts=2 et fdm=marker :
