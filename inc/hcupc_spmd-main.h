extern int _hupcpp_main(int argc, char **argv);
int main(int argc, char **argv)
{
	int rv = 0;

	/* hclib_init should always be called first.
	 */
	hupcpp::hcupc_init(&argc, argv);
	upcxx::init(&argc, &argv);

	hupcpp::showStatsHeader();

	// start the user main function
	rv = _hupcpp_main(argc, argv);

	hupcpp::showStatsFooter();
	upcxx::finalize();
	hupcpp::hcupc_finalize();

	return rv;
}
#define main _hupcpp_main
