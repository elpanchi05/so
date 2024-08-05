#!/bin/bash
make all
if [ $? -ne 0 ]; then
    echo "Make failed in MEMORIA"
    exit 1
fi
./bin/memoria ../config/memoria.config
if [ $? -ne 0 ]; then
    echo "Execution failed for MEMORIA"
    exit 1
fi