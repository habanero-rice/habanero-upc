include $(UPCPP_ROOT)/include/upcxx.mak
PROJECT_CXXFLAGS=$(UPCXX_CXXFLAGS) -I$(HCPP_ROOT)/include -I$(HCLIB_ROOT)/include -I$(HABANERO_UPC_ROOT)/include
PROJECT_LDFLAGS=
PROJECT_LDLIBS=

ifndef TBB_MALLOC
   PROJECT_LDFLAGS=$(LDFLAGS) $(UPCXX_LDFLAGS) -L$(HCPP_ROOT)/lib -L$(HCLIB_ROOT)/lib -L$(OCR_ROOT)/lib  -L$(HABANERO_UPC_ROOT)/lib
   PROJECT_LDLIBS=$(UPCXX_LDLIBS) -lhclib -locr -lhcpp -lhcupcpp
else
   PROJECT_LDFLAGS=$(LDFLAGS) $(UPCXX_LDFLAGS) -L$(HCPP_ROOT)/lib -L$(HCLIB_ROOT)/lib -L$(OCR_ROOT)/lib  -L$(HABANERO_UPC_ROOT)/lib -L$(TBB_MALLOC)
   PROJECT_LDLIBS=$(UPCXX_LDLIBS) -lhclib -locr -lhcpp -lhcupcpp -ltbbmalloc_proxy
endif
