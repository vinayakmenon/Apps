#!/usr/bin/perl
# This program searches through
# all files inside the directory
# passed as first argument, recursively.
# Then replaces the string passed as
# second argument with the string passed
# as third argument, in all the files.
#
# Example:
# ./search_and_replace.pl ./test " __devinit " "__devexit"
#
# The above example will replace all instances of "__devinit
# with a preceding space" with "__devexit".
# "./test" is the directory on which the processing is
# performed.
#
# Author: Vinayak Menon <vinayakm.list@gmail.com>
#

use File::Find;

my $dir = $ARGV[0];
my $org_string = $ARGV[1];
my $new_string = $ARGV[2];

print "Path entered: $dir\n";

find(\&process, $dir);

sub process
{
	if (-f $_) {
    		print "$File::Find::name\n";
		process_file($_);
	}
}

sub process_file
{
	my ($filename) = @_;
	local (@ARGV) = ($filename);
	local($^I) = '.bak';
	
	while (<>) {
		s/$org_string/$new_string/g;
	}
	continue {
		print;
	}

	my $delete = $filename.$^I;
	unlink $delete or warn "could not delete $delete: $!";
}
