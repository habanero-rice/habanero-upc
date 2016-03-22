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
#if defined(GASNET_SEQ)
    fprintf(stderr, "Sequential gasnet\n");
#elif defined(GASNET_PARSYNC)
    fprintf(stderr, "Parsync gasnet\n");
#elif defined(GASNET_PAR)
    fprintf(stderr, "Parallel gasnet\n");
#else
#endif
    hupcpp::launch(&argc, &argv, [=] {
        log("beginning");
        auto f = [] {
            int r;
            fib(20, &r);
            std::cout << upcxx::global_myrank() << ":" << hupcpp::get_hc_wid() << "--> Fib = " << r << std::endl;
        };

        hupcpp::barrier();
        log("after barrier");

        hupcpp::finish_spmd([=]() {
            log("inside finish_spmd");

            // for (int i = 0; i < upcxx::global_ranks(); i++) {
            //     if (i != upcxx::global_myrank()) {
            //         hupcpp::asyncAt(i,[=](){
            //             f();
            //         });
            //     }
            // }
        });

        log("ending");
    });

    return 0;
}
