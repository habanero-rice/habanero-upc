###  UTS - Unbalanced Tree Search  ###

# CONFFILE should be a symlink to your configuration
#CONFFILE    = config.in
#-include      $(CONFFILE)

MPICC=cc -O3 -lm -std=c99
DIST_EXCLUDE= DIST_EXCLUDE

COMMON_SRCS = uts.c
DM_SRCS     = uts_dm.c stats.c
QUEUE_SRCS  = dequeue.c dlist.c
SHM_SRCS    = uts_shm.c

TARGETS     = uts-seq           \
	      uts-mpi-ws        \
              time_rng time_poll

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

########## Distributed Memory Model Implementations ##########

time_poll:  time_poll.c $(RNG_SRC) $(COMMON_SRCS) stats.c mpi_workstealing.c $(QUEUE_SRCS)
	$(MPICC) $(MPICC_OPTS) $(RNG_DEF) -o $@ $+ $(MPILD_OPTS)

uts-mpi-ws: $(DM_SRCS) $(QUEUE_SRCS) mpi_workstealing.c $(RNG_SRC) $(COMMON_SRCS)
	$(MPICC) $(MPICC_OPTS) $(MPILD_OPTS) $(RNG_DEF) $(FLAGS) -D__VS_ORIG__ -D__MPI__ -o $@ $+

uts-mpi-ws-rand: $(DM_SRCS) $(QUEUE_SRCS) mpi_workstealing.c $(RNG_SRC) $(COMMON_SRCS)
	$(MPICC) $(MPICC_OPTS) $(MPILD_OPTS) $(RNG_DEF) $(FLAGS) -D__VS_RAND__ -D__MPI__ -o $@ $+

uts-mpi-ws-half: $(DM_SRCS) $(QUEUE_SRCS) mpi_wshalf.c $(RNG_SRC) $(COMMON_SRCS)
	$(MPICC) $(MPICC_OPTS) $(MPILD_OPTS) $(RNG_DEF) $(FLAGS) -D__VS_ORIG__ -D__SS_HALF__ -D__MPI__ -o $@ $+

uts-mpi-ws-half-rand: $(DM_SRCS) $(QUEUE_SRCS) mpi_wshalf.c $(RNG_SRC) $(COMMON_SRCS)
	$(MPICC) $(MPICC_OPTS) $(MPILD_OPTS) $(RNG_DEF) $(FLAGS) -D__VS_RAND__ -D__SS_HALF__ -D__MPI__ -o $@ $+

clean: config
	rm -f *.o $(TARGETS) tags uts-mpi-*

distclean: clean
	rm -f $(CONFFILE)

distrib: clean
	mkdir distrib
	tar -X DIST_EXCLUDE -c * | tar -C distrib/ -xf -
	bash -c 'cd distrib; echo ++ Entering `pwd`; for file in *.c *.h rng/alfg.*; do echo \ \ \ Inserting header into $$file; (echo 0a; cat ../DIST_HEADER; echo .; echo wq) | ed -s $$file; done'
	@echo "++ Distribution has been built in ./distrib/"
