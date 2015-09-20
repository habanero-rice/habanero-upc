include $(HABANERO_UPC_ROOT)/include/hcupc_spmd.mak

EXE=UTS

all: clean $(EXE) clean-obj

UTS.o: UTS.cpp
	$(CXX) -Wno-write-strings $(PROJECT_CXXFLAGS) -DBRG_RNG -c $^ uts.c rng/brg_sha1.c

UTS: UTS.o
	g++ -Wno-write-strings $(PROJECT_LDFLAGS) -o $@ $^ brg_sha1.o uts.o $(PROJECT_LDLIBS)

clean-obj:
	rm -rf *.o *.dSYM

clean:
	rm -rf *.o $(EXE) *.dSYM
