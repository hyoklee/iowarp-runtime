export CMAKE_PREFIX_PATH=$PREFIX
git clone --single-branch https://github.com/hyoklee/vcpkg
cd vcpkg
./bootstrap-vcpkg.sh
./vcpkg install content-transfer-engine
cd ..
sh env.sh
cmake -B build .
make -C build -j


