#!/usr/bin/perl -w

#########################################
# Author: Vivek Kumar (vivekk@rice.edu)
#########################################

if($#ARGV != 0) {
	print "USAGE: perl gen-generic-asyncAwait.pl <Total Args>\n";
 	exit();
}
####################################################
#template <typename T>
#void asyncAwait(DDF_t* ddf0, T lambda) {
#	hclib::asyncAwait<T>(ddf0, lambda);
#}
#template <typename T>
#void asyncAwait(DDF_t* ddf0, DDF_t* ddf1, T lambda) {
#	hclib::asyncAwait<T>(ddf0, ddf1, lambda);
#}
####################################################

for (my $j=0; $j<$ARGV[0]; $j++) {
	print "template <typename T>\n";
	print "void asyncAwait(DDF_t* ddf0";

	#Printing the DDF_t parameters
	for (my $i=1; $i<=$j; $i++) {
  		print ", DDF_t* ddf$i";
	}
	print ", T lambda) {\n";
	print "\thclib::asyncAwait<T>(";

	for (my $i=0; $i<=$j; $i++) {
  		print "ddf$i, ";
	}

	print "lambda);\n}\n";
}
