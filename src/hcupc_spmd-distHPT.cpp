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

namespace hupcpp {


typedef struct topology {
	/*
	 * c12-5c2s7n0 => column=12; row=5; chasis=2; blade=7; rank=0
	 */
	int	col;
	int row;
	int chasis;
	int blade;
	int rank;
} topology;

#define PLACEHOLDER_PLACE	-1
#define EDISON_NODES_PER_BLADE		4

typedef enum place_type {
	ROOT_PLACE,
	ROW_PLACE,
	CABINET_PLACE,
	CHASIS_PLACE,
	BLADE_PLACE,
	NODE_PLACE,
	NUM_OF_TYPES,
} place_type_t;

struct place_t;

typedef struct place_t {
	struct place_t * parent;
	struct place_t * child; /* the first child */
	struct place_t * next; /* the sibling link of the HPT */
	struct place_t * prev;
	int * workers; /* only for blades -- HCUPC++ ranks attached to this blade */
	int nworkers; /* only for blades */
	int id;
	short type;
} place_t ;

typedef struct cabinetLoad {
	place_t* cabinet;
	int threads;
} cabinetLoad;

static place_t* root = NULL;
static place_t* myBladePlace = NULL;

static cabinetLoad* cabinetLoad_array = NULL;
static int total_cabinets = 0;

static int* proximity;

void calculate_threads_distribution() {
	int index = 0;

	place_t* row = root->child;
	while(row) {
		place_t* cabinet = row->child;
		while(cabinet) {
			// allocate
			if(!cabinetLoad_array) {
				cabinetLoad_array = (cabinetLoad*) malloc(sizeof(cabinetLoad));
			}
			else {
				cabinetLoad_array = (cabinetLoad*) realloc(cabinetLoad_array, sizeof(cabinetLoad)*(index+1));
			}

			// populate cabinet
			cabinetLoad_array[index].cabinet = cabinet;

			place_t* chasis = cabinet->child;
			int threads = 0;
			while(chasis) {
				place_t* blade = chasis->child;
				while(blade) {
					threads += blade->nworkers;
					blade = blade->next;
				}
				chasis = chasis->next;
			}
			// populate cabinetLoad_array with threads at this cabinet
			cabinetLoad_array[index].threads = threads;

			// move ahead
			cabinet = cabinet->next;
			index++;
		}
		row = row->next;
	}
	total_cabinets = index;
}

void print_cabinet_load() {
	calculate_threads_distribution();
	printf("Total cabinets = %d\n",total_cabinets);
	for(int i=0; i<total_cabinets; i++) {
		cabinetLoad cabinet = cabinetLoad_array[i];
		int rowID = cabinet.cabinet->parent->id;
		int cabinetID = cabinet.cabinet->id;
		int threads = cabinet.threads;
		printf("[%d, %d] = %d\n",rowID, cabinetID, threads);
	}
}

inline place_t* get_new_place(short type, int id) {
	place_t* p = new place_t;
	assert(p);
	p->parent = NULL;
	p->child = NULL;
	p->next = NULL;
	p->prev = NULL;
	p->nworkers = 0;
	p->id = id;
	p->type = type;
	return p;
}

void print_edison_allocations() {
	place_t* rows = root->child;
	printf("==Logical_Root==\n\n");
	while(rows) {
		printf("Row_%d [\n",rows->id);
		place_t* cabinets = rows->child;
		while(cabinets) {
			printf("\t\tCabinet_%d [\n",cabinets->id);
			place_t* chasises = cabinets->child;
			while(chasises) {
				printf("\t\t\t\tChasis_%d [ ",chasises->id);
				place_t* blades = chasises->child;
				while(blades) {
					printf("Blade_%d{",blades->id);
					for(int i=0; i<blades->nworkers; i++) {
						int rank = blades->workers[i];
						printf("%d",rank);
						if(i+1 <blades->nworkers) printf(", ");
					}
					blades = blades->next;
					printf("} ");	// end blade printf
				}
				chasises = chasises->next;
				printf("]\n");	// end chasis printf
			}
			cabinets = cabinets->next;
			printf("\t\t]\n");	// end cabinet printf
		}
		rows = rows->next;
		printf("]\n");	// end row printf
	}
}

void print_my_place(int myrank) {
	printf("MyPosition(%d): Row=%d, Cabinet=%d, Chasis=%d, Blade=%d\n",myrank, myBladePlace->parent->parent->parent->id, myBladePlace->parent->parent->id, myBladePlace->parent->id, myBladePlace->id);
}

void print_topology_information() {
	if(root && proximity) {
		cout << ">>>>>>>>>>>>>>>>>>>>>>>>>> EDISON NODE ALLOCATION DETAILS <<<<<<<<<<<<<<<<<<<<<<<<<<<" << endl;
		print_edison_allocations();
		print_my_place(MYTHREAD);
		print_cabinet_load();
		cout << ">>>>>>>>>>>>>>>>>>>>>>>>>> EDISON NODE ALLOCATION DETAILS <<<<<<<<<<<<<<<<<<<<<<<<<<<" << endl;
	}
}

/*
 * Always returns the leftmost child in the current
 * level of tree (the level on this node is added).
 *
 * This method can be used on any level of the tree
 *
 * 1. Find if there is an entry already present
 * 2. If not found create one and add in sorted list
 */
inline void sortedInsertion(place_t** curr_head, place_t** node, int id) {
	place_t* head = (*curr_head)->child;

	if (head->id == id) {
		// node already present
		*node = head;
		return;
	}

	*node = get_new_place(PLACEHOLDER_PLACE, id);
	assert(*node);

	if (head->id > id) {
		(*node)->next = head;
		head->prev = *node;
		(*curr_head)->child = *node;
	}
	else {
		/* locate the node */
		bool found = false;
		while (head->next) {
			if(head->next->id > id) {
				break;
			}
			else if(head->next->id < id) {
				head = head->next;
			}
			else {
				found = true;
				break;
			}
		}

		if(!found) {
			(*node)->next = head->next;
			if(head->next) {
				head->next->prev = *node;
			}
			(*node)->prev = head;
			head->next = (*node);
		}
		else {
			*node = head->next;
		}
	}
}

inline place_t* add_place(place_t** head, short type, int id) {
	place_t* node = NULL;
	if((*head)->child == NULL) {
		node = get_new_place(type, id);
		(*head)->child = node;
	}
	else {
		/*
		 * Try finding existing entry for this cabinet,
		 * if no entry found, add an entry and return
		 * the left most place at this level
		 */
		sortedInsertion(head, &node, id);
		assert(node);
		node->type = type;
	}

	node->parent = (*head);	//logical root is always the parent of cabinet place
	return node;
}

inline void add_rank_in_blade(place_t* blade, int rank, int myrank) {
	if((blade->nworkers+1) > EDISON_NODES_PER_BLADE) {
		printf("ERROR: Cabinet_%d, Chasis_%d, Blade_%d -- Extra node allocation for rank %d\n",blade->parent->parent->id, blade->parent->id, blade->id, rank);
		print_edison_allocations();
		assert("More than 4 nodes in blades" && 0);
	}
	if(blade->nworkers == 0) {
		blade->workers = (int*) malloc(sizeof(int) * 1);
	}
	else {
		blade->workers = (int*) realloc(blade->workers, sizeof(int) * (blade->nworkers+1));
	}
	blade->workers[blade->nworkers++] = rank;
	if(rank == myrank) {
		assert(myBladePlace==NULL);
		myBladePlace = blade;
	}
}

void create_edison_topology(topology* place_coords, int places, int myrank) {
	// 1. create logical root
	root = get_new_place(ROOT_PLACE, 0);

	// 2. iterate the topology array
	for(int i=0; i<places; i++) {
		topology place_i = place_coords[i];
		int col_i = place_i.col;
		int row_i = place_i.row;
		int chasis_i = place_i.chasis;
		int blade_i = place_i.blade;
		int rank_i = place_i.rank;

		place_t* row_place_i = add_place(&root, ROW_PLACE, row_i);
		place_t* cabinet_place_i = add_place(&row_place_i, CABINET_PLACE, col_i);
		place_t* chasis_place_i = add_place(&cabinet_place_i, CHASIS_PLACE, chasis_i);
		place_t* blade_place_i = add_place(&chasis_place_i, BLADE_PLACE, blade_i);
		add_rank_in_blade(blade_place_i, rank_i, myrank);
	}
}

inline void add_ranks_inBlade_toProximityArray(place_t* blade, int* curr_index, int myrank=-1) {
	for(int i=0; i<blade->nworkers; i++) {
		int rank = blade->workers[i];
		if(rank != myrank)	proximity[(*curr_index)++] = rank;
	}
}

inline void add_blades_inChasis_toProximityArray(place_t* chasis, int* curr_index) {
	// go one level down and iterate over all the blades in this chasis
	place_t* blade_i = chasis->child;
	while(blade_i) {
		// go one level down and add all ranks running at this blade
		add_ranks_inBlade_toProximityArray(blade_i, curr_index);
		blade_i = blade_i->next;
	}
}

inline void add_chasis_inCabinets_toProximityArray(place_t* cabinet, int* curr_index, int id=-1) {
	place_t* chasis = cabinet->child;
	while(chasis) {
		// go one level down and iterate over all the blades in this chasis
		if(chasis->id != id) add_blades_inChasis_toProximityArray(chasis, curr_index);
		chasis = chasis->next;
	}
}

inline void add_cabinets_inRows_toProximityArray(place_t* row, int* curr_index) {
	place_t* cabinet = row->child;
	while(cabinet) {
		// go one level down and iterate over all the blades in this chasis
		add_chasis_inCabinets_toProximityArray(cabinet, curr_index);
		cabinet = cabinet->next;
	}
}

place_t* get_place_of_rank(int rank) {
	place_t* row = root->child;
	while(row) {
		place_t* cabinet = row->child;
		while(cabinet) {
			place_t* chasis = cabinet->child;
			while(chasis) {
				place_t* blade = chasis->child;
				while(blade) {
					for(int i=0; i<blade->nworkers; i++) {
						if(blade->workers[i] == rank) return blade;
					}
					blade = blade->next;
				}
				chasis = chasis->next;
			}
			cabinet = cabinet->next;
		}
		row = row->next;
	}
	return NULL;
}

/*
 * ====Tree traversal to find victim====
 */

void create_proximity_array(int places, int myrank) {
	proximity = new int[places-1];
	for(int i=0; i<places-1; i++) proximity[i] = -1;	//testing purpose

	int index = 0;

	place_t* blade = myBladePlace;

	// 1. Nodes on my blade are the first choice (PCIe-3 connection 16 bits at 8.0 GT/s)
	add_ranks_inBlade_toProximityArray(blade, &index, myrank);

	// 2. Add nodes from blade on my left and then from blade on my right -> repeat this step until done
	place_t* blade_left = myBladePlace->prev;
	place_t* blade_right = myBladePlace->next;

	while(blade_left || blade_right) {
		// 2a. first add the one on my left
		if(blade_left) {
			add_ranks_inBlade_toProximityArray(blade_left, &index);
			blade_left = blade_left->prev;
		}

		// 2b. now add the one on my right
		if(blade_right) {
			add_ranks_inBlade_toProximityArray(blade_right, &index);
			blade_right = blade_right->next;
		}
	}

	// 3. Go one level up in tree
	// 3a. add chasis on my left and then the chasis on my right
	place_t* chasis_left = myBladePlace->parent->prev;
	place_t* chasis_right = myBladePlace->parent->next;

	while(chasis_left || chasis_right) {
		// 2a. first add the one on my left
		if(chasis_left) {
			// add all blades in this chasis
			add_blades_inChasis_toProximityArray(chasis_left, &index);
			chasis_left = chasis_left->prev;
		}

		// 2b. now add the one on my right
		if(chasis_right) {
			// add all blades in this chasis
			add_blades_inChasis_toProximityArray(chasis_right, &index);
			chasis_right = chasis_right->next;
		}
	}

	// 4. Go one level up in tree (cabinet)
	// 4a. add cabinet on my left and then the cabinet on my right
	place_t* cabinet_left = myBladePlace->parent->parent->prev;
	place_t* cabinet_right = myBladePlace->parent->parent->next;

	while(cabinet_left || cabinet_right) {
		// 4c. first add the one on my left
		if(cabinet_left) {
			// iterate all the chasis in this cabinet
			add_chasis_inCabinets_toProximityArray(cabinet_left, &index);
			cabinet_left = cabinet_left->prev;
		}

		// 4d. now add the one on my right
		if(cabinet_right) {
			// iterate all the chasis in this cabinet
			add_chasis_inCabinets_toProximityArray(cabinet_right, &index);
			cabinet_right = cabinet_right->next;
		}
	}

	// 5. Go one level up in the tree (rows)
	// 5a. add rows on my left and then the rows on my right
	place_t* row_left = myBladePlace->parent->parent->parent->prev;
	place_t* row_right = myBladePlace->parent->parent->parent->next;

	while(row_left || row_right) {
		// 5b. first add the one on my left
		if(row_left) {
			// iterate all the chasis in this cabinet
			add_cabinets_inRows_toProximityArray(row_left, &index);
			row_left = row_left->prev;
		}

		// 5c. now add the one on my right
		if(row_right) {
			// iterate all the chasis in this cabinet
			add_cabinets_inRows_toProximityArray(row_right, &index);
			row_right = row_right->next;
		}
	}
}

void create_distributed_hpt(int row, int column, int chasis, int blade, int rank) {
	topology my_topology = {column, row, chasis, blade, rank};
	topology* global_topology = new topology[THREADS - 1];
	upcxx_allgather(&my_topology, global_topology, sizeof(topology));

	create_edison_topology(global_topology, THREADS, rank);
}

int* create_arrray_of_nearestVictim() {
	assert(root);
	create_proximity_array(THREADS, MYTHREAD);
	assert(proximity);
	for(int i=0; i<THREADS-1; i++) assert(proximity[i] != -1);
	return proximity;
}

}
