#include "hcpp-utils.h"

namespace hcpp
{
  using namespace std;

  
  void async_cpp_wrapper(void *args)
  {
    std::function<void()> *lambda = (std::function<void()> *)args;
    (*lambda)();
    delete lambda;
  }

  void finish(std::function<void()> lambda)
  {
    start_finish();
    lambda();
    end_finish();
  }

  void asyncComm(std::function<void()> &&lambda)
  {
    std::function<void()> * copy_of_lambda = new std::function<void()> (lambda);
    ::async(&async_cpp_wrapper, (void *)copy_of_lambda,NO_DDF, NO_PHASER, ASYNC_COMM);
  }

  void async(std::function<void()> &&lambda)
  {
    std::function<void()> * copy_of_lambda = new std::function<void()> (lambda);
    ::async(&async_cpp_wrapper, (void *)copy_of_lambda,NO_DDF, NO_PHASER, NO_PROP);
  }

  void forasync_cpp_wrapper(void *args, int iterator)
  {
    std::function<void(int)> *lambda = (std::function<void(int)> *)args;
    (*lambda)(iterator);
  }

  int numWorkers() {
    return ::get_worker_count();
  }

  void forasync(int lowBound, int highBound, std::function<void(int)> lambda) 
  {
    loop_domain_t *loop = new loop_domain_t; 
    loop->low = lowBound;
    loop->high =  highBound;
    loop->stride = 1;
    loop->tile =  (int)highBound/numWorkers();
    std::function<void(int)> * copy_of_lambda = new std::function<void(int)> (lambda);
    ::forasync((void*)&forasync_cpp_wrapper, (void *)copy_of_lambda, NULL, NULL, NULL, 1, loop, FORASYNC_MODE_FLAT);
  }

  DDF_t** DDF_LIST(int total_ddfs) {
    return new DDF_t* [total_ddfs];
  }

  DDF_t* DDF_CREATE() {
    return ddf_create();
  }

  void DDF_PUT(DDF_t* ddf, void* value) {
    ddf_put(ddf, value);
  }

  void* DDF_GET(DDF_t* ddf) {
    return ddf_get(ddf);
  }

#ifdef _PHASER_LIB_
  PHASER_t* PHASER_CREATE(PHASER_m mode) {
    PHASER_t *ph = new PHASER_t;
    phaser_create(ph, mode, 2);
    return ph;
  }

  void DROP(PHASER_t* ph) {
    phaser_drop(*ph);
  }

  void NEXT() {
    phaser_next();
  }
#endif // _PHASER_LIB_
}
