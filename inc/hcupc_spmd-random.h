/*
 * This file is originally taken from the github directory https://github.com/perarnau/uts
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
	int rank = MYTHREAD;
	last = (last + 1) % THREADS;
	if(last == rank) last = (last + 1) % THREADS;
	last_steal = last;
	return last;
}

inline void initialize_last_stolen() {
	last_steal = MYTHREAD;
}

inline void resetVictimArray() {
	last_steal = MYTHREAD;
}

#elif defined(__VS_RAND__)
inline void vsinit()
{
	if(MYTHREAD==0) std::cout<<"WARNING: __VS_RAND__ may not work as expected with current design" << std::endl;
	srand(MYTHREAD);
}

inline char * vsdescript(void)
{
	return "HabaneroUPC++ Distributed Workstealing (Rand)";
}

inline int selectvictim()
{
	int last = last_steal;
	int rank = MYTHREAD;
	do {
		last = rand()%THREADS;
	} while(last == rank);
	last_steal = last;
	return last;
}

inline void initialize_last_stolen() {
	last_steal = MYTHREAD;
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
	int rank = MYTHREAD;
	// allocate & init the victim array
	victims = new victimInfo_t[THREADS-1];
	for(i = 0, j = 0; i < THREADS; i++)
		if(i != MYTHREAD)
			victims[j++].rank = i;

	topology t_p;
	FILE * procfile = fopen("/proc/cray_xt/cname","r");
	HASSERT(procfile);
	char a, b, c, d;
	fscanf(procfile, "%c%d-%d%c%d%c%d%c%d", &a, &t_p.col, &t_p.row, &b, &t_p.cage, &c, &t_p.slot, &d, &t_p.anode);
	fclose(procfile);

	topology topo_all[THREADS];
	upcxx_allgather(&t_p, topo_all, sizeof(topology));

	int my_x = t_p.col;
	int my_y = t_p.row;
	int my_z = t_p.cage;
	int my_a = t_p.slot;
	int my_b = t_p.anode;

	// compute distance, using the coords
	for(i = 0 ; i < THREADS-1; i++)
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
	if(THREADS > 2) {
		qsort_victims(0, THREADS-2);
	}
}

inline void resetVictimArray() {
}

#elif defined(__VS_DIST_HPT__)

static int* victims = NULL;

inline void vsinit()
{
	int rank = MYTHREAD;
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
