# important: daint-gpu must be loaded to get access to many modules!
module load daint-gpu
module swap PrgEnv-cray PrgEnv-gnu
module load CMake
module load Boost
module load cray-libsci
module load cudatoolkit
module load cray-hdf5-parallel
module load cray-mpich
module load GSL

