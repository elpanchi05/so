#include <stdlib.h>
#include <stdio.h>
#include <utils/structures.h>
#include <utils/paquete.h>
#include <utils/setup.h>
#include <utils/socket.h>
#include <utils/environment_variables.h>
#include <pthread.h>
#include <semaphore.h>
#include <math.h>
#include <time.h>

t_log* logger;
t_config* config;
uint32_t socket_memoria;
uint32_t socket_cpu_interrupt;
uint32_t socket_cpu_dispatch;
uint32_t socket_kernel_interrupt;
uint32_t socket_kernel_dispatch;

int interrupt = -1;
op_code interrupt_code = -1;
bool resultado_ejecucion = 1;
pthread_mutex_t mutex_interrupt;
uint32_t tam_pagina;
int algoritmo = 0;
int cantidad_entradas = 0;
t_list* proximas_victimas = NULL;
t_list* tlb;
bool hay_tlb = false;

void conectar_modulos();
t_instruccion* obtener_instruccion(uint32_t pid, uint32_t program_counter);
t_instruccion* deserializar_instruccion(t_buffer* buffer, uint32_t pid);
bool ejecutar_instruccion(t_contexto* contexto, t_instruccion* instruccion);
t_registro registro_str_a_enum(char* reg);
void check_interrupt();
uint32_t dl_a_df(uint32_t dl, uint32_t pid);
void solicitar_marco_a_memoria(uint32_t pagina, uint32_t pid);
t_buffer* crear_buffer_con_df(uint32_t tamanio, uint32_t dl, t_contexto* contexto, char** log);
void solicitar_escritura(uint32_t tamanio, uint32_t dl, t_contexto* contexto, uint8_t* contenido_ext_8);
int consultar_tlb(uint32_t pagina, uint32_t pid);
void agregar_entrada(uint32_t pid, uint32_t pagina, uint32_t marco);
void reemplazar_entrada(uint32_t pid, uint32_t pagina, uint32_t marco);
uint32_t extraer_entero_de_registro(t_contexto* ctx, t_registro reg);