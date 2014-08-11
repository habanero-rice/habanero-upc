include $(HABANERO_UPC_ROOT)/include/hcupc_spmd.mak

EXE=sample_sort-hcupc
#EXE=sample_sort-upcxx

all: clean $(EXE) clean-obj

sample_sort-hcupc: sample_sort.cpp
	$(CXX) -DUSE_HABANERO_UPC=1 $(PROJECT_LDFLAGS) $(PROJECT_CXXFLAGS) -o $@ $^ $(PROJECT_LDLIBS)

sample_sort-upcxx: sample_sort.cpp
	$(CXX) -DUSE_HABANERO_UPC=0 $(PROJECT_LDFLAGS) $(PROJECT_CXXFLAGS) -o $@ $^ $(PROJECT_LDLIBS)

clean-obj:
	rm -rf *.o *.dSYM

clean:
	rm -rf *.o $(EXE) *.dSYM
