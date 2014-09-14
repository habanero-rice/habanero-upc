#default build suggestion of MPI + OPENMP with gcc on Livermore machines you might have to change the compiler name

SHELL = /bin/sh
#.SUFFIXES: .cc .o

.PHONY: all clean 

LULESH_EXEC = lulesh-hcupc
all: clean $(LULESH_EXEC) clean-obj

include $(HABANERO_UPC_ROOT)/include/hcupc_spmd.mak
VPATH=$(UPCPP_ROOT)/include

UPCXX = $(CXX) -DUSE_OMP=0 -DUSE_HABANERO_UPC=1

SOURCES2.0UPCXX = \
	lulesh.cc \
	lulesh-comm-upcxx.cc \
	lulesh-viz.cc \
	lulesh-util.cc \
	lulesh-init.cc 
OBJECTS2.0UPCXX = $(SOURCES2.0UPCXX:.cc=.upcxx.o)

%.upcxx.o: %.cc lulesh.h
	@echo "Building $<"
	$(UPCXX) -c -DUSE_MPI=0 -DUSE_UPCXX=1 $(PROJECT_CXXFLAGS) -o $@ $<

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

