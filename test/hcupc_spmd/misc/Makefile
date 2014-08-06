include $(HABANERO_UPC_INSTALL)/include/hcupc_spmd.mak

EXE=test test_ddf

all: clean $(EXE) clean-obj

test: test.cpp
	$(CXX) $(PROJECT_LDFLAGS) $(PROJECT_CXXFLAGS) -o $@ $^ $(PROJECT_LDLIBS)

test_ddf: test_ddf.cpp
	$(CXX) $(PROJECT_LDFLAGS) $(PROJECT_CXXFLAGS) -o $@ $^ $(PROJECT_LDLIBS)

clean-obj:
	rm -rf *.o *.dSYM

clean:
	rm -rf *.o $(EXE) *.dSYM
