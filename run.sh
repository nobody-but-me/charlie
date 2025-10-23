
#!/usr/bin/env bash

# if [ "$(type -t ./build/bin/mol)" = "file" ]; then
if [ -f ./build/bin/charlie ]; then
    echo "[INFO]: RUNNING...\n"
    
    ./build/bin/charlie $1
    
    cd ..
else
    echo "[ERROR]: COULD NOT RUN ENGINE: EXECUTABLE DOES NOT EXIST OR HAS SOME ERROR. \n"
fi
