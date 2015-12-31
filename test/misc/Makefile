include $(HABANERO_UPC_ROOT)/include/hcupc_spmd.mak

EXE=test test2 test_promise arrayadd1d

all: clean $(EXE) clean-obj

test_promise.o: test_promise.cpp
	$(CXX) $(PROJECT_CXXFLAGS) -c $^

test_promise: test_promise.o
	$(CXX_LINKER) $(PROJECT_LDFLAGS) -o $@ $^ $(PROJECT_LDLIBS)

test2.o: test2.cpp
	$(CXX) $(PROJECT_CXXFLAGS) -c $^

test2: test2.o
	$(CXX_LINKER) $(PROJECT_LDFLAGS) -o $@ $^ $(PROJECT_LDLIBS)

test.o: test.cpp
	$(CXX) $(PROJECT_CXXFLAGS) -c $^

test: test.o
	$(CXX_LINKER) $(PROJECT_LDFLAGS) -o $@ $^ $(PROJECT_LDLIBS)

arrayadd1d.o: arrayadd1d.cpp
	$(CXX) $(PROJECT_CXXFLAGS) -c $^

arrayadd1d: arrayadd1d.o
	$(CXX_LINKER) $(PROJECT_LDFLAGS) -o $@ $^ $(PROJECT_LDLIBS)

clean-obj:
	rm -rf *.o *.dSYM

clean:
	rm -rf *.o $(EXE) *.dSYM
