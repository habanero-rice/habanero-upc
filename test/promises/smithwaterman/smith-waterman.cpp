#include "hcupc_spmd.h"
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define GAP_PENALTY -1
#define TRANSITION_PENALTY -2
#define TRANSVERSION_PENALTY -4
#define MATCH 2

#define TRACE 0
#define debug 0

#define PROMISE_SIZE(guid) size_distribution_function( guid, tag_offset, inner_tile_width, inner_tile_height )
#define PROMISE_HOME(guid) row_cyclic_distribution_function (guid, tag_offset, j_domain, n_inner_tiles_height,nproc)

enum Nucleotide {GAP=0, ADENINE, CYTOSINE, GUANINE, THYMINE};

enum Corner {BOTTOM=0, RIGHT, DIAG};

signed char char_mapping ( char c ) {
	signed char to_be_returned = -1;
	switch(c) {
	case '_': to_be_returned = GAP; break;
	case 'A': to_be_returned = ADENINE; break;
	case 'C': to_be_returned = CYTOSINE; break;
	case 'G': to_be_returned = GUANINE; break;
	case 'T': to_be_returned = THYMINE; break;
	}
	return to_be_returned;
}

void print_matrix ( int** matrix, int n_rows, int n_columns ) {
	int i, j;
	for ( i = 0; i < n_rows; ++i ) {
		for ( j = 0; j < n_columns; ++j ) {
			printf( "%d ", matrix[i][j]);
		}
		printf( "\n");
	}
	printf("--------------------------------\n");
}

static char alignment_score_matrix[5][5] =
{
		{GAP_PENALTY,GAP_PENALTY,GAP_PENALTY,GAP_PENALTY,GAP_PENALTY},
		{GAP_PENALTY,MATCH,TRANSVERSION_PENALTY,TRANSITION_PENALTY,TRANSVERSION_PENALTY},
		{GAP_PENALTY,TRANSVERSION_PENALTY, MATCH,TRANSVERSION_PENALTY,TRANSITION_PENALTY},
		{GAP_PENALTY,TRANSITION_PENALTY,TRANSVERSION_PENALTY, MATCH,TRANSVERSION_PENALTY},
		{GAP_PENALTY,TRANSVERSION_PENALTY,TRANSITION_PENALTY,TRANSVERSION_PENALTY, MATCH}
};

size_t clear_whitespaces_do_mapping ( signed char* buffer, long size1 ) {
	size_t non_ws_index = 0, traverse_index = 0;

	while ( traverse_index < size1 ) {
		char curr_char = buffer[traverse_index];
		switch ( curr_char ) {
		case 'A': case 'C': case 'G': case 'T':
			/*this used to be a copy not also does mapping*/
			buffer[non_ws_index++] = char_mapping(curr_char);
			break;
		}
		++traverse_index;
	}
	return non_ws_index;
}

signed char* read_file( FILE* file, size_t* n_chars ) {
	fseek (file, 0L, SEEK_END);
	long file_size = ftell (file);
	fseek (file, 0L, SEEK_SET);

	signed char *file_buffer = (signed char *)malloc((1+file_size)*sizeof(signed char));

	size_t n_read_from_file = fread(file_buffer, sizeof(signed char), file_size, file);
	file_buffer[file_size] = '\n';

	/* shams' sample inputs have newlines in them */
	*n_chars = clear_whitespaces_do_mapping(file_buffer, file_size);
	return file_buffer;
}

typedef struct {
	hupcpp::promise_t* bottom_row;
	hupcpp::promise_t* right_column;
	hupcpp::promise_t* bottom_right;
} Tile_t;

static int size_distribution_function( int guid, int tag_offset, int tile_width, int tile_height ) {
	int left_above_diag = guid / tag_offset;
	int length;
	switch ( left_above_diag ) {
	case BOTTOM: // bottom_row
		length = tile_width;
		break;
	case RIGHT: // right_column
		length = tile_height;
		break;
	case DIAG: // bottom_right
		length = 1;
		break;
	}
	return sizeof(int)* length;
}

static int some_distribution_function ( int guid, int nproc,  int tag_offset) {
	guid =  guid % tag_offset;
	return guid % nproc;
}

static int column_cyclic_distribution_function ( int guid, int tag_offset, int j_domain, int n_inner_tiles_width, int nproc ) {
	guid =  guid % tag_offset;
	int j = guid % j_domain;
	//j = (j-1+n_inner_tiles_width)/n_inner_tiles_width;

	return j % nproc;
}

static int row_cyclic_distribution_function ( int guid, int tag_offset, int j_domain, int n_inner_tiles_height, int nproc ) {
	guid =  guid % tag_offset;
	int i = guid / j_domain;
	//i = (i-1)/n_inner_tiles_height;

	return i % nproc;
}

static int diagonal_cyclic_distribution_function ( int guid, int tag_offset, int j_domain, int n_inner_tiles_width, int n_inner_tiles_height, int n_tiles_width, int n_tiles_height, int nproc ) {
	guid =  guid % tag_offset;
	int i = guid / j_domain ;
	i = (i-1)/n_inner_tiles_height;
	int j = guid %  j_domain;
	j = (j-1)/n_inner_tiles_width;

	int n_left_complete_diagonals = -1;
	int n_complete_diagonals = -1;
	int n_right_complete_diagonals = -1;
	int index_in_diagonal = -1;

	int first_complete_diagonal_index = (n_tiles_width > n_tiles_height) ? n_tiles_height - 1: n_tiles_width - 1;
	int last_complete_diagonal_index = (n_tiles_width > n_tiles_height) ? n_tiles_width - 1 : n_tiles_height - 1;
	int diagonal_size = (n_tiles_width > n_tiles_height) ? n_tiles_height : n_tiles_width ;

	int on_diagonal = i+j;

	if ( on_diagonal <= first_complete_diagonal_index ) { // on the top left corner or the first diagonal
		n_left_complete_diagonals = i+j;
		n_complete_diagonals = 0;
		n_right_complete_diagonals = 0;
		index_in_diagonal = i;
	} else if ( on_diagonal <= last_complete_diagonal_index ) { // on the complete diagonals
		n_left_complete_diagonals = first_complete_diagonal_index;
		n_complete_diagonals = on_diagonal - first_complete_diagonal_index; // number of passed diagonals
		n_right_complete_diagonals = 0;
		index_in_diagonal = (n_tiles_width > n_tiles_height) ? i : i - n_complete_diagonals;
	} else {
		n_left_complete_diagonals = first_complete_diagonal_index;
		n_complete_diagonals = last_complete_diagonal_index - first_complete_diagonal_index + 1; // number of passed diagonals
		n_right_complete_diagonals = on_diagonal - last_complete_diagonal_index - 1;
		index_in_diagonal = (n_tiles_width > n_tiles_height) ? i - (on_diagonal - last_complete_diagonal_index) : i - (on_diagonal - first_complete_diagonal_index);
	}

	int offset_of_diagonal = ((n_left_complete_diagonals*(1+n_left_complete_diagonals))/2)
        														+ diagonal_size*n_complete_diagonals
        														+ ((diagonal_size-1)*diagonal_size)/2 - (((diagonal_size-n_right_complete_diagonals-1)*(diagonal_size-n_right_complete_diagonals))/2);

	return (offset_of_diagonal+index_in_diagonal) % nproc;
}

static int diagonal_block_cyclic_distribution_function ( int guid, int tag_offset, int j_domain, int n_inner_tiles_width, int n_inner_tiles_height,  int n_tiles_width, int n_tiles_height, int nproc ) {
	guid =  guid % tag_offset;
	int i = guid / j_domain ;
	i = (i-1)/n_inner_tiles_height;
	int j = guid %  j_domain;
	j = (j-1)/n_inner_tiles_width;

	int index_in_diagonal = -1;
	int curr_diag_size = -1;

	int first_complete_diagonal_index = (n_tiles_width > n_tiles_height) ? n_tiles_height - 1: n_tiles_width - 1;
	int last_complete_diagonal_index = (n_tiles_width > n_tiles_height) ? n_tiles_width - 1 : n_tiles_height - 1;
	int max_diagonal_size = (n_tiles_width > n_tiles_height) ? n_tiles_height : n_tiles_width ;

	int on_diagonal = i+j;

	if ( on_diagonal <= first_complete_diagonal_index ) { // on the top left corner or the first diagonal
		curr_diag_size = i+j+1;
		index_in_diagonal = i;
	} else if ( on_diagonal <= last_complete_diagonal_index ) { // on the complete diagonals
		curr_diag_size = max_diagonal_size;
		index_in_diagonal = (n_tiles_width > n_tiles_height) ? i : i - on_diagonal + first_complete_diagonal_index;
	} else {
		curr_diag_size = max_diagonal_size - (on_diagonal - last_complete_diagonal_index);
		index_in_diagonal = (n_tiles_width > n_tiles_height) ? i - (on_diagonal - last_complete_diagonal_index) : i - (on_diagonal - first_complete_diagonal_index);
	}

	int per_node = curr_diag_size / nproc;
	int extra = curr_diag_size - per_node*nproc;
	int speculated = 0;

	if (per_node) {
		speculated = -1;
		while ( index_in_diagonal >= 0 ) {
			index_in_diagonal -= per_node;
			if ( extra > 0 ) { --extra; --index_in_diagonal; }
			++speculated;
		}
	} else {
		speculated = index_in_diagonal;
	}
	return speculated;
}

int main ( int argc, char* argv[] ) {
    hupcpp::launch(&argc, &argv, [=]() {
        int rank = upcxx::global_myrank();
        int nproc = upcxx::global_ranks();

        fprintf(stderr, "rank=%d nproc=%d argc=%d\n", rank, nproc, argc);
        return;

        if ( argc < 7 ) {
            if (rank == 0) {
                printf("Usage: %s fileName1 fileName2 outerTileWidth outerTileHeight innerTileWidth innerTileHeight\n", argv[0]);
            }
            exit(-1);
        }


	    int i, j, index;
        if (TRACE) fprintf(stderr, "Inside main lambda\n");

	    int outer_tile_width = (int) atoi (argv[3]);
	    int outer_tile_height = (int) atoi (argv[4]);

	    int inner_tile_width = (int) atoi (argv[5]);
	    int inner_tile_height = (int) atoi (argv[6]);

	    int n_outer_tiles_width;
	    int n_outer_tiles_height;

	    int n_inner_tiles_width = outer_tile_width / inner_tile_width;
	    int n_inner_tiles_height = outer_tile_height / inner_tile_height;

	    if (rank == 0) {
	    	if ( outer_tile_width % inner_tile_width != 0 ) {
	    		printf("Outer tile width: %d, should be a multiple of inner tile width: %d", outer_tile_width, inner_tile_width);
	    		exit(-1);
	    	}

	    	if ( outer_tile_height % inner_tile_height != 0 ) {
	    		printf("Outer tile height: %d, should be a multiple of inner tile height: %d", outer_tile_height, inner_tile_height);
	    		exit(-1);
	    	}
	    }

        if (TRACE) fprintf(stderr, "Parsed CLI arguments\n");

	    signed char* string_1;
	    signed char* string_2;

	    int strlen_1;
	    int strlen_2;

	    if(rank == 0) {
	    	char* file_name_1 = argv[1];
	    	char* file_name_2 = argv[2];

	    	FILE* file_1 = fopen(file_name_1, "r");
	    	if (!file_1) { printf("could not open file %s\n",file_name_1); exit(-1); }
	    	size_t n_char_in_file_1 = 0;
	    	string_1 = read_file(file_1, &n_char_in_file_1);
	    	printf( "Size of input string 1 is %u\n", n_char_in_file_1 );

	    	FILE* file_2 = fopen(file_name_2, "r");
	    	if (!file_2) { printf("could not open file %s\n",file_name_2); exit(-1); }
	    	size_t n_char_in_file_2 = 0;
	    	string_2 = read_file(file_2, &n_char_in_file_2);
	    	printf( "Size of input string 2 is %u\n", n_char_in_file_2 );

	    	printf( "Outer tile width is %d\n", outer_tile_width);
	    	printf( "Outer tile height is %d\n", outer_tile_height);

	    	printf( "Inner tile width is %d\n", inner_tile_width);
	    	printf( "Inner tile height is %d\n", inner_tile_height);

	    	n_outer_tiles_width = n_char_in_file_1/outer_tile_width;
	    	n_outer_tiles_height = n_char_in_file_2/outer_tile_height;

	    	strlen_1 = n_char_in_file_1;
	    	strlen_2 = n_char_in_file_2;
	    }

	    printf("passed 1k\n");

	    if(rank==0) {
	    	fprintf(stderr," width:%d height:%d",n_outer_tiles_width,n_outer_tiles_height);
	    }

	    upcxx_bcast(&n_outer_tiles_width, &n_outer_tiles_width, sizeof(int), 0);
	    upcxx_bcast(&n_outer_tiles_height, &n_outer_tiles_height, sizeof(int), 0);

	    printf("passed 2\n");
	    upcxx_bcast(&strlen_1, &strlen_1, sizeof(int), 0);
	    upcxx_bcast(&strlen_2, &strlen_2, sizeof(int), 0);

	    printf("passed 3\n");

	    if ( rank != 0 ) {
	    	string_1 = (signed char*) malloc(strlen_1 * sizeof(signed char));
	    	string_2 = (signed char*) malloc(strlen_2 * sizeof(signed char));
	    }

	    printf("passed 4\n");

	    upcxx_bcast(string_1, string_1, sizeof(char)*strlen_1, 0);
	    upcxx_bcast(string_2, string_2, sizeof(char)*strlen_2, 0);

	    printf( "Imported %d x %d tiles.\n", n_outer_tiles_width, n_outer_tiles_height);

	    if ( rank == 0 ) {
	    	printf( "Allocating tile matrix\n");
	    }

	    int i_domain = 1+(n_outer_tiles_height)*n_inner_tiles_height;
	    int j_domain = 1+(n_outer_tiles_width)*n_inner_tiles_width;
	    int tag_offset = i_domain * j_domain;

	    // sagnak: all workers allocate their own copy of tile matrix
	    Tile_t** tile_matrix = (Tile_t **) malloc(sizeof(Tile_t*)*i_domain);
	    for ( i = 0; i < i_domain; ++i ) {
	    	tile_matrix[i] = (Tile_t *) malloc(sizeof(Tile_t)*j_domain);
	    	for ( j = 0; j < j_domain; ++j ) {
                // HANDLE should allocate on owner, return a local copy others
	    		tile_matrix[i][j].bottom_row   = hupcpp::PROMISE_HANDLE(
                        BOTTOM * tag_offset + i*j_domain + j ); 
	    		tile_matrix[i][j].right_column = hupcpp::PROMISE_HANDLE(
                        RIGHT * tag_offset + i*j_domain + j );
	    		tile_matrix[i][j].bottom_right = hupcpp::PROMISE_HANDLE(
                        DIAG * tag_offset + i*j_domain + j );
	    		assert( sizeof(int)*1 == ((hupcpp::dpromise_t*)tile_matrix[i][j].bottom_right)->count);
	    		assert( sizeof(int)*inner_tile_width == ((hupcpp::dpromise_t*)tile_matrix[i][j].bottom_row)->count );
	    		assert( sizeof(int)*inner_tile_height == ((hupcpp::dpromise_t*)tile_matrix[i][j].right_column)->count );
	    	}
	    }

	    if ( rank == 0 ) {
	    	printf( "Allocated tile matrix\n");
	    }

	    if (debug) fprintf(stderr, "i_domain: %d j_domain: %d\n", i_domain, j_domain);
	    i = 0;
	    j = 0;
	    if ( rank == PROMISE_HOME(i*j_domain + j) ) {
	    	if (debug) fprintf(stderr, "rank:%d put [%d][%d]\n", rank,i,j);
	    	int* allocated = (int*)malloc(sizeof(int));
	    	allocated[0] = 0;
	    	if(TRACE) fprintf(stderr, "Put item [tile_matrix: [%d, %d, %d]]\n",DIAG, i,j);
	    	hupcpp::PROMISE_PUT(tile_matrix[i][j].bottom_right, allocated);
	    	assert( rank == ((hupcpp::dpromise_t*)tile_matrix[i][j].bottom_right)->home_rank );
	    	assert( sizeof(int)*1 == ((hupcpp::dpromise_t*)tile_matrix[i][j].bottom_right)->count);
	    }

	    i = 0;
	    for ( j = 1; j < j_domain; ++j ) {
	    	if ( rank == PROMISE_HOME(i*j_domain + j) )  {
	    		if (debug) fprintf(stderr, "j-loop rank:%d put [%d][%d]\n", rank,i,j);
	    		int* allocated = (int*)malloc(sizeof(int)*inner_tile_width);
	    		for( index = 0; index < inner_tile_width; ++index ) {
	    			allocated[index] = GAP_PENALTY*((j-1)*inner_tile_width+index+1);
	    		}
	    		if(TRACE) fprintf(stderr, "Put item [tile_matrix: [%d, %d, %d]]\n",BOTTOM,i,j);
	    		hupcpp::PROMISE_PUT(tile_matrix[i][j].bottom_row, allocated);
	    		assert( rank == ((hupcpp::dpromise_t*)tile_matrix[i][j].bottom_row)->home_rank );
	    		assert( sizeof(int)*inner_tile_width == ((hupcpp::dpromise_t*)tile_matrix[i][j].bottom_row)->count );
	    	}

	    	if ( rank == PROMISE_HOME(i*j_domain + j) )  {
	    		if (debug) fprintf(stderr, "j-loop rank:%d put [%d][%d]\n", rank,i,j);
	    		int* allocated = (int*)malloc(sizeof(int));
	    		allocated[0] = GAP_PENALTY*(j*inner_tile_width);
	    		if(TRACE) fprintf(stderr, "Put item [tile_matrix: [%d, %d, %d]]\n",DIAG,i,j);
	    		hupcpp::PROMISE_PUT(tile_matrix[i][j].bottom_right, allocated);
	    		assert( rank == ((hupcpp::dpromise_t*)tile_matrix[i][j].bottom_right)->home_rank );
	    		assert( sizeof(int)*1 == ((hupcpp::dpromise_t*)tile_matrix[i][j].bottom_right)->count);
	    	}
	    }

	    j = 0;
	    for ( i = 1; i < i_domain; ++i ) {
	    	if ( rank == PROMISE_HOME(i*j_domain + j) ) {
	    		if (debug) fprintf(stderr, "i-loop rank:%d put [%d][%d]\n", rank,i,j);
	    		int* allocated = (int*)malloc(sizeof(int)*inner_tile_height);
	    		for ( index = 0; index < inner_tile_height ; ++index ) {
	    			allocated[index] = GAP_PENALTY*((i-1)*inner_tile_height+index+1);
	    		}
	    		if(TRACE) fprintf(stderr, "Put item [tile_matrix: [%d, %d, %d]]\n",RIGHT,i,j);
	    		hupcpp::PROMISE_PUT(tile_matrix[i][j].right_column, allocated);
	    		if (debug) fprintf(stderr, "i-loop rank:%d home_rank:%d \n", rank, ((hupcpp::dpromise_t*)tile_matrix[i][j].right_column)->home_rank);
	    		assert( rank == ((hupcpp::dpromise_t*)tile_matrix[i][j].right_column)->home_rank );
	    		assert( sizeof(int)*inner_tile_height == ((hupcpp::dpromise_t*)tile_matrix[i][j].right_column)->count );
	    	}

	    	if ( rank == PROMISE_HOME(i*j_domain + j) )  {
	    		if (debug) fprintf(stderr, "i-loop rank:%d put [%d][%d]\n", rank,i,j);
	    		int* allocated = (int*)malloc(sizeof(int));
	    		allocated[0] = GAP_PENALTY*(i*inner_tile_height);
	    		if(TRACE) fprintf(stderr, "Put item [tile_matrix: [%d, %d, %d]]\n",DIAG, i,j);
	    		hupcpp::PROMISE_PUT(tile_matrix[i][j].bottom_right, allocated);
	    		assert( rank == ((hupcpp::dpromise_t*)tile_matrix[i][j].bottom_right)->home_rank );
	    		assert( sizeof(int)*1 == ((hupcpp::dpromise_t*)tile_matrix[i][j].bottom_right)->count );
	    	}
	    }

	    if (debug) fprintf(stderr, "before finish_spmd\n");

	    struct timeval begin,end;
	    if ( rank == 0 ) {
	    	gettimeofday(&begin,0);
	    }

	    hupcpp::finish_spmd([=]() {
	    	for (int i = 1; i < i_domain; ++i ) {
	    		for (int j = 1; j < j_domain; ++j ) {
	    			if ( rank == PROMISE_HOME(i*j_domain+j) ) {
	    				if (debug) fprintf(stderr, "before rank:%d entered [%d][%d]\n", rank ,i,j);
	    				if (debug) fprintf(stderr, "[%p][%p][%p]\n", tile_matrix[i][j-1].right_column,tile_matrix[i-1][j].bottom_row,tile_matrix[i-1][j-1].bottom_right );
	    				hupcpp::async_await([=]() {
	    					if(TRACE) fprintf(stderr, "Start step (Compute: (%d,%d))\n",i,j);
	    					if (debug) fprintf(stderr, "rank:%d entered [%d][%d]\n", rank,i,j);
	    					if (debug) fprintf(stderr, "in async [%p][%p][%p]\n", tile_matrix[i][j-1].right_column,tile_matrix[i-1][j].bottom_row,tile_matrix[i-1][j-1].bottom_right );

	    					if (debug) fprintf(stderr,  "rank:%d, milestone 1\n",rank);
	    					int index, ii, jj;
	    					if(TRACE) fprintf(stderr, "Get item [tile_matrix: [%d, %d, %d]]\n",BOTTOM,i-1,j);
	    					if(TRACE) fprintf(stderr, "Get item [tile_matrix: [%d, %d, %d]]\n",RIGHT,i,j-1);
	    					if(TRACE) fprintf(stderr, "Get item [tile_matrix: [%d, %d, %d]]\n",DIAG,i-1,j-1);
	    					int* above_tile_bottom_row = (int*)hupcpp::FUTURE_GET(tile_matrix[i-1][j].bottom_row->get_future());
	    					int* left_tile_right_column = (int*)hupcpp::FUTURE_GET(tile_matrix[i][j-1].right_column->get_future());
	    					int* diagonal_tile_bottom_right = (int*)hupcpp::FUTURE_GET(tile_matrix[i-1][j-1].bottom_right->get_future());

	    					if (debug) fprintf(stderr,  "rank:%d, milestone 2\n",rank);
	    					int  * curr_tile_tmp = (int*) malloc(sizeof(int)*(1+inner_tile_width)*(1+inner_tile_height));
	    					int ** curr_tile = (int**) malloc(sizeof(int*)*(1+inner_tile_height));
	    					for (index = 0; index < inner_tile_height+1; ++index) {
	    						curr_tile[index] = &curr_tile_tmp[index*(1+inner_tile_width)];
	    					}

	    					if (debug) fprintf(stderr,  "rank:%d, milestone 3\n",rank);
	    					curr_tile[0][0] = diagonal_tile_bottom_right[0];
	    					for ( index = 1; index < inner_tile_height+1; ++index ) {
	    						curr_tile[index][0] = left_tile_right_column[index-1];
	    					}

	    					if (debug) fprintf(stderr,  "rank:%d, milestone 4\n",rank);
	    					for ( index = 1; index < inner_tile_width+1; ++index ) {
	    						curr_tile[0][index] = above_tile_bottom_row[index-1];
	    					}

	    					if (debug) fprintf(stderr,  "rank:%d, milestone 5\n",rank);
	    					for ( ii = 1; ii < inner_tile_height+1; ++ii ) {
	    						for ( jj = 1; jj < inner_tile_width+1; ++jj ) {
	    							signed char char_from_1 = string_1[(j-1)*inner_tile_width+(jj-1)];
	    							signed char char_from_2 = string_2[(i-1)*inner_tile_height+(ii-1)];

	    							int diag_score = curr_tile[ii-1][jj-1] + alignment_score_matrix[char_from_2][char_from_1];
	    							int left_score = curr_tile[ii  ][jj-1] + alignment_score_matrix[char_from_1][GAP];
	    							int  top_score = curr_tile[ii-1][jj  ] + alignment_score_matrix[GAP][char_from_2];

	    							int bigger_of_left_top = (left_score > top_score) ? left_score : top_score;
	    							curr_tile[ii][jj] = (bigger_of_left_top > diag_score) ? bigger_of_left_top : diag_score;
	    						}
	    					}

	    					int* curr_bottom_right = (int*)malloc(sizeof(int));
	    					curr_bottom_right[0] = curr_tile[inner_tile_height][inner_tile_width];
	    					if(TRACE) fprintf(stderr, "Put item [tile_matrix: [%d, %d, %d]]\n",DIAG, i,j);
	    					hupcpp::PROMISE_PUT(tile_matrix[i][j].bottom_right, curr_bottom_right);
	    					assert( rank == ((hupcpp::dpromise_t*)tile_matrix[i][j].bottom_right)->home_rank );
	    					assert( sizeof(int)*1 == ((hupcpp::dpromise_t*)tile_matrix[i][j].bottom_right)->count );

	    					int* curr_right_column = (int*)malloc(sizeof(int)*inner_tile_height);
	    					for ( index = 0; index < inner_tile_height; ++index ) {
	    						curr_right_column[index] = curr_tile[index+1][inner_tile_width];
	    					}
	    					if(TRACE) fprintf(stderr, "Put item [tile_matrix: [%d, %d, %d]]\n",RIGHT,i,j);
	    					hupcpp::PROMISE_PUT(tile_matrix[i][j].right_column, curr_right_column);
	    					assert( rank == ((hupcpp::dpromise_t*)tile_matrix[i][j].right_column)->home_rank );
	    					assert( sizeof(int)*inner_tile_height == ((hupcpp::dpromise_t*)tile_matrix[i][j].right_column)->count );

	    					int* curr_bottom_row = (int*)malloc(sizeof(int)*inner_tile_width);
	    					for ( index = 0; index < inner_tile_width; ++index ) {
	    						curr_bottom_row[index] = curr_tile[inner_tile_height][index+1];
	    					}
	    					if(TRACE) fprintf(stderr, "Put item [tile_matrix: [%d, %d, %d]]\n",BOTTOM,i,j);
	    					hupcpp::PROMISE_PUT(tile_matrix[i][j].bottom_row, curr_bottom_row);
	    					assert( rank == ((hupcpp::dpromise_t*)tile_matrix[i][j].bottom_row)->home_rank );
	    					assert( sizeof(int)*inner_tile_width == ((hupcpp::dpromise_t*)tile_matrix[i][j].bottom_row)->count );

	    					free(curr_tile);
	    					free(curr_tile_tmp);
	    					if (debug) fprintf(stderr, "rank:%d exited [%d][%d]\n",rank,i,j);
	    					if(TRACE) fprintf(stderr, "End step (Compute: (%d,%d))\n",i,j);
	    				}, tile_matrix[i][j-1].right_column->get_future(),
                            tile_matrix[i-1][j].bottom_row->get_future(),
                            tile_matrix[i-1][j-1].bottom_right->get_future());
	    			};
	    		}
	    	}
	    });

	    if ( rank == 0 ) {
	    	gettimeofday(&end,0);
	    	printf( "The computation took %f seconds\n",((end.tv_sec - begin.tv_sec)*1000000+(end.tv_usec - begin.tv_usec))*1.0/1000000);

	    }

	    if ( rank == PROMISE_HOME(j_domain-1+j_domain*(i_domain-1)) ) {
	    	int score = ((int *)hupcpp::FUTURE_GET(tile_matrix[i_domain-1][j_domain-1].bottom_row->get_future()))[inner_tile_width-1];
	    	printf( "score: %d\n", score);
	    }
    });

	return 0;
}
