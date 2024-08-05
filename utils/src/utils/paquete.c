#include "paquete.h"

// BUFFER

t_buffer* crear_buffer() {
    t_buffer* buffer = malloc(sizeof(t_buffer));

    if (buffer != NULL) {
        buffer->size = 0;
        buffer->stream = NULL;
    }

    return buffer;
}

void agregar_a_buffer(t_buffer* buffer, void* magic, uint32_t size) {
    if (buffer->stream == NULL) {
        buffer->stream = malloc(buffer->size + size);
    } else {
        buffer->stream = realloc(buffer->stream, buffer->size + size);
    }

    if (buffer->stream != NULL) {
        memcpy(buffer->stream + buffer->size, magic, size);
        buffer->size += size;
    }
}

void agregar_op_code(t_buffer* buffer, op_code operation_code) {
    agregar_a_buffer(buffer, &operation_code, sizeof(int));
}

t_buffer* recibir_buffer(uint32_t socket_cliente) {
    t_buffer* buffer = crear_buffer();
    
    recv(socket_cliente, &buffer->size, sizeof(uint32_t), MSG_WAITALL);
    buffer->stream = malloc(buffer->size);
    recv(socket_cliente, buffer->stream, buffer->size, MSG_WAITALL);

    return buffer;
}

void enviar_buffer(uint32_t socket, t_buffer* buffer) {
    send(socket, &buffer->size, sizeof(uint32_t), 0);
    send(socket, buffer->stream, buffer->size, 0);

    liberar_buffer(buffer);
}

void liberar_buffer(t_buffer* buffer) {
    free(buffer->stream);
    free(buffer);
}

// PAQUETE

t_paquete* crear_paquete_codigo_operacion(op_code op_code) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = op_code;

    return paquete;
}

void enviar_codigo(op_code op_code, uint32_t socket) {
    t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = op_code;
	int i = 0;
	agregar_a_buffer(paquete->buffer, &i, sizeof(int));
	enviar_paquete(paquete, socket);
}

void enviar_int(uint32_t n, uint32_t socket) {
	t_paquete* paquete = crear_paquete();
    paquete->codigo_operacion = 0;
	agregar_a_buffer(paquete->buffer, &n, sizeof(uint32_t));
	enviar_paquete(paquete, socket);
}

t_paquete* crear_paquete() {
    t_paquete* paquete = malloc(sizeof(t_paquete));
    paquete->buffer = crear_buffer();

    return paquete;
}

t_paquete* recibir_paquete(uint32_t socket) {
    t_paquete* paquete = crear_paquete();

    recv(socket, &paquete->codigo_operacion, sizeof(op_code), MSG_WAITALL);
    t_buffer* buffer = recibir_buffer(socket);
    paquete->buffer = buffer;

    return paquete;
}

void* serializar_paquete(t_paquete* paquete, uint32_t bytes) {
	void * magic = malloc(bytes);
	uint32_t desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(op_code));
	desplazamiento += sizeof(op_code);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(uint32_t));
	desplazamiento += sizeof(uint32_t);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento += paquete->buffer->size;

	return magic;
}

void enviar_paquete(t_paquete* paquete, uint32_t socket_cliente) {
	uint32_t bytes = paquete->buffer->size + 2*sizeof(uint32_t);
	void* a_enviar = serializar_paquete(paquete, bytes);
	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}

void eliminar_paquete(t_paquete* paquete) {
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void crear_proceso_en_memoria(uint32_t pid, char *path, uint32_t fd) {
	t_paquete *paquete = crear_paquete_codigo_operacion(CREAR_PROCESO);
	size_t largo_string = strlen(path) + 1;
	agregar_a_buffer(paquete->buffer, &pid, sizeof(uint32_t));
	agregar_a_buffer(paquete->buffer, &largo_string, sizeof(size_t));
	agregar_a_buffer(paquete->buffer, path, largo_string);

	enviar_paquete(paquete, fd);
}

void enviar_contexto(t_contexto *ctx, uint32_t fd) {
	t_paquete *paquete = crear_paquete_codigo_operacion(CONTEXTO);

	serializar_contexto(ctx, paquete);

	enviar_paquete(paquete, fd);
}

void serializar_contexto(t_contexto *ctx, t_paquete *paquete) {
	agregar_a_buffer(paquete->buffer, &(ctx->pid), sizeof(int));
	agregar_a_buffer(paquete->buffer, &(ctx->program_counter), sizeof(int));
	serializar_registros(paquete, ctx);
}

void serializar_registros(t_paquete *paquete, t_contexto *ctx) {
	agregar_a_buffer(paquete->buffer, &(ctx->registros[0]), sizeof(u_int8_t));
	agregar_a_buffer(paquete->buffer, &(ctx->registros[1]), sizeof(u_int8_t));
	agregar_a_buffer(paquete->buffer, &(ctx->registros[2]), sizeof(u_int8_t));
	agregar_a_buffer(paquete->buffer, &(ctx->registros[3]), sizeof(u_int8_t));
    
    agregar_a_buffer(paquete->buffer, &(ctx->registros_ext[0]), sizeof(u_int32_t));
	agregar_a_buffer(paquete->buffer, &(ctx->registros_ext[1]), sizeof(u_int32_t));
	agregar_a_buffer(paquete->buffer, &(ctx->registros_ext[2]), sizeof(u_int32_t));
	agregar_a_buffer(paquete->buffer, &(ctx->registros_ext[3]), sizeof(u_int32_t));

    agregar_a_buffer(paquete->buffer, &(ctx->registros_dir[0]), sizeof(u_int32_t));
	agregar_a_buffer(paquete->buffer, &(ctx->registros_dir[1]), sizeof(u_int32_t));
}

t_contexto* deserializar_contexto(t_buffer* buffer) {
	t_contexto* contexto = malloc(sizeof(t_contexto));
	contexto->pid = 0;
	contexto->program_counter = 0;
	
	void* aux = buffer->stream;

	memcpy(&contexto->pid, aux, sizeof(uint32_t));
	aux += sizeof(uint32_t);

	memcpy(&contexto->program_counter, aux, sizeof(uint32_t));
	aux += sizeof(uint32_t);
	
	for (int i = 0; i < 4; i++)
	{
		memcpy(&contexto->registros[i], aux, sizeof(uint8_t));
		aux += sizeof(uint8_t);
		//printf("REG %d\n", contexto->registros[i]);
	}
	for (int i = 0; i < 4; i++)
	{
		memcpy(&contexto->registros_ext[i], aux, sizeof(uint32_t));
		aux += sizeof(uint32_t);
		//printf("REG %d\n", contexto->registros_ext[i]);
	}
	for (int i = 0; i < 2; i++)
	{
		memcpy(&contexto->registros_dir[i], aux, sizeof(uint32_t));
		aux += sizeof(uint32_t);
		//printf("REG %d\n", contexto->registros_dir[i]);
	}

	return contexto;
}

void enviar_codigo_solo(op_code op_code, uint32_t socket) {
	send(socket, &op_code, sizeof(op_code), 0);
}

op_code recibir_op_code(uint32_t socket) {
	op_code op_code = -1;
	recv(socket, &op_code, sizeof(op_code), MSG_WAITALL);
	return op_code;
}

void esperar_ok(uint32_t socket, t_log* logger)
{
	int op_code = recibir_op_code(socket);
	if(op_code != 1) {
		log_error(logger, "Error en la petición");
		exit(-1);
	}
}

uint32_t calcular_ultimo_contenido(uint32_t tam_reg, uint32_t tam_primer_contenido, uint32_t cantidad_paginas, uint32_t tam_pagina) {
	int temp_tam_ultimo_contenido = (int)tam_reg - (int)(tam_primer_contenido + (cantidad_paginas - 2) * tam_pagina);
	if(temp_tam_ultimo_contenido < 0) {
		return 0;
	} else {
		return (unsigned int)temp_tam_ultimo_contenido;
	}
}