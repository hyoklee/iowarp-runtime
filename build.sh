rm -rf build
export CMAKE_PREFIX_PATH=$PREFIX:$CONDA_PREFIX/pkgs/mercury-0.0.0-py312_0/share/cmake
export CMAKE_INCLUDE_PATH=$PREFIX/include
export PKG_CONFIG_PATH=$CONDA_PREFIX/lib/pkgconfig:$PKG_CONFIG_PATH
# export CC=/usr/bin/mpicc
# export CXX=/usr/bin/mpicxx
cmake -DBUILD_MPI_TESTS:BOOL=OFF -B build .
make -C build -j 2


