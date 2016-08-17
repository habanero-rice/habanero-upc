 /* Print the coordinates of all MPI ranks in TOFU.
 */
#include <stdio.h>
#include <mpi.h>
#include <mpi-ext.h>


int main(int argc, char **argv)
{
	MPI_Init(&argc, &argv);
	int comm_size, comm_rank;
	MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &comm_rank);
	if(comm_rank == 0)
	{
		// print the tofu coords
		for(int i = 0 ; i < comm_size; i++)
		{
			//find coords of this rank
			int x,y,z,a,b,c;
			FJMPI_Topology_sys_rank2xyzabc(i,&x,&y,&z,&a,&b,&c);
			fprintf(stdout,"%d: %d %d %d %d %d %d\n",i,x,y,z,a,b,c);
		}
	}
	MPI_Barrier(MPI_COMM_WORLD);
	MPI_Finalize();
	return 0;
}
