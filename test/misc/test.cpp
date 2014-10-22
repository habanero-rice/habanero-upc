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
  auto f = [](){ std::cout << MYTHREAD << ":" << get_worker_id_hc() << "--> Hello World !!!!!" << std::endl;};
  auto f_fib = [](){ int r; fib(20, &r); std::cout << MYTHREAD << ":" << get_worker_id_hc() << "--> Fib = " << r << std::endl;};
  hcpp::finish_spmd([=]() {
    for(int i=0; i<THREADS; i++) {
      if(i != MYTHREAD) {
        hcpp::asyncAt(i,[=](){f_fib();});
        hcpp::asyncAt(i,[=](){f();});
      }
    }
  });
  cout <<MYTHREAD <<": Out of finish_spmd" << endl;
 
  return 0;
}
