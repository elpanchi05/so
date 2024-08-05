#include "memoria.h"

int main(int argc, char* argv[]) {
    initialize_setup(argv[1], "memoria", &logger, &config);
	
	pthread_mutex_init(&mutex_procesos, NULL);
	pthread_mutex_init(&mutex_memoria, NULL);
	pthread_mutex_init(&mutex_bitarray_marcos, NULL);

	retardo = config_get_int_value(config, "RETARDO_RESPUESTA");
	tam_memoria = config_get_int_value(config, "TAM_MEMORIA");
	tam_pagina = config_get_int_value(config, "TAM_PAGINA");
	cant_marcos = tam_memoria / tam_pagina;
	memoria = malloc(tam_memoria);
	memset(memoria, 0, tam_memoria);
	procesos = list_create();
	void* puntero_bitmap = malloc(cant_marcos / 8);
	memset(puntero_bitmap, 0, cant_marcos / 8);
	bitmap_marcos = bitarray_create_with_mode(puntero_bitmap, cant_marcos / 8, LSB_FIRST);

	conectar_modulos(); 

	int result = pthread_create(&thread_kernel, NULL, (void*)atender_kernel, NULL);
    if (result != 0) {
        log_error(logger, "Error al crear el hilo de atender_kernel: %s", strerror(result));
        exit(EXIT_FAILURE);
    }
	result = pthread_create(&thread_cpu, NULL, (void*)atender_cpu, NULL);
    if (result != 0) {
        log_error(logger, "Error al crear el hilo de atender_cpu: %s", strerror(result));
        exit(EXIT_FAILURE);
    }
	
	pthread_join(thread_kernel, NULL);
	pthread_join(thread_cpu, NULL);

	liberar_estructuras();
	free(puntero_bitmap);
}

void* atender_kernel(void* arg) {
	while(1) {
		t_paquete* paquete = recibir_paquete(socket_kernel);
		manejar_peticion(paquete);
		eliminar_paquete(paquete);
	}
}

void manejar_peticion(t_paquete* paquete) {
	sleep(retardo / 1000);
	switch(paquete->codigo_operacion) {
			case CREAR_PROCESO:
			{
				t_proceso* proceso_nuevo = malloc(sizeof(t_proceso));
				void* aux = paquete->buffer->stream;
				uint32_t pid = 0;
				char* path = NULL;
				size_t largo_buffer = 0;

				memcpy(&pid, aux, sizeof(uint32_t));
				aux += sizeof(uint32_t);

				memcpy(&largo_buffer, aux, sizeof(size_t));
				aux += sizeof(size_t);
				path = malloc(largo_buffer);
				memcpy(path, aux, largo_buffer);	

				FILE* archivo = fopen(path, "r");
				if (archivo == NULL) {
					log_error(logger, "No se pudo abrir el archivo");
				}
				else {
					t_list* instrucciones = parsear_archivo(archivo);
					fclose(archivo);
					proceso_nuevo->pid = pid;
					proceso_nuevo->instrucciones = instrucciones;
					proceso_nuevo->tabla_paginas = list_create();

					log_info(logger, "Creación de tabla de páginas de PID: %d", pid);
					pthread_mutex_lock(&mutex_procesos);
					list_add(procesos, proceso_nuevo);
					pthread_mutex_unlock(&mutex_procesos);

					op_code op_code = OK;
					send(socket_kernel, &op_code, sizeof(op_code), 0);

					break;
				}
			}
			case FINALIZAR_PROCESO:
			{
				uint32_t pid = 0;
				memcpy(&pid, paquete->buffer->stream, sizeof(uint32_t));
				liberar_proceso(pid);

				op_code op_code = OK;
				send(socket_kernel, &op_code, sizeof(op_code), 0);
				break;
			}
			default:
			{
				log_info(logger, "Código no admitido [de KERNEL]");
				exit(EXIT_FAILURE);
			}
		}
}

static void atender_cpu(){
	while(1) {
		t_paquete* paquete = recibir_paquete(socket_cpu);
		sleep(retardo / 1000);
		switch(paquete->codigo_operacion) {
			case INSTRUCCION:
			{
				uint32_t pid = 0;
				uint32_t pc = 0;

				void* aux = paquete->buffer->stream;
				memcpy(&pid, aux, sizeof(uint32_t));
				aux += sizeof(uint32_t);
				memcpy(&pc, aux, sizeof(uint32_t));
				aux += sizeof(uint32_t);

				t_proceso* proceso = buscar_proceso(pid);
				t_instruccion* instruccion = list_get(proceso->instrucciones, pc);

				// empaquetar instruccion: Codigo de instruccion | Numero de parametros | {Largo parametro | Parametro}
				t_buffer* buffer = crear_buffer();
				agregar_a_buffer(buffer, &instruccion->codigo_instruccion, sizeof(t_codigo_instruccion));
				int numero_parametros = list_size(instruccion->parametros);
				agregar_a_buffer(buffer, &numero_parametros, sizeof(int));
				//log_trace(logger, "N parametros %d", numero_parametros);
				for(int i = 0; i < numero_parametros; i++) {
					char* parametro = list_get(instruccion->parametros, i);
					//log_trace(logger, "Parametro %s", parametro);
					size_t largo = strlen(parametro) + 1;
					agregar_a_buffer(buffer, &largo, sizeof(size_t));
					agregar_a_buffer(buffer, parametro, largo);
				}
				enviar_buffer(socket_cpu, buffer);
				break;
			}
			case RESIZE_OP_CODE:
			{
				uint32_t tamanio = 0;
				uint32_t pid = 0;
				void* aux = paquete->buffer->stream;

				memcpy(&pid, aux, sizeof(uint32_t));
				aux += sizeof(uint32_t);
				memcpy(&tamanio, aux, sizeof(uint32_t));
				
				t_proceso* proceso = buscar_proceso(pid);
				uint32_t tamanio_actual = list_size(proceso->tabla_paginas) * tam_pagina; 
				if(tamanio_actual <= tamanio) {
					if(ampliar_proceso(proceso, tamanio_actual, tamanio)) {
						op_code op_code = OK;
						send(socket_cpu, &op_code, sizeof(op_code), 0);
					}
					else {
						op_code op_code = OUT_OF_MEMORY;
						send(socket_cpu, &op_code, sizeof(op_code), 0);
					}
				}
				else { 
					reducir_proceso(proceso, tamanio_actual, tamanio);
					op_code op_code = OK;
					send(socket_cpu, &op_code, sizeof(op_code), 0);
				}

				break;
			}
			case LECTURA:
			{
				uint32_t pags_a_leer = 0;
				uint32_t tam_a_leer = 0;
				uint32_t pid = 0;
				
				t_paquete* p = crear_paquete_codigo_operacion(OK);

				// Recibo DF, cargo buffer con contenido. En la primera y última recibo el tamaño a leer luego de la DF.
				void* aux = paquete->buffer->stream;
				memcpy(&pid, aux, sizeof(uint32_t));
				aux += sizeof(uint32_t);
				memcpy(&pags_a_leer, aux, sizeof(uint32_t));
				aux += sizeof(uint32_t);
				uint32_t df = 0;
				uint32_t df0 = 0;
				uint32_t tamanio_leido = 0;

				//log_trace(logger, "Pags a leer %d", pags_a_leer);
				for(int i = 0; i < pags_a_leer; i++) {
					memcpy(&df, aux, sizeof(uint32_t));
					aux += sizeof(uint32_t);

					if(i==0) df0 = df;

					tam_a_leer = tam_pagina;
					if(i == 0 || i == pags_a_leer - 1) {
						memcpy(&tam_a_leer, aux, sizeof(uint32_t));
						aux += sizeof(uint32_t);
						if(i == pags_a_leer - 1 && tam_a_leer == 0) {
							break;
						}
					}
					//log_trace(logger, "DF %d", df);
					//log_trace(logger, "tam_a_leer %d", tam_a_leer);
					tamanio_leido += tam_a_leer;
					pthread_mutex_lock(&mutex_memoria);
					agregar_a_buffer(p->buffer, memoria + df, tam_a_leer);
					pthread_mutex_unlock(&mutex_memoria);
				}
				//log_trace(logger, "%s", mem_hexstring(memoria + df0, pags_a_leer*tam_pagina));
				log_info(logger, "PID: %d - Accion: LEER - Direccion fisica: %d - Tamaño %d", pid, df0, tamanio_leido);
				enviar_paquete(p, socket_cpu);

				break;
			}
			case ESCRITURA:
			{
				uint32_t pags_a_escribir = 0;
				uint32_t tam_a_escribir = 0;
				uint32_t df = 0;
				void* aux = paquete->buffer->stream;
				uint32_t df0 = 0; //
				uint32_t pid = 0;
				uint32_t tamanio_escrito = 0;

				memcpy(&pid, aux, sizeof(uint32_t));
				aux += sizeof(uint32_t);
				memcpy(&pags_a_escribir, aux, sizeof(uint32_t));
				aux += sizeof(uint32_t);

				//log_trace(logger, "Pags a escribir %d", pags_a_escribir);
				for(int i = 0; i < pags_a_escribir; i++) {
					memcpy(&df, aux, sizeof(uint32_t));
					aux += sizeof(uint32_t);
					//log_trace(logger, "DF %d", df);
					if(i == 0) df0 = df;

					if(i == 0 || i == pags_a_escribir - 1) {
						memcpy(&tam_a_escribir, aux, sizeof(uint32_t));
						aux += sizeof(uint32_t);
						tamanio_escrito += tam_a_escribir;
						if(tam_a_escribir == 0) break;
						pthread_mutex_lock(&mutex_memoria);
						memcpy(memoria + df, aux, tam_a_escribir);
						pthread_mutex_unlock(&mutex_memoria);
						aux += tam_a_escribir;
					}
					else {
						pthread_mutex_lock(&mutex_memoria);
						memcpy(memoria + df, aux, tam_pagina);
						pthread_mutex_unlock(&mutex_memoria);
						aux += tam_pagina;
						tamanio_escrito += tam_pagina;
					}
					//log_trace(logger, "Tamaño a escribir %d", tam_a_escribir);
				}
				//pthread_mutex_lock(&mutex_memoria);
				//log_trace(logger, "%s", mem_hexstring(memoria + df0, pags_a_escribir * tam_pagina));
				//pthread_mutex_unlock(&mutex_memoria);
				
				log_info(logger, "PID: %d - Accion: ESCRIBIR - Direccion fisica: %d - Tamaño %d", pid, df0, tamanio_escrito);
				enviar_codigo_solo(OK, socket_cpu);
				break;
			}
			case SOLICITAR_MARCO:
			{
				uint32_t pid = 0;
				uint32_t pagina = 0;
				t_paquete* paquete_marco = NULL;
				void* aux = paquete->buffer->stream;
				memcpy(&pid, aux, sizeof(uint32_t));
				aux += sizeof(uint32_t);
				memcpy(&pagina, aux, sizeof(uint32_t));

				for(int i = 0; i < list_size(procesos); i++) {
					t_proceso* pr = buscar_proceso(pid);
					if(pr->pid == pid) {
						for(int j = 0; j < list_size(pr->tabla_paginas); j++) {
							if(j == pagina) {
								uint32_t* entrada = list_get(pr->tabla_paginas, j);
								paquete_marco = crear_paquete_codigo_operacion(OK);
								agregar_a_buffer(paquete_marco->buffer, entrada, sizeof(uint32_t));
								enviar_paquete(paquete_marco, socket_cpu);
								log_info(logger, "PID: %d - Página: %d - Marco: %d", pr->pid, pagina, *entrada);
								goto marco_encontrado;
							}
						}
						if(paquete_marco == NULL) {
							log_error(logger, "Memory access violation. El proceso %d no tiene asignada la dirección solicitada.", pr->pid);
							exit(EXIT_FAILURE);
						}
					}
				} 
				marco_encontrado:
				break;
			}
			default:
			{
				log_info(logger, "Código no admitido [de CPU]");
				exit(EXIT_FAILURE);
			}
		}
		eliminar_paquete(paquete); 
	}
}

void atender_io(void* args) {
	t_io* io = (t_io*) args;
	uint32_t socket_io = io->socket_io;
	sleep(retardo / 1000);
	while(1) {
		t_paquete* paquete = recibir_paquete(socket_io); 
		//log_trace(logger, "Paquete de IO");
		switch(paquete->codigo_operacion) {
			case ESCRITURA:
			{
				//log_trace(logger, "Escritura");
				uint32_t pags_a_escribir = 0;
				uint32_t tam_a_escribir = 0;
				uint32_t df = 0;
				void* aux = paquete->buffer->stream;
				uint32_t df0 = 0; 
				uint32_t pid = 0;
				uint32_t tamanio_escrito = 0;

				memcpy(&pid, aux, sizeof(uint32_t));
				aux += sizeof(uint32_t);
				memcpy(&pags_a_escribir, aux, sizeof(uint32_t));
				aux += sizeof(uint32_t);
				//log_trace(logger, "PID %d", pid);
				//log_trace(logger, "Pags a escribir %d", pags_a_escribir);
				for(int i = 0; i < pags_a_escribir; i++) {
					memcpy(&df, aux, sizeof(uint32_t));
					aux += sizeof(uint32_t);

					if(i == 0) df0 = df;

					//log_trace(logger, "DF %d", df);
					if(i == 0 || i == pags_a_escribir - 1) {
						memcpy(&tam_a_escribir, aux, sizeof(uint32_t));
						aux += sizeof(uint32_t);
						if(tam_a_escribir == 0) break;
						pthread_mutex_lock(&mutex_memoria);
						memcpy(memoria + df, aux, tam_a_escribir);
						pthread_mutex_unlock(&mutex_memoria);
						aux += tam_a_escribir;
						tamanio_escrito += tam_a_escribir;
					}
					else {
						pthread_mutex_lock(&mutex_memoria);
						memcpy(memoria + df, aux, tam_pagina);
						pthread_mutex_unlock(&mutex_memoria);
						aux += tam_pagina;
						tamanio_escrito += tam_pagina;
					}
					//log_trace(logger, "A escribir %d", tam_a_escribir);
				}
				// pthread_mutex_lock(&mutex_memoria);
				//log_trace(logger, "%s", mem_hexstring(memoria + df0, pags_a_escribir*tam_pagina));
				// pthread_mutex_unlock(&mutex_memoria);
				log_info(logger, "PID: %d - Accion: LEER - Direccion fisica: %d - Tamaño %d", pid, df0, tamanio_escrito);
				enviar_codigo_solo(OK, socket_io);
				break;
			}
			case LECTURA:
			{
				//log_trace(logger, "LECTURA");
				uint32_t pags_a_leer = 0;
				uint32_t tam_a_leer = 0;
				t_paquete* p = crear_paquete_codigo_operacion(OK);
				uint32_t df0 = 0; 
				uint32_t pid = 0;
				void* aux = paquete->buffer->stream;
				uint32_t tamanio_leido = 0;

				memcpy(&pid, aux, sizeof(uint32_t));
				aux += sizeof(uint32_t);
				memcpy(&pags_a_leer, aux, sizeof(uint32_t));
				aux += sizeof(uint32_t);

				//log_trace(logger, "Pags a leer %d", pags_a_leer);
				for(int i = 0; i < pags_a_leer; i++) {
					uint32_t df = 0;
					memcpy(&df, aux, sizeof(uint32_t));
					aux += sizeof(uint32_t);
					if(i==0) df0 = df;
					//log_trace(logger, "DF %d", df);

					tam_a_leer = tam_pagina;
					if(i == 0 || i == pags_a_leer - 1) {
						memcpy(&tam_a_leer, aux, sizeof(uint32_t));
						aux += sizeof(uint32_t);
						if(i == pags_a_leer - 1 && tam_a_leer == 0) {
							break;
						}
					}
					tamanio_leido += tam_a_leer;
					pthread_mutex_lock(&mutex_memoria);
					agregar_a_buffer(p->buffer, memoria + df, tam_a_leer);
					pthread_mutex_unlock(&mutex_memoria);
					//log_trace(logger, "tam_a_leer %d", tam_a_leer);
				}
				pthread_mutex_lock(&mutex_memoria);
				log_trace(logger, "%s", mem_hexstring(memoria + df0, pags_a_leer*tam_pagina));
				pthread_mutex_unlock(&mutex_memoria);
				log_info(logger, "PID: %d - Accion: LEER - Direccion fisica: %d” - Tamaño %d", pid, df0, tamanio_leido);
				enviar_paquete(p, socket_io);
				break;
			}
			default:
			{
				log_info(logger, "Código inválido recibido por IO.");
				exit(EXIT_FAILURE);
			}
		}
		eliminar_paquete(paquete);
	}
}

bool ampliar_proceso(t_proceso* proceso, uint32_t tamanio_actual, uint32_t tamanio) {
	uint32_t tamanio_a_agregar = tamanio - tamanio_actual; 
	double tamanio_aux = (double)tamanio_a_agregar / (double)tam_pagina;
	uint32_t paginas_a_agregar = (uint32_t)ceil(tamanio_aux);
	
	log_info(logger, "PID: %d - Tamaño Actual: %d - Tamaño a ampliar: %d", proceso->pid, tamanio_actual, tamanio_a_agregar);
	for(int i = 0; i < paginas_a_agregar; i++) {
		uint32_t* entrada_tp = malloc(sizeof(uint32_t));
		
		pthread_mutex_lock(&mutex_bitarray_marcos);
		for(int j = 0; j < cant_marcos; j++) {
			if(j == (cant_marcos - 1) && bitarray_test_bit(bitmap_marcos, j)) {
				free(entrada_tp); 
				pthread_mutex_unlock(&mutex_bitarray_marcos);
				return 0;
			}
			if(!bitarray_test_bit(bitmap_marcos, j)) {
				*entrada_tp = j;
				bitarray_set_bit(bitmap_marcos, j);
				list_add(proceso->tabla_paginas, entrada_tp);
				break;
			}
		}
		pthread_mutex_unlock(&mutex_bitarray_marcos);
	}

	return 1;
}

void reducir_proceso(t_proceso* proceso, uint32_t tamanio_actual, uint32_t tamanio) {
	uint32_t tamanio_a_reducir = tamanio_actual - tamanio; 
	uint32_t paginas_actuales = tamanio_actual / tam_pagina; 
	uint32_t paginas_a_liberar = tamanio_a_reducir / tam_pagina;

	log_info(logger, "PID: %d - Tamaño Actual: %d - Tamaño a reducir: %d", proceso->pid, tamanio_actual, tamanio_a_reducir);

	if(paginas_a_liberar == 0) return;

	int i = paginas_actuales-paginas_a_liberar;
	while(i != 0) {
		uint32_t* entrada_tp = list_get(proceso->tabla_paginas, i);

		pthread_mutex_lock(&mutex_bitarray_marcos);
		bitarray_clean_bit(bitmap_marcos, i);
		pthread_mutex_unlock(&mutex_bitarray_marcos);
		list_remove(proceso->tabla_paginas, i);
		free(entrada_tp);
		i--;
	}
}

t_proceso* buscar_proceso(uint32_t pid) {
	pthread_mutex_lock(&mutex_procesos);
	for(int i = 0; i < list_size(procesos); i++) {
		t_proceso* p2 = list_get(procesos, i);
		if(p2->pid == pid) {
			pthread_mutex_unlock(&mutex_procesos);
			return p2;
		}
	}
	log_error(logger, "Proceso %d no encontrado", pid);
	exit(EXIT_FAILURE);
}

void liberar_estructuras() {
	free(memoria);
	for(int i = 0; i < list_size(procesos); i++) {
		liberar_proceso(i);
	}
	bitarray_destroy(bitmap_marcos);
}

void liberar_proceso(uint32_t pid) {
	t_proceso* proceso = buscar_proceso(pid);

	log_info(logger, "Destrucción de tabla de páginas de PID: %d - Tamaño %d", pid, list_size(proceso->tabla_paginas));
	for(int j = 0; j < list_size(proceso->tabla_paginas); j++) { // Libero tabla de páginas
		uint32_t* entrada = list_get(proceso->tabla_paginas, j);
		free(entrada);
	}
	list_destroy(proceso->tabla_paginas);
	for(int k = 0; k < list_size(proceso->instrucciones); k++) { // Libero instrucciones
		t_instruccion* instruccion = list_get(proceso->instrucciones, k);
		for(int l = 0; l < list_size(instruccion->parametros); l++) {
			char* parametro = list_get(instruccion->parametros, l);
			free(parametro);
		}
		list_destroy(instruccion->parametros);
		free(instruccion);
	}
	list_destroy(proceso->instrucciones);
	pthread_mutex_lock(&mutex_procesos);
	list_remove_element(procesos, proceso);
	pthread_mutex_unlock(&mutex_procesos);
	free(proceso);
}

void conectar_modulos() {
	socket_memoria = start_server_module("MEMORIA", IP_CONFIG_PATH);
	log_info(logger, "Creacion de servidor MEMORIA en socket %d", socket_memoria);

    socket_cpu = accept(socket_memoria, NULL, NULL);
	log_info(logger, "Conexion con cliente CPU en socket %i", socket_cpu);

	send(socket_cpu, &tam_pagina, sizeof(uint32_t), 0);

    socket_kernel = accept(socket_memoria, NULL, NULL);
	log_info(logger, "Conexion con cliente KERNEL en socket %i", socket_kernel);
	
	pthread_create(&thread_aceptar_io, NULL, (void*)conectar_ios, NULL);
	pthread_detach(thread_aceptar_io);
}

void conectar_ios() { 
	while(1) {
		uint32_t socket_io = accept(socket_memoria, NULL, NULL); 
		log_info(logger, "Conexion con cliente IO en socket %d", socket_io);
		send(socket_io, &tam_pagina, sizeof(uint32_t), 0);

		t_io* args = malloc(sizeof(t_io));
		args->socket_io = socket_io;

		pthread_create(&thread_io, NULL, (void*)atender_io, (void*)args);
		pthread_detach(thread_io);
	}
}