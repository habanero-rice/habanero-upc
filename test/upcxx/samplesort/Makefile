CC=CC

include $(UPCPP_INSTALL)/include/upcxx.mak

EXE=sample_sort

all: clean $(EXE) clean-obj

sample_sort: sample_sort.o 
	$(CC) $(LDFLAGS) $(UPCXX_LDFLAGS) -o $@ $^ $(UPCXX_LDLIBS)

sample_sort.o: sample_sort.cpp
	$(CC) $(UPCXX_CXXFLAGS) -o $@ -c $<

clean-obj:
	rm -f *.o

clean:
	rm -rf *.o $(EXE) *.dSYM
