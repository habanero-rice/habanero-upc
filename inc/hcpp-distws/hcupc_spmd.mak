include $(UPCPP_ROOT)/include/upcxx.mak

PROJECT_CXXFLAGS=$(UPCXX_CXXFLAGS) -I$(HABANERO_UPC_ROOT)/include -DDIST_WS -I$(HCPP_ROOT)/include -I$(LIBXML2)/include/libxml2 -DHUPCPP -DHCPP_COMM_WORKER 
PROJECT_LDFLAGS = $(LDFLAGS) $(UPCXX_LDFLAGS) -L$(HABANERO_UPC_ROOT)/lib -L$(HCPP_ROOT)/lib -L$(LIBXML2)/lib
PROJECT_LDLIBS = $(UPCXX_LDLIBS) -lhcupcpp -lhcpp -lxml2

ifdef TBB_MALLOC
  PROJECT_LDFLAGS += -L$(TBB_MALLOC)
  PROJECT_LDLIBS += -ltbbmalloc_proxy
endif 