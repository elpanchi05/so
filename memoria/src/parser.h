#include <stdlib.h>
#include <stdio.h>
#include <utils/structures.h>
#include <utils/paquete.h>
#include <utils/setup.h>
#include <utils/socket.h>
#include <utils/environment_variables.h>
#include <commons/collections/list.h>

t_list* parsear_archivo(FILE* archivo);
void crear_instruccion(FILE* archivo, t_list* lista_instrucciones);
void crear_diccionario();
int tamano_vector(char** vector);