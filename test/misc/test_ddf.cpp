#include "hcupc_spmd.h"

using namespace upcxx;

int main(int argc, char **argv)
{
  hupcpp::init(&argc, &argv);
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

  hupcpp::DDF_t* ddf = hupcpp::ddf_create();
  auto f = [](){ std::cout << MYTHREAD << ":" << hupcpp::get_hc_wid() << "--> Hello ! DDF_PUT has been done !!" << std::endl;};
  hupcpp::finish_spmd([=]() {
    hupcpp::asyncAwait(ddf, [=]() { f(); });
    hupcpp::asyncCopy(src, dst, sz, ddf);
  });
 
  printf("Rank %d finishes asyncCopy, src[%lu] = %f, dst[%lu] = %f\n",
         myrank(), sz-1, src[sz-1].get(), sz-1, dst[sz-1].get());

  hupcpp::finalize();
  return 0;
}
