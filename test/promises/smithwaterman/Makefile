include $(HABANERO_UPC_ROOT)/include/hcupc_spmd.mak

EXE=smith-waterman

all: clean $(EXE) clean-obj

smith-waterman: smith-waterman.cpp
	$(CXX) $(PROJECT_CFLAGS) $(PROJECT_CXXFLAGS) $(PROJECT_LDFLAGS) -o $@ $^ $(PROJECT_LDLIBS) -g -O0

clean-obj:
	rm -rf *.o *.dSYM

clean:
	rm -rf *.o $(EXE) *.dSYM
