extern int _hcpp_main(int argc, char **argv);
int main(int argc, char **argv)
{
	int rv = 0;

	/* BUG FIX: hclib_init should always be called first. Noticed
	 * segfaults with increase in UPCXX processes if
 	 * upcxx_init called before hclib_init.
	 */
	hclib_init(&argc, argv);
	upcxx::init(&argc, &argv);

	// start the user main function
	rv = _hcpp_main(argc, argv);

	upcxx::finalize();
	hclib_finalize();

	return rv;
}
#define main _hcpp_main
