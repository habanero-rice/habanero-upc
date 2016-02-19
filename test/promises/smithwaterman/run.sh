HCMPI_SRC_NAME=smith-waterman-mpi-v7
HCMPI_EXEC_NAME=$HCMPI_SRC_NAME

if [ $# -ne 2 ]; then
	echo "USAGE: ./run.sh <NUM_NODES> <WORKLOAD>"
	echo "WORKLOAD=tiny, medium, large, huge"
	exit
fi

NODES=$1
SIZE=$2

#SIZE=tiny
#SIZE=medium
#SIZE=large

INPUT_FILE_1="./input/string1-$SIZE.txt"
INPUT_FILE_2="./input/string2-$SIZE.txt"

if [ "$SIZE" == "tiny" ]; then
        TILE_WIDTH=4
        TILE_HEIGHT=4
        INNER_TILE_WIDTH=2
        INNER_TILE_HEIGHT=2
        EXPECTED_RESULT=12
else
if [ "$SIZE" == "medium" ]; then
        TILE_WIDTH=232
        TILE_HEIGHT=240
        INNER_TILE_WIDTH=29
        INNER_TILE_HEIGHT=30
        EXPECTED_RESULT=3640
else
if [ "$SIZE" == "large" ]; then
        TILE_WIDTH=2320
        TILE_HEIGHT=2400
        INNER_TILE_WIDTH=232
        INNER_TILE_HEIGHT=240
        EXPECTED_RESULT=36472
else
if [ "$SIZE" == "huge" ]; then
        TILE_WIDTH=11600
        TILE_HEIGHT=12000
        INNER_TILE_WIDTH=725
        INNER_TILE_HEIGHT=750
        EXPECTED_RESULT=364792
fi
fi
fi
fi

export HCLIB_WORKERS=6
echo "mpirun -np ${NODES} ./smith-waterman ${INPUT_FILE_1} ${INPUT_FILE_2} ${TILE_WIDTH} ${TILE_HEIGHT} ${INNER_TILE_WIDTH} ${INNER_TILE_HEIGHT}"
mpirun -np ${NODES} ./smith-waterman ${INPUT_FILE_1} ${INPUT_FILE_2} ${TILE_WIDTH} ${TILE_HEIGHT} ${INNER_TILE_WIDTH} ${INNER_TILE_HEIGHT}
# srun --partition=interactive --exclusive --time=5 --nodes=${NODES} --ntasks-per-node=2 --cpus-per-task=6 ./smith-waterman ${INPUT_FILE_1} ${INPUT_FILE_2} ${TILE_WIDTH} ${TILE_HEIGHT} ${INNER_TILE_WIDTH} ${INNER_TILE_HEIGHT}

