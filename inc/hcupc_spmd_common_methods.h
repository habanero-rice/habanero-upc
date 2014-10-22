namespace hcpp {
void async_comm_hclib(std::function<void()> &&lambda);
void register_outgoing_async();
void upcxx_copy_lock();
void upcxx_copy_unlock();
void finish_spmd(std::function<void()> lambda);
int barrier();
void register_incoming_async();
void register_incoming_async_ddf(void* ddf);
}
