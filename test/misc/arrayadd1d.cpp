/*
 * HC CONCORD foreach add.hc example 
 */

#include "hcupc_spmd.h"

void check(int *a,int val,int num_iters){
	int i;
	for(i=0;i< num_iters;i++){
		if(a[i]!=val){
			printf("ERROR a[%d]=%d!=%d\n",i,a[i],val);
			exit(0);
		}
	}
}

int main(int argc, char *argv[])
{
    int num_iters;
    int tilesize;
    int i;
    int *a,*b,*c;
   
  hupcpp::init(&argc, &argv);
   if(argc!=3){printf("USAGE:./arrayadd1d NUM_ITERS TILE_SIZE\n");return -1;}
    num_iters= atoi(argv[1]);
    tilesize = atoi(argv[2]);

    a=(int*)malloc(sizeof(int)*num_iters);
    b=(int*)malloc(sizeof(int)*num_iters);
    c=(int*)malloc(sizeof(int)*num_iters);

  //Initialize the Values
    for(i=0; i<num_iters; i++){

	a[i]=0;
	b[i]=100;
	c[i]=1;

    }
   //Add the elements of arrays b and c and store them in a
   hupcpp::start_finish();
	 hupcpp::_loop_domain_t loop = {0, num_iters, 1, tilesize};
	 hupcpp::forasync1D(&loop, [=](int i) {
		a[i]=b[i]+c[i];
	 }, FORASYNC_MODE_RECURSIVE);
	 //}, FORASYNC_MODE_FLAT);
   hupcpp::end_finish();

   check(a,101,num_iters);
   printf("Test passed\n");
  hupcpp::finalize();
   return 0;
}
