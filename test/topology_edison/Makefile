include $(UPCPP_ROOT)/include/upcxx.mak

#all: gsl
all: topo

topo: topo.cpp
	mpicxx $(UPCXX_CXXFLAGS) -I/usr/local/Cellar/gsl/1.16/include $(LDFLAGS) -D__VS_GSLUI__ $(UPCXX_LDFLAGS) -L/usr/local/Cellar/gsl/1.16/lib -o $@ $^ $(UPCXX_LDLIBS) -lgsl

gsl: gsl.c
	cc gsl.c -I/usr/local/Cellar/gsl/1.16/include -L/usr/local/Cellar/gsl/1.16/lib -lgsl

clean:
	rm topo gsl *.o
