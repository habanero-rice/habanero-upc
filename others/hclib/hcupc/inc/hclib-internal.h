#define EMPTY_DATUM_ERROR_MSG "can not put sentinel value for \"uninitialized\" as a value into DDF"

// For 'headDDTWaitList' when a DDF has been satisfied
#define DDF_SATISFIED NULL

// Default value of a DDF datum
#define UNINITIALIZED_DDF_DATA_PTR NULL

// For waiting frontier (last element of the list)
#define UNINITIALIZED_DDF_WAITLIST_PTR ((struct ddt_st *) -1)

// struct ddf_st is the opaque we expose.
// We define a typedef in this unit for convenience
typedef struct ddf_st {
    void * datum;
    struct ddt_st * headDDTWaitList;
} ddf_t;

