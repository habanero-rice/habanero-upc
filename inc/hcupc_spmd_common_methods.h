namespace hupcpp {

void finish_spmd(std::function<void()> lambda);
int barrier();
void showStatsHeader();
void showStatsFooter();
void queue_source_place_of_remoteTask(int i);
upcxx::event* get_upcxx_event();
void init(int * argc, char *** argv);
void finalize();
}
