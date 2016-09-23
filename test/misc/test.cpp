#include "hcupc_spmd.h"

using namespace upcxx;
using namespace std;

void fib(int n, int* res)
{
    if (n <= 2) {
        *res = 1;
    }
    else {
        int x, y;
        hupcpp::finish( [&] () {
          hupcpp::async([&](){fib(n-1, &x);});
          hupcpp::async([&](){fib(n-2, &y);});
        });
        *res = x + y;
    }
}

int main(int argc, char **argv)
{
  hupcpp::init(&argc, &argv);
  auto f = [](){ int r; fib(20, &r); std::cout << MYTHREAD << ":" << hupcpp::get_hc_wid() << "--> Fib = " << r << std::endl;};
  hupcpp::finish_spmd([=]() {
    for(int i=0; i<THREADS; i++) {
      if(i != MYTHREAD) {
        hupcpp::asyncAt(i,[=](){
		f();
	});
      }
    }
  });
  cout <<MYTHREAD <<": Out of finish_spmd" << endl;
  hupcpp::finalize();
  return 0;
}
