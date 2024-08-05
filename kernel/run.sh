#!/bin/bash

make all
if [ $? -ne 0 ]; then
    echo "Make failed in KERNEL"
    exit 1
fi

#PIPE=/tmp/input_pipe
#mkfifo $PIPE
./bin/kernel  ../config/kernel.config #< $PIPE &
#echo "INICIAR_PROCESO ../tests/test1.txt" > $PIPE
#rm $PIPE

if [ $? -ne 0 ]; then
    echo "Execution failed for KERNEL"
    exit 1
fi
