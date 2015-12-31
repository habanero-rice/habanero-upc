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
#void asyncAwait(promise_t* promise0, T lambda) {
#	hclib::asyncAwait<T>(promise0, lambda);
#}
#template <typename T>
#void asyncAwait(promise_t* promise0, promise_t* promise1, T lambda) {
#	hclib::asyncAwait<T>(promise0, promise1, lambda);
#}
####################################################

for (my $j=0; $j<$ARGV[0]; $j++) {
	print "template <typename T>\n";
	print "void asyncAwait(promise_t* promise0";

	#Printing the promise_t parameters
	for (my $i=1; $i<=$j; $i++) {
  		print ", promise_t* promise$i";
	}
	print ", T lambda) {\n";
	print "\thclib::asyncAwait<T>(";

	for (my $i=0; $i<=$j; $i++) {
  		print "promise$i, ";
	}

	print "lambda);\n}\n";
}
