#include <iostream>
#include <random>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>

#define RANGE1 -900.0
#define RANGE2 900.0

std::random_device rd; 
std::mt19937 rng(rd());
std::uniform_real_distribution<float> uni(RANGE1, RANGE2);

float _rand()
{
	return uni(rng);
}

int main(int argc, char **argv) {
	int i ;
	float posx, posy, posz, mass;
	if(argc != 3) {
		printf("USAGE: ./a.out <NUM_BODIES> <output filename>\n");
		exit(0);
	}
	int numbodies = atoi(argv[1]);
	assert(numbodies > 1);
	FILE *fp = fopen(argv[2],"w");
	if(fp==NULL) {printf("Error opening file %s\n",argv[2]);exit(0);}
	
	srand(time(NULL));

	// 1. First write the posx as an 1D-array
	// reading from file is slow on edison for large number of bodies
	fprintf(fp, "const static float posx[NUMBODIES] = {");
	for(i=0; i<numbodies; i++) {
		fprintf(fp, "%.6f", _rand());
		if(i < numbodies-1) fprintf(fp, ", ");
	}
	fprintf(fp, "};\n");

	// 2. Write the posy as an 1D-array
	fprintf(fp, "const static float posy[NUMBODIES] = {");
	for(i=0; i<numbodies; i++) {
		fprintf(fp, "%.6f", _rand());
		if(i < numbodies-1) fprintf(fp, ", ");
	}
	fprintf(fp, "};\n");

	// 3. Write the posz as an 1D-array
	fprintf(fp, "const static float posz[NUMBODIES] = {");
	for(i=0; i<numbodies; i++) {
		fprintf(fp, "%.6f", _rand());
		if(i < numbodies-1) fprintf(fp, ", ");
	}
	fprintf(fp, "};\n");

	// 4. Write the mass as an 1D-array
	fprintf(fp, "const static float mass[NUMBODIES] = {");
	for(i=0; i<numbodies; i++) {
		fprintf(fp, "%.6f", fabs(_rand()));
		if(i < numbodies-1) fprintf(fp, ", ");
	}
	fprintf(fp, "};\n");

	fclose(fp);
	printf("Done\n");
	return 0;
}
