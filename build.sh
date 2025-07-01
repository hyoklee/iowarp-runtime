rm -rf build
export CMAKE_PREFIX_PATH=$PREFIX
export CMAKE_INCLUDE_PATH=$PREFIX/include
export PKG_CONFIG_PATH=$CONDA_PREFIX/lib/pkgconfig:$PKG_CONFIG_PATH
# git clone --single-branch https://github.com/hyoklee/vcpkg
# cd vcpkg
# ./bootstrap-vcpkg.sh
# ./vcpkg install margo
# cd ..
# sh env.sh
cmake -B build .
# make -C build -j 4


