#include "lulesh.h" // intel mpiicpc likes include mpi.h first
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>


#ifdef VIZ_MESH

#ifdef __cplusplus
  extern "C" {
#endif
#include "silo.h"
#if USE_MPI
# include "pmpio.h"
#endif
#ifdef __cplusplus
  }
#endif

// Function prototypes
static void 
DumpDomainToVisit(DBfile *db, Domain& domain, int myRank);
static


#if USE_MPI
// For some reason, earlier versions of g++ (e.g. 4.2) won't let me
// put the 'static' qualifier on this prototype, even if it's done
// consistently in the prototype and definition
void
DumpMultiblockObjects(DBfile *db, PMPIO_baton_t *bat, 
                      char basename[], int numRanks);

// Callback prototypes for PMPIO interface (only useful if we're
// running parallel)
static void *
LULESH_PMPIO_Create(const char *fname,
		     const char *dname,
		     void *udata);
static void *
LULESH_PMPIO_Open(const char *fname,
		   const char *dname,
		   PMPIO_iomode_t ioMode,
		   void *udata);
static void
LULESH_PMPIO_Close(void *file, void *udata);

#else
void
DumpMultiblockObjects(DBfile *db, char basename[], int numRanks);
#endif


/**********************************************************************/
void DumpToVisit(Domain& domain, int numFiles, int myRank, int numRanks) 
{
  char subdirName[32];
  char basename[32];
  DBfile *db;


  sprintf(basename, "lulesh_plot_c%d", domain.cycle());
  sprintf(subdirName, "data_%d", myRank);

#if USE_MPI

  PMPIO_baton_t *bat = PMPIO_Init(numFiles,
				  PMPIO_WRITE,
				  MPI_COMM_WORLD,
				  10101,
				  LULESH_PMPIO_Create,
				  LULESH_PMPIO_Open,
				  LULESH_PMPIO_Close,
				  NULL);

  int myiorank = PMPIO_GroupRank(bat, myRank);

  char fileName[64];
  
  if (myiorank == 0) 
    strcpy(fileName, basename);
  else
    sprintf(fileName, "%s.%03d", basename, myiorank);

  db = (DBfile*)PMPIO_WaitForBaton(bat, fileName, subdirName);

  DumpDomainToVisit(db, domain, myRank);

  // Processor 0 writes out bit of extra data to its file that
  // describes how to stitch all the pieces together
  if (myRank == 0) {
    DumpMultiblockObjects(db, bat, basename, numRanks);
  }

  PMPIO_HandOffBaton(bat, db);

  PMPIO_Finish(bat);
#else

  db = (DBfile*)DBCreate(basename, DB_CLOBBER, DB_LOCAL, NULL, DB_HDF5X);

  if (db) {
     DBMkDir(db, subdirName);
     DBSetDir(db, subdirName);
     DumpDomainToVisit(db, domain, myRank);
     DumpMultiblockObjects(db, basename, numRanks);
  }
  else {
     printf("Error writing out viz file - rank %d\n", myRank);
  }

#endif
}



/**********************************************************************/

static void 
DumpDomainToVisit(DBfile *db, Domain& domain, int myRank)
{
   int ok = 0;
   
   /* Create an option list that will give some hints to VisIt for
    * printing out the cycle and time in the annotations */
   DBoptlist *optlist;


   /* Write out the mesh connectivity in fully unstructured format */
   int shapetype[1] = {DB_ZONETYPE_HEX};
   int shapesize[1] = {8};
   int shapecnt[1] = {domain.numElem()};
   ok += DBPutZonelist2(db, "connectivity", domain.numElem(), 3,
                        domain.nodelist(), domain.numElem()*8,
                        0,0,0, /* Not carrying ghost zones */
                        shapetype, shapesize, shapecnt,
                        1, NULL);

   /* Write out the mesh coordinates associated with the mesh */
   const char* coordnames[3] = {"X", "Y", "Z"};
   void* coords[3] = {domain.x(), domain.y(), domain.z()};
   optlist = DBMakeOptlist(2);
   ok += DBAddOption(optlist, DBOPT_DTIME, &domain.time());
   ok += DBAddOption(optlist, DBOPT_CYCLE, &domain.cycle());
   ok += DBPutUcdmesh(db, "mesh", 3, (char**)&coordnames[0], (float**)coords,
                      domain.numNode(), domain.numElem(), "connectivity",
                      0, DB_DOUBLE, optlist);
   ok += DBFreeOptlist(optlist);

   /* Write out the materials */
   int *matnums = new int[domain.numReg()];
   int dims[1] = {domain.numElem()}; // No mixed elements
   for(int i=0 ; i<domain.numReg() ; ++i)
      matnums[i] = i+1;
   
   ok += DBPutMaterial(db, "regions", "mesh", domain.numReg(),
                       matnums, domain.regNumList(), dims, 1,
                       NULL, NULL, NULL, NULL, 0, DB_DOUBLE, NULL);
   delete [] matnums;

   /* Write out pressure, energy, relvol, q */
   ok += DBPutUcdvar1(db, "e", "mesh", (float*)domain.e(),
                      domain.numElem(), NULL, 0, DB_DOUBLE, DB_ZONECENT,
                      NULL);
   ok += DBPutUcdvar1(db, "p", "mesh", (float*)domain.p(),
                      domain.numElem(), NULL, 0, DB_DOUBLE, DB_ZONECENT,
                      NULL);
   ok += DBPutUcdvar1(db, "v", "mesh", (float*)domain.v(),
                      domain.numElem(), NULL, 0, DB_DOUBLE, DB_ZONECENT,
                      NULL);
   ok += DBPutUcdvar1(db, "q", "mesh", (float*)domain.q(),
                      domain.numElem(), NULL, 0, DB_DOUBLE, DB_ZONECENT,
                      NULL);

   /* Write out nodal speed, velocities */
   Real_t *speed = new double[domain.numNode()];
   for(int i=0 ; i<domain.numNode() ; ++i) {
      Real_t xd = domain.xd(i);
      Real_t yd = domain.yd(i);
      Real_t zd = domain.zd(i);
      speed[i] = sqrt((xd*xd)+(yd*yd)+(zd*zd));
   }
   ok += DBPutUcdvar1(db, "speed", "mesh", (float*)speed,
                      domain.numNode(), NULL, 0, DB_DOUBLE, DB_NODECENT,
                      NULL);
   delete [] speed;
   ok += DBPutUcdvar1(db, "xd", "mesh", (float*)domain.xd(),
                      domain.numNode(), NULL, 0, DB_DOUBLE, DB_NODECENT,
                      NULL);
   ok += DBPutUcdvar1(db, "yd", "mesh", (float*)domain.yd(),
                      domain.numNode(), NULL, 0, DB_DOUBLE, DB_NODECENT,
                      NULL);
   ok += DBPutUcdvar1(db, "zd", "mesh", (float*)domain.zd(),
                      domain.numNode(), NULL, 0, DB_DOUBLE, DB_NODECENT,
                      NULL);

   if (ok != 0) {
      printf("Error writing out viz file - rank %d\n", myRank);
   }
}

/**********************************************************************/

#if USE_MPI     
void
   DumpMultiblockObjects(DBfile *db, PMPIO_baton_t *bat, 
                         char basename[], int numRanks)
#else
void
  DumpMultiblockObjects(DBfile *db, char basename[], int numRanks)
#endif
{
   /* MULTIBLOCK objects to tie together multiple files */
  char **multimeshObjs;
  char **multimatObjs;
  char ***multivarObjs;
  int *blockTypes;
  int *varTypes;
  int ok = 0;
  // Make sure this list matches what's written out above
  char vars[][10] = {"p","e","v","q", "speed", "xd", "yd", "zd"};
  int numvars = sizeof(vars)/sizeof(vars[0]);

  // Reset to the root directory of the silo file
  DBSetDir(db, "/");

  // Allocate a bunch of space for building up the string names
  multimeshObjs = new char*[numRanks];
  multimatObjs = new char*[numRanks];
  multivarObjs = new char**[numvars];
  blockTypes = new int[numRanks];
  varTypes = new int[numRanks];

  for(int v=0 ; v<numvars ; ++v) {
     multivarObjs[v] = new char*[numRanks];
  }
  
  for(int i=0 ; i<numRanks ; ++i) {
     multimeshObjs[i] = new char[32];
     multimatObjs[i] = new char[32];
     multivarObjs[i] = new char*[numvars];
     for(int v=0 ; v<numvars ; ++v) {
        multivarObjs[v][i] = new char[16];
     }
     blockTypes[i] = DB_UCDMESH;
     varTypes[i] = DB_UCDVAR;
  }
      
  // Build up the multiobject names
  for(int i=0 ; i<numRanks ; ++i) {
#if USE_MPI     
    int iorank = PMPIO_GroupRank(bat, i);
#else
    int iorank = 0;
#endif

    if (iorank == 0) {
      sprintf(multimeshObjs[i], "/data_%d/mesh", i);
      snprintf(multimatObjs[i], 32, "/data_%d/regions",i);
      for(int v=0 ; v<numvars ; ++v) {
	snprintf(multivarObjs[v][i], 16, "/data_%d/%s", i, vars[v]);
      }
     
    }
    else {
      snprintf(multimeshObjs[i], 32, "%s.%03d:/data_%d/mesh",
               basename, iorank, i);
      snprintf(multimatObjs[i], 32, "%s.%03d:/data_%d/regions", 
	       basename, iorank, i);
      for(int v=0 ; v<numvars ; ++v) {
         snprintf(multivarObjs[v][i], 16, "%s.%03d:/data_%d/%s", 
                  basename, iorank, i, vars[v]);
      }
    }
  }

  // Now write out the objects
  ok += DBPutMultimesh(db, "mesh", numRanks,
		       (char**)multimeshObjs, blockTypes, NULL);
  ok += DBPutMultimat(db, "regions", numRanks,
		      (char**)multimatObjs, NULL);
  for(int v=0 ; v<numvars ; ++v) {
     ok += DBPutMultivar(db, vars[v], numRanks,
                         (char**)multivarObjs[v], varTypes, NULL);
  }

  // Clean up
  for(int i=0 ; i<numRanks ; i++) {
    delete multimeshObjs[i];
    delete multimatObjs[i];
    for(int v=0 ; v<numvars ; ++v) {
       delete multivarObjs[v][i];
    }
    delete multivarObjs[i];
  }
  delete [] multimeshObjs;
  delete [] multimatObjs;
  delete [] multivarObjs;
  delete [] blockTypes;
  delete [] varTypes;

  if (ok != 0) {
    printf("Error writing out multiXXX objs to viz file - rank 0\n");
  }
}

# if USE_MPI

/**********************************************************************/

static void *
LULESH_PMPIO_Create(const char *fname,
		     const char *dname,
		     void *udata)
{
   /* Create the file */
   DBfile* db = DBCreate(fname, DB_CLOBBER, DB_LOCAL, NULL, DB_HDF5X);

   /* Put the data in a subdirectory, so VisIt only sees the multimesh
    * objects we write out in the base file */
   if (db) {
     DBMkDir(db, dname);
     DBSetDir(db, dname);
   }
   return (void*)db;
}

   
/**********************************************************************/

static void *
LULESH_PMPIO_Open(const char *fname,
		   const char *dname,
		   PMPIO_iomode_t ioMode,
		   void *udata)
{
   /* Open the file */
  DBfile* db = DBOpen(fname, DB_UNKNOWN, DB_APPEND);

   /* Put the data in a subdirectory, so VisIt only sees the multimesh
    * objects we write out in the base file */
   if (db) {
     DBMkDir(db, dname);
     DBSetDir(db, dname);
   }
   return (void*)db;
}

   
/**********************************************************************/

static void
LULESH_PMPIO_Close(void *file, void *udata)
{
  DBfile *db = (DBfile*)file;
  if (db)
    DBClose(db);
}
# endif

   
#else

void DumpToVisit(Domain& domain, int numFiles, int myRank, int numRanks)
{
   if (myRank == 0) {
      printf("Must enable -DVIZ_MESH at compile time to call DumpDomain\n");
   }
}

#endif

