#!/bin/bash -l

#SBATCH --partition=debug
#SBATCH --nodes=64
#SBATCH --tasks-per-node=24
#SBATCH --time=00:15:00
#SBATCH --job-name=vivekk

export GASNET_MAX_SEGSIZE='512MB'
export UPC_SHARED_HEAP_SIZE='128MB'

ITER=5
UPC_TASK=1536
LOGDIR="/global/homes/v/vivekk/ipdps16/logs"
BENCH_OPTIONS_T3WL="${T3WL}"
BENCH_OPTIONS_T1WL="${T1WL}"
EXE="./uts-upc-enhanced-rand"
LOGFILE_T3WL="${LOGDIR}/UTST3WL.3096.1024.Jan2016UPCRand.i40.c40.places-${UPC_TASK}.log"
LOGFILE_T1WL="${LOGDIR}/UTST1WL.3096.1024.Jan2016UPCRand.i20.c20.places-${UPC_TASK}.log"

j=0
while [ $j -lt ${ITER} ]; do
    srun -n ${UPC_TASK} ./${EXE} ${BENCH_OPTIONS_T3WL} -c 40 -i 40 >& "${LOGFILE}.t1"
    cat "${LOGFILE}.t1" | sed '/resources: utime/d' > "${LOGFILE}.t2"
    cat "${LOGFILE}.t2"  >> "${LOGFILE_T3WL}"
    rm "${LOGFILE}.t1" "${LOGFILE}.t2"


    srun -n ${UPC_TASK} ./${EXE} ${BENCH_OPTIONS_T1WL} -c 20 -i 20 >& "${LOGFILE}.t1"
    cat "${LOGFILE}.t1" | sed '/resources: utime/d' > "${LOGFILE}.t2"
    cat "${LOGFILE}.t2"  >> "${LOGFILE_T1WL}"
    rm "${LOGFILE}.t1" "${LOGFILE}.t2"

    j=`expr ${j} + 1`
done    


