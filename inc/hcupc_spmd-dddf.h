namespace hupcpp {

#define DDF_HANDLE(i)           	__ddfHandle(i, DDF_HOME(i), DDF_SIZE(i))
#define DDF_GET_HOME(ddf) 			__ddfGetHome(ddf)

/*
 * hclib-ddf.h also contains the exact enum values
 */
typedef enum DDDF_Kind {
	DDF_KIND_UNKNOWN=0,
	DDF_KIND_SHARED,
	DDF_KIND_DISTRIBUTED_OWNER,
	DDF_KIND_DISTRIBUTED_REMOTE,
} DDDF_Kind_t;

typedef struct _DDDF_t {
    DDF_t theDDF;
    int global_id;
    int home_rank; /* home is where the put is */
    int put_initiator_rank;
	int count;
	bool putRequestedFromHome;
	upcxx::shared_array<char>* datum;
} DDDF_t;

DDF_t* __ddfHandle(int gid, int hid, size_t count);
int __ddfGetHome(DDF_t* ddf);
void DDF_PUT(DDF_t* ddf, void* data);
void* DDF_GET(DDF_t* ddf);
void DDF_FREE(DDF_t* ddf);

}
