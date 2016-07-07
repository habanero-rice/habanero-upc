#include <iostream>
#include <stdio.h>
#include <sys/time.h>
#include "hcupc_spmd.h"

#define LONG long
#define SIZE 11
static int threshold = 4;

LONG solutions[20] =
{
		1,
		0,
		0,
		2,
		10, /* 5 */
		4,
		40,
		92,
		352,
		724, /* 10 */
		2680,
		14200,
		73712,
		365596,
		2279184,
		14772512,
		95815104,
		666090624, /* N=18 */
		4968057848,
		39029188884
};

static LONG* local_solutions;

typedef struct data_t {
	LONG A[SIZE];
	LONG depth;
} data_t;

int ok(LONG n,  LONG* A) {
	int i, j;
	for (i =  0; i < n; i++) {
		LONG p = A[i];

		for (j =  (i +  1); j < n; j++) {
			LONG q = A[j];
			if (q == p || q == p - (j - i) || q == p + (j - i))
				return 1;
		}
	}
	return 0;
}

void nqueens_serial(data_t data) {
	LONG* A = data.A;
	LONG depth = data.depth;

	if (SIZE == depth) {
		local_solutions[hupcpp::get_hc_wid()]++;
		return;
	}
	/* try each possible position for queen <depth> */
	for(int i=0; i<SIZE; i++) {
		data_t child;
		child.depth = depth+1;
		memcpy((void*)(child.A), (void*)A, sizeof(LONG)*depth);
		child.A[depth] = i;
		int failed = ok((depth +  1), child.A);
		if (!failed) {
			nqueens_serial(child);
		}
	}
}


void nqueens_kernel(data_t data) {
	LONG* A = data.A;
	LONG depth = data.depth;

	if (depth >= threshold) {
		nqueens_serial(data);
		return;
	}

	/* try each possible position for queen <depth> */
	for(int i=0; i<SIZE; i++) {
		data_t child;
		child.depth = depth+1;
		memcpy((void*)(child.A), (void*)A, sizeof(LONG)*depth);
		child.A[depth] = i;
		int failed = ok((depth +  1), child.A);
		if (!failed) {
			hupcpp::asyncAny([child]() {
				nqueens_kernel(child);
			});
		}
	}
}

void verify_queens(LONG sum) {
	if ( sum == solutions[SIZE-1] )
		printf("OK\n");
	else
		printf("Incorrect Answer (%d!=%d)\n",sum,solutions[SIZE-1]);
}

long get_usecs (void)
{
	struct timeval t;
	gettimeofday(&t,NULL);
	return t.tv_sec*1000000+t.tv_usec;
}

int main(int argc, char* argv[])
{
	hupcpp::init(&argc, &argv);

        if(argc>1) threshold = atoi(argv[1]);
	local_solutions = new LONG[hupcpp::numWorkers()];
	for(int i=0; i<hupcpp::numWorkers(); i++) local_solutions[i] = 0;
	double dur = 0;
	long start = get_usecs();
	hupcpp::finish_spmd([=]() {
		if(upcxx::global_myrank() == 0) {
			data_t data;
			data.depth = 0;
			nqueens_kernel(data);
		}
	});

	LONG sum=0, result;
	for(int i=0; i<hupcpp::numWorkers(); i++) sum += local_solutions[i];
	upcxx::reduce<LONG>(&sum, &result, 1, 0, UPCXX_SUM, UPCXX_LONG_LONG);

	if(upcxx::global_myrank() == 0) {
		long end = get_usecs();
		dur = ((double)(end-start))/1000000;
		verify_queens(result);
		printf("NQueens(%d)(threshold=%d) Time = %fsec\n",SIZE,threshold,dur);
	}
	delete(local_solutions);
	hupcpp::finalize();
	return 0;
}
