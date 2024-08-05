#include <stdlib.h>
#include <stdio.h>
#include <utils/structures.h>
#include <utils/paquete.h>
#include <utils/setup.h>
#include <utils/socket.h>
#include <utils/environment_variables.h>
#include <commons/collections/list.h>
#include <commons/bitarray.h>
#include <commons/memory.h>
#include "parser.h"
#include <pthread.h>
#include <math.h>

typedef struct {
    uint32_t socket_io;
} t_socket_io;

t_log* logger;
t_config* config;
uint32_t socket_memoria;
uint32_t socket_cpu;
uint32_t socket_kernel;

uint32_t tam_memoria;
uint32_t tam_pagina;
uint32_t cant_marcos;
uint32_t retardo;
void* memoria;
t_bitarray* bitmap_marcos;
t_list* procesos;

pthread_t thread_kernel;
pthread_t thread_cpu;
pthread_t thread_aceptar_io;
pthread_t thread_io;
pthread_mutex_t mutex_procesos;
pthread_mutex_t mutex_memoria;
pthread_mutex_t mutex_bitarray_marcos;

void conectar_modulos();
static void atender_cpu();
void* atender_kernel(void* arg);
void atender_io(void* args);
bool ampliar_proceso(t_proceso* proceso, uint32_t tamanio_actual, uint32_t tamanio);
void reducir_proceso(t_proceso* proceso, uint32_t tamanio_actual, uint32_t tamanio);
void liberar_estructuras();
void liberar_proceso(uint32_t pid);
t_proceso* buscar_proceso(uint32_t pid);
void conectar_ios();
void manejar_peticion(t_paquete* paquete);