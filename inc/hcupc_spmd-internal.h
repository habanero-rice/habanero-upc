#include <assert.h>
#include "hcupc_spmd-runtime-interface.h"
#include <upcxx.h>
#include<pthread.h>
#include<unistd.h>
#include <sys/time.h>

// never declare using namespace hcpp ... code will hang
using namespace upcxx;
using namespace std;

namespace hupcpp {

/*
 * Any function which is not exposed to user or its not used inside
 * hcupc_spmd-utils.h is internal to the library and hence should
 * be declared only here.
 */

typedef unsigned long long counter_t;

void initialize_distws_setOfThieves();
void get_totalAsyncAny_stats(counter_t *tasksStolen, counter_t* successSteals, counter_t* failSteals, counter_t* recvStealReqsWhenFree, counter_t*, counter_t*);
void get_steal_stats(int* s1, int* s2, int* s3, int* s4, int* s4P);
int detectWork();
int total_asyncs_inFlight();
bool serve_pending_distSteal_request();
bool search_tasks_globally();
void decrement_tasks_in_flight_count();
bool received_tasks_from_victim();
void create_distributed_hpt(int row, int column, int chasis, int blade, int rank);
int* create_arrray_of_nearestVictim();
void print_topology_information();
void semiConcDequeInit();
void increment_task_in_flight_self();
void publish_local_load_info();
void increment_outgoing_tasks();
}
