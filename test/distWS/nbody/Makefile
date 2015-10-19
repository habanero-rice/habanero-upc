include $(HABANERO_UPC_ROOT)/include/hcupc_spmd.mak

EXE=nbody gen-input

all: clean $(EXE) clean-obj

gen-input: gen-input.cpp
	g++ -std=c++11 -o gen-input gen-input.cpp

nbody.o: nbody.cpp
	$(CXX) $(PROJECT_CXXFLAGS) -c nbody.cpp 

nbody: nbody.o
	$(CXX_LINKER) $(PROJECT_LDFLAGS) -o $@ $^ $(PROJECT_LDLIBS)

clean-obj:
	rm -rf *.o *.dSYM

clean:
	rm -rf *.o $(EXE) *.dSYM *.out
