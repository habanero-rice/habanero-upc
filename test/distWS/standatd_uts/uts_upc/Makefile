###  UTS - Unbalanced Tree Search  ###

# CONFFILE should be a symlink to your configuration
CONFFILE    = config.in
-include      $(CONFFILE)

DIST_EXCLUDE= DIST_EXCLUDE

COMMON_SRCS = uts.c
DM_SRCS     = uts_dm.c stats.c
QUEUE_SRCS  = dequeue.c dlist.c
SHM_SRCS    = uts_shm.c

TARGETS     = uts-seq uts-dfs uts-stat          \
              uts-omp uts-pthread               \
              uts-upc uts-upc-dcq               \
	      uts-upc-enhanced                  \
              uts-shmem uts-gpshmem             \
              uts-mpi-wm uts-mpi-wm-nb          \
	      uts-mpi-ws uts-mpi-hws            \
              uts-mta uts-gtc time_rng time_poll\
              uts-dot

# ------------------------------------- #
# Set Random Number Generator sources:
# ------------------------------------- #

# Set the default RNG
ifndef RNG
RNG=BRG
endif

ifeq ($(RNG), Devine) 
RNG_SRC = rng/devine_sha1.c
RNG_INCL= rng/devine_sha1.h
RNG_DEF = -DDEVINE_RNG
endif
ifeq ($(RNG), BRG)
RNG_SRC = rng/brg_sha1.c
RNG_INCL= rng/brg_sha1.h
RNG_DEF = -DBRG_RNG
endif
ifeq ($(RNG), ALFG)
RNG_SRC = rng/alfg.c
RNG_INCL= rng/alfg.h
RNG_DEF = -DUTS_ALFG
endif

# ------------------------------------- #
# Targets:
# ------------------------------------- #

.PHONY: clean uts-mpi

ifndef TARGETS_ALL
TARGETS_ALL=uts-seq
endif

all: $(TARGETS_ALL)

$(CONFFILE):
	$(error UTS has not been configured.  Please run configure.sh)

tags:
	ctags --recurse --language-force=C rng *.c *.h

########## Sequential Implementations ##########

uts-seq:  $(SHM_SRCS) $(RNG_SRC) $(COMMON_SRCS)
	$(CC) $(CC_OPTS) $(LD_OPTS) $(RNG_DEF) $(FLAGS) -o $@ $+

uts-dfs:  uts_dfs.c $(RNG_SRC) $(COMMON_SRCS)
	$(CC) $(CC_OPTS) $(LD_OPTS) $(RNG_DEF) -o $@ $+

uts-dot:  uts_dot.c $(RNG_SRC) $(COMMON_SRCS)
	$(CC) $(CC_OPTS) $(LD_OPTS) $(RNG_DEF) -o $@ $+

uts-stat: $(SHM_SRCS) $(RNG_SRC) $(COMMON_SRCS)
	$(CC) $(CC_OPTS) $(LD_OPTS) $(RNG_DEF) -DUTS_STAT -o $@ $+

time_rng:  time_rng.c $(RNG_SRC) $(COMMON_SRCS)
	$(CC) $(CC_OPTS) $(RNG_DEF) -o $@ $+ $(LD_OPTS)


########## Distributed Memory Model Implementations ##########

uts-mpi: uts-mpi-wm uts-mpi-ws

uts-mpi-wm: $(DM_SRCS) $(QUEUE_SRCS) mpi_worksharing.c $(RNG_SRC) $(COMMON_SRCS)
	$(MPICC) $(MPICC_OPTS) $(MPILD_OPTS) $(RNG_DEF) $(FLAGS) -D__MPI__ -o $@ $+

uts-mpi-wm-nb: $(DM_SRCS) $(QUEUE_SRCS) mpi_worksharing.c $(RNG_SRC) $(COMMON_SRCS)
	$(MPICC) $(MPICC_OPTS) $(MPILD_OPTS) $(RNG_DEF) $(FLAGS) -DNONBLOCK -D__MPI__ -o $@ $+

uts-mpi-ws: $(DM_SRCS) $(QUEUE_SRCS) mpi_workstealing.c $(RNG_SRC) $(COMMON_SRCS)
	$(MPICC) $(MPICC_OPTS) $(MPILD_OPTS) $(RNG_DEF) $(FLAGS) -D__MPI__ -o $@ $+ 

time_poll:  time_poll.c $(RNG_SRC) $(COMMON_SRCS) stats.c mpi_workstealing.c $(QUEUE_SRCS)
	$(MPICC) $(MPICC_OPTS) $(RNG_DEF) -o $@ $+ $(MPILD_OPTS)

uts-gtc: $(DM_SRCS) gtc_ldbal.c $(RNG_SRC) $(COMMON_SRCS)
	$(MPICC) $(MPICC_OPTS) $(RNG_DEF) $(FLAGS) -DUSING_GTC -I$(ARMCI_INCLUDE) -I$(GA_INCLUDE) -I$(SCIOTO_INCLUDE) -D__MPI__ -c $(RNG_SRC)
	$(MPICC) $(MPICC_OPTS) $(RNG_DEF) $(FLAGS) -DUSING_GTC -I$(ARMCI_INCLUDE) -I$(GA_INCLUDE) -I$(SCIOTO_INCLUDE) -D__MPI__ -c $+
	$(MPICC) $(MPICC_OPTS) $(MPILD_OPTS) $(RNG_DEF) $(FLAGS) -DUSING_GTC -I$(ARMCI_INCLUDE) -I$(GA_INCLUDE) -I$(SCIOTO_INCLUDE) \
        -L$(ARMCI_LIBS) -L$(GA_LIBS) -L$(SCIOTO_LIBS) -D__MPI__ -o $@ $(+:.c=.o) -ltc -lsynch -larmci -lm

uts-mpi-hws: $(RNG_SRC) uts_hws.c
	$(MPICC) $(MPICC_OPTS) $(MPILD_OPTS) $(RNG_DEF) $(FLAGS) -D__MPI__ -D__PTHREADS__ -lpthread -o $@ $+

uts-upc-dcq: $(DM_SRCS) $(RNG_SRC) $(COMMON_SRCS) $(QUEUE_SRCS) upc_worksharing.c shared_dequeue.c shared_dlist.c
	$(UPCC) $(UPCC_OPTS) $(UPCLD_OPTS) $(RNG_DEF) $(FLAGS) -o $@ $+


########## Shared Memory Model Implementations ##########

uts-pthread: $(SHM_SRCS) $(RNG_SRC) $(COMMON_SRCS)
	$(CC) $(CC_OPTS) $(LD_OPTS) $(RNG_DEF) $(FLAGS) -D__PTHREADS__ -lpthread -o $@ $+

uts-omp: $(SHM_SRCS) $(RNG_SRC) $(COMMON_SRCS)
	$(OMPCC) $(OMPCC_OPTS) $(OMPLD_OPTS) $(RNG_DEF) $(FLAGS) -o $@ $+

uts-upc: $(SHM_SRCS) $(RNG_SRC) $(COMMON_SRCS)
	$(UPCC) $(UPCC_OPTS) $(UPCLD_OPTS) $(RNG_DEF) $(FLAGS) -o $@ $+

uts-upc-enhanced-rand: uts_upc_enhanced.c $(RNG_SRC) $(COMMON_SRCS)
	$(UPCC) -D_VS_RAND_ $(UPCC_OPTS) $(UPCLD_OPTS) $(RNG_DEF) $(FLAGS) -o $@ $+

uts-upc-enhanced: uts_upc_enhanced.c $(RNG_SRC) $(COMMON_SRCS)
	$(UPCC) $(UPCC_OPTS) $(UPCLD_OPTS) $(RNG_DEF) $(FLAGS) -o $@ $+

uts-upc-enhanced-load-unaware: uts_upc_enhanced.c $(RNG_SRC) $(COMMON_SRCS)
	$(UPCC) -DNOT_LOAD_AWARE $(UPCC_OPTS) $(UPCLD_OPTS) $(RNG_DEF) $(FLAGS) -o $@ $+

uts-mta:  uts_dfs.c $(RNG_SRC) $(COMMON_SRCS)
	$(CC) $(CC_OPTS) $(RNG_DEF) -D__MTA__ -o $@ $+ $(LD_OPTS)

uts-shmem: $(SHM_SRCS) $(RNG_SRC) $(COMMON_SRCS)
	$(SHMCC) $(SHMCC_OPTS) $(SHMLD_OPTS) $(RNG_DEF) $(FLAGS) -D_SHMEM -o $@ $+

uts-gpshmem: uts_gpshmem.c $(RNG_SRC) $(COMMON_SRCS)
	$(GPSHMCC) $(GPSHMCC_OPTS) $(GPSHMLD_OPTS) $(RNG_DEF) -D_GPSHMEM -o $@ $+


clean: config
	rm -f *.o $(TARGETS) tags

distclean: clean
	rm -f $(CONFFILE)

distrib: clean
	mkdir distrib
	tar -X DIST_EXCLUDE -c * | tar -C distrib/ -xf -
	bash -c 'cd distrib; echo ++ Entering `pwd`; for file in *.c *.h rng/alfg.*; do echo \ \ \ Inserting header into $$file; (echo 0a; cat ../DIST_HEADER; echo .; echo wq) | ed -s $$file; done'
	@echo "++ Distribution has been built in ./distrib/"
