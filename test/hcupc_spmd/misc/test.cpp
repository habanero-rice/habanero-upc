#include "hcupc_spmd.h"

using namespace upcxx;

void fib(int n, int* res)
{
    if (n <= 2) {
        *res = 1;
    }
    else {
        int x, y;
        hcpp::finish( [&] () {
          hcpp::async([&](){fib(n-1, &x);});
          hcpp::async([&](){fib(n-2, &y);});
        });
        *res = x + y;
    }
}

int main(int argc, char **argv)
{
  // Test async_put
  global_ptr<double> src, dst;
  size_t sz = 1024*1024;
  src = allocate<double>((myrank()+1)%THREADS, sz);
  assert(src != NULL);
  dst = allocate<double>(myrank(), sz);
  assert(dst != NULL);
  src[sz-1] = 123456.789 + (double)myrank();
  dst[sz-1] = 0.0;
  printf("Rank %d starts async_put...\n", MYTHREAD);
  barrier();

  auto f = [](){ std::cout << MYTHREAD << ":" << get_worker_id_hc() << "--> Hello World !!!!!" << std::endl;};
  auto f_fib = [](){ int r; fib(20, &r); std::cout << MYTHREAD << ":" << get_worker_id_hc() << "--> Fib = " << r << std::endl;};
  auto f_fib_1 = [](){ int r; fib(20, &r); std::cout << MYTHREAD << ":" << get_worker_id_hc() << "--> Fib_1 = " << r << std::endl;};
  hcpp::finish_spmd([=]() {
    hcpp::async([=]() { f_fib(); });
    hcpp::async_put(src, dst, sz);
    if(MYTHREAD==0) hcpp::async_at(1,[=](){f_fib_1();});
  });
  cout <<MYTHREAD <<": Out of finish_spmd" << endl;
 
  printf("Rank %d finishes async_put, src[%lu] = %f, dst[%lu] = %f\n",
         myrank(), sz-1, src[sz-1].get(), sz-1, dst[sz-1].get());

  return 0;
}
