include $(HABANERO_UPC_ROOT)/include/hcupc_spmd.mak

EXE=sample_sort-hcupc sample_sort-upcxx

all: clean $(EXE) clean-obj

sample_sort-hcupc: sample_sort.cpp
	$(CXX) -DUSE_HABANERO_UPC $(PROJECT_LDFLAGS) $(PROJECT_CXXFLAGS) -o $@ $^ $(PROJECT_LDLIBS)

sample_sort-upcxx: sample_sort.cpp
	$(CXX) $(UPCXX_LDFLAGS) $(UPCXX_CXXFLAGS) -o $@ $^ $(UPCXX_LDLIBS)

clean-obj:
	rm -rf *.o *.dSYM

clean:
	rm -rf *.o $(EXE) *.dSYM
