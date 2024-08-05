#include <commons/collections/list.h>
#include <commons/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include "structures.h"

t_buffer* crear_buffer();
void agregar_a_buffer(t_buffer* buffer, void* magic, uint32_t size);
void agregar_op_code(t_buffer* buffer, op_code operation_code);
t_buffer* recibir_buffer(uint32_t socket_cliente);
void enviar_buffer(uint32_t socket, t_buffer* buffer);
void liberar_buffer(t_buffer* buffer);

t_paquete* crear_paquete();
t_paquete* crear_paquete_codigo_operacion(op_code op_code);
void* serializar_paquete(t_paquete* paquete, uint32_t bytes);
t_paquete* recibir_paquete(uint32_t socket);
void enviar_paquete(t_paquete* paquete, uint32_t socket_cliente);
void eliminar_paquete(t_paquete* paquete);

void crear_proceso_en_memoria(uint32_t pid, char *path, uint32_t fd);
void enviar_contexto(t_contexto *ctx, uint32_t fd);
void serializar_contexto(t_contexto *ctx, t_paquete *paquete);
void serializar_registros(t_paquete *paquete, t_contexto *ctx);
t_contexto* deserializar_contexto(t_buffer* buffer);

op_code recibir_op_code(uint32_t socket);
void esperar_ok(uint32_t socket, t_log* logger);
void enviar_codigo(op_code op_code, uint32_t socket);
void enviar_codigo_solo(op_code op_code, uint32_t socket);

uint32_t calcular_ultimo_contenido(uint32_t tam_reg, uint32_t tam_primer_contenido, uint32_t cantidad_paginas, uint32_t tam_pagina);