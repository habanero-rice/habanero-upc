#ifndef VICTIMSELECT_H
#define VICTIMSELECT_H 1

#if defined(__VS_ORIG__)

inline void vsinit(int rank, int size)
{
}

inline char * vsdescript(void)
{
#if defined(__SS_HALF__)
	return "MPI Workstealing (Half Round Robin)";
#else
	return "MPI Workstealing (Round Robin)";
#endif
}

inline int selectvictim(int rank, int size, int last)
{
	last = (last + 1) % size;
	if(last == rank) last = (last + 1) % size;
	return last;
}

#elif defined(__VS_RAND__)

inline void vsinit(int rank, int size)
{
	srand(rank);
}

inline char * vsdescript(void)
{
#if defined(__SS_HALF__)
	return "MPI Workstealing (Half Rand)";
#else
	return "MPI Workstealing (Rand)";
#endif
}

inline int selectvictim(int rank, int size, int last)
{
	do {
		last = rand()%size;
	} while(last == rank);
	return last;
}
#else
#error "You forgot to select a victim selection"
#endif /* Strategy selection */

#endif /* VICTIMSELECT_H */
