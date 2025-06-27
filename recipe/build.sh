export CMAKE_PREFIX_PATH=$PREFIX
git clone --single-branch https://github.com/hyoklee/vcpkg
cd vcpkg
./bootstrap-vcpkg.sh
./vcpkg install cte-hermes-shm
cd ..
sh env.sh
cmake -B build .
make -C build -j


