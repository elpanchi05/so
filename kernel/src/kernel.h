#include <stdlib.h>
#include <stdio.h>
#include <utils/structures.h>
#include <utils/paquete.h>
#include <utils/setup.h>
#include <utils/socket.h>
#include <utils/environment_variables.h>
#include <semaphore.h>
#include <pthread.h>
#include <time.h>
#include <commons/temporal.h>
#include <commons/string.h>

typedef struct {
    t_io* io;
} t_struct_esperar_io;

typedef struct {
    uint32_t quantum;
} t_quantum_remanente;

typedef struct {
    t_pcb* pcb;
    op_code operacion;
    t_paquete* paquete_bloqueado;
} t_pcb_bloqueado_io;

uint32_t PID = 0;
int GRADO_MULTIPROG;
char* ALGORITHM;
bool planificacion_apagada = false;

t_log* logger;
t_config* config;
uint32_t socket_memoria;
uint32_t socket_cpu_dispatch;
uint32_t socket_cpu_interrupt;
uint32_t socket_kernel;
t_list* interfaces;
char** recursos;
int* instancias_recursos;
int cantidad_recursos;
int quantum;
t_list* list_priority_ready;
double tiempo_ejecutado;
clock_t tiempo_salida, tiempo_llegada;
t_temporal* tiempo_ejecucion; 

// Semaphores
sem_t semaphore_ready;
sem_t semaphore_multiprog;
sem_t semaphore_new;
sem_t sem_proceso_en_io;
sem_t sem_cpu_available;
sem_t sem_cpu_executing;
sem_t sem_prender_planificacion;

// Mutexs
pthread_mutex_t mutex_new;
pthread_mutex_t mutex_ready;
pthread_mutex_t mutex_running_pcb;
pthread_mutex_t mutex_priority_ready;
pthread_mutex_t mutex_io;

// Lists
t_list *list_ready;
t_list *list_new;
t_list *list_running;
t_list *colas_bloqueados_recursos;

// Threads
pthread_t thread_consola;
pthread_t thread_conectar_ios;
pthread_t thread_cpu_listener;
pthread_t thread_largo_plazo;
pthread_t thread_corto_plazo;
pthread_t thread_quantum_corriendo;
pthread_t thread_aceptar_io;

t_pcb* running_pcb;
bool cpu_ocupada = false;
bool pid_finalizado = false;

void inicializar_variables();
void conectar_modulos();
void correr_consola();
void conectar_ios();
void agregar_pcb_thread_safe(t_pcb *pcb, t_list *list, pthread_mutex_t mu);
t_pcb *remover_pcb_thread_safe(t_list *list, pthread_mutex_t mu);
static void planificador_corto_plazo();
static void planificador_largo_plazo();
t_pcb *crear_pcb(char* path);
void iniciar_threads();
void ejecutar_script(char* path);
int solicitar_recurso(t_pcb *pcb, char* recurso);
int buscar_recurso(char* recurso);
int ceder_recurso(t_pcb *pcb, char* recurso);
void enviar_operacion_a_io(t_pcb *pcb, char* nombre_io, op_code operacion, t_paquete* paquete);
t_paquete* crear_paquete_sleep_io(t_pcb* pcb, op_code operacion, uint32_t sleep);
void* esperar_io(void* args);
void enviar_operacion(int socket, t_pcb* pcb, op_code operacion, uint32_t sleep);
t_io* buscar_io(char* nombre);
void validar_operacion(t_io* io, op_code operacion);
void iniciar_quantum(void* args);
void parsear_linea(char* line);
void finalizar_proceso(t_pcb* pcb, const char* motivo);
void enviar_pcb_a_cpu(t_pcb *pcb);
void recibir_contexto_de_cpu();
void conectar_ios();
void enviar_interrupcion(op_code op_code, uint32_t pid);
void obtener_procesos_estado();
void imprimir_cola_ready();
void imprimir_cola_ready_prioritaria();