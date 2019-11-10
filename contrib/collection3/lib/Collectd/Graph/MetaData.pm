package Collectd::Graph::MetaData;

use strict;
use warnings;
use utf8;

use Carp (qw(cluck confess));

return (1);

sub new
{
  my $pkg = shift;
  my $graph = shift;
  my $obj = bless {}, $pkg;

  $obj->{'_graph'} = $graph;
  $obj->{'_next_id'} = 0;

  return ($obj);
} # new

sub setStackTop
{
  my $obj = shift;
  my $top = shift;

  $obj->{'_stack_top'} = $top;

  return (1);
}

sub getStackTop
{
  return ($_[0]->{'_stack_top'});
}

sub getGraph
{
  return ($_[0]->{'_graph'});
}

sub getID
{
  my $obj = shift;
  my $id;

  $id = 'id' . $obj->{'_next_id'};
  $obj->{'_next_id'}++;

  return ($id);
}

# vim: set sw=2 sts=2 et fdm=marker :
