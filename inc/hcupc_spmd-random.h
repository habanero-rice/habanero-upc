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

/*
 * This code in this file to find victim based on Euclidian distance from the thief
 * is taken from the github directory https://github.com/perarnau/uts
 */

namespace hupcpp {
#if defined(__VS_RR__)
inline void vsinit()
{
}

inline char * vsdescript(void)
{
	return "HabaneroUPC++ Distributed Workstealing (Round Robin)";
}

inline int selectvictim()
{
	int last = last_steal;
	int rank = upcxx::global_myrank();
	last = (last + 1) % upcxx::global_ranks();
	if(last == rank) last = (last + 1) % upcxx::global_ranks();
	last_steal = last;
	return last;
}

inline void initialize_last_stolen() {
	last_steal = upcxx::global_myrank();
}

inline void resetVictimArray() {
	last_steal = upcxx::global_myrank();
}

#elif defined(__VS_RAND__)
inline void vsinit()
{
        srand(upcxx::global_myrank());
}

inline char * vsdescript(void)
{
        return "HabaneroUPC++ Distributed Workstealing (Rand)";
}

inline int selectvictim()
{
	const int ranks = upcxx::global_ranks();
        const int me = upcxx::global_myrank();
	// might attempting same victim multiple times
        int last = last_steal;
        do {
                last = rand()%ranks;
        } while(last == me);
        last_steal = last;
        return last;
}

inline void initialize_last_stolen() {
        last_steal = upcxx::global_myrank();
}

inline void resetVictimArray() {
}

#elif defined(__VS_DISTANCE__)

typedef struct topology {
	int col;
	int row;
	int cage;
	int slot;
	int anode;
} topology;

typedef struct victimInfo_t {
	double distance;
	int rank;
} victimInfo_t;

victimInfo_t *victims;

/**** quick sort ***/
inline void swap_victims(int i, int j) {
	const int rank = victims[i].rank;
	const double distance = victims[i].distance;
	victims[i].rank = victims[j].rank;
	victims[i].distance = victims[j].distance;
	victims[j].rank = rank;
	victims[j].distance = distance;
}

inline int partition(int left, int right) {
	int i = left;
	int j = right;
	const double pivot = victims[(left + right) / 2].distance;

	while (i <= j) {
		while (victims[i].distance < pivot) i++;
		while (victims[j].distance > pivot) j--;
		if (i <= j) {
			swap_victims(i, j);
			i++;
			j--;
		}
	}

	return i;
}

void qsort_victims(int left, int right) {
	const int index = partition(left, right);
	if (left < index - 1) {
		qsort_victims(left, (index - 1));
	}

	if (index < right) {
		qsort_victims(index, right);
	}
}

inline void vsinit()
{
	int i,j;
	int rank = upcxx::global_myrank();
	// allocate & init the victim array
	victims = new victimInfo_t[upcxx::global_ranks()-1];
	for(i = 0, j = 0; i < upcxx::global_ranks(); i++)
		if(i != upcxx::global_myrank())
			victims[j++].rank = i;

	topology t_p;
	FILE * procfile = fopen("/proc/cray_xt/cname","r");
	HASSERT(procfile);
	char a, b, c, d;
	fscanf(procfile, "%c%d-%d%c%d%c%d%c%d", &a, &t_p.col, &t_p.row, &b, &t_p.cage, &c, &t_p.slot, &d, &t_p.anode);
	fclose(procfile);

	topology topo_all[upcxx::global_ranks()];
	upcxx_allgather(&t_p, topo_all, sizeof(topology));

	int my_x = t_p.col;
	int my_y = t_p.row;
	int my_z = t_p.cage;
	int my_a = t_p.slot;
	int my_b = t_p.anode;

	// compute distance, using the coords
	for(i = 0 ; i < upcxx::global_ranks()-1; i++)
	{
		topology* t_i = &topo_all[victims[i].rank];

		//find coords of this rank
		int p_x = t_i->col;
		int p_y = t_i->row;
		int p_z = t_i->cage;
		int p_a = t_i->slot;
		int p_b = t_i->anode;
		HASSERT(p_x>=0 && p_y>=0 && p_z>=0 && p_a>=0 && p_b>=0);
		// compute euclidian distance between nodes
		double d = pow(my_x - p_x,2) + pow(my_y - p_y,2) + pow(my_z - p_z,2) + pow(my_a - p_a,2) + pow(my_b - p_b,2);

		//if(d < 1.0) d = 1.0;
		d = sqrt(d);
		victims[i].distance = d;
	}
	// sort in increasing distance order
	if(upcxx::global_ranks() > 2) {
		qsort_victims(0, upcxx::global_ranks()-2);
	}
}

inline void resetVictimArray() {
}

#elif defined(__VS_DIST_HPT__)

static int* victims = NULL;

inline void vsinit()
{
	int rank = upcxx::global_myrank();
	int row, column, chasis, blade, node;
	FILE * procfile = fopen("/proc/cray_xt/cname","r");
	HASSERT(procfile);
	char a, b, c, d;
	fscanf(procfile, "%c%d-%d%c%d%c%d%c%d", &a, &column, &row, &b, &chasis, &c, &blade, &d, &node);
	fclose(procfile);

	create_distributed_hpt(row, column, chasis, blade, rank);
	victims = create_arrray_of_nearestVictim();
}

inline char * vsdescript(void)
{
	return "HabaneroUPC++ Distributed Workstealing (Distributed HPT)";
}

inline int selectvictim()
{
	return victims[last_steal++];
}

inline void initialize_last_stolen() {
	last_steal = 0;
}

inline void resetVictimArray() {
	last_steal = 0;
}

#else
#error "You forgot to select a victim selection"
#endif /* Strategy selection */
}
