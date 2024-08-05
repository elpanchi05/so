#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <stdint.h>
#include <commons/collections/list.h>
#include <semaphore.h>

typedef enum {
    HANDSHAKE,
    OK,
    INSTRUCCION,
    CREAR_PROCESO,
    FINALIZAR_PROCESO,
    CONTEXTO,
    OUT_OF_MEMORY,
    RESIZE_OP_CODE,
    IO_GEN_SLEEP_OP_CODE,
    IO_STDIN_READ_OP_CODE,
    IO_STDOUT_WRITE_OP_CODE,
    IO_FS_CREATE_OP_CODE,
    IO_FS_DELETE_OP_CODE,
    IO_FS_TRUNCATE_OP_CODE,
    IO_FS_WRITE_OP_CODE,
    IO_FS_READ_OP_CODE,
    WAIT_OP_CODE,
    SIGNAL_OP_CODE,
    FIN_Q,
    SOLICITAR_MARCO,
    LECTURA,
    ESCRITURA,
    STRCPY,
    BLOQUEADO,
    INVALID_INTERFACE,
    INVALID_RESOURCE,
    EXIT_OP_CODE
} op_code;

typedef enum
{
    NEW,
    READY,
    RUNNING,
    BLOCKED,
    FINISHED
} estado_proceso;

typedef enum {
    SET,
    SUM,
    SUB,
    JNZ,
    SLEEP,
    WAIT,
    SIGNAL,
    RESIZE,
    MOV_IN,
    MOV_OUT,
    COPY_STRING,
    IO_GEN_SLEEP,
    IO_STDIN_READ,
    IO_STDOUT_WRITE,
    IO_FS_CREATE,
    IO_FS_DELETE,
    IO_FS_TRUNCATE,
    IO_FS_WRITE,
    IO_FS_READ,
    EXIT,
} t_codigo_instruccion;

typedef struct {
    t_codigo_instruccion codigo_instruccion;
    t_list* parametros;
} t_instruccion;

typedef struct {
    uint32_t size;
    void* stream;
} t_buffer;

typedef struct {
    op_code codigo_operacion;
    t_buffer* buffer;
} t_paquete;

typedef struct {
    uint32_t pid;
	uint32_t program_counter;
    uint8_t registros[4]; 
    uint32_t registros_ext[4];
    uint32_t registros_dir[2];
} t_contexto;

typedef struct {
    estado_proceso estado;
    t_contexto contexto;
    uint32_t quantum;
    char* path_instrucciones;
    t_list* recursos_tomados;
} t_pcb;

typedef enum {
    AX,
    BX,
    CX,
    DX,
    EAX,
    EBX,
    ECX,
    EDX,
    SI,
    DI,
    PC
} t_registro;

typedef struct {
    uint32_t pid;
    uint32_t pagina;
    uint32_t marco;
    clock_t ultimo_acceso;
} t_entrada_tlb;

typedef struct {
    uint32_t socket_io;
    sem_t sem_io;
    char* nombre;
    t_list* cola_bloqueados;
    t_list* operaciones;
} t_io;

// ----- MEMORIA ----- //
typedef struct {
    uint32_t pid;
    t_list* instrucciones;
    t_list* tabla_paginas;
} t_proceso;

#endif