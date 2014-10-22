#include <upcxx.h>
#include <list>
#include <functional>

#define __HCPP_DEV_BUILD__ 0

#ifdef __HCPP_DEV_BUILD__
#define HCPP_ASSERT(cond) 	assert(cond);
#else
#define HCPP_ASSERT(cond)	//Do Nothing
#endif

extern "C" {
#include "hclib.h"
}

#include<pthread.h>
#include<stdlib.h>
#include<unistd.h>

#define ASYNC_COMM ((int) 0x2)

#ifdef DIST_WS

#endif

// never declare using namespace hcpp ... code will hang
using namespace upcxx;
