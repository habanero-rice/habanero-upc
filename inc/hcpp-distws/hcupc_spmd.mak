include $(UPCPP_ROOT)/include/upcxx.mak

PROJECT_CXXFLAGS=$(UPCXX_CXXFLAGS) -I$(HABANERO_UPC_ROOT)/include -DDIST_WS -I$(HCPP_ROOT)/include -I$(LIBXML2_INCLUDE) -DHUPCPP -DHCPP_COMM_WORKER 
PROJECT_LDFLAGS = $(LDFLAGS) $(UPCXX_LDFLAGS) -L$(HABANERO_UPC_ROOT)/lib -L$(HCPP_ROOT)/lib -L$(LIBXML2_LIBS)
PROJECT_LDLIBS = $(UPCXX_LDLIBS) -lhcupcpp -lhcpp -lxml2

ifdef TBB_MALLOC
  PROJECT_LDFLAGS += -L$(TBB_MALLOC)
  PROJECT_LDLIBS += -ltbbmalloc_proxy
endif 
