/*
 * HC forasync nbody example 
 Author :Deepak Majeti

 Ported to HabaneroUPC++: Vivek Kumar (vivekk@rice.edu)
 */

#include <stdio.h>
#include <math.h>
#include <sys/time.h>
#include <iostream>
#include "hcupc_spmd.h"
#include "forasyncAny1D.h"

//-----------------------------------------------------------------
#define MAX_STEPS 	1
#define NUMBODIES	8192
#include "8192_inp.h"
#define TILESIZE	1
#define VERIFY_SHORT
//-----------------------------------------------------------------

static float accx[NUMBODIES];
static float accy[NUMBODIES];
static float accz[NUMBODIES];

#ifdef VERIFY_SHORT
#define MAX_HCLIB_WORKERS 24
/* For stats generation: */
typedef unsigned long long counter_t;
static counter_t TOTAL_COUNTS[MAX_HCLIB_WORKERS];
#define COUNT_OPS		{ TOTAL_COUNTS[hupcpp::get_hc_wid()]++; }
#else
#define COUNT_OPS
#endif

#if 0
bool verify_result(char *arg, float *acc){
	//verify the result
	bool success = true;
	int i;
	FILE *fp;
	fp=fopen(arg,"r");
	for(i=0;i<NUMBODIES;i+=512)
	{
		float temp;
		fscanf(fp,"%f",&temp);
		if(acc[i*1]-temp>0.00001){
			success = false;
			break;
		}
	}
	return success;
}
#endif

void calculate_forces() {
	hupcpp::loop_domain_t loop = {0, NUMBODIES, 1, TILESIZE};
	forasyncAny1D(&loop, [=](int i) {
		COUNT_OPS
		float s = 0.0;
		int sum = 0;
		for(int j=0;j< NUMBODIES;j++,sum++) {
			float dx = posx[i]-posx[j];
			float dy = posy[i]-posy[j];
			float dz = posz[i]-posz[j];

			float sqr_dist = dx * dx + dy * dy + dz * dz;
			if (sqr_dist!=0) {
				float inv_distance_sq = sqrt(1/sqr_dist);
				s = (mass[i] * inv_distance_sq) * (inv_distance_sq * inv_distance_sq);
			}
			accx[i] += dx * s;
			accy[i] += dy * s;
			accz[i] += dz * s;
		}
		assert(sum == NUMBODIES);
	});
}

long get_usecs (void)
{
   struct timeval t;
   gettimeofday(&t,NULL);
   return t.tv_sec*1000000+t.tv_usec;
}

int main(int argc,char **argv) {
	hupcpp::init(&argc, &argv);

#if 0
	if(argc!=5) {
		printf("(ERROR) USAGE: ./a.out <total bodies> <tileSize> <in data file> <out data file>\n");
		hupcpp::finalize();
	}
	assert(atoi(argv[1]) == NUMBODIES);
	assert(atoi(argv[2]) == TILESIZE);
	//Init(argv[3]);
#endif

	long start = get_usecs();
	for(int time_steps=0;time_steps<MAX_STEPS;time_steps++) {
		hupcpp::finish_spmd([=]() {
			if(MYTHREAD ==0) {
				calculate_forces();
			}
		});
	}
	long end = get_usecs();
 	double dur = ((double)(end-start))/1000000;
#ifdef VERIFY_SHORT
	counter_t sum = 0;
	for(int i=0; i<MAX_HCLIB_WORKERS; i++) {
		sum+=TOTAL_COUNTS[i];
	}
	counter_t total_sum;
	upcxx::upcxx_reduce<counter_t>(&sum, &total_sum, 1, 0, UPCXX_SUM, UPCXX_ULONG_LONG);
	if(MYTHREAD == 0) {
		const counter_t expected = NUMBODIES * MAX_STEPS;
		const char* res = expected == total_sum ? "PASSED" : "FAILED";
		printf("Test %s, Time = %0.3f\n",res,dur);
	}
#endif

#if 0
	// gather result from all other processes using upcxx_reduce
	float accx_all[NUMBODIES], accy_all[NUMBODIES], accz_all[NUMBODIES];
	upcxx::upcxx_reduce<float>(accx, accx_all, NUMBODIES, 0, UPCXX_SUM, UPCXX_FLOAT);
	upcxx::upcxx_reduce<float>(accy, accy_all, NUMBODIES, 0, UPCXX_SUM, UPCXX_FLOAT);
	upcxx::upcxx_reduce<float>(accz, accz_all, NUMBODIES, 0, UPCXX_SUM, UPCXX_FLOAT);

	if(MYTHREAD ==0) {
		printf("0: Computation done\n");
		printf("Test Passed=%d\n",verify_result(argv[4],accx_all));
	}
#endif

	hupcpp::barrier();
	hupcpp::finalize();
	return 0;
}
