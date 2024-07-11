

## EPFL logic synthesis libraries
The  libraries and several examples on how to use and integrate the libraries can be found in the [logic synthesis tool showcase](https://github.com/lsils/lstools-showcase).
## XAG Network Optimiser Project
mkdir build

cd build

cmake -DCMAKE_CXX_COMPILER=g++-10 -DMOCKTURTLE_EXAMPLES=ON -DMOCKTURTLE_EXPERIMENTS=ON -DMOCKTURTLE_TEST=ON ..

make

./experiments/xag_optimizer
