#include "hcupc_spmd.h"

using namespace upcxx;

typedef struct tmp {
  int value;
} tmp;

int main(int argc, char **argv)
{
  hupcpp::init(&argc, &argv);
  tmp t = {0};

  hcpp::finish_spmd([&]() {
    if(MYTHREAD==0) hcpp::asyncAt(1,[&](){
      // this is possible only if "t" is captured by reference and not by value.
      t.value = 100;  
      cout << MYTHREAD << ": tmp.value = " << t.value << endl;
    });
  });
  cout <<MYTHREAD <<": Out of finish_spmd" << endl;
  // This will still show "0" as the changes made by asyncAt is invisible to Place0
  // In X10 both Place0 and Place1 will show the same result.
  cout << MYTHREAD <<": t.value = " << t.value << endl;
  hupcpp::finalize();
  return 0;
}
