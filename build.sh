
#!/usr/bin/env bash

echo "[CONFIG]: Are you generating ninja files? (y or n)(DEFAULT: n)"
read -p " > " build
echo "[CONFIG]: Do you want to run the project right after building it? (y or n)(DEFAULT: y)"
read -p " > " run

echo "[INFO]: CREATING BUILD FOLDER..."
mkdir -p ./build && cd ./build

if [ "$build" = "y" ]; then
    echo "[INFO]: GENERATING NINJA FILES..."
    cmake .. -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
fi

echo "[INFO]: BUILDING..."
ninja

cd ..

if  [ "$run" != "n" ]; then
    echo "\n--------------------------------------------------\n"
    ./run.sh
fi

