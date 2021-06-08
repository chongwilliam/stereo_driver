mkdir -p build_dbg &&
cd build_dbg &&
cmake .. -DCMAKE_BUILD_TYPE=Debug &&
make -j4 &&
cd ..
