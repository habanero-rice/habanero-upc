/* Copyright (c) 2015, Rice University

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

1.  Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above
     copyright notice, this list of conditions and the following
     disclaimer in the documentation and/or other materials provided
     with the distribution.
3.  Neither the name of Rice University
     nor the names of its contributors may be used to endorse or
     promote products derived from this software without specific
     prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

/*
 *      Author: Vivek Kumar (vivekk@rice.edu)
 */

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
int detectWork();
int total_asyncs_inFlight();
bool serve_pending_distSteal_request_successonly();
bool serve_pending_distSteal_request_glb();
bool serve_pending_distSteal_request_baseline();
bool search_tasks_globally_successonly();
bool search_tasks_globally_successonly_glb();
bool search_tasks_globally_glb();
bool search_tasks_globally_baseline();
void decrement_tasks_in_flight_count();
bool received_tasks_from_victim();
void create_distributed_hpt(int row, int column, int chasis, int blade, int rank);
int* create_arrray_of_nearestVictim();
void print_topology_information();
void semiConcDequeInit();
void increment_task_in_flight_self();
void publish_local_load_info();
void increment_outgoing_tasks();
void initialize_hcWorker();

const static char* successonly_distWS = getenv("HCPP_DIST_WS_SUCCESSONLY");
const static char* glb_distWS = successonly_distWS ? NULL : getenv("HCPP_DIST_WS_GLB");
const static char* successonly_glb_distWS = (successonly_distWS || glb_distWS) ? NULL : getenv("HCPP_DIST_WS_SUCCESSONLY_GLB");
const static char* baseline_distWS =  (successonly_distWS || glb_distWS || successonly_glb_distWS) ? NULL : "true";

// statistics related
void get_totalAsyncAny_stats(counter_t *tasksStolen, counter_t* successSteals, counter_t* failSteals, counter_t* recvStealReqsWhenFree, counter_t*, counter_t*);
void get_steal_stats(int* s1, int* s2, int* s3, int* s4, int* s4P);
void record_failedSteal_timeline();
void contacted_victims_statistics(int victims_contacted);
bool check_cyclicSteals(int v, int head, int tail, int* queued_thieves);
void check_if_out_of_work_stats(bool out_of_work);
void stats_initTimelineEvents();
void success_steals_stats();
void total_asyncany_rdma_probes_stats();
void start_finish_spmd_timer();
void end_finish_spmd_timer();
}
