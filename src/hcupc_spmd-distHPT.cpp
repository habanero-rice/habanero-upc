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
	 * c12-5c2s7n0 => column=12; row=5; chasis=2; blade=7; node=0
	 */
	int col;
	int row;
	int chasis;
	int blade;
	int rank;
} topology;

#define PLACEHOLDER_PLACE	-1
#define EDISON_NODES_PER_BLADE		4

typedef enum place_type {
	ROOT_PLACE,
	CABINET_GROUP_PLACE,
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

static place_t* root = NULL;
static place_t* myBladePlace = NULL;
static int* proximity;

/*
 * See: https://www.nersc.gov/users/computational-systems/edison/configuration/edison-cabinets/
 * In actual its 15 as one group  (C1R0 and C0R0) is missing.
 */
#define MAX_CABINET_GROUP_ON_EDISON		16
#define MAX_CABINET_GROUP_IN_ROW_ON_EDISON	4
#define MAX_CABINET_IN_ONE_GROUP_ON_EDISON	2
#define MAX_CHASIS_ON_A_CABINET			3


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
	place_t* cabinet_groups = root->child;
	printf("==Logical_Root==\n\n");
	while(cabinet_groups) {
		printf("Cabinet_Group_%d [\n",cabinet_groups->id);
		place_t* chasis = cabinet_groups->child;
		while(chasis) {
			printf("\tChasis_%d [\n",chasis->id);
			place_t* blades = chasis->child;
			while(blades) {
				printf("\t\tBlade_%d{",blades->id);
				for(int i=0; i<blades->nworkers; i++) {
					int rank = blades->workers[i];
					printf("%d",rank);
					if(i+1 <blades->nworkers) printf(", ");
				}
				blades = blades->next;
				printf("}\n ");	// end blade printf
			}
			chasis = chasis->next;
			printf("\t]\n");	// end chasis printf
		}
		cabinet_groups = cabinet_groups->next;
		printf("]\n");	// end row printf
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
		 * Try finding existing entry for this place,
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

inline int edison_cabinet_group_id(int col, int row) {
	/*
	 * See: https://www.nersc.gov/users/computational-systems/edison/configuration/edison-cabinets/
	 *
	 * a) There are total 15 group of cabinets. Each group contains two cabinets
	 *    which are at same hierarchy (e.g., C1R0 and C0R0)
	 * b) These 15 groups are arranged into total 4 rows (rows=0-3).
	 * c) But we consider a total of 16 cabinet groups to cover for the missing group id 12.
	 * d) The range of group id = 0 to 15, where group id 12 is missing.
	 */

	/*
	 * The groups can be assumed to be occupying a slot in 4x4 matrix,
	 * where the group id 12 is missing.
	 * Hence, no such cabinet exists: C0R3 and C1R3.
	 */
	if(row == 3) assert(col!=0 && col!=1);
	return ((MAX_CABINET_GROUP_IN_ROW_ON_EDISON * row) + ((int)(col/MAX_CABINET_IN_ONE_GROUP_ON_EDISON)));
}

inline int edison_chasis_index_in_cabinet_group(int col, int chasis) {
	/*
	 * a) Chasis are numbered as [0,2] in each cabinet.
	 * b) Each cabinet group in a row contains two cabinets, in which there are altogether 6 chasis.
	 * c) Each of these 6 chasises are at same hierarchy in tree.
	 * d) To allocate a chasis at cabinet group level, we consider the value of column
	 * e) There will be an odd and an even column id in each cabinet group.
	 * f) Chasis id on an odd column cabinet remains unchanged.
	 * g) Chasis id on an even column is added with MAX_CHASIS_ON_A_CABINET (=3)
	 */
	int chasis_new_id = (col%2==0) ? (chasis + MAX_CHASIS_ON_A_CABINET) : chasis;
	assert(chasis_new_id < (2*MAX_CHASIS_ON_A_CABINET));
	return chasis_new_id;
}

inline void add_rank_in_blade(place_t* blade, int rank, int myrank) {
	if((blade->nworkers+1) > EDISON_NODES_PER_BLADE) {
		printf("ERROR: Cabinet_%d, Chasis_%d, Blade_%d -- Extra node allocation for rank %d\n",blade->parent->parent->id, blade->parent->id, blade->id, rank);
		//print_edison_allocations();
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
		int chasis_i = edison_chasis_index_in_cabinet_group(col_i, place_i.chasis);
		int blade_i = place_i.blade;
		int rank_i = place_i.rank;

		int cabinet_gid = edison_cabinet_group_id(col_i, row_i);
		place_t* cabinet_group_place_i = add_place(&root, CABINET_GROUP_PLACE, cabinet_gid);
		place_t* chasis_place_i = add_place(&cabinet_group_place_i, CHASIS_PLACE, chasis_i);
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

/*
 * ====Tree traversal to find victim====
 */

void create_proximity_array(int places, int myrank) {
	proximity = new int[places-1];
	for(int i=0; i<places-1; i++) proximity[i] = -1;	//testing purpose

	int index = 0;

	place_t* blade = myBladePlace;

	// 1. Nodes on my blade are the first choice (PCIe-3 connection 16 bits at 8.0 GT/s)
	// 	This is the closest/fastest connection between nodes.
	add_ranks_inBlade_toProximityArray(blade, &index, myrank);

	// 2. Add nodes from blade on my left and then from blade on my right -> repeat this step until done
	// All of these nodes talk to an aries chip, through a backplane, to
	// a second aries chip and to the node. This is an all-to-all network.
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
	// There are 6 chasis in a cabinet group, each at same hierarchy
	// and this is also an all-to-all connection connecting aries in groups of 6.
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

	// 4. Go one level up in tree (cabinet). We consider all cabinet groups
	//    at same hierarchy.
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
}

void print_topology_information() {
	if(!root) return;
	cout << ">>>>>>>>>>>>>>>>>>>>>>>>>> EDISON NODE ALLOCATION DETAILS <<<<<<<<<<<<<<<<<<<<<<<<<<<" << endl;
	print_edison_allocations();
	cout << ">>>>>>>>>>>>>>>>>>>>>>>>>> EDISON NODE ALLOCATION DETAILS <<<<<<<<<<<<<<<<<<<<<<<<<<<" << endl;
}

void create_distributed_hpt(int row, int column, int chasis, int blade, int rank) {
	topology my_topology = {column, row, chasis, blade, rank};
	topology* global_topology = new topology[upcxx::global_ranks()];
	upcxx_allgather(&my_topology, global_topology, sizeof(topology));
	create_edison_topology(global_topology, upcxx::global_ranks(), rank);
}

int* create_arrray_of_nearestVictim() {
	assert(root);
	create_proximity_array(upcxx::global_ranks(), upcxx::global_myrank());
	assert(proximity);
	for(int i=0; i<upcxx::global_ranks()-1; i++) assert(proximity[i] != -1);
	return proximity;
}

}
