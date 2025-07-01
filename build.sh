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
mkdir build
cd build 
cmake ..
cmake --build . --config Release 
# cmake --install . --config Release 
# make -C build -j 4


