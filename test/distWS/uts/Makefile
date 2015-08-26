include $(HABANERO_UPC_ROOT)/include/hcupc_spmd.mak

EXE=UTS
#EXE=UTSv2

all: clean $(EXE) clean-obj

UTS: UTS.cpp
	$(CXX) -Wno-write-strings $(PROJECT_CXXFLAGS) -DBRG_RNG $(PROJECT_LDFLAGS) -o $@ $^ uts.c rng/brg_sha1.c $(PROJECT_LDLIBS)

UTSv2: UTSv2.cpp
	$(CXX) -Wno-write-strings $(PROJECT_LDFLAGS) $(PROJECT_CXXFLAGS) -lm -DBRG_RNG -o $@ $^ uts.c rng/brg_sha1.c uts_dm_hooks.c $(PROJECT_LDLIBS)

clean-obj:
	rm -rf *.o *.dSYM

clean:
	rm -rf *.o $(EXE) *.dSYM
