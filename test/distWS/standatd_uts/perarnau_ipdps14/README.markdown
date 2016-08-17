Variations around the MPI Workstealing implementation of UTS
============================================================

This repo contains code to generate several binary versions of the MPI workstealing
implementation of the UTS benchmark.

Most of the functionalities of these binaries are in 3 source files:

- `mpi_workstealing.c`
- `mpi_wshalf.c`
- `victimselect.h`

The `mpi_*` files contain the basic implementation of WS. The `mpi_wshalf.c`
file is a modification to steal half the number of chunks instead of just one.

The `victimselect.h` implements several victim selection strategies, and one is
chosen at compile time using defines.


Names of the binaries are chosen to easily identify all these parameters (half
steal or not, victim selection process).

`uts-mpi-ws` use a round robin victim selection process, and steals 1 chunk.
It is equivalent to the original UTS code.

Versions with `half` in there names steal half the chunks.

Versions with `rand` uses the `rand()` function to select a victim.

Versions with `gslui` uses the GNU Scientific Library, uniform random
distribution to select a victim.

Versions with `gslrd` uses the GNU Scientific Library, and apply a weight one
the probability of a thief choosing someone, using the TOFU coordinates of a
process.

Versions with `fix` in the name correct a divide by zero bug when using
weights: two process on the same processor have the same coordinates.

`mpi_coords.c` is a simple MPI program printing the tofu coordinates of all
processes.

## Note about the trace/notrace versions

UTS provides a -DTRACE to trace session of work, idle, search in an execution.
We generate a different binary for every version. Binaries with `nt` in their
name do not generate this trace.

## Note about this repo history

The first commit contains the original UTS code.
The second commit contains the codes as they were used for the draft paper
'Victim Selection and Distributed Work Stealing Performance: a Case Study'
Newer versions are post-submission cleanups, intended to make further analysis
easier.
