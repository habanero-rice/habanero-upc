include $(HABANERO_UPC_ROOT)/include/hcupc_spmd.mak

EXE=nqueens

all: clean $(EXE) clean-obj

nqueens.o: nqueens.cpp
	$(CXX) $(PROJECT_CXXFLAGS) -c nqueens.cpp 

nqueens: nqueens.o
	$(CXX_LINKER) $(PROJECT_LDFLAGS) -o $@ $^ $(PROJECT_LDLIBS)

clean-obj:
	rm -rf *.o *.dSYM

clean:
	rm -rf *.o $(EXE) *.dSYM *.out
