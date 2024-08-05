#include "cpu.h"

const char* instruccion_string[] = {
	"SET",
    "SUM",
    "SUB",
    "JNZ",
    "SLEEP",
    "WAIT",
    "SIGNAL",
	"RESIZE",
    "MOV_IN",
    "MOV_OUT",
    "COPY_STRING",
    "IO_GEN_SLEEP",
    "IO_STDIN_READ",
    "IO_STDOUT_WRITE",
    "IO_FS_CREATE",
    "IO_FS_DELATE",
    "IO_FS_TRUNCATE",
    "IO_FS_WRITE",
    "IO_FS_READ",
    "EXIT",
};

const char* string_a_registro[] = {
	"AX",
	"BX",
	"CX",
	"DX",
	"EAX",
	"EBX",
	"ECX",
	"EDX",
	"SI",
	"DI",
	"PC"
};

int main(int argc, char* argv[]) {
    initialize_setup(argv[1], "cpu", &logger, &config);
	proximas_victimas = list_create();
	char* algoritmo_string = config_get_string_value(config, "ALGORITMO_TLB");
	if(strcmp(algoritmo_string, "FIFO") == 0) algoritmo = 0;
	if(strcmp(algoritmo_string, "LRU") == 0) algoritmo = 1;
	cantidad_entradas = config_get_int_value(config, "CANTIDAD_ENTRADAS_TLB");
	if(cantidad_entradas != 0) {
		hay_tlb = true;
		tlb = list_create();
	}
	conectar_modulos();
	pthread_mutex_init(&mutex_interrupt, NULL);
	
	while(1) {
		t_paquete* paquete = recibir_paquete(socket_kernel_dispatch);
		t_contexto* contexto = deserializar_contexto(paquete->buffer);
		while(resultado_ejecucion) {
			pthread_mutex_lock(&mutex_interrupt);
			if(interrupt == contexto->pid) {
				enviar_contexto(contexto, socket_kernel_dispatch);
				enviar_codigo(interrupt_code, socket_kernel_dispatch);
				interrupt = -1;
				pthread_mutex_unlock(&mutex_interrupt);
				resultado_ejecucion = 0;
			} 
			else {
				pthread_mutex_unlock(&mutex_interrupt);
				log_info(logger, "PID: %d - FETCH - Program Counter: %d", contexto->pid, contexto->program_counter);
				t_instruccion* instruccion = obtener_instruccion(contexto->pid, contexto->program_counter);
				contexto->program_counter++;
				resultado_ejecucion = ejecutar_instruccion(contexto, instruccion);
			}
		}
		resultado_ejecucion = 1;
	}
}

void check_interrupt() {
	while(1) {
		t_paquete* p = recibir_paquete(socket_kernel_interrupt);
		uint32_t pid = 0;
		memcpy(&pid, p->buffer->stream, sizeof(uint32_t));
		//log_debug(logger, "Interrupción recibida en proceso %d", pid);
		pthread_mutex_lock(&mutex_interrupt);
		interrupt = pid;
		interrupt_code = p->codigo_operacion;
		pthread_mutex_unlock(&mutex_interrupt);
		eliminar_paquete(p);
	}
}

t_instruccion* obtener_instruccion(uint32_t pid, uint32_t program_counter) {
	t_paquete* paquete = crear_paquete_codigo_operacion(INSTRUCCION);
	agregar_a_buffer(paquete->buffer, &pid, sizeof(uint32_t));
	agregar_a_buffer(paquete->buffer, &program_counter, sizeof(uint32_t));
	enviar_paquete(paquete, socket_memoria);

	t_buffer* buffer = recibir_buffer(socket_memoria);
    t_instruccion* instruccion = malloc(sizeof(t_instruccion));
    instruccion = deserializar_instruccion(buffer, pid);
	liberar_buffer(buffer);
	return instruccion;
} 

bool ejecutar_instruccion(t_contexto* contexto, t_instruccion* instruccion) {
	switch(instruccion->codigo_instruccion) {
		case SET:
		{
			char* registro_string = (char*)list_get(instruccion->parametros, 0);
			t_registro registro = registro_str_a_enum(registro_string);
			
			if(registro >= 0 && registro <= 3) {
				uint8_t valor = atoi(list_get(instruccion->parametros, 1));
				contexto->registros[registro] = valor;
			}
			if(registro >= 4 && registro <= 7) {
				uint32_t valor = atoi(list_get(instruccion->parametros, 1));
				contexto->registros_ext[registro - 4] = valor;
			}
			if(registro >= 8 && registro <= 9) {
				uint32_t valor = atoi(list_get(instruccion->parametros, 1));
				contexto->registros_dir[registro - 8] = valor;
			}
			if(registro == 10) {
				uint32_t valor = atoi(list_get(instruccion->parametros, 1));
				contexto->program_counter = valor;
			}

			return 1;
		}
		case SUM:
		{
			char* registro_destino_string = (char*)list_get(instruccion->parametros, 0);
			t_registro registro_destino = registro_str_a_enum(registro_destino_string);
			char* registro_origen_string = (char*)list_get(instruccion->parametros, 1);
			t_registro registro_origen = registro_str_a_enum(registro_origen_string);
			
			if(registro_destino >= 0 && registro_destino <= 3) {
				contexto->registros[registro_destino] += contexto->registros[registro_origen];
			}
			if(registro_destino >= 4 && registro_destino <= 7) {
				if(registro_origen >= 0 && registro_origen <= 3) {
					contexto->registros_ext[registro_destino - 4] += contexto->registros[registro_origen];
				}
				if(registro_origen >= 4 && registro_origen <= 7) {
					contexto->registros_ext[registro_destino - 4] += contexto->registros_ext[registro_origen - 4];
				}
			}
			return 1;
		}
		case SUB:
		{
			char* registro_destino_string = (char*)list_get(instruccion->parametros, 0);
			t_registro registro_destino = registro_str_a_enum(registro_destino_string);
			char* registro_origen_string = (char*)list_get(instruccion->parametros, 1);
			t_registro registro_origen = registro_str_a_enum(registro_origen_string);
			
			if(registro_destino >= 0 && registro_destino <= 3) {
				contexto->registros[registro_destino] -= contexto->registros[registro_origen];
			}
			if(registro_destino >= 4 && registro_destino <= 7) {
				if(registro_origen >= 0 && registro_origen <= 3) {
					contexto->registros_ext[registro_destino - 4] -= contexto->registros[registro_origen];
				}
				if(registro_origen >= 4 && registro_origen <= 7) {
					contexto->registros_ext[registro_destino - 4] -= contexto->registros_ext[registro_origen - 4];
				}
			}
			return 1;
		}
		case JNZ:
		{
			char* registro_string = (char*)list_get(instruccion->parametros, 0);
			t_registro registro = registro_str_a_enum(registro_string);
			char* nuevo_pc_str = (char*)list_get(instruccion->parametros, 1);
			uint32_t nuevo_pc = atoi(nuevo_pc_str);
			
			if(registro >= 0 && registro <= 3) {
				if(contexto->registros[registro] != 0) {
					contexto->program_counter = nuevo_pc;
				}
			}
			if(registro >= 4 && registro <= 7) {
				if(contexto->registros_ext[registro] == 0) {
					contexto->program_counter = nuevo_pc;
				}
			}
			return 1;
		}
		case IO_GEN_SLEEP:
		{
			char* interfaz = (char*)list_get(instruccion->parametros, 0);
			uint32_t tiempo = atoi((char*)list_get(instruccion->parametros, 1));
			
			enviar_contexto(contexto, socket_kernel_dispatch);
			t_paquete* paquete = crear_paquete_codigo_operacion(IO_GEN_SLEEP_OP_CODE);

			size_t largo_string = strlen(interfaz) + 1;
			agregar_a_buffer(paquete->buffer, &largo_string, sizeof(size_t));
			agregar_a_buffer(paquete->buffer, interfaz, largo_string);
			agregar_a_buffer(paquete->buffer, &tiempo, sizeof(uint32_t));

			enviar_paquete(paquete, socket_kernel_dispatch);
			return 0;
		}
		case IO_STDIN_READ:
		{
			// N° pags | DF | Primer contenido | DFs | DF | Ultimo contenido -> KERNEL -> IO
			char* interfaz = (char*)list_get(instruccion->parametros, 0);
			size_t largo_interfaz = strlen(interfaz) + 1;
			char* registro_direccion_string = (char*)list_get(instruccion->parametros, 1);
			t_registro registro_direccion = registro_str_a_enum(registro_direccion_string);
			char* registro_tamanio_string = (char*)list_get(instruccion->parametros, 2);
			t_registro registro_tamanio = registro_str_a_enum(registro_tamanio_string);

			t_paquete* p = crear_paquete_codigo_operacion(IO_STDIN_READ_OP_CODE);

			agregar_a_buffer(p->buffer, &largo_interfaz, sizeof(size_t));
			agregar_a_buffer(p->buffer, interfaz, largo_interfaz);

			uint32_t dl = extraer_entero_de_registro(contexto, registro_direccion);
			uint32_t tamanio = extraer_entero_de_registro(contexto, registro_tamanio);

			agregar_a_buffer(p->buffer, &tamanio, sizeof(uint32_t));
	
			t_buffer* b = crear_buffer_con_df(tamanio, dl, contexto, NULL);
			agregar_a_buffer(p->buffer, b->stream, b->size);

			enviar_contexto(contexto, socket_kernel_dispatch);
			enviar_paquete(p, socket_kernel_dispatch);
			liberar_buffer(b);

			op_code op_code = recibir_op_code(socket_kernel_dispatch);
			if(op_code == OK) return 0;
			return 1;
		}
		case IO_STDOUT_WRITE:
		{
			char* interfaz = (char*)list_get(instruccion->parametros, 0);
			size_t largo_interfaz = strlen(interfaz) + 1;
			char* registro_direccion_string = (char*)list_get(instruccion->parametros, 1);
			t_registro registro_direccion = registro_str_a_enum(registro_direccion_string);
			char* registro_tamanio_string = (char*)list_get(instruccion->parametros, 2);
			t_registro registro_tamanio = registro_str_a_enum(registro_tamanio_string);
			
			uint32_t dl = extraer_entero_de_registro(contexto, registro_direccion);
			uint32_t tamanio = extraer_entero_de_registro(contexto, registro_tamanio);

			t_paquete* paquete_stdout = crear_paquete_codigo_operacion(IO_STDOUT_WRITE_OP_CODE);
			agregar_a_buffer(paquete_stdout->buffer, &largo_interfaz, sizeof(size_t));
			agregar_a_buffer(paquete_stdout->buffer, interfaz, largo_interfaz);
			agregar_a_buffer(paquete_stdout->buffer, &tamanio, sizeof(uint32_t));
			t_buffer* b = crear_buffer_con_df(tamanio, dl, contexto, NULL);
			agregar_a_buffer(paquete_stdout->buffer, b->stream, b->size);
			enviar_contexto(contexto, socket_kernel_dispatch);
			enviar_paquete(paquete_stdout, socket_kernel_dispatch);
			liberar_buffer(b);

			op_code op_code = recibir_op_code(socket_kernel_dispatch);
			if(op_code == OK) return 0;
			return 1;
		}
		case MOV_IN:
		{
			char* registro_direccion_string = (char*)list_get(instruccion->parametros, 1);
			t_registro registro_direccion = registro_str_a_enum(registro_direccion_string);
			char* registro_datos_string = (char*)list_get(instruccion->parametros, 0);
			t_registro registro_datos = registro_str_a_enum(registro_datos_string);
			
			uint32_t dl = extraer_entero_de_registro(contexto, registro_direccion);

			uint32_t tam_reg = 0;
			if(registro_datos >= 0 && registro_datos <= 3) {
				tam_reg = 1;
			}
			if(registro_datos >= 4 && registro_datos <= 7) {
				tam_reg = 4;
			}
			if(registro_datos >= 8 && registro_datos <= 9) {
				tam_reg = 4;
			} 

			char* log = string_new();
			t_buffer* b = crear_buffer_con_df(tam_reg, dl, contexto, &log);
			t_paquete* p = crear_paquete_codigo_operacion(LECTURA);
			agregar_a_buffer(p->buffer, b->stream, b->size);
			enviar_paquete(p, socket_memoria);
			p = recibir_paquete(socket_memoria); 

			// Guardar lectura en registro de datos
				if(registro_datos >= 0 && registro_datos <= 3) {
					memcpy(contexto->registros + registro_datos * sizeof(uint32_t), p->buffer->stream, sizeof(uint8_t));
					string_append_with_format(&log, "%d", *(contexto->registros + registro_datos * sizeof(uint32_t)));
				}
				if(registro_datos >= 4 && registro_datos <= 7) {
					memcpy(contexto->registros_ext + (registro_datos - 4) * sizeof(uint32_t), p->buffer->stream, sizeof(uint32_t));
					string_append_with_format(&log, "%d", *(uint32_t*)(contexto->registros_ext + (registro_datos - 4) * sizeof(uint32_t)));
				}
				if(registro_datos >= 8 && registro_datos <= 9) {
					memcpy(contexto->registros_dir + (registro_datos - 8) * sizeof(uint32_t), p->buffer->stream, sizeof(uint32_t));
					string_append_with_format(&log, "%d", *(contexto->registros_dir + (registro_datos - 8) * sizeof(uint32_t)));
				}

				log_info(logger, "%s", log);
				eliminar_paquete(p);
			//

			liberar_buffer(b);
			free(log);
			return 1;
		}
		case MOV_OUT:
		{
			// [Páginas a escribir | DF | Tamaño primer contenido | Contenido | {DF | Contenido} | DF | Tamaño último contenido | Contenido]
			char* registro_direccion_string = (char*)list_get(instruccion->parametros, 0);
			t_registro registro_direccion = registro_str_a_enum(registro_direccion_string);
			char* registro_datos_string = (char*)list_get(instruccion->parametros, 1);
			t_registro registro_datos = registro_str_a_enum(registro_datos_string);
			
			uint32_t dl = extraer_entero_de_registro(contexto, registro_direccion);
			//log_trace(logger, "DL %d", dl);
			uint32_t tam_reg = 0;
			uint32_t* contenido_ext = 0;
			uint8_t* contenido_ext_8 = malloc(sizeof(uint8_t) * 4);

			if(registro_datos >= 0 && registro_datos <= 3) {
				tam_reg = 1;
				contenido_ext_8[0] = *(contexto->registros + registro_datos);
			}
			if(registro_datos >= 4 && registro_datos <= 7) {
				tam_reg = 4;
				contenido_ext = contexto->registros_ext + registro_datos - 4; 	
				contenido_ext_8[0] = (*(uint32_t*)contenido_ext >> 0) & 0xFF; 
				contenido_ext_8[1] = (*(uint32_t*)contenido_ext >> 8) & 0xFF;
				contenido_ext_8[2] = (*(uint32_t*)contenido_ext >> 16) & 0xFF;
				contenido_ext_8[3] = (*(uint32_t*)contenido_ext >> 24) & 0xFF;
			}
			if(registro_datos >= 8 && registro_datos <= 9) {
				tam_reg = 4;
				contenido_ext = contexto->registros_dir + registro_datos - 8;
				contenido_ext_8[0] = (*(uint32_t*)contenido_ext >> 0) & 0xFF; 
				contenido_ext_8[1] = (*(uint32_t*)contenido_ext >> 8) & 0xFF;
				contenido_ext_8[2] = (*(uint32_t*)contenido_ext >> 16) & 0xFF;
				contenido_ext_8[3] = (*(uint32_t*)contenido_ext >> 24) & 0xFF;
			} 
			
			solicitar_escritura(tam_reg, dl, contexto, contenido_ext_8);

			return 1;
		}
		case COPY_STRING:
		{
			uint32_t dl_origen = contexto->registros_dir[0];
			uint32_t dl_destino = contexto->registros_dir[1];
			uint32_t tamanio = atoi(list_get(instruccion->parametros, 0));
			uint8_t* string = malloc(sizeof(uint8_t) * tamanio);
			
			char* log = string_new();
			t_buffer* b = crear_buffer_con_df(tamanio, dl_origen, contexto, &log);
			t_paquete* p = crear_paquete_codigo_operacion(LECTURA);
			agregar_a_buffer(p->buffer, b->stream, b->size);
			enviar_paquete(p, socket_memoria);
			p = recibir_paquete(socket_memoria); 
			for(int i = 0; i < tamanio; i++) {
				memcpy(string + i, p->buffer->stream + i, sizeof(uint8_t)); 
			}
			//log_trace(logger, "%s", string);
			string_append_with_format(&log, "%s", string);
			log_info(logger, "%s", log); // TODO: aparecen dos caracteres basura al principio del ultimo string

			solicitar_escritura(tamanio, dl_destino, contexto, string);

			liberar_buffer(b);
			free(log);
			return 1;
		}
		case RESIZE:
		{
			uint32_t tamanio = atoi(list_get(instruccion->parametros, 0));
			t_paquete* paquete = crear_paquete_codigo_operacion(RESIZE_OP_CODE);
			agregar_a_buffer(paquete->buffer, &contexto->pid, sizeof(uint32_t));
			agregar_a_buffer(paquete->buffer, &tamanio, sizeof(uint32_t));
			enviar_paquete(paquete, socket_memoria);
			
			op_code op_code = recibir_op_code(socket_memoria);
			if(op_code == OUT_OF_MEMORY) {
				enviar_contexto(contexto, socket_kernel_dispatch);
				enviar_codigo(OUT_OF_MEMORY, socket_kernel_dispatch);
				log_info(logger, "Out of memory");
				return 0;
			}
			if(op_code != OK) {
				log_error(logger, "Error en la petición");
				exit(-1);
			}
			return 1;
		}
		case WAIT:
		{
			char* recurso = (char*)list_get(instruccion->parametros, 0);
			uint32_t largo = strlen(recurso) + 1;
			enviar_contexto(contexto, socket_kernel_dispatch);
			t_paquete* p = crear_paquete_codigo_operacion(WAIT_OP_CODE);
			agregar_a_buffer(p->buffer, &largo, sizeof(uint32_t));
			agregar_a_buffer(p->buffer, recurso, largo);
			enviar_paquete(p, socket_kernel_dispatch);
			
			op_code op_code = recibir_op_code(socket_kernel_dispatch);
			if(op_code != OK) return 0;
			return 1;
		}
		case SIGNAL:
		{
			char* recurso = (char*)list_get(instruccion->parametros, 0);
			uint32_t largo = strlen(recurso) + 1;
			enviar_contexto(contexto, socket_kernel_dispatch);
			t_paquete* p = crear_paquete_codigo_operacion(SIGNAL_OP_CODE);
			agregar_a_buffer(p->buffer, &largo, sizeof(uint32_t));
			agregar_a_buffer(p->buffer, recurso, largo);
			enviar_paquete(p, socket_kernel_dispatch);

			op_code op_code = recibir_op_code(socket_kernel_dispatch);
			if(op_code != OK) return 0;
			return 1;
		}
		case IO_FS_CREATE:
		{
			char* interfaz = (char*)list_get(instruccion->parametros, 0);
			size_t largo_interfaz = strlen(interfaz) + 1;
			char* nombre_archivo = (char*)list_get(instruccion->parametros, 1);
			size_t largo_nombre_archivo = strlen(nombre_archivo) + 1;

			enviar_contexto(contexto, socket_kernel_dispatch);
			t_paquete* p = crear_paquete_codigo_operacion(IO_FS_CREATE_OP_CODE);
			agregar_a_buffer(p->buffer, &largo_interfaz, sizeof(size_t));
			agregar_a_buffer(p->buffer, interfaz, largo_interfaz);
			agregar_a_buffer(p->buffer, &largo_nombre_archivo, sizeof(size_t));
			agregar_a_buffer(p->buffer, nombre_archivo, largo_nombre_archivo);
			enviar_paquete(p, socket_kernel_dispatch);

			return 0;
		}
		case IO_FS_DELETE:
		{
			char* interfaz = (char*)list_get(instruccion->parametros, 0);
			size_t largo_interfaz = strlen(interfaz) + 1;
			char* nombre_archivo = (char*)list_get(instruccion->parametros, 1);
			size_t largo_nombre_archivo = strlen(nombre_archivo) + 1;

			enviar_contexto(contexto, socket_kernel_dispatch);
			t_paquete* p = crear_paquete_codigo_operacion(IO_FS_DELETE_OP_CODE);
			agregar_a_buffer(p->buffer, &largo_interfaz, sizeof(size_t));
			agregar_a_buffer(p->buffer, interfaz, largo_interfaz);
			agregar_a_buffer(p->buffer, &largo_nombre_archivo, sizeof(size_t));
			agregar_a_buffer(p->buffer, nombre_archivo, largo_nombre_archivo);
			enviar_paquete(p, socket_kernel_dispatch);
			
			return 0;
		}
		case IO_FS_TRUNCATE:
		{
			char* interfaz = (char*)list_get(instruccion->parametros, 0);
			size_t largo_interfaz = strlen(interfaz) + 1;
			char* nombre_archivo = (char*)list_get(instruccion->parametros, 1);
			size_t largo_nombre_archivo = strlen(nombre_archivo) + 1;
			char* str_tamanio = (char*)list_get(instruccion->parametros, 2);
			uint32_t tamanio = extraer_entero_de_registro(contexto, registro_str_a_enum(str_tamanio));

			enviar_contexto(contexto, socket_kernel_dispatch);
			t_paquete* p = crear_paquete_codigo_operacion(IO_FS_TRUNCATE_OP_CODE);
			agregar_a_buffer(p->buffer, &largo_interfaz, sizeof(size_t));
			agregar_a_buffer(p->buffer, interfaz, largo_interfaz);
			agregar_a_buffer(p->buffer, &largo_nombre_archivo, sizeof(size_t));
			agregar_a_buffer(p->buffer, nombre_archivo, largo_nombre_archivo);
			agregar_a_buffer(p->buffer, &tamanio, sizeof(uint32_t));
			enviar_paquete(p, socket_kernel_dispatch);
			
			return 0;
		}
		case IO_FS_WRITE:
		{
			char* interfaz = (char*)list_get(instruccion->parametros, 0);
			size_t largo_interfaz = strlen(interfaz) + 1;
			char* nombre_archivo = (char*)list_get(instruccion->parametros, 1);
			size_t largo_nombre_archivo = strlen(nombre_archivo) + 1;

			char* registro_direccion_string = (char*)list_get(instruccion->parametros, 2);
			t_registro registro_direccion = registro_str_a_enum(registro_direccion_string);
			char* registro_tamanio_string = (char*)list_get(instruccion->parametros, 3);
			t_registro registro_tamanio = registro_str_a_enum(registro_tamanio_string);
			char* registro_puntero_string = (char*)list_get(instruccion->parametros, 4);
			t_registro registro_puntero = registro_str_a_enum(registro_puntero_string);

			uint32_t dl = extraer_entero_de_registro(contexto, registro_direccion);
			uint32_t tamanio = extraer_entero_de_registro(contexto, registro_tamanio);
			uint32_t puntero = extraer_entero_de_registro(contexto, registro_puntero);

			enviar_contexto(contexto, socket_kernel_dispatch);
			t_paquete* p = crear_paquete_codigo_operacion(IO_FS_WRITE_OP_CODE);
			agregar_a_buffer(p->buffer, &largo_interfaz, sizeof(size_t));
			agregar_a_buffer(p->buffer, interfaz, largo_interfaz);
			agregar_a_buffer(p->buffer, &largo_nombre_archivo, sizeof(size_t));
			agregar_a_buffer(p->buffer, nombre_archivo, largo_nombre_archivo);
			agregar_a_buffer(p->buffer, &tamanio, sizeof(uint32_t));
			agregar_a_buffer(p->buffer, &puntero, sizeof(uint32_t));
			t_buffer* b = crear_buffer_con_df(tamanio, dl, contexto, NULL);
			agregar_a_buffer(p->buffer, b->stream, b->size);
			enviar_paquete(p, socket_kernel_dispatch);
			liberar_buffer(b);

			return 0;
		}
		case IO_FS_READ:
		{
			char* interfaz = (char*)list_get(instruccion->parametros, 0);
			size_t largo_interfaz = strlen(interfaz) + 1;
			char* nombre_archivo = (char*)list_get(instruccion->parametros, 1);
			size_t largo_nombre_archivo = strlen(nombre_archivo) + 1;

			char* registro_direccion_string = (char*)list_get(instruccion->parametros, 2);
			t_registro registro_direccion = registro_str_a_enum(registro_direccion_string);
			char* registro_tamanio_string = (char*)list_get(instruccion->parametros, 3);
			t_registro registro_tamanio = registro_str_a_enum(registro_tamanio_string);
			char* registro_puntero_string = (char*)list_get(instruccion->parametros, 4);
			t_registro registro_puntero = registro_str_a_enum(registro_puntero_string);

			uint32_t dl = extraer_entero_de_registro(contexto, registro_direccion);
			uint32_t tamanio = extraer_entero_de_registro(contexto, registro_tamanio);
			uint32_t puntero = extraer_entero_de_registro(contexto, registro_puntero);

			enviar_contexto(contexto, socket_kernel_dispatch);
			t_paquete* p = crear_paquete_codigo_operacion(IO_FS_READ_OP_CODE);
			agregar_a_buffer(p->buffer, &largo_interfaz, sizeof(size_t));
			agregar_a_buffer(p->buffer, interfaz, largo_interfaz);
			agregar_a_buffer(p->buffer, &largo_nombre_archivo, sizeof(size_t));
			agregar_a_buffer(p->buffer, nombre_archivo, largo_nombre_archivo);
			agregar_a_buffer(p->buffer, &tamanio, sizeof(uint32_t));
			agregar_a_buffer(p->buffer, &puntero, sizeof(uint32_t));
			t_buffer* b = crear_buffer_con_df(tamanio, dl, contexto, NULL);
			agregar_a_buffer(p->buffer, b->stream, b->size);
			enviar_paquete(p, socket_kernel_dispatch);
			liberar_buffer(b);
			
			return 0;
		}
		case EXIT:
		{
			enviar_contexto(contexto, socket_kernel_dispatch);
			enviar_codigo(EXIT_OP_CODE, socket_kernel_dispatch);
			//log_debug(logger, "Proceso %d finalizado", contexto->pid);
			return 0;
		}
		default:
			exit(0);
	}
}	

uint32_t extraer_entero_de_registro(t_contexto* contexto, t_registro reg) {
	uint32_t n = 0;
	if(reg >= 0 && reg <= 3) {
		n = contexto->registros[reg];
	}
	else if(reg >= 4 && reg <= 7) {
		n = contexto->registros_ext[reg - 4];
	}
	else if(reg >= 8 && reg <= 9) {
		n = contexto->registros_dir[reg - 8];
	}
	return n;
}

// Usada para cargar paquete para IO (y que ésta solicite lectura/escritura a memoria) 
// y para solicitar lectura desde CPU
t_buffer* crear_buffer_con_df(uint32_t tamanio, uint32_t dl, t_contexto* contexto, char** log) {
	uint32_t pags_a_leer = 0;
	uint32_t df = 0;
	uint32_t df_inicial = 0;
	uint32_t tam_primer_contenido = 0;
	uint32_t tam_ultimo_contenido = 0;

	//
		// {DF | TAM PRIMER CONTENIDO - DF - DF | TAM ULTIMO CONTENIDO}
		// Mando a leer la cantidad de páginas equivalentes al tamaño del registro destino. Buffer: [DF1|DF2|...|Tamaño a leer de última página]
		// -Leo desde el principio de pagina una pagina entera OK
		// -Leo desde el principio de pagina una pagina y un poco mas OK
		// -Leo desde el principio de pagina menos de una pagina OK
		// -Mitad de pagina hasta final OK
		// -Mitad de pagina hasta poco mas de siguiente pagina OK
		// -Mitad de pagina hasta final de siguiente pagina OK
	//

		t_buffer* b = crear_buffer();
		agregar_a_buffer(b, &contexto->pid, sizeof(uint32_t));	
		
		pags_a_leer = tamanio / tam_pagina + 1;
		if(tamanio < tam_pagina) pags_a_leer += 1;
		agregar_a_buffer(b, &pags_a_leer, sizeof(uint32_t));
		
		df = dl_a_df(dl, contexto->pid);
		agregar_a_buffer(b, &df, sizeof(uint32_t));
		tam_primer_contenido = tam_pagina - df % tam_pagina; 
		if((df == 0 && tamanio < tam_pagina) || (tamanio < tam_primer_contenido)) tam_primer_contenido = tamanio;
		dl += tam_primer_contenido;
		agregar_a_buffer(b, &tam_primer_contenido, sizeof(uint32_t));

		for(int i = 1; i < pags_a_leer; i++) { 
			if(i == pags_a_leer - 1) {
				tam_ultimo_contenido = calcular_ultimo_contenido(tamanio, tam_primer_contenido, pags_a_leer, tam_pagina);
				if(tam_ultimo_contenido != 0) {  
					df = dl_a_df(dl, contexto->pid);
				}
				else df = 0; 
			}
			else {
				df = dl_a_df(dl, contexto->pid);
				dl += tam_pagina;
			}
			agregar_a_buffer(b, &df, sizeof(uint32_t));
		}
		agregar_a_buffer(b, &tam_ultimo_contenido, sizeof(uint32_t));
		
		if(log != NULL) string_append_with_format(log, "PID: %d - Acción: LEER - Dirección Física: %d - Valor: ", contexto->pid, df_inicial);

		return b;
	//
}

// Usada para solicitar escritura desde CPU
void solicitar_escritura(uint32_t tamanio, uint32_t dl, t_contexto* contexto, uint8_t* contenido_ext_8) {
	uint32_t pags_a_escribir = 0;
	uint32_t df = 0;
	uint32_t df_inicial = 0;
	uint32_t tam_primer_contenido = 0;
	uint32_t tam_ultimo_contenido = 0;

	// Carga de buffer
		t_paquete* p = crear_paquete_codigo_operacion(ESCRITURA);
		agregar_a_buffer(p->buffer, &contexto->pid, sizeof(uint32_t));
		pags_a_escribir = tamanio / tam_pagina + 1;
		if(tamanio < tam_pagina) pags_a_escribir += 1;
		agregar_a_buffer(p->buffer, &pags_a_escribir, sizeof(uint32_t));
		//log_trace(logger, "pags_a_escribir %d", pags_a_escribir);

		//Primera pagina
		df = dl_a_df(dl, contexto->pid);
		df_inicial = df;
		tam_primer_contenido = tam_pagina - df % tam_pagina; 
		if((df == 0 && tamanio < tam_pagina) || (tamanio < tam_primer_contenido)) tam_primer_contenido = tamanio;
		dl += tam_primer_contenido;
		agregar_a_buffer(p->buffer, &df, sizeof(uint32_t));
		agregar_a_buffer(p->buffer, &tam_primer_contenido, sizeof(uint32_t));
		tam_ultimo_contenido = calcular_ultimo_contenido(tamanio, tam_primer_contenido, pags_a_escribir, tam_pagina);
		for(int j = 0; j < tam_primer_contenido; j++) {
			//log_trace(logger, "Byte %d", contenido_ext_8[j]);
			agregar_a_buffer(p->buffer, &contenido_ext_8[j], sizeof(uint8_t));
		}

		for(int i = 1; i < pags_a_escribir; i++) { 
			if(i == pags_a_escribir - 1) {
				//log_trace(logger, "Ultimo contenido %d", tam_ultimo_contenido); 
				if(tam_ultimo_contenido == 0) df = 0;
				else df = dl_a_df(dl, contexto->pid); 
				agregar_a_buffer(p->buffer, &df, sizeof(uint32_t));
				agregar_a_buffer(p->buffer, &tam_ultimo_contenido, sizeof(uint32_t));

				int ultimos_bytes = tamanio - tam_ultimo_contenido;
				for(int j = ultimos_bytes; tam_ultimo_contenido != 0; tam_ultimo_contenido--, j++) { // TODO: chequear segundo incremento
					agregar_a_buffer(p->buffer, &contenido_ext_8[j], sizeof(uint8_t));
					//log_trace(logger, "Byte %d", contenido_ext_8[j]);
				}
			}
			else {
				uint32_t bytes_a_escribir = tamanio - tam_primer_contenido - tam_ultimo_contenido;
				i--; 
				for(int j = 0; j < bytes_a_escribir; j++) {
					if(j % tam_pagina == 0) {
						df = dl_a_df(dl, contexto->pid);
						agregar_a_buffer(p->buffer, &df, sizeof(uint32_t));
						//log_trace(logger, "DF %d", df);
						dl += tam_pagina;
						i++;
					}
					agregar_a_buffer(p->buffer, &contenido_ext_8[j + tam_primer_contenido], sizeof(uint8_t)); // TODO: chequear indice
					//log_trace(logger, "Byte %d", contenido_ext_8[j + tam_primer_contenido]);
				}
			}
		}
		enviar_paquete(p, socket_memoria);
	//

	if(tamanio == 1) log_info(logger, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %d", contexto->pid, df_inicial, contenido_ext_8[0]);
	else log_info(logger, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %s", contexto->pid, df_inicial, (contenido_ext_8));

	free(contenido_ext_8);
	esperar_ok(socket_memoria, logger);
}

uint32_t dl_a_df(uint32_t dl, uint32_t pid) {
	uint32_t pagina = floor(dl / tam_pagina);
	int busqueda_tlb = consultar_tlb(pagina, pid);
	uint32_t desplazamiento = dl - pagina * tam_pagina;

	if(busqueda_tlb == -1) {
		int marco = 0;
		solicitar_marco_a_memoria(pagina, pid);

		t_paquete* p = recibir_paquete(socket_memoria);
		//log_trace(logger, "OpCode %d", p->codigo_operacion);
		if (p->codigo_operacion == OK)
		{
			memcpy(&marco, p->buffer->stream, sizeof(uint32_t));
			if(hay_tlb) agregar_entrada(pid, pagina, marco);
			log_info(logger, "PID: %d - OBTENER MARCO - Página: %d - Marco: %d.", pid, pagina, marco);
			return marco * tam_pagina + desplazamiento;
		} 
		else { log_error(logger, "Código inválido recibido luego de solicitar marco."); exit(EXIT_FAILURE); }
	}
	return (uint32_t)busqueda_tlb * tam_pagina + desplazamiento;
}

int consultar_tlb(uint32_t pagina, uint32_t pid) {
	if(hay_tlb == false) return -1;
	if(!list_is_empty(tlb)) {
		for(int i = 0; i < list_size(tlb); i++) {
			t_entrada_tlb* e = list_get(tlb, i);
			if(e->pid == pid) {
				if(e->pagina == pagina) {
					log_info(logger, "PID: %d - TLB HIT - Pagina: %d", pid, pagina);
					if(algoritmo == 1) {
						time_t ultimo_acceso = 0;
						time(&ultimo_acceso);
						e->ultimo_acceso = ultimo_acceso;
					}
					//log_trace(logger, "Marco %d", e->marco);
					return e->marco;
				}
			}
		}
		log_info(logger, "PID: %d - TLB MISS - Pagina: %d", pid, pagina);
		return -1;
	}
	log_info(logger, "PID: %d - TLB MISS - Pagina: %d", pid, pagina);
	return -1;
}

void agregar_entrada(uint32_t pid, uint32_t pagina, uint32_t marco) {
	if(list_size(tlb) < cantidad_entradas) {
		t_entrada_tlb* e = malloc(sizeof(t_entrada_tlb));
		e->pid = pid;
		e->pagina = pagina;
		e->marco = marco;
		list_add(tlb, e);
		if(algoritmo == 0) list_add(proximas_victimas, e);
		if(algoritmo == 1) {
			e->ultimo_acceso = clock();
		}
	}
	else reemplazar_entrada(pid, pagina, marco);
}

void reemplazar_entrada(uint32_t pid, uint32_t pagina, uint32_t marco) {   
	t_entrada_tlb* victima;
	if(algoritmo == 0)
    {   
        victima = list_remove(proximas_victimas, 0);
    } 
    if(algoritmo == 1)
    {
		victima = list_get(tlb, 0);
		for(int i = 1; i < cantidad_entradas; i++) {
			t_entrada_tlb* victima2 = list_get(tlb, i);
			if(victima2->ultimo_acceso < victima->ultimo_acceso) {
				victima = victima2;
			}
		} 
    } 
	log_trace(logger, "Reemplazo página %d marco %d", victima->pagina, victima->marco);
	list_remove_element(tlb, victima);
	free(victima);
	agregar_entrada(pid, pagina, marco);
}

void solicitar_marco_a_memoria(uint32_t pagina, uint32_t pid) {
    t_paquete* p = crear_paquete_codigo_operacion(SOLICITAR_MARCO);
	agregar_a_buffer(p->buffer, &pid, sizeof(uint32_t));
	agregar_a_buffer(p->buffer, &pagina, sizeof(uint32_t));
	enviar_paquete(p, socket_memoria);
}

t_instruccion* deserializar_instruccion(t_buffer* buffer, uint32_t pid) { 
	char* log = NULL;
	t_instruccion* instruccion =  malloc(sizeof(t_instruccion));
	instruccion->codigo_instruccion = 0;
	instruccion->parametros = list_create();
	void* aux = buffer->stream;
	
	memcpy(&instruccion->codigo_instruccion, buffer->stream, sizeof(t_codigo_instruccion));
	aux += sizeof(t_codigo_instruccion);

	log = string_from_format("PID: %d - Ejecutando: %s - ", pid, instruccion_string[instruccion->codigo_instruccion]);

	int nro_params;
	memcpy(&nro_params, aux, sizeof(int));
	aux += sizeof(int);

	for(int i = 0; i < nro_params; i++)
	{
		size_t largo_parametro = 0;

		memcpy(&largo_parametro, aux, sizeof(size_t));
		aux += sizeof(size_t);

		char* parametro = malloc(largo_parametro);
		memcpy(parametro, aux, largo_parametro);
		aux += largo_parametro;
		
		if(i == nro_params) string_append_with_format(&log, "%s.", parametro);
		string_append_with_format(&log, "%s ", parametro);
		
		list_add(instruccion->parametros, parametro);
	}

	log_info(logger, "%s", log);
    return instruccion;
}

void conectar_modulos() {
    socket_memoria = start_client_module("MEMORIA", IP_CONFIG_PATH);
	log_info(logger, "Conexion con servidor MEMORIA en socket %d", socket_memoria);

	recv(socket_memoria, &tam_pagina, sizeof(uint32_t), MSG_WAITALL);

	socket_cpu_interrupt = start_server_module("CPU_INTERRUPT", IP_CONFIG_PATH);
	log_info(logger, "Creacion de servidor CPU INTERRUPT en socket %d", socket_cpu_dispatch);

	socket_cpu_dispatch = start_server_module("CPU_DISPATCH", IP_CONFIG_PATH);
	log_info(logger, "Creacion de servidor CPU DISPATCH en socket %d", socket_cpu_dispatch);

	socket_kernel_interrupt = accept(socket_cpu_interrupt, NULL, NULL);
	log_info(logger, "Conexion INTERRUPT con cliente KERNEL en socket %i", socket_kernel_interrupt);

	pthread_t hilo_interrupt;
	pthread_create(&hilo_interrupt, NULL, (void*)check_interrupt, NULL);
	pthread_detach(hilo_interrupt);

	socket_kernel_dispatch = accept(socket_cpu_dispatch, NULL, NULL);
	log_info(logger, "Conexion DISPATCH con cliente KERNEL en socket %i", socket_kernel_dispatch);	
}

t_registro registro_str_a_enum(char* reg) {
	if(strcmp(reg, "AX") == 0) {
		return AX;
	} else if(strcmp(reg, "BX") == 0) {
		return BX;
	} else if(strcmp(reg, "CX") == 0) {
		return CX;
	} else if(strcmp(reg, "DX") == 0) {
		return DX;
	} else if(strcmp(reg, "EAX") == 0) {
		return EAX;
	} else if(strcmp(reg, "EBX") == 0) {
		return EBX;
	} else if(strcmp(reg, "ECX") == 0) {
		return ECX;
	} else if(strcmp(reg, "EDX") == 0) {
		return EDX;
	} else if(strcmp(reg, "DI") == 0) {
		return DI;
	} else if(strcmp(reg, "SI") == 0) {
		return SI;
	} else if(strcmp(reg, "PC") == 0) {
		return PC;
	}
	else exit(-1);
}