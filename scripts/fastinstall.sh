git clone https://github.com/tezevsk/Sbix.git
cd ./Sbix

mkdir build

cmake -B build -DCMAKE_BUILD_TYPE=Release .
cmake --build build

cmake --install build
echo "Sbix installed succesfully. Thank you"
