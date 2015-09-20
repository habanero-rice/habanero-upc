include $(HABANERO_UPC_ROOT)/include/hcupc_spmd.mak

EXE=UTS

all: clean $(EXE) clean-obj

UTS.o: UTS.cpp
	$(CXX) -Wno-write-strings $(PROJECT_CXXFLAGS) -DBRG_RNG -c UTS.cpp

deps:
	$(CXX) -Wno-write-strings -DBRG_RNG -c rng/brg_sha1.c uts.c

UTS: UTS.o deps
	$(CXX_LINKER) -Wno-write-strings $(PROJECT_LDFLAGS) -o $@ UTS.o uts.o brg_sha1.o $(PROJECT_LDLIBS)

clean-obj:
	rm -rf *.o *.dSYM

clean:
	rm -rf *.o $(EXE) *.dSYM
