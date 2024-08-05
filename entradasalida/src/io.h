#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <utils/structures.h>
#include <utils/paquete.h>
#include <utils/setup.h>
#include <utils/socket.h>
#include <utils/environment_variables.h>
#include <commons/bitarray.h>
#include <commons/collections/dictionary.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <commons/memory.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>

typedef struct {
    t_config* config;
    char* nombre;
    char* path;
    uint32_t bloque_inicial;
    uint32_t tamanio;
} t_archivo;

uint32_t retraso_compactacion;
uint32_t tamanio_bitmap;
t_log* logger;
t_config* config;
uint32_t socket_memoria;
uint32_t socket_kernel;
char* tipo_interfaz;
char* nombre_interfaz;
int tiempo_unidad_de_trabajo;
int block_size;
int block_count;
char* path_base_dialfs;
char* nombre_interfaz;
int fd_archivo_de_bloques;
int tam_archivo_de_bloques;
void* ptr_a_bloques;
int fd_bitmap;
char* puntero_bitmap;
t_bitarray* bloques_libres;
int ip_kernel;
int puerto_memoria;
int ip_memoria;
int puerto_kernel;
t_bitarray* bitarray_bloques;
uint32_t tam_pagina;
t_list* archivos_abiertos;
t_dictionary* bloques_iniciales;

const char *instruccion_string[] = {
    "IO_GEN_SLEEP",
    "IO_STDIN_READ",
    "IO_STDOUT_WRITE",
    "IO_FS_CREATE",
    "IO_FS_DELETE",
    "IO_FS_TRUNCATE",
    "IO_FS_WRITE",
    "IO_FS_READ"
};

void conectar_modulos();
void iniciar_interfaz(t_paquete* paquete, uint32_t pid);
void manejar_interfaz_generica(t_paquete* paquete);
void manejar_interfaz_STDOUT(t_paquete *paquete);
void manejar_interfaz_STDIN(t_paquete *paquete);
void clean_stdin();

void manejar_interfaz_DialFS(t_paquete *paquete, uint32_t pid);
void inicializar_estructuras_fs();
void inicializar_archivo_de_bloques();
void inicializar_bitmap();
void levantar_metadata_persistidos();
void crear_archivo(uint32_t pid, char* nombre_archivo);
void cerrar_archivo(t_archivo* archivo);
void borrar_archivo(t_archivo* archivo);
void asignar_bloques_contiguos(uint32_t pid, int tamanio_nuevo, t_archivo* archivo);
void sacar_bloques_bitmap(int cant_bloques_a_sacar, t_archivo* archivo);
t_archivo* buscar_archivo(char* nombre);
void truncar_archivo(uint32_t pid, int tamanio_nuevo, t_archivo* archivo);
void solicitar_escritura_en_memoria(t_paquete* paquete_io, void* aux, uint32_t pags_a_escribir, uint32_t tamanio, char* buffer_texto);
void escribir_archivo_de_bloques(int bloque, void* contenido, uint32_t tamanio);
char* leer_archivo_de_bloques(int bloque, uint32_t tamanio);
void solicitar_escritura_en_memoria(t_paquete* paquete_io, void* aux, uint32_t pags_a_escribir, uint32_t tamanio, char* buffer_texto);
uint32_t buscar_bloque_libre();
void compactar_fs(t_archivo* archivo);
uint32_t contar_bloques_libres();
void actualizar_bitarray();
void actualizar_bloques();