include $(HABANERO_UPC_ROOT)/include/hcupc_spmd.mak

EXE=test test2 test_ddf nqueens test_asyncAny0  arrayadd1d

all: clean $(EXE) clean-obj

arrayadd1d: arrayadd1d.cpp
	$(CXX) $(PROJECT_LDFLAGS) $(PROJECT_CXXFLAGS) -o $@ $^ $(PROJECT_LDLIBS)

test_asyncAny0: test_asyncAny0.cpp
	$(CXX) $(PROJECT_LDFLAGS) $(PROJECT_CXXFLAGS) -o $@ $^ $(PROJECT_LDLIBS)

temp: temp.cpp
	$(CXX) $(PROJECT_LDFLAGS) $(PROJECT_CXXFLAGS) -o $@ $^ $(PROJECT_LDLIBS)

nqueens: nqueens.cpp
	$(CXX) $(PROJECT_LDFLAGS) $(PROJECT_CXXFLAGS) -o $@ $^ $(PROJECT_LDLIBS)

test2: test2.cpp
	$(CXX) $(PROJECT_LDFLAGS) $(PROJECT_CXXFLAGS) -o $@ $^ $(PROJECT_LDLIBS)

test: test.cpp
	$(CXX) $(PROJECT_LDFLAGS) $(PROJECT_CXXFLAGS) -o $@ $^ $(PROJECT_LDLIBS)

test_ddf: test_ddf.cpp
	$(CXX) $(PROJECT_LDFLAGS) $(PROJECT_CXXFLAGS) -o $@ $^ $(PROJECT_LDLIBS)

clean-obj:
	rm -rf *.o *.dSYM

clean:
	rm -rf *.o $(EXE) *.dSYM
