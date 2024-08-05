#!/bin/bash
cd config

if [ $# -lt 1 ]; then
    echo "Parámetros insuficientes"
    exit 1
fi

case $1 in
    "plani")
        if [ $# -lt 2 ]; then
            echo "Parámetros insuficientes"
            exit 1
        fi
        ./configure.sh QUANTUM 2750
        ./configure.sh RECURSOS "[RECURSO]"
        ./configure.sh INSTANCIAS_RECURSOS "[1]"
        ./configure.sh GRADO_MULTIPROGRAMACION 10
        ./configure.sh CANTIDAD_ENTRADAS_TLB 32
        ./configure.sh ALGORITMO_TLB FIFO
        ./configure.sh TAM_MEMORIA 1024
        ./configure.sh TAM_PAGINA 32
        ./configure.sh RETARDO_RESPUESTA 1000
        ./configure.sh TIPO_INTERFAZ GENERICA generica.config
        ./configure.sh TIEMPO_UNIDAD_TRABAJO 50 generica.config
        case $2 in
            "1")
                echo "Configuración de Plani (FIFO)"
                ./configure.sh ALGORITMO_PLANIFICACION FIFO
                ;;
            "2")
                echo "Configuración de Plani (RR)"
                ./configure.sh ALGORITMO_PLANIFICACION RR
                ;;
            "3")
                echo "Configuración de Prueba Plani (VRR)"
                ./configure.sh ALGORITMO_PLANIFICACION VRR
                ;;
        esac
        ;;
    "deadlock")
        ./configure.sh ALGORITMO_PLANIFICACION FIFO
        ./configure.sh QUANTUM 1500
        ./configure.sh RECURSOS "[RA,RB,RC,RD]"
        ./configure.sh INSTANCIAS_RECURSOS "[1,1,1,1]"
        ./configure.sh GRADO_MULTIPROGRAMACION 10
        ./configure.sh CANTIDAD_ENTRADAS_TLB 0
        ./configure.sh ALGORITMO_TLB FIFO
        ./configure.sh TAM_MEMORIA 1024
        ./configure.sh TAM_PAGINA 32
        ./configure.sh RETARDO_RESPUESTA 1000
        ./configure.sh TIPO_INTERFAZ GENERICA generica.config
        ./configure.sh TIEMPO_UNIDAD_TRABAJO 500 generica.config
        echo "Configuración de Deadlock"
        ;;
    "memoria")
        if [ $# -lt 2 ]; then
            echo "Parámetros insuficientes"
            exit 1
        fi
        ./configure.sh ALGORITMO_PLANIFICACION FIFO
        ./configure.sh QUANTUM 2000
        ./configure.sh RECURSOS "[RECURSO]"
        ./configure.sh INSTANCIAS_RECURSOS "[1]"
        ./configure.sh GRADO_MULTIPROGRAMACION 10
        ./configure.sh CANTIDAD_ENTRADAS_TLB 4
        ./configure.sh TAM_MEMORIA 1024
        ./configure.sh TAM_PAGINA 32
        ./configure.sh RETARDO_RESPUESTA 1000
        ./configure.sh TIPO_INTERFAZ GENERICA generica.config
        ./configure.sh TIEMPO_UNIDAD_TRABAJO 250 generica.config
        case $2 in
            "1")
                echo "Configuración de Prueba Memoria (FIFO)"
                ./configure.sh ALGORITMO_TLB FIFO
                ;;
            "2")
                echo "Configuración de Prueba Memoria (LRU)"
                ./configure.sh ALGORITMO_TLB LRU
                ;;
        esac
        ;;
    "io")
        ./configure.sh ALGORITMO_PLANIFICACION RR
        ./configure.sh QUANTUM 750
        ./configure.sh RECURSOS "[REC1]"
        ./configure.sh INSTANCIAS_RECURSOS "[1]"
        ./configure.sh GRADO_MULTIPROGRAMACION 10
        ./configure.sh CANTIDAD_ENTRADAS_TLB 0
        ./configure.sh TAM_MEMORIA 1024
        ./configure.sh TAM_PAGINA 16
        ./configure.sh RETARDO_RESPUESTA 100
        ./configure.sh TIPO_INTERFAZ GENERICA generica.config
        ./configure.sh TIEMPO_UNIDAD_TRABAJO 250 generica.config
        ./configure.sh TIPO_INTERFAZ STDIN teclado.config
        ./configure.sh TIEMPO_UNIDAD_TRABAJO 250 teclado.config
        ./configure.sh TIPO_INTERFAZ STDOUT monitor.config
        ./configure.sh TIEMPO_UNIDAD_TRABAJO 250 monitor.config
        echo "Configuración de Prueba IO"
        ;;
    "fs")
        ./configure.sh ALGORITMO_PLANIFICACION RR
        ./configure.sh QUANTUM 5000
        ./configure.sh RECURSOS [REC1]
        ./configure.sh INSTANCIAS_RECURSOS [1]
        ./configure.sh GRADO_MULTIPROGRAMACION 10
        ./configure.sh CANTIDAD_ENTRADAS_TLB 0
        ./configure.sh TAM_MEMORIA 1024
        ./configure.sh TAM_PAGINA 16
        ./configure.sh RETARDO_RESPUESTA 100
        ./configure.sh TIPO_INTERFAZ DIALFS fs.config
        ./configure.sh TIEMPO_UNIDAD_TRABAJO 2000 fs.config
        ./configure.sh PATH_BASE_DIALFS dialfs fs.config
        ./configure.sh BLOCK_SIZE 16
        ./configure.sh BLOCK_COUNT 32
        ./configure.sh RETRASO_COMPACTACION 7500
        ./configure.sh TIPO_INTERFAZ STDIN teclado.config
        ./configure.sh TIEMPO_UNIDAD_TRABAJO 250 teclado.config
        ./configure.sh TIPO_INTERFAZ STDOUT monitor.config
        ./configure.sh TIEMPO_UNIDAD_TRABAJO 250 monitor.config
        echo "Configuración de Prueba FS"
        ;;
    "final")
        ./configure.sh ALGORITMO_PLANIFICACION VRR
        ./configure.sh QUANTUM 500
        ./configure.sh RECURSOS "[RA,RB,RC,RD]"
        ./configure.sh INSTANCIAS_RECURSOS "[1,1,1,1]"
        ./configure.sh GRADO_MULTIPROGRAMACION 3
        ./configure.sh CANTIDAD_ENTRADAS_TLB 16
        ./configure.sh ALGORITMO_TLB LRU
        ./configure.sh TAM_MEMORIA 4096
        ./configure.sh TAM_PAGINA 32
        ./configure.sh RETARDO_RESPUESTA 100
        ./configure.sh TIPO_INTERFAZ GENERICA generica.config
        ./configure.sh TIEMPO_UNIDAD_TRABAJO 250 generica.config
        ./configure.sh TIPO_INTERFAZ GENERICA slp1.config
        ./configure.sh TIEMPO_UNIDAD_TRABAJO 50 slp1.config
        ./configure.sh TIPO_INTERFAZ GENERICA espera.config
        ./configure.sh TIEMPO_UNIDAD_TRABAJO 500 espera.config
        ./configure.sh TIPO_INTERFAZ STDIN teclado.config
        ./configure.sh TIEMPO_UNIDAD_TRABAJO 250 teclado.config
        ./configure.sh TIPO_INTERFAZ STDOUT monitor.config
        ./configure.sh TIEMPO_UNIDAD_TRABAJO 250 monitor.config
        echo "Configuración de Prueba Salvation's Edge"
        ;;
    *)
        echo "No existe la prueba"
        ;;
esac
