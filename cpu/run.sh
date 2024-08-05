#!/bin/bash
make all
if [ $? -ne 0 ]; then
    echo "Make failed in CPU"
    exit 1
fi
./bin/cpu ../config/cpu.config
if [ $? -ne 0 ]; then
    echo "Execution failed for CPU"
    exit 1
fi