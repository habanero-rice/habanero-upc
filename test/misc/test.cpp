#include "hcupc_spmd.h"
#include "hclib_cpp.h"

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

void log(const char *msg) {
    printf(">>> rank = %d # ranks = %d <<<< %s\n", upcxx::global_myrank(),
            upcxx::global_ranks(), msg);
}

int main(int argc, char **argv) {
  hupcpp::launch(&argc, &argv, [=] {
      const int curr_rank = upcxx::global_myrank();
      auto f = [curr_rank](){ int r; fib(20, &r); std::cout << curr_rank << " -> " << upcxx::global_myrank() << ":" << hupcpp::get_hc_wid() << "--> Fib = " << r << std::endl;};
      hupcpp::finish_spmd([=]() {
        for(int i=0; i<upcxx::global_ranks(); i++) {
          if(i != upcxx::global_myrank()) {
            hupcpp::asyncAt(i,[=](){
            f();
        });
          }
        }
      });
  });
  return 0;
}
