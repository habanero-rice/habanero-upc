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

#include "hcupc_spmd-internal.h"
#include "hcupc_spmd_common_methods.h"
#include "hcupc_spmd-commTask.h"
#include "hcupc_spmd-atomics.h"

/*
 * Methods used here are only used for statistics
 */

namespace hupcpp {


counter_t	total_success_steals=0;
counter_t	total_tasks_stolen=0;
counter_t	total_failed_steals=0;
counter_t	total_received_stealRequests_when_idle=0;
counter_t	total_cyclic_steals=0;
counter_t	total_asyncany_rdma_probes=0;

int		total_steal_1 = 0;
int		total_steal_2 = 0;
int		total_steal_3 = 0;
int		total_steal_4 = 0;
int		total_steal_4Plus = 0;

/*
 * only for statistics
 */
void check_cyclicSteals(int v, int head, int tail, int* queued_thieves) {
	while(head!=tail) {
		if(v==queued_thieves[++head % THREADS]) {
			total_cyclic_steals++;
			return;
		}
	}
}

void check_if_out_of_work_stats(bool out_of_work) {
	if(out_of_work) total_received_stealRequests_when_idle++;
}

void success_steals_stats() {
	total_success_steals++;
}

// ----------------------STATISTICS FOR FAILED STEALS TIMELINE -----------------------------

const static char* app_total_time_estimate = getenv("HCPP_APP_EXEC_TIME");
// App execution time divided into total of 20 timesteps
#define MAX_TIMESTEPS	20
static double app_timesteps[MAX_TIMESTEPS];
static counter_t fail_steals_timeline[MAX_TIMESTEPS];
static long app_tStart = -1;
static double app_tTotal = -1;
static double app_tEnd = -1;

long mysecond() {
   struct timeval t;
   gettimeofday(&t,NULL);
   return t.tv_sec*1000000+t.tv_usec;
}

void stats_initTimelineEvents() {
	if(app_total_time_estimate) {
		assert(app_total_time_estimate != NULL);
		app_tTotal = atof(app_total_time_estimate);
		app_tStart = mysecond();
		double curr_tStep = 0;
		double tStep = app_tTotal/((double) MAX_TIMESTEPS);
		// calculate timesteps values
		for(int i=0; i<MAX_TIMESTEPS; i++) {
			curr_tStep += tStep;
			app_timesteps[i] = curr_tStep;
			fail_steals_timeline[i] = 0;
		}
		if(MYTHREAD == 0) {
			printf(">>> HCPP_APP_EXEC_TIME\t\t= %f seconds\n",app_tTotal);
		}
	}
}

void record_failedSteal_timeline() {
	total_failed_steals++;
	if(app_total_time_estimate) {
		const double tDuration = ((double)(mysecond()-app_tStart))/1000000;
		bool found = false;
		for(int i=0; i<MAX_TIMESTEPS; i++) {
			if(tDuration < app_timesteps[i]) {
				fail_steals_timeline[i]++;
				found = true;
				break;
			}
		}
		if(!found) {
			/*
			 * This will execute for those cases the total estimate for
			 * execution timing of this app has exceeded. If its taking
			 * more time to finish than estimated then we simly add this
			 * in the last timestep.
			 */
			fail_steals_timeline[MAX_TIMESTEPS-1]++;
		}
	}
}

// collective operation
static void showStats_failedSteals_timeline() {
	if(app_total_time_estimate) {
		static counter_t timeline[MAX_TIMESTEPS];
		for(int i=0; i<MAX_TIMESTEPS; i++) timeline[i] = 0;
		upcxx::upcxx_reduce<counter_t>(fail_steals_timeline, timeline, MAX_TIMESTEPS, 0, UPCXX_SUM, UPCXX_ULONG_LONG);
		if(MYTHREAD == 0) {
			printf("============================ FailStealTimeline Statistics Totals ============================\n");
			counter_t sum_total_failed_steals = 0;
			for(int i=0; i<MAX_TIMESTEPS; i++) {
				printf("tStep%d\t",i);
				sum_total_failed_steals += timeline[i];
			}
			printf("\n");
			for(int i=0; i<MAX_TIMESTEPS; i++) {
				//                      printf("%llu\t",timeline[i]);
				// calculate percentage of failed steals in this timeline
				double percent = sum_total_failed_steals>0 ? (100.0 *((timeline[i]*1.0) / sum_total_failed_steals)) : 0;
				if(timeline[i]>0) printf("%.3f\t",percent);
				else printf("0\t");
			}
			printf("\n------------------------------ End FailStealTimeline Statistics -----------------------------\n");
		}
	}
}

// ---------------------------- TIMELINE ENDS ----------------------------------------------
/*
 * only for statistics
 */
void contacted_victims_statistics(int victims_contacted) {
	if(victims_contacted) {
		switch(victims_contacted) {
		case 1:
			total_steal_1++;
			break;
		case 2:
			total_steal_2++;
			break;
		case 3:
			total_steal_3++;
			break;
		case 4:
			total_steal_4++;
			break;
		default:
			total_steal_4Plus++;
			break;
		}
	}
}

/*
 * only for statistics
 */
void get_totalAsyncAny_stats(counter_t *tasksStolen, counter_t* successSteals, counter_t* failSteals, counter_t* recvStealReqsWhenFree, counter_t* cyclic_steals, counter_t* rdma_probes) {
	*tasksStolen = total_tasks_stolen;
	*successSteals = total_success_steals;
	*failSteals = total_failed_steals;
	*recvStealReqsWhenFree = total_received_stealRequests_when_idle;
	*cyclic_steals = total_cyclic_steals;
	*rdma_probes = total_asyncany_rdma_probes;
	total_cyclic_steals = 0;
	total_asyncany_rdma_probes = 0;
	total_failed_steals = 0;
	total_success_steals = 0;
	total_tasks_stolen = 0;
	total_received_stealRequests_when_idle = 0;
}

void increment_outgoing_tasks() {
	total_tasks_stolen++;
}

void total_asyncany_rdma_probes_stats() {
	total_asyncany_rdma_probes++;
}

void total_failed_steals_stats() {
	total_failed_steals++;
}

void get_steal_stats(int* s1, int* s2, int* s3, int* s4, int* s4P) {
	*s1 = total_steal_1;
	*s2 = total_steal_2;
	*s3 = total_steal_3;
	*s4 = total_steal_4;
	*s4P = total_steal_4Plus;
	total_steal_1 = 0;
	total_steal_2 = 0;
	total_steal_3 = 0;
	total_steal_4 = 0;
	total_steal_4Plus = 0;
}

/*
 * NOTE: this is a collective routine
 */

static void runtime_statistics(double duration) {
#ifdef HC_COMM_WORKER_STATS
	shared_array<int> push_outd_myPlace;
	shared_array<int> push_ind_myPlace;
	shared_array<int> steal_ind_myPlace;

	push_outd_myPlace.init(THREADS);
	push_ind_myPlace.init(THREADS);
	steal_ind_myPlace.init(THREADS);

	int c1, c2, c3;
	hcpp::gather_commWorker_Stats(&c1, &c2, &c3);

	push_outd_myPlace[MYTHREAD] = c1;
	push_ind_myPlace[MYTHREAD] = c2;
	steal_ind_myPlace[MYTHREAD] = c3;
#endif

	shared_array<counter_t> tasks_sendto_remotePlace;
	shared_array<counter_t> total_dist_successSteals;
	shared_array<counter_t> total_dist_failedSteals;
	shared_array<counter_t> total_recvStealReqsWhenFree;
	shared_array<counter_t> total_cyclic_steals;
	shared_array<counter_t> total_rdma_probes;

	tasks_sendto_remotePlace.init(THREADS);
	total_dist_successSteals.init(THREADS);
	total_dist_failedSteals.init(THREADS);
	total_recvStealReqsWhenFree.init(THREADS);
	total_cyclic_steals.init(THREADS);
	total_rdma_probes.init(THREADS);

	counter_t c4, c5, c6, c7, c8, c9;

	get_totalAsyncAny_stats(&c4, &c5, &c6, &c7, &c8, &c9);

	tasks_sendto_remotePlace[MYTHREAD] = c4;
	total_dist_successSteals[MYTHREAD] = c5;
	total_dist_failedSteals[MYTHREAD] = c6;
	total_recvStealReqsWhenFree[MYTHREAD] = c7;
	total_cyclic_steals[MYTHREAD] = c8;
	total_rdma_probes[MYTHREAD] = c9;

	int s1=0, s2=0, s3=0, s4=0, s4P=0;
	get_steal_stats(&s1, &s2, &s3, &s4, &s4P);

	shared_array<int> total_steal_1;
	shared_array<int> total_steal_2;
	shared_array<int> total_steal_3;
	shared_array<int> total_steal_4;
	shared_array<int> total_steal_4P;

	total_steal_1.init(THREADS);
	total_steal_2.init(THREADS);
	total_steal_3.init(THREADS);
	total_steal_4.init(THREADS);
	total_steal_4P.init(THREADS);

	total_steal_1[MYTHREAD] = s1;
	total_steal_2[MYTHREAD] = s2;
	total_steal_3[MYTHREAD] = s3;
	total_steal_4[MYTHREAD] = s4;
	total_steal_4P[MYTHREAD] = s4P;

	shared_array<double> tWork_i;
	shared_array<double> tOvh_i;
	shared_array<double> tSearch_i;

	tWork_i.init(THREADS);
	tOvh_i.init(THREADS);
	tSearch_i.init(THREADS);
	double tWork, tOvh, tSearch;
	hcpp::hcpp_getAvgTime (&tWork, &tOvh, &tSearch);
	tWork_i[MYTHREAD] = tWork;
	tOvh_i[MYTHREAD] = tOvh;
	tSearch_i[MYTHREAD] = tSearch;

	hupcpp::barrier();
#ifdef DIST_WS
	if(MYTHREAD == 0) {
#ifdef HC_COMM_WORKER_STATS
		int t1=0, t2=0, t3=0;
#endif
		counter_t t4=0, t5=0, t6=0, t7=0, t7b=0, t7c=0;
		int t8=0,t9=0,t10=0,t11=0,t12=0;
		double t13=0, t14=0, t15=0;
		for(int i=0; i<THREADS; i++) {
#ifdef HC_COMM_WORKER_STATS
			t1 += push_outd_myPlace[i];
			t2 += push_ind_myPlace[i];
			t3 += steal_ind_myPlace[i];
#endif
			t4 += tasks_sendto_remotePlace[i];
			t5 += total_dist_successSteals[i];
			t6 += total_dist_failedSteals[i];
			t7 += total_recvStealReqsWhenFree[i];
			t7b += total_cyclic_steals[i];
			t7c += total_rdma_probes[i];

			t8 += total_steal_1[i];
			t9 += total_steal_2[i];
			t10 += total_steal_3[i];
			t11 += total_steal_4[i];
			t12 += total_steal_4P[i];

			t13 += tWork_i[i];
			t14 += tOvh_i[i];
			t15 += tSearch_i[i];
		}

		t13 = t13/THREADS;
		t14 = t14/THREADS;
		t15 = t15/THREADS;

		printf("============================ MMTk Statistics Totals ============================\n");
#ifdef HC_COMM_WORKER_STATS
		printf("time.mu\ttotalPushOutDeq\ttotalPushInDeq\ttotalStealsInDeq\ttotalTasksSendToRemote\ttotalSuccessDistSteals\ttotalFailedDistSteals\ttotalIncomingStealReqsWhenIdle\ttotalCyclicSteals\ttotalRDMAasyncAnyProbes\ttWork\ttOverhead\ttSearch");
		printf("\tS1\tS2\tS3\tS4\tS4Plus\n");

		printf("%.3f\t%d\t%d\t%d\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%.3f\t%.3f\t%.5f",duration,t1,t2, t3, t4, t5, t6, t7, t7b, t7c, t13, t14, t15);
		printf("\t%d\t%d\t%d\t%d\t%d\n",t8,t9,t10,t11,t12);

#else
		printf("time.mu\ttotalTasksSendToRemote\ttotalSuccessDistSteals\ttotalFailedDistSteal\ttotalIncomingStealReqsWhenIdle\ttotalCyclicSteals\ttotalRDMAasyncAnyProbes");
		printf("\tS1\tS2\tS3\tS4\tS4Plus\n");

		printf("%.3f\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu",duration,t4, t5, t6, t7, t7b, t7c);
		printf("\t%d\t%d\t%d\t%d\t%d\n",t8,t9,t10,t11,t12);

#endif
		printf("Total time: %.3f ms\n",duration);
		printf("------------------------------ End MMTk Statistics -----------------------------\n");
		printf("===== TEST PASSED in %.3f msec =====\n",duration);
	}
	if(getenv("HCPP_DIST_WS_BASELINE")) {
		showStats_failedSteals_timeline();
	}
#else // !DIST_WS
	if(MYTHREAD == 0) {
#ifdef HC_COMM_WORKER_STATS
		int t1=0, t2=0, t3=0;
#endif
		counter_t t4=0;
		for(int i=0; i<THREADS; i++) {
#ifdef HC_COMM_WORKER_STATS
			t1 += push_outd_myPlace[i];
			t2 += push_ind_myPlace[i];
			t3 += steal_ind_myPlace[i];
#endif
			t4 += tasks_sendto_remotePlace[i];
		}

		printf("============================ MMTk Statistics Totals ============================\n");
#ifdef HC_COMM_WORKER_STATS
		printf("time.mu\ttotalPushOutDeq\ttotalPushInDeq\ttotalStealsInDeq\ttotalTasksSendToRemote\n");
		printf("%.3f\t%d\t%d\t%d\t%llu\n",duration,t1,t2, t3, t4);

#else
		printf("time.mu\ttotalTasksSendToRemote\n");
		printf("%.3f\t%llu\n",duration,t4);

#endif
		printf("Total time: %.3f ms\n",duration);
		printf("------------------------------ End MMTk Statistics -----------------------------\n");
		printf("===== TEST PASSED in %.3f msec =====\n",duration);
	}
#endif // end DIST_WS
	hupcpp::barrier();
}

static long benchmark_start_time_stats = 0;

void showStatsHeader() {
	if(MYTHREAD == 0) {
		cout << endl;
		cout << "-----" << endl;
		cout << "mkdir timedrun fake" << endl;
		cout << endl;
	}
	initialize_hcWorker();
	if(MYTHREAD == 0) {
		cout << endl;
		cout << "-----" << endl;
	}
	benchmark_start_time_stats = mysecond();
}

void showStatsFooter() {
	HASSERT(benchmark_start_time_stats != 0);
	double dur = (((double)(mysecond()-benchmark_start_time_stats))/1000000) * 1000; //msec
	if(MYTHREAD == 0) {
		print_topology_information();
	}
	runtime_statistics(dur);
}

}
