#include "kernel.h"

int main(int argc, char* argv[]) {
    initialize_setup(argv[1], "kernel", &logger, &config);
	inicializar_variables();
	conectar_modulos();
	iniciar_threads(); 
	pthread_join(thread_consola, NULL);
}

void inicializar_variables()
{
    list_new = list_create();
    list_ready = list_create();
    list_priority_ready = list_create();

    char* multiprog = config_get_string_value(config, "GRADO_MULTIPROGRAMACION");
    GRADO_MULTIPROG = atoi(multiprog);

    sem_init(&semaphore_ready, 0, 0);
    sem_init(&semaphore_multiprog, 0, GRADO_MULTIPROG);
    sem_init(&semaphore_new, 0, 0);
    sem_init(&sem_proceso_en_io, 0, 0);
    sem_init(&sem_cpu_available, 0, 1);
    sem_init(&sem_cpu_executing, 0, 0);
    sem_init(&sem_prender_planificacion, 0, 0);

    pthread_mutex_init(&mutex_new, NULL);
    pthread_mutex_init(&mutex_ready, NULL);
    pthread_mutex_init(&mutex_running_pcb, NULL);
    pthread_mutex_init(&mutex_priority_ready, NULL);
    pthread_mutex_init(&mutex_io, NULL);

    ALGORITHM = config_get_string_value(config, "ALGORITMO_PLANIFICACION");
    if (!strcmp(ALGORITHM, "RR") || !strcmp(ALGORITHM, "VRR")) quantum = config_get_int_value(config, "QUANTUM");

    colas_bloqueados_recursos = list_create();
    recursos = config_get_array_value(config, "RECURSOS"); 
	char** instancias_str = config_get_array_value(config, "INSTANCIAS_RECURSOS");
    int size = 0;
	while(instancias_str[size]!= NULL){
		size++;
	}
    cantidad_recursos = size;
	instancias_recursos = (int*)malloc(size * sizeof(int));
	for(int i= 0; i<size; i++){
		instancias_recursos[i] = atoi(instancias_str[i]);
        t_list* cola_bloqueados = list_create();
        list_add(colas_bloqueados_recursos, cola_bloqueados);
	}

    interfaces = list_create();
}

void conectar_modulos() {
    socket_memoria = start_client_module("MEMORIA", IP_CONFIG_PATH);
	log_info(logger, "Conexion con servidor MEMORIA en socket %d", socket_memoria);

    socket_cpu_interrupt = start_client_module("CPU_INTERRUPT", IP_CONFIG_PATH);
	log_info(logger, "Conexion con servidor CPU INTERRUPT en socket %d", socket_cpu_interrupt);

	socket_cpu_dispatch = start_client_module("CPU_DISPATCH", IP_CONFIG_PATH);
	log_info(logger, "Conexion con servidor CPU DISPATCH en socket %d", socket_cpu_dispatch);

	socket_kernel = start_server_module("KERNEL", IP_CONFIG_PATH);
	log_info(logger, "Creacion de servidor KERNEL en socket %d", socket_kernel);

    pthread_create(&thread_conectar_ios, NULL, (void*)conectar_ios, NULL);
    pthread_detach(thread_conectar_ios);
}

void conectar_ios() { 
    uint32_t socket_io;
    while(1) {
        socket_io = accept(socket_kernel, NULL, NULL);
	    log_info(logger, "Conexion con cliente IO en socket %d", socket_io);
        t_paquete* paquete = recibir_paquete(socket_io);
        void* stream = paquete->buffer->stream;
        size_t tamanio_nombre = 0;
        char* nombre = NULL;
        uint32_t cantidad_operaciones = 0;
        t_io* io = malloc(sizeof(t_io));
        
        memcpy(&tamanio_nombre, stream , sizeof(size_t));
        stream += sizeof(size_t);
        nombre = malloc(tamanio_nombre);
        
        memcpy(nombre, stream, tamanio_nombre);
        stream += tamanio_nombre;

        memcpy(&cantidad_operaciones, stream, sizeof(uint32_t));
        stream += sizeof(uint32_t);
        io->socket_io = socket_io;
        sem_init(&io->sem_io, 0, 0);
        io->nombre = nombre;
        io->cola_bloqueados = list_create();
        io->operaciones = list_create();
    
        for(int i = 0; i < cantidad_operaciones; i++) {
            op_code* op = malloc(sizeof(op_code));
            memcpy(op, stream, sizeof(op_code));
            stream += sizeof(op_code);
            list_add(io->operaciones, op);
        }

        pthread_mutex_lock(&mutex_io);
        list_add(interfaces, io);
        pthread_mutex_unlock(&mutex_io);

        pthread_t thread_io_en_proceso;
        t_struct_esperar_io* args = malloc(sizeof(t_struct_esperar_io));
        args->io = io;
        pthread_create(&thread_io_en_proceso, NULL, esperar_io, (void*)args);
        pthread_detach(thread_io_en_proceso);
        
        eliminar_paquete(paquete);
    }
}

void iniciar_threads() {
    pthread_create(&thread_consola, NULL, (void*)correr_consola, NULL);
    pthread_create(&thread_cpu_listener, NULL, (void*)recibir_contexto_de_cpu, NULL);
    pthread_create(&thread_corto_plazo, NULL, (void*)planificador_corto_plazo, NULL);
    pthread_create(&thread_largo_plazo, NULL, (void*)planificador_largo_plazo, NULL);
    pthread_create(&thread_aceptar_io, NULL, (void*)conectar_ios, NULL);
}

void correr_consola() {

    printf("Ingrese a continuacion los comandos que desea ejecutar:\n");
    while (1)
    {

        char line[1000];

        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            perror("Error al leer la linea.\n");
            break;
        }
		parsear_linea(line);
        //clear_input_buffer();
    }
}

void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { }
}

void parsear_linea(char* line) {
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n')
    {
        line[len - 1] = '\0';
    }

    char **args = string_split(line, " ");

	/*
    EJECUTAR_SCRIPT ../tests/scripts/PRUEBA_PLANI
    EJECUTAR_SCRIPT /home/utnso/Desktop/c-comenta-pruebas/scripts_kernel/PRUEBA_DEADLOCK
    EJECUTAR_SCRIPT /home/utnso/Desktop/c-comenta-pruebas/scripts_kernel/PRUEBA_IO
    EJECUTAR_SCRIPT /home/utnso/Desktop/c-comenta-pruebas/scripts_memoria/PRUEBA_MEMORIA
    INICIAR_PROCESO /home/utnso/Desktop/c-comenta-pruebas/scripts_memoria/MEMORIA_1
    INICIAR_PROCESO /home/utnso/Desktop/c-comenta-pruebas/scripts_memoria/FS_1
    INICIAR_PROCESO /home/utnso/Desktop/tp-2024-1c-La-Tercera/tests/test1.txt
    INICIAR_PROCESO /home/utnso/Desktop/tp-2024-1c-La-Tercera/tests/test2.txt
    FINALIZAR_PROCESO 2
    FINALIZAR_PROCESO 3
	*/

    if (string_equals_ignore_case(args[0], "EJECUTAR_SCRIPT")) {
	    if (args[1] != NULL) {
	        ejecutar_script(args[1]);
	    } else {
	        perror("Argumentos insuficientes\n");
	    }
    }

    if (string_equals_ignore_case(args[0], "INICIAR_PROCESO")) {
		if (args[1] != NULL) {
			t_pcb* pcb = crear_pcb(args[1]);
            size_t largo = strlen(args[1]) + 1;
            char* path = malloc(largo);
            strcpy(path, args[1]);
			crear_proceso_en_memoria(pcb->contexto.pid, path, socket_memoria);
            esperar_ok(socket_memoria, logger);
			agregar_pcb_thread_safe(pcb, list_new, mutex_new);
            sem_post(&semaphore_new);
            log_info(logger, "Se crea el proceso %d en NEW", pcb->contexto.pid);
        }
		else {
			perror("Argumentos insuficientes\n");
		}
    }

    if (string_equals_ignore_case(args[0], "FINALIZAR_PROCESO")) {
		if (args[1] != NULL) {
            t_pcb* pcb = NULL;
            uint32_t pid = atoi(args[1]);

            // TODO: mutex?
            //log_trace(logger, "id a finalizar %d %d", running_pcb->contexto.pid, pid); // TODO: si se bloquean todos queda en running el ultimo en bloquearse
            
            if(running_pcb != NULL) {
                pthread_mutex_lock(&mutex_running_pcb);
                if(running_pcb->contexto.pid == pid) {
                    log_trace(logger, "Buscando proceso en colas");
                    log_info(logger, "PID: %d - Estado Anterior: EXEC - Estado Actual: EXIT", running_pcb->contexto.pid);
                    enviar_interrupcion(FINALIZAR_PROCESO, running_pcb->contexto.pid);
                    pthread_mutex_unlock(&mutex_running_pcb);
                    return;
                }
                pthread_mutex_unlock(&mutex_running_pcb);
            }

            pthread_mutex_lock(&mutex_new);
            for(int i = 0; i < list_size(list_new); i++) {
                pcb = list_get(list_new, i);
                if(pcb->contexto.pid == pid) {
                    list_remove_element(list_new, pcb);
                    pthread_mutex_unlock(&mutex_new);
                    log_info(logger, "PID: %d - Estado Anterior: NEW - Estado Actual: EXIT", pcb->contexto.pid);
                    goto fin;
                }
            }
            pthread_mutex_unlock(&mutex_new);

            
            pthread_mutex_lock(&mutex_ready);
            for(int i = 0; i < list_size(list_ready); i++) {
                pcb = list_get(list_ready, i);
                if(pcb->contexto.pid == pid) {
                    list_remove_element(list_ready, pcb);
                    sem_post(&semaphore_multiprog);
                    pthread_mutex_unlock(&mutex_ready);
                    log_info(logger, "PID: %d - Estado Anterior: READY - Estado Actual: EXIT", pcb->contexto.pid);
                    goto fin;
                }
            }
            pthread_mutex_unlock(&mutex_ready);

            for(int i = 0; i < cantidad_recursos; i++) {
                t_list* cola_bloqueados = list_get(colas_bloqueados_recursos, i);
                for(int j = 0; j < list_size(cola_bloqueados); j++) {
                    // TODO: mutex
                    pcb = list_get(cola_bloqueados, j);
                    if(pcb->contexto.pid == pid) {
                        list_remove_element(cola_bloqueados, pcb);
                        sem_post(&semaphore_multiprog);
                        log_info(logger, "PID: %d - Estado Anterior: BLOCK - Estado Actual: EXIT", pcb->contexto.pid);
                        goto fin;
                    }
                }
            }

            pthread_mutex_lock(&mutex_io);
            for(int i = 0; i < list_size(interfaces); i++) {
                t_io* io = list_get(interfaces, i);
                for(int j = 0; j < list_size(io->cola_bloqueados); j++) {
                    t_pcb_bloqueado_io* pcb_bloqueado = list_get(io->cola_bloqueados, j);
                    pcb = pcb_bloqueado->pcb;
                    if(pcb->contexto.pid == pid) {
                        list_remove_element(io->cola_bloqueados, pcb);
                        pid_finalizado = true;
                        sem_post(&semaphore_multiprog);
                        pthread_mutex_unlock(&mutex_io);
                        log_info(logger, "PID: %d - Estado Anterior: BLOCK - Estado Actual: EXIT", pcb->contexto.pid);
                        goto fin;
                    }
                }
            }
            pthread_mutex_unlock(&mutex_io);

            fin:
            finalizar_proceso(pcb, "INTERRUPTED_BY_USER");
		} 
        else perror("Argumentos insuficientes\n");
    }

    if (string_equals_ignore_case(args[0], "DETENER_PLANIFICACION")) {
        planificacion_apagada = true;
    }

    if (string_equals_ignore_case(args[0], "INICIAR_PLANIFICACION")) {
        planificacion_apagada = false;
        sem_post(&sem_prender_planificacion);
        sem_post(&sem_prender_planificacion);
        sem_post(&sem_prender_planificacion);
    }

    if (string_equals_ignore_case(args[0], "MULTIPROGRAMACION")) {
        if (args[1] != NULL) {
            int nuevo_grado = atoi(args[1]);
            if(GRADO_MULTIPROG < nuevo_grado) {
                for(int i = 0; i < nuevo_grado - GRADO_MULTIPROG; i++) {
                    sem_post(&semaphore_multiprog);
                }
            }
            else if(GRADO_MULTIPROG > nuevo_grado) {
                // TODO: hilo para decrementar gradualmente ??
            }
            GRADO_MULTIPROG = nuevo_grado;
        } else {
            perror("Argumentos insuficientes\n");
        }
    }

    if (string_equals_ignore_case(args[0], "PROCESO_ESTADO")) {
        obtener_procesos_estado();
    }

    for (int i = 0; args[i] != NULL; i++) {
        free(args[i]);
    }
	free(args);
}

void destruir_pcb(t_pcb* pcb) {
    for(int i = 0; i < list_size(pcb->recursos_tomados); i++) 
        ceder_recurso(pcb, list_get(pcb->recursos_tomados, i));
    //free(pcb->path_instrucciones);
    list_clean(pcb->recursos_tomados); 
    free(pcb);
}

void ejecutar_script(char* path) {
	FILE *script = fopen(path, "r");
    if (script == NULL) {
        perror("Error al abrir el script");
        return;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while ((read = getline(&line, &len, script)) != -1) {
        parsear_linea(line);
    }

    free(line);
    fclose(script);
}

t_pcb *crear_pcb(char* path) {
    t_pcb *pcb = (t_pcb *)malloc(sizeof(t_pcb));
	pcb->contexto.pid = PID;
	pcb->contexto.program_counter = 0;
	PID++;
    pcb->estado = NEW;
	pcb->path_instrucciones = path;
    pcb->recursos_tomados = list_create();
    pcb->quantum = quantum;
    return pcb;
}

static void planificador_largo_plazo()
{
    while(true)
    {
        if (planificacion_apagada) {
            sem_wait(&sem_prender_planificacion);
        }
        sem_wait(&semaphore_new);
        sem_wait(&semaphore_multiprog);
        t_pcb *pcb = remover_pcb_thread_safe(list_new, mutex_new);
        agregar_pcb_thread_safe(pcb, list_ready, mutex_ready);
        sem_post(&semaphore_ready);
        log_info(logger, "PID: %d - Estado Anterior: NEW - Estado Actual: READY", pcb->contexto.pid);
        imprimir_cola_ready();
    }
}

static void planificador_corto_plazo()
{
    while(true)
    {
        if (planificacion_apagada) {
            sem_wait(&sem_prender_planificacion);
        }
        sem_wait(&sem_cpu_available);
        sem_wait(&semaphore_ready);
        if (!strcmp(ALGORITHM, "FIFO") || !strcmp(ALGORITHM, "RR"))
        {
            t_pcb *pcb = remover_pcb_thread_safe(list_ready, mutex_ready);
            if (pcb == NULL) {
                log_trace(logger, "BUG ENCONTRADO");
                sem_post(&sem_cpu_available);
                continue;
            }
            log_info(logger, "PID: %d - Estado Anterior: READY - Estado Actual: EXEC", pcb->contexto.pid);
            enviar_pcb_a_cpu(pcb);
            cpu_ocupada = true;
        }
        else if (!strcmp(ALGORITHM, "VRR"))
        {
            t_pcb* pcb = NULL;
            if (list_size(list_priority_ready) == 0) {
                pcb = remover_pcb_thread_safe(list_ready, mutex_ready);
            }
            else pcb = remover_pcb_thread_safe(list_priority_ready, mutex_ready);
            log_info(logger, "PID: %d - Estado Anterior: READY - Estado Actual: EXEC", pcb->contexto.pid);
            enviar_pcb_a_cpu(pcb);
            cpu_ocupada = true;
        }
        else
        {
            log_error(logger, "No se determino el algoritmo de planificacion a emplear");
        }
    }
}

void imprimir_cola_ready() {
    char* log = string_from_format("Cola Ready: ");
    t_pcb* pcb = NULL;
    pthread_mutex_lock(&mutex_ready);
    for (int i = 0; i < list_size(list_ready); i++)
    {
        pcb = list_get(list_ready, i);
        string_append_with_format(&log, "- %d -", pcb->contexto.pid);
    }
    pthread_mutex_unlock(&mutex_ready);
    log_info(logger, "%s", log);
}

void imprimir_cola_ready_prioritaria() {
    char* log = string_from_format("Cola Ready Prioridad: ");
    t_pcb* pcb = NULL;
    pthread_mutex_lock(&mutex_priority_ready);
    for (int i = 0; i < list_size(list_priority_ready); i++)
    {
        pcb = list_get(list_priority_ready, i);
        string_append_with_format(&log, "- %d -", pcb->contexto.pid);
    }
    pthread_mutex_unlock(&mutex_priority_ready);
    log_info(logger, "%s", log);
}

t_pcb *remover_pcb_thread_safe(t_list *list, pthread_mutex_t mu)
{
    t_pcb *pcb;
    pthread_mutex_lock(&mu);
    if (list_size(list) > 0) {
        pcb = list_remove(list, 0);
    } else pcb = NULL;
    pthread_mutex_unlock(&mu);
    return pcb;
}

void agregar_pcb_thread_safe(t_pcb *pcb, t_list *list, pthread_mutex_t mu)
{
    pthread_mutex_lock(&mu);
    list_add(list, pcb);
    pthread_mutex_unlock(&mu);
}

void enviar_pcb_a_cpu(t_pcb *pcb)
{
	pcb->estado = RUNNING;
    pthread_mutex_lock(&mutex_running_pcb);
    running_pcb = pcb;
    enviar_contexto(&(pcb->contexto), socket_cpu_dispatch); // TODO: limpiar contexto?
    pthread_mutex_unlock(&mutex_running_pcb);
    tiempo_ejecucion = temporal_create();
    sem_post(&sem_cpu_executing);
    cpu_ocupada = true;
    t_quantum_remanente* q = malloc(sizeof(t_quantum_remanente));
    if (!strcmp(ALGORITHM, "VRR")) {
        q->quantum = pcb->quantum;
        pthread_create(&thread_quantum_corriendo, NULL, (void*)iniciar_quantum, (void*)q);
        pthread_detach(thread_quantum_corriendo);
    }
    if (!strcmp(ALGORITHM, "RR")) {
        q->quantum = quantum;
        pthread_create(&thread_quantum_corriendo, NULL, (void*)iniciar_quantum, (void*)q);
        pthread_detach(thread_quantum_corriendo);
    }
}

void recibir_contexto_de_cpu()
{
    while (true) {
        sem_wait(&sem_cpu_executing);
        t_paquete* paquete = recibir_paquete(socket_cpu_dispatch);
        temporal_stop(tiempo_ejecucion);
        if (!strcmp(ALGORITHM, "RR") || !strcmp(ALGORITHM, "VRR")) {
            pthread_cancel(thread_quantum_corriendo);
        }
        if (planificacion_apagada) {
            sem_wait(&sem_prender_planificacion);
        }
        t_contexto* contexto = deserializar_contexto(paquete->buffer);
        pthread_mutex_lock(&mutex_running_pcb);
        running_pcb->contexto = *contexto; 
        //log_trace(logger, "Proceso %d devuelto", running_pcb->contexto.pid);
        t_paquete* paquete_opcode = recibir_paquete(socket_cpu_dispatch);
        //log_debug(logger, "Código recibido: %d", paquete_opcode->codigo_operacion);
        if (!strcmp(ALGORITHM, "VRR")) {
            if (paquete_opcode->codigo_operacion != FIN_Q) {
                running_pcb->quantum = quantum - temporal_gettime(tiempo_ejecucion);
                //log_trace(logger, "Q restante %d", running_pcb->quantum);
            } 
            else {
               running_pcb->quantum = quantum;
            }
        }
    
        switch(paquete_opcode->codigo_operacion) {
            case WAIT_OP_CODE:
            {
                void* stream = paquete_opcode->buffer->stream;
                uint32_t tamanio_recurso;
                char* recurso;
                memcpy(&tamanio_recurso, stream, sizeof(uint32_t));
                stream += sizeof(uint32_t);
                recurso = malloc(tamanio_recurso);
                memcpy(recurso, stream, tamanio_recurso);
                
                int res = solicitar_recurso(running_pcb, recurso);
                if (res == -1) {
                    enviar_interrupcion(INVALID_RESOURCE, running_pcb->contexto.pid);
                    finalizar_proceso(running_pcb, "INVALID_RESOURCE");
                    sem_post(&sem_cpu_executing);
                    break;
                } else if (res == 0) {
                    log_info(logger, "PID: %d - Estado Anterior: RUNNING - Estado Actual: BLOCK", running_pcb->contexto.pid);
                    enviar_codigo_solo(BLOQUEADO, socket_cpu_dispatch);
                    running_pcb = NULL;
                    sem_post(&sem_cpu_available);
                    break;
                }

                enviar_codigo_solo(OK, socket_cpu_dispatch);
                sem_post(&sem_cpu_executing);
                break;
            }
            case SIGNAL_OP_CODE:
            {
                void* stream = paquete_opcode->buffer->stream;
                int tamanio_recurso;
                char* recurso;
                memcpy(&tamanio_recurso, stream , sizeof(int));
                stream += sizeof(int);
                recurso = malloc(tamanio_recurso);
                memcpy(recurso, stream, tamanio_recurso);

                int res = ceder_recurso(running_pcb, recurso);
                if (res == -1) {
                    log_error(logger, "Recurso no existente");
                    exit(EXIT_FAILURE);
                }

                enviar_codigo_solo(OK, socket_cpu_dispatch);
                sem_post(&sem_cpu_executing);
                break;
            }
            case IO_STDIN_READ_OP_CODE: 
            {
                size_t largo_nombre_interfaz = 0;
                char* nombre_interfaz = NULL;

                void* stream = paquete_opcode->buffer->stream;
                memcpy(&largo_nombre_interfaz, stream , sizeof(size_t));
                stream += sizeof(size_t);
                nombre_interfaz = malloc(largo_nombre_interfaz);
                memcpy(nombre_interfaz, stream, largo_nombre_interfaz);
                stream += largo_nombre_interfaz;

                uint32_t size_buffer = paquete_opcode->buffer->size - largo_nombre_interfaz - sizeof(size_t);
                t_paquete* p = crear_paquete_codigo_operacion(IO_STDIN_READ_OP_CODE);
                agregar_a_buffer(p->buffer, &contexto->pid, sizeof(uint32_t));
                agregar_a_buffer(p->buffer, stream, size_buffer);

                enviar_operacion_a_io(running_pcb, nombre_interfaz, IO_STDIN_READ_OP_CODE, p);
                enviar_codigo_solo(OK, socket_cpu_dispatch);
                running_pcb = NULL;
                sem_post(&sem_cpu_available);
                break;
            }
            case IO_STDOUT_WRITE_OP_CODE: 
            {
                size_t largo_nombre_interfaz = 0;
                char* nombre_interfaz = NULL;

                void* stream = paquete_opcode->buffer->stream;
                memcpy(&largo_nombre_interfaz, stream , sizeof(size_t));
                stream += sizeof(size_t);
                nombre_interfaz = malloc(largo_nombre_interfaz);
                memcpy(nombre_interfaz, stream, largo_nombre_interfaz);
                stream += largo_nombre_interfaz;

                uint32_t size_buffer = paquete_opcode->buffer->size - largo_nombre_interfaz - sizeof(size_t);
                t_paquete* p = crear_paquete_codigo_operacion(IO_STDOUT_WRITE_OP_CODE);
                agregar_a_buffer(p->buffer, &contexto->pid, sizeof(uint32_t));
                agregar_a_buffer(p->buffer, stream, size_buffer);
                
                enviar_operacion_a_io(running_pcb, nombre_interfaz, IO_STDOUT_WRITE_OP_CODE, p);
                enviar_codigo_solo(OK, socket_cpu_dispatch);
                running_pcb = NULL;
                sem_post(&sem_cpu_available);
                break;
            }
            case IO_GEN_SLEEP_OP_CODE:
            {
                size_t largo_nombre_interfaz = 0;
                char* nombre_interfaz = NULL;
                uint32_t tiempo_sleep = 0;

                void* stream = paquete_opcode->buffer->stream;
                memcpy(&largo_nombre_interfaz, stream , sizeof(size_t));
                stream += sizeof(size_t);
                nombre_interfaz = malloc(largo_nombre_interfaz);
                memcpy(nombre_interfaz, stream, largo_nombre_interfaz);
                stream += largo_nombre_interfaz;
                memcpy(&tiempo_sleep, stream, sizeof(uint32_t));
                t_paquete* p = crear_paquete_sleep_io(running_pcb, IO_GEN_SLEEP_OP_CODE, tiempo_sleep);

                enviar_operacion_a_io(running_pcb, nombre_interfaz, IO_GEN_SLEEP_OP_CODE, p);
                running_pcb = NULL;
                sem_post(&sem_cpu_available);
                break;
            }
            case IO_FS_CREATE_OP_CODE:
            {
                size_t largo_nombre_interfaz = 0;
                char* nombre_interfaz = NULL;

                void* stream = paquete_opcode->buffer->stream;
                memcpy(&largo_nombre_interfaz, stream , sizeof(size_t));
                stream += sizeof(size_t);
                nombre_interfaz = malloc(largo_nombre_interfaz);
                memcpy(nombre_interfaz, stream, largo_nombre_interfaz);
                stream += largo_nombre_interfaz;

                uint32_t size_buffer = paquete_opcode->buffer->size - largo_nombre_interfaz - sizeof(size_t);
                t_paquete* p = crear_paquete_codigo_operacion(IO_FS_CREATE_OP_CODE);
                agregar_a_buffer(p->buffer, &(running_pcb->contexto.pid), sizeof(uint32_t));
                agregar_a_buffer(p->buffer, stream, size_buffer);

                enviar_operacion_a_io(running_pcb, nombre_interfaz, IO_FS_CREATE_OP_CODE, p);
                running_pcb = NULL;
                sem_post(&sem_cpu_available);
                break;
            }
            case IO_FS_DELETE_OP_CODE:
            {
                size_t largo_nombre_interfaz = 0;
                char* nombre_interfaz = NULL;

                void* stream = paquete_opcode->buffer->stream;
                memcpy(&largo_nombre_interfaz, stream , sizeof(size_t));
                stream += sizeof(size_t);
                nombre_interfaz = malloc(largo_nombre_interfaz);
                memcpy(nombre_interfaz, stream, largo_nombre_interfaz);
                stream += largo_nombre_interfaz;

                uint32_t size_buffer = paquete_opcode->buffer->size - largo_nombre_interfaz - sizeof(size_t);
                t_paquete* p = crear_paquete_codigo_operacion(IO_FS_DELETE_OP_CODE);
                agregar_a_buffer(p->buffer, &(running_pcb->contexto.pid), sizeof(uint32_t));
                agregar_a_buffer(p->buffer, stream, size_buffer);

                enviar_operacion_a_io(running_pcb, nombre_interfaz, IO_FS_DELETE_OP_CODE, p);
                running_pcb = NULL;
                sem_post(&sem_cpu_available);
                break;
            }
            case IO_FS_TRUNCATE_OP_CODE:
            {
                size_t largo_nombre_interfaz = 0;
                char* nombre_interfaz = NULL;

                void* stream = paquete_opcode->buffer->stream;
                memcpy(&largo_nombre_interfaz, stream , sizeof(size_t));
                stream += sizeof(size_t);
                nombre_interfaz = malloc(largo_nombre_interfaz);
                memcpy(nombre_interfaz, stream, largo_nombre_interfaz);
                stream += largo_nombre_interfaz;

                uint32_t size_buffer = paquete_opcode->buffer->size - largo_nombre_interfaz - sizeof(size_t);
                t_paquete* p = crear_paquete_codigo_operacion(IO_FS_TRUNCATE_OP_CODE);
                agregar_a_buffer(p->buffer, &(running_pcb->contexto.pid), sizeof(uint32_t));
                agregar_a_buffer(p->buffer, stream, size_buffer);

                enviar_operacion_a_io(running_pcb, nombre_interfaz, IO_FS_TRUNCATE_OP_CODE, p);
                running_pcb = NULL;
                sem_post(&sem_cpu_available);
                break;
            }
            case IO_FS_WRITE_OP_CODE:
            {
                size_t largo_nombre_interfaz = 0;
                char* nombre_interfaz = NULL;

                void* stream = paquete_opcode->buffer->stream;
                memcpy(&largo_nombre_interfaz, stream , sizeof(size_t));
                stream += sizeof(size_t);
                nombre_interfaz = malloc(largo_nombre_interfaz);
                memcpy(nombre_interfaz, stream, largo_nombre_interfaz);
                stream += largo_nombre_interfaz;

                uint32_t size_buffer = paquete_opcode->buffer->size - largo_nombre_interfaz - sizeof(size_t);
                t_paquete* p = crear_paquete_codigo_operacion(IO_FS_WRITE_OP_CODE);
                agregar_a_buffer(p->buffer, &(running_pcb->contexto.pid), sizeof(uint32_t));
                agregar_a_buffer(p->buffer, stream, size_buffer);

                enviar_operacion_a_io(running_pcb, nombre_interfaz, IO_FS_WRITE_OP_CODE, p);
                running_pcb = NULL;
                sem_post(&sem_cpu_available);
                break;
            }
            case IO_FS_READ_OP_CODE:
            {
                size_t largo_nombre_interfaz = 0;
                char* nombre_interfaz = NULL;

                void* stream = paquete_opcode->buffer->stream;
                memcpy(&largo_nombre_interfaz, stream , sizeof(size_t));
                stream += sizeof(size_t);
                nombre_interfaz = malloc(largo_nombre_interfaz);
                memcpy(nombre_interfaz, stream, largo_nombre_interfaz);
                stream += largo_nombre_interfaz;

                uint32_t size_buffer = paquete_opcode->buffer->size - largo_nombre_interfaz - sizeof(size_t);
                t_paquete* p = crear_paquete_codigo_operacion(IO_FS_READ_OP_CODE);
                agregar_a_buffer(p->buffer, &(running_pcb->contexto.pid), sizeof(uint32_t));
                agregar_a_buffer(p->buffer, stream, size_buffer);

                enviar_operacion_a_io(running_pcb, nombre_interfaz, IO_FS_READ_OP_CODE, p);
                running_pcb = NULL;
                sem_post(&sem_cpu_available);
                break;
            }
            case EXIT_OP_CODE:
            {
                finalizar_proceso(running_pcb, "SUCCESS");
                running_pcb = NULL;
                sem_post(&sem_cpu_available);
                break;
            }
            case FINALIZAR_PROCESO:
            {
                finalizar_proceso(running_pcb, "INTERRUPTED_BY_USER");
                running_pcb = NULL;
                sem_post(&sem_cpu_available);
                break;
            }
            case FIN_Q:
            {  
                log_debug(logger, "Proceso %d desalojado por fin de quantum", contexto->pid);
                agregar_pcb_thread_safe(running_pcb, list_ready, mutex_ready);
                running_pcb = NULL;
                sem_post(&sem_cpu_available);
                sem_post(&semaphore_ready);
                break;
            }
            case OUT_OF_MEMORY:
            {
                enviar_interrupcion(OUT_OF_MEMORY, running_pcb->contexto.pid);
                finalizar_proceso(running_pcb, "OUT_OF_MEMORY");
                running_pcb = NULL;
                sem_post(&sem_cpu_available);
                break;
            }
            default:
            {
                log_error(logger, "Código no admitido");
                exit(-1);
            }
        }
        pthread_mutex_unlock(&mutex_running_pcb);
    }
}

void finalizar_proceso(t_pcb* pcb, const char* motivo) {
    t_paquete* paquete = crear_paquete_codigo_operacion(FINALIZAR_PROCESO);
    agregar_a_buffer(paquete->buffer, &(pcb->contexto.pid), sizeof(uint32_t));
    enviar_paquete(paquete, socket_memoria);
    esperar_ok(socket_memoria, logger);
    log_info(logger, "Finaliza el proceso %d - Motivo: %s", pcb->contexto.pid, motivo);
    destruir_pcb(pcb);
}

void iniciar_quantum(void* args) {
    t_quantum_remanente* quantum = (t_quantum_remanente*) args;    
    usleep(quantum->quantum * 1000);
    //pthread_mutex_lock(&mutex_running_pcb);
    log_trace(logger, "PID: %d - Desalojado por fin de Quantum", running_pcb->contexto.pid);
    enviar_interrupcion(FIN_Q, running_pcb->contexto.pid);
    //pthread_mutex_unlock(&mutex_running_pcb);
}

void enviar_interrupcion(op_code op_code, uint32_t pid) {
    t_paquete* p = crear_paquete_codigo_operacion(op_code);
    agregar_a_buffer(p->buffer, &pid, sizeof(uint32_t));
    enviar_paquete(p, socket_cpu_interrupt);
}

void enviar_operacion_a_io(t_pcb *pcb, char* nombre_io, op_code operacion, t_paquete* paquete) {
    t_io* io = buscar_io(nombre_io);
    if(io == NULL) {
        enviar_interrupcion(INVALID_INTERFACE, pcb->contexto.pid);
        finalizar_proceso(running_pcb, "INVALID_INTERFACE");
        return;
    }
    validar_operacion(io, operacion);

    if(list_size(io->cola_bloqueados) == 0) {
        enviar_paquete(paquete, io->socket_io);
    }
    t_pcb_bloqueado_io* pcb_bloqueado_io = malloc(sizeof(t_pcb_bloqueado_io));
    pcb_bloqueado_io->pcb = pcb;
    pcb_bloqueado_io->paquete_bloqueado = paquete;
    list_add(io->cola_bloqueados, pcb_bloqueado_io);
    log_info(logger, "PID: %d - Estado Anterior: EXEC - Estado Actual: BLOCK", pcb->contexto.pid);
    log_info(logger, "PID: %d - Bloqueado por: %s", pcb->contexto.pid, nombre_io);
    sem_post(&io->sem_io);
}

void* esperar_io(void* args) {
    t_struct_esperar_io* estructura_esperar_io = (t_struct_esperar_io*) args;
    t_io* io = estructura_esperar_io->io;
    uint32_t socket = io->socket_io;

    while(true) {
        if (planificacion_apagada) {
            sem_wait(&sem_prender_planificacion);
        }
        sem_wait(&io->sem_io);
        //log_trace(logger, "Esperando OK de IO %d", socket);
        esperar_ok(socket, logger);       
        //log_trace(logger, "IO finalizada");

        if(list_is_empty(io->cola_bloqueados)) continue;
        t_pcb_bloqueado_io* pcb = list_remove(io->cola_bloqueados, 0);
        if(pid_finalizado) {
            pid_finalizado = false;
            if (list_size(io->cola_bloqueados) > 0) {
                enviar_paquete(pcb->paquete_bloqueado, socket);
                sem_post(&io->sem_io);
            }
            continue;
        }

        if(!strcmp(ALGORITHM, "VRR")) {
            agregar_pcb_thread_safe(pcb->pcb, list_priority_ready, mutex_ready);
		    log_info(logger, "PID: %d - Estado Anterior: BLOCK - Estado Actual: READY+", pcb->pcb->contexto.pid);
            imprimir_cola_ready_prioritaria();
        } 
        else {
            agregar_pcb_thread_safe(pcb->pcb, list_ready, mutex_ready);
		    log_info(logger, "PID: %d - Estado Anterior: BLOCK - Estado Actual: READY", pcb->pcb->contexto.pid);
            imprimir_cola_ready();
        }

        free(pcb);
        sem_post(&semaphore_ready);
        if (list_size(io->cola_bloqueados) > 0) {
            t_pcb_bloqueado_io* pcb_bloqueado = list_get(io->cola_bloqueados, 0);
            enviar_paquete(pcb_bloqueado->paquete_bloqueado, socket);
            sem_post(&io->sem_io);
        }
    }
}

t_paquete* crear_paquete_sleep_io(t_pcb* pcb, op_code operacion, uint32_t sleep) {
    t_paquete* paquete = crear_paquete_codigo_operacion(operacion);
	agregar_a_buffer(paquete->buffer, &pcb->contexto.pid, sizeof(uint32_t));
	agregar_a_buffer(paquete->buffer, &sleep, sizeof(uint32_t));

	return paquete;
}

t_io* buscar_io(char* nombre) {
    pthread_mutex_lock(&mutex_io);
    for (int i = 0; i < list_size(interfaces); i++)
	{
        t_io* io = list_get(interfaces, i);
		if (strcmp(io->nombre, nombre) == 0) {
            pthread_mutex_unlock(&mutex_io);
			return io;
		}
	}
    pthread_mutex_unlock(&mutex_io);
	return NULL;
}

void validar_operacion(t_io* io, op_code operacion) {
    pthread_mutex_lock(&mutex_io);
    for(int i = 0; i < list_size(io->operaciones); i++) {
        //log_trace(logger, "Operacion %d", *(op_code*)list_get(io->operaciones, i));
        if(*(op_code*)list_get(io->operaciones, i) == operacion) {
            pthread_mutex_unlock(&mutex_io);
            return;
        }
    }
    log_error(logger, "Operación no admitida por interfaz");
    exit(EXIT_FAILURE);
}

int solicitar_recurso(t_pcb *pcb, char* recurso) {
    int indice_recurso = buscar_recurso(recurso);
    if (indice_recurso == -1) {
        return indice_recurso;
    }
    if (instancias_recursos[indice_recurso] > 0) {
        instancias_recursos[indice_recurso]--;
        list_add(pcb->recursos_tomados, recurso);
        //log_trace(logger, "Proceso %d toma recurso %s", pcb->contexto.pid, recurso);
        return 1;
    } else {
        list_add(list_get(colas_bloqueados_recursos, indice_recurso), pcb);
        if(list_is_empty(list_ready) && list_is_empty(list_priority_ready)) {
            cpu_ocupada = false;
        } 
		log_info(logger, "PID: %d - Estado Anterior: READY - Estado Actual: BLOCK", pcb->contexto.pid);
		log_info(logger, "PID: %d - Bloqueado por: %s", pcb->contexto.pid, recurso);
        return 0;
    }
}

int buscar_recurso(char* recurso) {
    for (int i = 0; i < cantidad_recursos; i++)
	{
		if (strcmp(recursos[i], recurso) == 0) {
			return i;
		}
	}
	return -1;
}

int ceder_recurso(t_pcb *pcb, char* recurso) {
    int indice_recurso = buscar_recurso(recurso);
    if (indice_recurso == -1) {
        return indice_recurso;
    }
    for (int i = 0; i < list_size(pcb->recursos_tomados); i++) {
        char* recurso_tomado = list_get(pcb->recursos_tomados, i);
        if (strcmp(recurso_tomado, recurso) == 0) {
            list_remove(pcb->recursos_tomados, i);
            //log_trace(logger, "Proceso %d cede recurso %s", pcb->contexto.pid, recurso);
            break;
        }
    }
    t_list* cola_bloqueados = list_get(colas_bloqueados_recursos, indice_recurso);
    if (list_size(cola_bloqueados) > 0) {
        t_pcb* pcb_bloqueado = list_remove(cola_bloqueados, 0);
        if(!strcmp(ALGORITHM, "VRR")) {
            agregar_pcb_thread_safe(pcb_bloqueado, list_priority_ready, mutex_ready);
            imprimir_cola_ready_prioritaria();
            log_info(logger, "PID: %d - Estado Anterior: BLOCK - Estado Actual: READY+", pcb_bloqueado->contexto.pid);
        }
        else {
            agregar_pcb_thread_safe(pcb_bloqueado, list_ready, mutex_ready);
            log_info(logger, "PID: %d - Estado Anterior: BLOCK - Estado Actual: READY", pcb_bloqueado->contexto.pid);
            imprimir_cola_ready();
        }

        //log_trace(logger, "Proceso %d desbloqueado y toma recurso %s", pcb_bloqueado->contexto.pid, recurso);
        list_add(pcb_bloqueado->recursos_tomados, recurso);
        sem_post(&semaphore_ready);
        return 0;
    }
    instancias_recursos[indice_recurso]++;
    return 0;
}

void obtener_procesos_estado(){
    if(!cpu_ocupada) printf("\n----------Proceso en EXEC----------\n");
    else printf("\n----------Proceso en EXEC----------\n- %d -", running_pcb->contexto.pid);

    t_pcb* pcb_proceso;
    printf("\n---------- Procesos en NEW----------\n");
    for (int i = 0; i < list_size(list_new); i++)
    {
        pcb_proceso = list_get(list_new, i);
        printf("- %d -", pcb_proceso->contexto.pid);
    }

    printf("\n---------- Procesos en READY----------\n");
    for (int i = 0; i < list_size(list_ready); i++)
    {
        pcb_proceso = list_get(list_ready, i);
        printf("- %d -", pcb_proceso->contexto.pid);
    }

    for (int i = 0; i < cantidad_recursos; i++)
    {
        printf("\n---------- Procesos bloqueados para recurso: %s ----------\n", recursos[i]);
        t_list* cola_bloqueados = list_get(colas_bloqueados_recursos, i);
        for(int j = 0; j < list_size(cola_bloqueados); j++) {
            pcb_proceso = list_get(cola_bloqueados, j);
            printf("- %d -", pcb_proceso->contexto.pid);
        }
    }

    for(int i = 0; i < list_size(interfaces); i++) {
        t_io* io = list_get(interfaces, i);
        printf("\n---------- Procesos bloqueados en IO: %s ----------\n", io->nombre);
        for(int j = 0; j < list_size(io->cola_bloqueados); j++) {
            t_pcb_bloqueado_io* pcb_bloqueado = list_get(io->cola_bloqueados, j); 
            printf("- %d -", pcb_bloqueado->pcb->contexto.pid); // TODO: no imprime bien
        }
    }
}
