#!/bin/bash

make all

if [ $? -ne 0 ]; then
    echo "Make failed in IO"
    exit 1
fi

case $1 in
    "plani")
        ./bin/entradasalida ../config/generica.config SLP1
        ;;
    "deadlock")
        ./bin/entradasalida ../config/generica.config ESPERA
        ;;
    "memoria")
        ./bin/entradasalida ../config/generica.config IO_GEN_SLEEP
        ;;
    "io")
        case $2 in
            "1")
                ./bin/entradasalida ../config/generica.config GENERICA
                ;;
            "2")
                ./bin/entradasalida ../config/teclado.config TECLADO
                ;;
            "3")
                ./bin/entradasalida ../config/monitor.config MONITOR
                ;;
        esac
        ;;
    "fs")
        case $2 in
            "1")
                ./bin/entradasalida ../config/fs.config FS
                ;;
            "2")
                ./bin/entradasalida ../config/teclado.config TECLADO
                ;;
            "3")
                ./bin/entradasalida ../config/monitor.config MONITOR
                ;;
        esac
        ;;
    "final")
        case $2 in
            "1")
                ./bin/entradasalida ../config/generica.config GENERICA
                ;;
            "2")
                ./bin/entradasalida ../config/slp1.config SLP1
                ;;
            "3")
                ./bin/entradasalida ../config/espera.config ESPERA
                ;;
            "4")
                ./bin/entradasalida ../config/teclado.config TECLADO
                ;;
            "5")
                ./bin/entradasalida ../config/monitor.config MONITOR
                ;;
        esac
        ;;
esac
    

if [ $? -ne 0 ]; then
    echo "Execution failed for IO"
    exit 1
fi
