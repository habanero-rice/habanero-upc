#default build suggestion of MPI + OPENMP with gcc on Livermore machines you might have to change the compiler name

SHELL = /bin/sh
#.SUFFIXES: .cc .o

.PHONY: all clean 

LULESH_EXEC = lulesh-hcupc
all: clean $(LULESH_EXEC) clean-obj

include $(HABANERO_UPC_INSTALL)/include/hcupc_spmd.mak
VPATH=$(UPCPP_INSTALL)/include

MPI_INC = /opt/local/include/openmpi
MPI_LIB = /opt/local/lib

#CXX = icpc -fast -DUSE_OMP=1 -fopenmp 
#MPICXX = mpicxx -fast -DUSE_OMP=1 -fopenmp
#UPCXX = mpicxx -fast -DUSE_OMP=1 -fopenmp
#CXX = clang++ -DUSE_OMP=0
#MPICXX = mpicxx -DUSE_OMP=0
UPCXX = $(CXX) -DUSE_OMP=0

SOURCES2.0 = \
	lulesh.cc \
	lulesh-comm.cc \
	lulesh-viz.cc \
	lulesh-util.cc \
	lulesh-init.cc 
OBJECTS2.0 = $(SOURCES2.0:.cc=.o)

SOURCES2.0MPI = \
	lulesh.cc \
	lulesh-comm.cc \
	lulesh-viz.cc \
	lulesh-util.cc \
	lulesh-init.cc 
OBJECTS2.0MPI = $(SOURCES2.0MPI:.cc=.mpi.o)

SOURCES2.0UPCXX = \
	lulesh.cc \
	lulesh-comm-upcxx.cc \
	lulesh-viz.cc \
	lulesh-util.cc \
	lulesh-init.cc 
OBJECTS2.0UPCXX = $(SOURCES2.0UPCXX:.cc=.upcxx.o)

## -wd161 disable the openmp pragma warnings by Intel compilers
#OMP_FLAG = -wd161 #-fopenmp
#CXXFLAGS = -O3 $(OMP_FLAG)
#LDFLAGS = $(OMP_FLAG)

#common places you might find silo on the Livermore machines.
#SILO_INCDIR = /opt/local/include
#SILO_LIBDIR = /opt/local/lib
#SILO_INCDIR = /usr/gapps/silo/current/chaos_5_x86_64/include
#SILO_LIBDIR = /usr/gapps/silo/current/chaos_5_x86_64/lib

#below is and example of how to make with silo, hdf5 to get vizulization by default all this is turned off
#CXXFLAGS = -g -openmp -DVIZ_MESH -I${SILO_INCDIR} -Wall -Wno-pragmas
#LDFLAGS = -g -L${SILO_LIBDIR} -Wl,-rpath -Wl,${SILO_LIBDIR} -lsiloh5 -lhdf5

%.o: %.cc lulesh.h
	@echo "Building $<"
	$(CXX) -c -DUSE_MPI=0 -DUSE_UPCXX=0 $(CXXFLAGS) -o $@ $<

%.mpi.o: %.cc lulesh.h
	@echo "Building $<"
	$(MPICXX) -c -DUSE_MPI=1 -DUSE_UPCXX=0 $(CXXFLAGS) -o $@ $<

%.upcxx.o: %.cc lulesh.h
	@echo "Building $<"
	$(UPCXX) -c -DUSE_MPI=0 -DUSE_UPCXX=1 $(PROJECT_CXXFLAGS) -o $@ $<

lulesh2.0: $(OBJECTS2.0) 
	@echo "Linking"
	$(CXX) $(LDFLAGS) $(OBJECTS2.0) -lm -o $@

lulesh2.0-mpi: $(OBJECTS2.0MPI) 
	@echo "Linking"
	$(MPICXX) $(LDFLAGS) $(OBJECTS2.0MPI) -lm -o $@

lulesh-hcupc: $(OBJECTS2.0UPCXX)
	@echo "Linking"
	$(UPCXX) $(PROJECT_LDFLAGS) $(OBJECTS2.0UPCXX) -o $@ -lm $(PROJECT_LDLIBS)

clean-obj:
	rm -rf *.o *~ $(OBJECTS) *.dSYM

clean:
	/bin/rm -f *.o *~ $(OBJECTS) $(LULESH_EXEC)
	/bin/rm -rf *.dSYM

tar: clean
	cd .. ; tar cvf lulesh-2.0.tar LULESH-2.0 ; mv lulesh-2.0.tar LULESH-2.0

