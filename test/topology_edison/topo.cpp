#include <upcxx.h>
#include <iostream>
#include <cmath>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <assert.h>

using namespace std;
using namespace upcxx;

typedef struct topology {
	int col;
	int row;
	int cage;
	int slot;
	int anode;
} topology;

int main(int argc, char** argv) {
	upcxx::init(&argc, &argv);
	int *victims;
	double *weights;
	gsl_rng *rng;
	gsl_ran_discrete_t *table;

	int i,j;
	int rank = MYTHREAD;
	// allocate & init the victim array
	victims = new int[THREADS-1];
	for(i = 0, j = 0; i < THREADS; i++)
		if(i != MYTHREAD)
			victims[j++] = i;

	topology t_p;
	FILE * procfile = fopen("/proc/cray_xt/cname","r");
	assert(procfile);
	char a, b, c, d;
	fscanf(procfile, "%c%d-%d%c%d%c%d%c%d", &a, &t_p.col, &t_p.row, &b, &t_p.cage, &c, &t_p.slot, &d, &t_p.anode);
	fclose(procfile);
	printf("%d: col=%d, row=%d, cage=%d, slot=%d, node=%d\n",MYTHREAD,t_p.col,t_p.row,t_p.cage,t_p.slot,t_p.anode);

	topology topo_all[THREADS];
	upcxx_allgather(&t_p, topo_all, sizeof(topology));	

	int my_x = t_p.col;
	int my_y = t_p.row;
	int my_z = t_p.cage;
	int my_a = t_p.slot;
	int my_b = t_p.anode;

	// compute weights, using the coords
	weights = new double[THREADS-1];
	cout << MYTHREAD << ": ";
	for(i = 0 ; i < THREADS-1; i++)
	{
		topology* t_i = &topo_all[victims[i]];

		//find coords of this rank
		int p_x = t_i->col;
		int p_y = t_i->row;
		int p_z = t_i->cage;
		int p_a = t_i->slot;
		int p_b = t_i->anode;
		// compute euclidian distance between nodes
		double d = pow(my_x - p_x,2) + pow(my_y - p_y,2) + pow(my_z - p_z,2) + pow(my_a - p_a,2) + pow(my_b - p_b,2);

		if(d < 1.0) d = 1.0;
		d = sqrt(d);
		weights[i] = 1.0/d;
		cout << d <<":"<<weights[i]<< ", ";
	}
	cout << endl;

	// init an rng, using the rank-th number from the global sequence as seed
	gsl_rng_env_setup();
	const gsl_rng_type *T = gsl_rng_default;
	rng = gsl_rng_alloc(T);
	for(i = 0; i < rank; i++)
		gsl_rng_uniform_int(rng,UINT_MAX);
	gsl_rng_set(rng,gsl_rng_uniform_int(rng,UINT_MAX));
	table = gsl_ran_discrete_preproc(THREADS-1,weights);
	upcxx::finalize();
	return 0;
}
