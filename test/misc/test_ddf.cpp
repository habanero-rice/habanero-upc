#include "hcupc_spmd.h"

using namespace upcxx;

int main(int argc, char **argv)
{
  // Test asyncCopy + DDF
  global_ptr<double> src, dst;
  size_t sz = 1024*1024;
  src = allocate<double>((myrank()+1)%THREADS, sz);
  assert(src != NULL);
  dst = allocate<double>(myrank(), sz);
  assert(dst != NULL);
  src[sz-1] = 123456.789 + (double)myrank();
  dst[sz-1] = 0.0;
  printf("Rank %d starts asyncCopy...\n", MYTHREAD);

  DDF_t* ddf = hcpp::DDF_CREATE();
  auto f = [](){ std::cout << MYTHREAD << ":" << get_worker_id_hc() << "--> Hello ! DDF_PUT has been done !!" << std::endl;};
  hcpp::finish_spmd([=]() {
    hcpp::asyncAwait(ddf, [=]() { f(); });
    hcpp::asyncCopy(src, dst, sz, ddf);
  });
 
  printf("Rank %d finishes asyncCopy, src[%lu] = %f, dst[%lu] = %f\n",
         myrank(), sz-1, src[sz-1].get(), sz-1, dst[sz-1].get());

  return 0;
}
